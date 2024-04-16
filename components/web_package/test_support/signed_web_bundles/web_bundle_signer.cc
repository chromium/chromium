// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"

#include "base/containers/extend.h"
#include "base/containers/to_vector.h"
#include "base/functional/overloaded.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"
#include "crypto/secure_hash.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

namespace {

using ErrorForTesting = WebBundleSigner::ErrorForTesting;
using ErrorsForTesting = WebBundleSigner::ErrorsForTesting;

cbor::Value CreateSignatureStackEntryAttributes(
    const Ed25519PublicKey& public_key,
    ErrorsForTesting errors_for_testing = {}) {
  std::vector<uint8_t> public_key_bytes = base::ToVector(public_key.bytes());
  if (errors_for_testing.Has(ErrorForTesting::kInvalidPublicKeyLength)) {
    public_key_bytes.push_back(42);
  }

  cbor::Value::MapValue attributes;

  if (!errors_for_testing.Has(
          ErrorForTesting::kNoPublicKeySignatureStackEntryAttribute)) {
    if (errors_for_testing.Has(
            ErrorForTesting::kWrongSignatureStackEntryAttributeName)) {
      // Add a typo: "ee" instead of "ed".
      attributes[cbor::Value("ee25519PublicKey")] =
          cbor::Value(public_key_bytes);
    } else if (errors_for_testing.Has(
                   ErrorForTesting::
                       kWrongSignatureStackEntryAttributeNameLength)) {
      attributes[cbor::Value("ed25519")] = cbor::Value(public_key_bytes);

    } else {
      attributes[cbor::Value("ed25519PublicKey")] =
          cbor::Value(public_key_bytes);
    }
  }

  if (errors_for_testing.Has(
          ErrorForTesting::kAdditionalSignatureStackEntryAttribute)) {
    attributes[cbor::Value("foo")] = cbor::Value(42);
  }

  return cbor::Value(attributes);
}

cbor::Value CreateSignatureStackEntry(
    const Ed25519PublicKey& public_key,
    std::vector<uint8_t> signature,
    ErrorsForTesting errors_for_testing = {}) {
  if (errors_for_testing.Has(ErrorForTesting::kInvalidSignatureLength)) {
    signature.push_back(42);
  }

  cbor::Value::ArrayValue entry;
  entry.emplace_back(
      CreateSignatureStackEntryAttributes(public_key, errors_for_testing));
  entry.emplace_back(signature);

  if (errors_for_testing.Has(
          ErrorForTesting::kAdditionalSignatureStackEntryElement)) {
    entry.emplace_back("foo");
  }

  return cbor::Value(entry);
}

std::vector<uint8_t> SignMessage(
    base::span<const uint8_t> message,
    const WebBundleSigner::Ed25519KeyPair& key_pair) {
  std::vector<uint8_t> signature(ED25519_SIGNATURE_LEN);
  CHECK_EQ(key_pair.private_key.size(),
           static_cast<size_t>(ED25519_PRIVATE_KEY_LEN));
  CHECK_EQ(ED25519_sign(signature.data(), message.data(), message.size(),
                        key_pair.private_key.data()),
           1);
  if (key_pair.produce_invalid_signature) {
    signature[0] ^= 0xff;
  }
  return signature;
}

}  // namespace

cbor::Value WebBundleSigner::CreateIntegrityBlock(
    const cbor::Value::ArrayValue& signature_stack,
    ErrorsForTesting errors_for_testing) {
  cbor::Value::ArrayValue integrity_block;
  // magic bytes
  integrity_block.emplace_back(cbor::Value::BinaryValue(
      {0xF0, 0x9F, 0x96, 0x8B, 0xF0, 0x9F, 0x93, 0xA6}));
  // version
  integrity_block.emplace_back(
      errors_for_testing.Has(ErrorForTesting::kInvalidVersion)
          ? cbor::Value::BinaryValue({'1', 'p', '\0', '\0'})  // Invalid.
          : cbor::Value::BinaryValue({'1', 'b', '\0', '\0'}));
  // signature stack
  integrity_block.emplace_back(signature_stack);
  if (errors_for_testing.Has(
          ErrorForTesting::kInvalidIntegrityBlockStructure)) {
    integrity_block.emplace_back(signature_stack);
  }

  return cbor::Value(integrity_block);
}

cbor::Value WebBundleSigner::CreateIntegrityBlockForBundle(
    base::span<const uint8_t> unsigned_bundle,
    const std::vector<KeyPair>& key_pairs,
    ErrorsForTesting errors_for_testing) {
  // Calculate the SHA512 hash of the bundle.
  auto secure_hash = crypto::SecureHash::Create(crypto::SecureHash::SHA512);
  secure_hash->Update(unsigned_bundle.data(), unsigned_bundle.size());
  std::vector<uint8_t> unsigned_bundle_hash(secure_hash->GetHashLength());
  secure_hash->Finish(unsigned_bundle_hash.data(), unsigned_bundle_hash.size());

  std::vector<cbor::Value> signature_stack;
  for (const KeyPair& key_pair : key_pairs) {
    // Create an integrity block with all previous signature stack entries.
    std::optional<std::vector<uint8_t>> integrity_block = cbor::Writer::Write(
        CreateIntegrityBlock(signature_stack, errors_for_testing));

    absl::visit(
        base::Overloaded{[&](const auto& key_pair) {
          // Create the attributes map for the current signature stack entry.
          std::optional<std::vector<uint8_t>> attributes = cbor::Writer::Write(
              CreateSignatureStackEntryAttributes(key_pair.public_key));

          // Build the payload to sign and then sign it.
          std::vector<uint8_t> payload_to_sign = CreateSignaturePayload(
              {.unsigned_web_bundle_hash = unsigned_bundle_hash,
               .integrity_block_cbor = *integrity_block,
               .attributes_cbor = *attributes});

          signature_stack.push_back(CreateSignatureStackEntry(
              key_pair.public_key, SignMessage(payload_to_sign, key_pair),
              errors_for_testing));
        }},
        key_pair);
  }

  return CreateIntegrityBlock(signature_stack, errors_for_testing);
}

std::vector<uint8_t> WebBundleSigner::SignBundle(
    base::span<const uint8_t> unsigned_bundle,
    const std::vector<KeyPair>& key_pairs,
    ErrorsForTesting errors_for_testing) {
  std::optional<std::vector<uint8_t>> integrity_block =
      cbor::Writer::Write(CreateIntegrityBlockForBundle(
          unsigned_bundle, key_pairs, errors_for_testing));

  std::vector<uint8_t> signed_web_bundle;
  base::Extend(signed_web_bundle, base::span(*integrity_block));
  base::Extend(signed_web_bundle, unsigned_bundle);

  return signed_web_bundle;
}

// static
WebBundleSigner::Ed25519KeyPair WebBundleSigner::Ed25519KeyPair::CreateRandom(
    bool produce_invalid_signature) {
  std::array<uint8_t, ED25519_PUBLIC_KEY_LEN> public_key;
  std::array<uint8_t, ED25519_PRIVATE_KEY_LEN> private_key;
  ED25519_keypair(public_key.data(), private_key.data());
  return Ed25519KeyPair(std::move(public_key), std::move(private_key),
                        produce_invalid_signature);
}

WebBundleSigner::Ed25519KeyPair::Ed25519KeyPair(
    base::span<const uint8_t, ED25519_PUBLIC_KEY_LEN> public_key_bytes,
    base::span<const uint8_t, ED25519_PRIVATE_KEY_LEN> private_key_bytes,
    bool produce_invalid_signature)
    : public_key(Ed25519PublicKey::Create(public_key_bytes)),
      produce_invalid_signature(produce_invalid_signature) {
  std::array<uint8_t, ED25519_PRIVATE_KEY_LEN> private_key_array;
  base::ranges::copy(private_key_bytes, private_key_array.begin());
  private_key = std::move(private_key_array);
}

WebBundleSigner::Ed25519KeyPair::Ed25519KeyPair(
    const WebBundleSigner::Ed25519KeyPair&) = default;
WebBundleSigner::Ed25519KeyPair& WebBundleSigner::Ed25519KeyPair::operator=(
    const Ed25519KeyPair&) = default;

WebBundleSigner::Ed25519KeyPair::Ed25519KeyPair(Ed25519KeyPair&&) noexcept =
    default;
WebBundleSigner::Ed25519KeyPair& WebBundleSigner::Ed25519KeyPair::operator=(
    WebBundleSigner::Ed25519KeyPair&&) noexcept = default;

WebBundleSigner::Ed25519KeyPair::~Ed25519KeyPair() = default;

}  // namespace web_package
