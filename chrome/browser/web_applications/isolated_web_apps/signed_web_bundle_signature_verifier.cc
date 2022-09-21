// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_verifier.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_integrity_block.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/shared_file.h"
#include "components/web_package/signed_web_bundles/integrity_block_parser.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"
#include "crypto/secure_hash.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace web_app {

namespace {

// Creates a CBOR-encoded integrity block with an empty signature stack.
std::vector<uint8_t> CreateIntegrityBlockCBOR() {
  std::vector<uint8_t> integrity_block;
  integrity_block.insert(
      integrity_block.end(),
      std::begin(web_package::IntegrityBlockParser::kIntegrityBlockMagicBytes),
      std::end(web_package::IntegrityBlockParser::kIntegrityBlockMagicBytes));
  integrity_block.insert(
      integrity_block.end(),
      std::begin(
          web_package::IntegrityBlockParser::kIntegrityBlockVersionMagicBytes),
      std::end(
          web_package::IntegrityBlockParser::kIntegrityBlockVersionMagicBytes));

  // Encode the length of the signature stack array, which is an empty array.
  integrity_block.push_back(static_cast<uint8_t>(0x80));
  return integrity_block;
}

}  // namespace

SignedWebBundleSignatureVerifier::SignedWebBundleSignatureVerifier(
    uint64_t web_bundle_chunk_size)
    : web_bundle_chunk_size_(web_bundle_chunk_size) {}

SignedWebBundleSignatureVerifier::~SignedWebBundleSignatureVerifier() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SignedWebBundleSignatureVerifier::VerifySignatures(
    scoped_refptr<web_package::SharedFile> file,
    SignedWebBundleIntegrityBlock integrity_block,
    SignatureVerificationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t integrity_block_size =
      base::checked_cast<int64_t>(integrity_block.size_in_bytes());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &SignedWebBundleSignatureVerifier::CalculateHashOfUnsignedWebBundle,
          file, base::checked_cast<int64_t>(web_bundle_chunk_size_),
          integrity_block_size),
      base::BindOnce(&SignedWebBundleSignatureVerifier::
                         OnHashOfUnsignedWebBundleCalculated,
                     weak_factory_.GetWeakPtr(), std::move(integrity_block),
                     std::move(callback)));
}

// static
base::expected<
    std::array<uint8_t, SignedWebBundleSignatureVerifier::kSHA512DigestLength>,
    std::string>
SignedWebBundleSignatureVerifier::CalculateHashOfUnsignedWebBundle(
    scoped_refptr<web_package::SharedFile> file,
    int64_t web_bundle_chunk_size,
    int64_t integrity_block_size) {
  // No `DCHECK_CALLED_ON_VALID_SEQUENCE` annotation here since this runs on a
  // different sequence where blocking is allowed.

  int64_t file_length = (*file)->GetLength();
  if (file_length < 0) {
    return base::unexpected(
        base::File::ErrorToString((*file)->GetLastFileError()));
  }

  auto secure_hash = crypto::SecureHash::Create(crypto::SecureHash::SHA512);

  // The unsigned Web Bundle begins after the integrity block, thus this loop
  // initializes `offset` with the size of the integrity block.
  for (int64_t offset = integrity_block_size; offset < file_length;) {
    std::vector<char> data(std::min(web_bundle_chunk_size, file_length));
    int bytes_read = (*file)->Read(offset, data.data(), data.size());
    if (bytes_read < 0) {
      return base::unexpected(
          base::File::ErrorToString((*file)->GetLastFileError()));
    }
    data.resize(bytes_read);
    secure_hash->Update(data.data(), data.size());

    if (!base::CheckAdd(offset, bytes_read).AssignIfValid(&offset)) {
      return base::unexpected("The Signed Web Bundle is too large.");
    }
  }

  std::array<uint8_t, kSHA512DigestLength> unsigned_bundle_hash;
  DCHECK_EQ(static_cast<size_t>(kSHA512DigestLength),
            secure_hash->GetHashLength());
  static_assert(kSHA512DigestLength == SHA512_DIGEST_LENGTH);
  secure_hash->Finish(unsigned_bundle_hash.data(), unsigned_bundle_hash.size());
  return unsigned_bundle_hash;
}

void SignedWebBundleSignatureVerifier::OnHashOfUnsignedWebBundleCalculated(
    SignedWebBundleIntegrityBlock integrity_block,
    SignatureVerificationCallback callback,
    base::expected<std::array<uint8_t, kSHA512DigestLength>, std::string>
        unsigned_web_bundle_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!unsigned_web_bundle_hash.has_value()) {
    std::move(callback).Run(
        Error::ForInternalError(unsigned_web_bundle_hash.error()));
    return;
  }

  if (integrity_block.signature_stack().size() != 1) {
    std::move(callback).Run(Error::ForInvalidSignature(base::StringPrintf(
        "Only a single signature is currently supported, got %zu signatures.",
        integrity_block.signature_stack().size())));
    return;
  }

  const SignedWebBundleSignatureStackEntry& signature_stack_entry =
      integrity_block.signature_stack()[0];

  auto payload_to_verify = web_package::CreateSignaturePayload({
      .unsigned_web_bundle_hash = *unsigned_web_bundle_hash,
      .integrity_block_cbor = CreateIntegrityBlockCBOR(),
      .attributes_cbor = signature_stack_entry.attributes_cbor(),
  });

  if (!signature_stack_entry.signature().Verify(
          payload_to_verify, signature_stack_entry.public_key())) {
    std::move(callback).Run(
        Error::ForInvalidSignature("The signature is invalid."));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

}  // namespace web_app
