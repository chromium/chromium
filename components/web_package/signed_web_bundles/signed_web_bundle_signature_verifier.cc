// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"

#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_utils.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"
#include "components/web_package/signed_web_bundles/integrity_block_parser.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"
#include "crypto/secure_hash.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace web_package {

namespace {

std::vector<uint8_t> CreateIntegrityBlockCbor(
    const SignedWebBundleIntegrityBlock& integrity_block) {
  std::vector<uint8_t> ib_cbor;

  // 0x84 is the encoding byte for an array of length 4.
  ib_cbor.push_back(0x84);
  base::Extend(ib_cbor,
               *cbor::Writer::Write(cbor::Value(kIntegrityBlockMagicBytes)));
  base::Extend(ib_cbor, *cbor::Writer::Write(
                            cbor::Value(kIntegrityBlockV2VersionBytes)));
  // We cannot parse `attributes().cbor()` back to a cbor::Value as they
  // technically represent untrusted input.
  base::Extend(ib_cbor, integrity_block.attributes_cbor());
  // Encode an empty signature array.
  base::Extend(ib_cbor,
               *cbor::Writer::Write(cbor::Value(cbor::Value::ArrayValue())));

  return ib_cbor;
}

base::expected<void, SignedWebBundleSignatureVerifier::Error>
ValidateWebBundleId(const std::string& web_bundle_id,
                    const SignedWebBundleSignatureStack& signatures) {
  return IdentityValidator::GetInstance()
      ->ValidateWebBundleIdentity(web_bundle_id, signatures.public_keys())
      .transform_error([](const std::string& error) {
        return SignedWebBundleSignatureVerifier::Error::ForWebBundleIdError(
            error);
      });
}

}  // namespace

SignedWebBundleSignatureVerifier::SignedWebBundleSignatureVerifier() {
  static_assert(kSHA512DigestLength == SHA512_DIGEST_LENGTH);
}

SignedWebBundleSignatureVerifier::~SignedWebBundleSignatureVerifier() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SignedWebBundleSignatureVerifier::SetWebBundleChunkSizeForTesting(
    uint64_t web_bundle_chunk_size) {
  web_bundle_chunk_size_ = web_bundle_chunk_size;
}

void SignedWebBundleSignatureVerifier::VerifySignatures(
    base::File file,
    SignedWebBundleIntegrityBlock integrity_block,
    SignatureVerificationCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t integrity_block_size =
      base::checked_cast<int64_t>(integrity_block.size_in_bytes());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &SignedWebBundleSignatureVerifier::CalculateHashOfUnsignedWebBundle,
          std::move(file), base::checked_cast<int64_t>(web_bundle_chunk_size_),
          integrity_block_size),
      base::BindOnce(&SignedWebBundleSignatureVerifier::
                         OnHashOfUnsignedWebBundleCalculated,
                     weak_factory_.GetWeakPtr(), std::move(integrity_block),
                     std::move(callback)));
}

// static
base::expected<SignedWebBundleSignatureVerifier::SHA512Digest, std::string>
SignedWebBundleSignatureVerifier::CalculateHashOfUnsignedWebBundle(
    base::File file,
    int64_t web_bundle_chunk_size,
    int64_t integrity_block_size) {
  // No `DCHECK_CALLED_ON_VALID_SEQUENCE` annotation here since this runs on a
  // different sequence where blocking is allowed.

  int64_t file_length = file.GetLength();
  if (file_length < 0) {
    return base::unexpected(base::File::ErrorToString(file.GetLastFileError()));
  }

  auto secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA512);

  // Calculate the hash of the Signed Web Bundle excluding its integrity block.
  // The file might be too big to read it into memory all at once, which is why
  // it is read in chunks of size `web_bundle_chunk_size`.
  for (int64_t offset = integrity_block_size; offset < file_length;) {
    std::vector<uint8_t> data(
        // The size of the last chunk (`file_length - offset`) might be smaller
        // than `web_bundle_chunk_size`. Make sure to only reserve as much
        // memory as is really needed.
        std::min(web_bundle_chunk_size, file_length - offset));
    std::optional<size_t> bytes_read = file.Read(offset, data);
    if (!bytes_read) {
      return base::unexpected(
          base::File::ErrorToString(file.GetLastFileError()));
    }
    data.resize(*bytes_read);
    secure_hash->Update(data.data(), data.size());

    if (!base::CheckAdd(offset, *bytes_read).AssignIfValid(&offset)) {
      return base::unexpected("The Signed Web Bundle is too large.");
    }
  }

  CHECK_EQ(kSHA512DigestLength, secure_hash->GetHashLength());
  SHA512Digest digest;
  secure_hash->Finish(digest.data(), digest.size());
  return digest;
}

void SignedWebBundleSignatureVerifier::OnHashOfUnsignedWebBundleCalculated(
    SignedWebBundleIntegrityBlock integrity_block,
    SignatureVerificationCallback callback,
    base::expected<SHA512Digest, std::string> unsigned_web_bundle_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RETURN_IF_ERROR(unsigned_web_bundle_hash, [&](std::string error) {
    std::move(callback).Run(
        base::unexpected(Error::ForInternalError(std::move(error))));
  });

  // The algorithm shown in [1] is a more abstract view of the verification
  // process, i.e., it does not take the actual technical limitations imposed by
  // the browser architecture into account:
  //
  // Instead of reading the integrity block from the Signed Web Bundle and
  // popping the top signature stack entry from the signature stack to create an
  // empty integrity block, we create an empty integrity block here from
  // scratch. This is because we cannot do any CBOR parsing of untrusted input
  // (like the Signed Web Bundle) outside of the data decoder process due to the
  // "Rule Of 2". Any CBOR parsing must occur in `WebBundleParser`.
  //
  // Similarly, magic bytes and version of the integrity block are read deep
  // within the low-level `IntegrityBlockParser`.
  //
  // [1]
  // https://github.com/WICG/webpackage/blob/main/explainers/integrity-signature.md

  std::move(callback).Run(VerifyWithHashForIntegrityBlock(
      *unsigned_web_bundle_hash, std::move(integrity_block)));
}

base::expected<void, SignedWebBundleSignatureVerifier::Error>
SignedWebBundleSignatureVerifier::VerifyWithHashForIntegrityBlock(
    SHA512Digest unsigned_web_bundle_hash,
    SignedWebBundleIntegrityBlock integrity_block) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& signatures = integrity_block.signature_stack();
  RETURN_IF_ERROR(ValidateWebBundleId(integrity_block.web_bundle_id().id(),
                                      integrity_block.signature_stack()));

  std::vector<uint8_t> ib_cbor = CreateIntegrityBlockCbor(integrity_block);
  for (const auto& signature_stack_entry : signatures.entries()) {
    std::vector<uint8_t> payload_to_verify = CreateSignaturePayload({
        .unsigned_web_bundle_hash = unsigned_web_bundle_hash,
        .integrity_block_cbor = ib_cbor,
        .attributes_cbor = signature_stack_entry.attributes_cbor(),
    });

    bool valid_signature = absl::visit(
        base::Overloaded{[&payload_to_verify](const auto& signature_info) {
                           return signature_info.signature().Verify(
                               payload_to_verify, signature_info.public_key());
                         },
                         [&](const SignedWebBundleSignatureInfoUnknown&) {
                           // For the v2 integrity block, unknown signatures
                           // imply something that the current version of the
                           // browser cannot really process; such entries can
                           // thus be safely skipped.
                           // Note that SignedWebBundleIntegrityBlock can only
                           // be created if there's at least one valid signature
                           // with a known type in the list.
                           return true;
                         }},
        signature_stack_entry.signature_info());

    if (!valid_signature) {
      return base::unexpected(
          Error::ForInvalidSignature("The signature is invalid."));
    }
  }
  return base::ok();
}

}  // namespace web_package
