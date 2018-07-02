// Copyright (c) 2018      The Bitcoin Private developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/joinsplit.h>

#include <init.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <zcash/Zcash.h>
#include <zcash/Proof.hpp>

bool CheckTransactionJoinSplits(const CTransaction& tx, CValidationState &state)
{
    if (tx.vjoinsplit.size() > 0) {
        // Empty output script.
        CScript scriptCode;
        uint256 dataToBeSigned;
        int hashtype = SIGHASH_ALL;
        int flags = SCRIPT_VERIFY_FORKID; // TODO TEMP
        if (flags & SCRIPT_VERIFY_FORKID)
            hashtype |= SIGHASH_FORKID;

        try {
            dataToBeSigned = SignatureHash(scriptCode, tx, NOT_AN_INPUT,
                                           hashtype, (flags & SCRIPT_VERIFY_FORKID) ? FORKID_IN_USE : FORKID_NONE, 0, SigVersion::BASE); //TODO use txdata=nullptr?
        } catch (std::logic_error ex) {
            return state.DoS(100, error("CheckTransactionJoinSplits(): error computing signature hash"),
                             REJECT_INVALID, "error-computing-signature-hash");
        }

        BOOST_STATIC_ASSERT(crypto_sign_PUBLICKEYBYTES == 32);

        // We rely on libsodium to check that the signature is canonical.
        // https://github.com/jedisct1/libsodium/commit/62911edb7ff2275cccd74bf1c8aefcc4d76924e0
        if (crypto_sign_verify_detached(&tx.joinSplitSig[0],
                                        dataToBeSigned.begin(), 32,
                                        tx.joinSplitPubKey.begin()
                ) != 0) {

            return state.DoS(100, error("CheckTransactionJoinSplits(): invalid joinsplit signature"),
                             REJECT_INVALID, "bad-txns-invalid-joinsplit-signature");
        }

        // Ensure that zk-SNARKs verify
        auto verifier = libzcash::ProofVerifier::Strict();
        for (const JSDescription &joinsplit : tx.vjoinsplit) {
            if (!joinsplit.Verify(*pzcashParams, verifier, tx.joinSplitPubKey)) {
                return state.DoS(100, error("CheckTransactionJoinSplits(): joinsplit does not verify"),
                                 REJECT_INVALID, "bad-txns-joinsplit-verification-failed");
            }
        }
    }
    return true;
}
