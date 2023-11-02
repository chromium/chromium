// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CRX_VERIFIER_H_
#define COMPONENTS_CRX_FILE_CRX_VERIFIER_H_

#include <stdint.h>
#include <string>
#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace crx_file {

enum class VerifierFormat {
  CRX3,                            // Accept only Crx3.
  CRX3_WITH_TEST_PUBLISHER_PROOF,  // Accept only Crx3 with a test or production
                                   // publisher proof.
  CRX3_WITH_PUBLISHER_PROOF,       // Accept only Crx3 with a production
                                   // publisher proof.
};

enum class VerifierResult {
  OK_FULL,   // The file verifies as a correct full CRX file.
  OK_DELTA,  // The file verifies as a correct differential CRX file.
  ERROR_FILE_NOT_READABLE,      // Cannot open the CRX file.
  ERROR_HEADER_INVALID,         // Failed to parse or understand CRX header.
  ERROR_EXPECTED_HASH_INVALID,  // Expected hash is not well-formed.
  ERROR_FILE_HASH_FAILED,       // The file's actual hash != the expected hash.
  ERROR_SIGNATURE_INITIALIZATION_FAILED,  // A signature or key is malformed.
  ERROR_SIGNATURE_VERIFICATION_FAILED,    // A signature doesn't match.
  ERROR_REQUIRED_PROOF_MISSING,           // RequireKeyProof was unsatisfied.
};

// Verify the file at |crx_path| as a valid Crx of |format|. The Crx must be
// well-formed, contain no invalid proofs, match the |required_file_hash| (if
// non-empty), and contain a proof with each of the |required_key_hashes|.
// If and only if this function returns OK_FULL or OK_DELTA, and only if
// |public_key| / |crx_id| are non-null, they will be updated to contain the
// public key (PEM format, without the header/footer) and crx id (encoded in
// base16 using the characters [a-p]). |compressed_verified_contents| will be
// updated if it is non-null and if the verified contents are present in the
// unsigned section of the header.
VerifierResult Verify(
    const base::FilePath& crx_path,
    const VerifierFormat& format,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    const std::vector<uint8_t>& required_file_hash,
    std::string* public_key,
    std::string* crx_id,
    std::vector<uint8_t>* compressed_verified_contents);

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CRX_VERIFIER_H_
