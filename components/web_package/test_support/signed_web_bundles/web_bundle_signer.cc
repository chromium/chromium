// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"

#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"
#include "crypto/secure_hash.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

cbor::Value WebBundleSigner::CreateIntegrityBlock(
    const cbor::Value::ArrayValue& signature_stack) {
  cbor::Value::ArrayValue integrity_block;
  // magic bytes
  integrity_block.push_back(cbor::Value(cbor::Value::BinaryValue(
      {0xF0, 0x9F, 0x96, 0x8B, 0xF0, 0x9F, 0x93, 0xA6})));
  // version
  integrity_block.push_back(
      cbor::Value(cbor::Value::BinaryValue({'1', 'b', '\0', '\0'})));
  // signature stack
  integrity_block.emplace_back(signature_stack);

  return cbor::Value(integrity_block);
}

cbor::Value WebBundleSigner::CreateSignatureStackEntry(
    base::span<const uint8_t> public_key,
    std::vector<uint8_t> signature,
    ErrorForTesting error_for_testing) {
  if (error_for_testing == ErrorForTesting::kInvalidSignatureLength) {
    signature.push_back(42);
  }

  cbor::Value::ArrayValue entry;
  entry.push_back(cbor::Value(CreateSignatureStackEntryAttributes(
      std::vector(public_key.begin(), public_key.end()), error_for_testing)));
  entry.push_back(cbor::Value(signature));

  if (error_for_testing ==
      ErrorForTesting::kAdditionalSignatureStackEntryElement) {
    entry.push_back(cbor::Value("foo"));
  }

  return cbor::Value(entry);
}

cbor::Value WebBundleSigner::CreateSignatureStackEntryAttributes(
    std::vector<uint8_t> public_key,
    ErrorForTesting error_for_testing) {
  if (error_for_testing == ErrorForTesting::kInvalidPublicKeyLength) {
    public_key.push_back(42);
  }

  cbor::Value::MapValue attributes;

  if (error_for_testing !=
      ErrorForTesting::kNoPublicKeySignatureStackEntryAttribute) {
    if (error_for_testing ==
        ErrorForTesting::kWrongSignatureStackEntryAttributeName) {
      // Add a typo: "ee" instead of "ed".
      attributes[cbor::Value("ee25519PublicKey")] = cbor::Value(public_key);
    } else {
      attributes[cbor::Value("ed25519PublicKey")] = cbor::Value(public_key);
    }
  }

  if (error_for_testing ==
      ErrorForTesting::kAdditionalSignatureStackEntryAttribute) {
    attributes[cbor::Value("foo")] = cbor::Value(42);
  }

  return cbor::Value(attributes);
}

cbor::Value WebBundleSigner::CreateIntegrityBlockForBundle(
    base::span<const uint8_t> unsigned_bundle,
    const std::vector<KeyPair>& key_pairs,
    ErrorForTesting error_for_testing) {
  // Calculate the SHA512 hash of the bundle.
  auto secure_hash = crypto::SecureHash::Create(crypto::SecureHash::SHA512);
  secure_hash->Update(unsigned_bundle.data(), unsigned_bundle.size());
  std::vector<uint8_t> unsigned_bundle_hash(secure_hash->GetHashLength());
  secure_hash->Finish(unsigned_bundle_hash.data(), unsigned_bundle_hash.size());

  std::vector<cbor::Value> signature_stack;
  for (const KeyPair& key_pair : key_pairs) {
    // Create an integrity block with all previous signature stack entries.
    absl::optional<std::vector<uint8_t>> integrity_block =
        cbor::Writer::Write(CreateIntegrityBlock(signature_stack));

    // Create the attributes map for the current signature stack entry.
    absl::optional<std::vector<uint8_t>> attributes = cbor::Writer::Write(
        CreateSignatureStackEntryAttributes(key_pair.public_key));

    // Build the payload to sign and then sign it.
    std::vector<uint8_t> payload_to_sign = CreateSignaturePayload(
        unsigned_bundle_hash, *integrity_block, *attributes);

    std::vector<uint8_t> signature(ED25519_SIGNATURE_LEN);
    CHECK_EQ(key_pair.private_key.size(),
             static_cast<size_t>(ED25519_PRIVATE_KEY_LEN));
    CHECK_EQ(ED25519_sign(signature.data(), payload_to_sign.data(),
                          payload_to_sign.size(), key_pair.private_key.data()),
             1);

    signature_stack.push_back(CreateSignatureStackEntry(
        key_pair.public_key, signature, error_for_testing));
  }

  return CreateIntegrityBlock(signature_stack);
}

std::vector<uint8_t> WebBundleSigner::SignBundle(
    base::span<const uint8_t> unsigned_bundle,
    const std::vector<KeyPair>& key_pairs,
    ErrorForTesting error_for_testing) {
  absl::optional<std::vector<uint8_t>> integrity_block =
      cbor::Writer::Write(CreateIntegrityBlockForBundle(
          unsigned_bundle, key_pairs, error_for_testing));

  std::vector<uint8_t> signed_web_bundle;
  signed_web_bundle.insert(signed_web_bundle.end(), integrity_block->begin(),
                           integrity_block->end());
  signed_web_bundle.insert(signed_web_bundle.end(), unsigned_bundle.begin(),
                           unsigned_bundle.end());

  return signed_web_bundle;
}

WebBundleSigner::KeyPair::KeyPair(base::span<const uint8_t> public_key,
                                  base::span<const uint8_t> private_key)
    : public_key(public_key.begin(), public_key.end()),
      private_key(private_key.begin(), private_key.end()){};

WebBundleSigner::KeyPair::KeyPair(const WebBundleSigner::KeyPair& other) =
    default;

WebBundleSigner::KeyPair::~KeyPair() = default;

}  // namespace web_package
