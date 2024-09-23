// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_VERIFIER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_VERIFIER_H_

#include <optional>
#include <string>

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {

class SignedWebBundleIntegrityBlock;

// This class can be used to verify the signatures contained in a Signed Web
// Bundle's integrity block.
// github.com/WICG/webpackage/blob/main/explainers/integrity-signature.md
class SignedWebBundleSignatureVerifier {
 public:
  struct Error {
    static Error ForInternalError(const std::string& message) {
      return {.type = Type::kInternalError, .message = message};
    }
    static Error ForInvalidSignature(const std::string& message) {
      return {.type = Type::kSignatureInvalidError, .message = message};
    }
    static Error ForWebBundleIdError(const std::string& message) {
      return {.type = Type::kWebBundleIdError, .message = message};
    }

    enum class Type {
      kInternalError,
      kSignatureInvalidError,
      kWebBundleIdError,
    } type;
    std::string message;
  };

  SignedWebBundleSignatureVerifier();

  // Changes the chunk size in which the Signed Web Bundle is read for
  // calculating its SHA512 hash. Higher values use more RAM, but may
  // potentially be a bit faster. Only used in tests.
  void SetWebBundleChunkSizeForTesting(uint64_t web_bundle_chunk_size);

  virtual ~SignedWebBundleSignatureVerifier();

  using SignatureVerificationCallback =
      base::OnceCallback<void(base::expected<void, Error>)>;

  // Verifies the signatures of the Signed Web Bundle `file` with the integrity
  // block `integrity_block`. Executes the `callback` with `std::nullopt` on
  // success, or an instance of `Error` on error. Only one signature is
  // currently supported.
  //
  // TODO(crbug.com/40239682): Support more than one signature.
  virtual void VerifySignatures(base::File file,
                                SignedWebBundleIntegrityBlock integrity_block,
                                SignatureVerificationCallback callback) const;

 private:
  // We don't use `SHA512_DIGEST_LENGTH` here, because we don't want to include
  // large BoringSSL headers in a header file.
  static constexpr size_t kSHA512DigestLength = 64;

  using SHA512Digest = std::array<uint8_t, kSHA512DigestLength>;

  // Calculate the SHA-512 hash of the Signed Web Bundle excluding the integrity
  // block, i.e., the unsigned Web Bundle.
  static base::expected<SHA512Digest, std::string>
  CalculateHashOfUnsignedWebBundle(base::File file,
                                   int64_t web_bundle_chunk_size,
                                   int64_t integrity_block_size);

  void OnHashOfUnsignedWebBundleCalculated(
      SignedWebBundleIntegrityBlock integrity_block,
      SignatureVerificationCallback callback,
      base::expected<SHA512Digest, std::string> unsigned_web_bundle_hash) const;

  base::expected<void, Error> VerifyWithHashForIntegrityBlock(
      SHA512Digest unsigned_web_bundle_hash,
      SignedWebBundleIntegrityBlock integrity_block) const;

  // The chunk size in which the Signed Web Bundle is read for calculating its
  // SHA512 hash. Default is ~10mb.
  uint64_t web_bundle_chunk_size_ = 10 * 1000 * 1000;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SignedWebBundleSignatureVerifier> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_VERIFIER_H_
