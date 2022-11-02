// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_VERIFIER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_VERIFIER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_package {

class SharedFile;
class SignedWebBundleIntegrityBlock;

// This class can be used to verify the signatures contained in a Signed Web
// Bundle's integrity block. Currently, only one signature is supported, as
// described in the explainer here:
// github.com/WICG/webpackage/blob/main/explainers/integrity-signature.md
//
// TODO(crbug.com/1366303): Support more than one signature.
class SignedWebBundleSignatureVerifier {
 public:
  struct Error {
    static Error ForInternalError(const std::string& message) {
      return {.type = Type::kInternalError, .message = message};
    }
    static Error ForInvalidSignature(const std::string& message) {
      return {.type = Type::kSignatureInvalidError, .message = message};
    }

    enum class Type { kInternalError, kSignatureInvalidError };
    Type type;
    std::string message;
  };

  // Takes the chunk size in which the Signed Web Bundle is read for calculating
  // its SHA512 hash. Higher values use more RAM, but may potentially be a bit
  // faster. It is exposed here instead of being a constant mainly for testing.
  explicit SignedWebBundleSignatureVerifier(
      uint64_t web_bundle_chunk_size = 10 * 1000 * 1000);

  virtual ~SignedWebBundleSignatureVerifier();

  using SignatureVerificationCallback =
      base::OnceCallback<void(absl::optional<Error>)>;

  // Verifies the signatures of the Signed Web Bundle `file` with the integrity
  // block `integrity_block`. Executes the `callback` with `absl::nullopt` on
  // success, or an instance of `Error` on error. Only one signature is
  // currently supported.
  //
  // TODO(crbug.com/1366303): Support more than one signature.
  virtual void VerifySignatures(scoped_refptr<SharedFile> file,
                                SignedWebBundleIntegrityBlock integrity_block,
                                SignatureVerificationCallback callback);

 private:
  // We don't use `SHA512_DIGEST_LENGTH` here, because we don't want to include
  // large BoringSSL headers in a header file.
  static constexpr size_t kSHA512DigestLength = 64;

  // Calculate the SHA512 hash of the Signed Web Bundle excluding the integrity
  // block, i.e., the unsigned Web Bundle.
  static base::expected<std::array<uint8_t, kSHA512DigestLength>, std::string>
  CalculateHashOfUnsignedWebBundle(scoped_refptr<SharedFile> file,
                                   int64_t web_bundle_chunk_size,
                                   int64_t integrity_block_size);

  void OnHashOfUnsignedWebBundleCalculated(
      SignedWebBundleIntegrityBlock integrity_block,
      SignatureVerificationCallback callback,
      base::expected<std::array<uint8_t, kSHA512DigestLength>, std::string>
          unsigned_web_bundle_hash);

  const uint64_t web_bundle_chunk_size_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SignedWebBundleSignatureVerifier> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_SIGNATURE_VERIFIER_H_
