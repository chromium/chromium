// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"

#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/test_future.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"

namespace web_package::test {

namespace {

mojom::SignatureInfoPtr CreateSignatureInfo(
    const Ed25519PublicKey& public_key,
    base::span<const uint8_t> signature) {
  auto ed25519_signature_info = mojom::SignatureInfoEd25519::New();
  ed25519_signature_info->public_key = public_key;
  ed25519_signature_info->signature = *Ed25519Signature::Create(signature);
  return mojom::SignatureInfo::NewEd25519(std::move(ed25519_signature_info));
}

mojom::SignatureInfoPtr CreateSignatureInfo(
    const EcdsaP256PublicKey& public_key,
    base::span<const uint8_t> signature) {
  auto ecdsa_p256_sha256_signature_info =
      mojom::SignatureInfoEcdsaP256SHA256::New();
  ecdsa_p256_sha256_signature_info->public_key = public_key;
  ecdsa_p256_sha256_signature_info->signature =
      *EcdsaP256SHA256Signature::Create(signature);
  return mojom::SignatureInfo::NewEcdsaP256Sha256(
      std::move(ecdsa_p256_sha256_signature_info));
}

}  // namespace

FakeSignatureVerifier::FakeSignatureVerifier(
    std::optional<SignedWebBundleSignatureVerifier::Error> error,
    base::RepeatingClosure on_verify_signatures)
    : error_(std::move(error)),
      on_verify_signatures_(std::move(on_verify_signatures)) {}

FakeSignatureVerifier::~FakeSignatureVerifier() = default;

void FakeSignatureVerifier::VerifySignatures(
    base::File file,
    web_package::SignedWebBundleIntegrityBlock integrity_block,
    SignatureVerificationCallback callback) const {
  on_verify_signatures_.Run();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          [&]()
              -> base::expected<void, SignedWebBundleSignatureVerifier::Error> {
            if (error_) {
              return base::unexpected(*error_);
            }
            return base::ok();
          }()));
}

mojom::BundleIntegrityBlockSignatureStackEntryPtr MakeSignatureStackEntry(
    const PublicKey& public_key,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> attributes_cbor) {
  auto raw_signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();

  raw_signature_stack_entry->attributes_cbor =
      std::vector(std::begin(attributes_cbor), std::end(attributes_cbor));
  raw_signature_stack_entry->signature_info = absl::visit(
      [&](const auto& public_key) {
        return CreateSignatureInfo(public_key, signature);
      },
      public_key);
  return raw_signature_stack_entry;
}

SignedWebBundleIntegrityBlock ParseIntegrityBlockFromValue(
    const cbor::Value& integrity_block) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  const auto& signature_stack = integrity_block.GetArray().back().GetArray();
  for (const auto& signature_stack_entry : signature_stack) {
    const auto& attributes = signature_stack_entry.GetArray()[0];
    auto attributes_cbor = *cbor::Writer::Write(attributes);

    const auto& signature = signature_stack_entry.GetArray()[1].GetBytestring();

    if (auto* ed25519_pk = base::FindOrNull(
            attributes.GetMap(), cbor::Value(kEd25519PublicKeyAttributeName))) {
      raw_signature_stack.push_back(MakeSignatureStackEntry(
          *Ed25519PublicKey::Create(ed25519_pk->GetBytestring()), signature,
          attributes_cbor));
    } else if (auto* ecdsa_p256_pk = base::FindOrNull(
                   attributes.GetMap(),
                   cbor::Value(kEcdsaP256PublicKeyAttributeName))) {
      raw_signature_stack.push_back(MakeSignatureStackEntry(
          *EcdsaP256PublicKey::Create(ecdsa_p256_pk->GetBytestring()),
          signature, attributes_cbor));
    } else {
      NOTREACHED();
    }
  }

  const auto& attributes = integrity_block.GetArray()[2].GetMap();
  const auto& web_bundle_id =
      base::FindOrNull(attributes, cbor::Value(kWebBundleIdAttributeName))
          ->GetString();

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = cbor::Writer::Write(integrity_block)->size();
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);
  raw_integrity_block->attributes = IntegrityBlockAttributes(
      web_bundle_id, *cbor::Writer::Write(cbor::Value(attributes)));

  return *SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
}

SignedWebBundleIntegrityBlock ParseIntegrityBlock(
    base::span<const uint8_t> swbn) {
  // The size of the bundle itself is written in big endian in the last 8 bytes.
  uint32_t bundle_size =
      base::checked_cast<uint32_t>(base::U64FromBigEndian(swbn.last<8>()));
  CHECK_LT(bundle_size, swbn.size());
  uint32_t ib_size = swbn.size() - bundle_size;
  return ParseIntegrityBlockFromValue(*cbor::Reader::Read(swbn.first(ib_size)));
}

base::expected<void, SignedWebBundleSignatureVerifier::Error> VerifySignatures(
    const SignedWebBundleSignatureVerifier& signature_verifier,
    const base::File& file,
    const SignedWebBundleIntegrityBlock& integrity_block) {
  base::test::TestFuture<
      base::expected<void, SignedWebBundleSignatureVerifier::Error>>
      future;
  signature_verifier.VerifySignatures(file.Duplicate(), integrity_block,
                                      future.GetCallback());
  return future.Take();
}

web_package::IntegrityBlockAttributes GetAttributesForSignedWebBundleId(
    const std::string& signed_web_bundle_id) {
  cbor::Value::MapValue cbor_map;
  cbor_map.emplace(web_package::kWebBundleIdAttributeName,
                   signed_web_bundle_id);
  return {signed_web_bundle_id,
          *cbor::Writer::Write(cbor::Value(std::move(cbor_map)))};
}
}  // namespace web_package::test
