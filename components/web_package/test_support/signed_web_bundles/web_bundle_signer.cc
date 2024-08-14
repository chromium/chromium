// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"

#include <limits>

#include "base/check_is_test.h"
#include "base/containers/extend.h"
#include "base/containers/to_vector.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/web_package/signed_web_bundles/constants.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_utils.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace web_package {

namespace {

using IntegrityBlockErrorForTesting =
    WebBundleSigner::IntegrityBlockErrorForTesting;
using IntegritySignatureErrorForTesting =
    WebBundleSigner::IntegritySignatureErrorForTesting;
using IntegritySignatureErrorsForTesting =
    WebBundleSigner::IntegritySignatureErrorsForTesting;

using PublicKey = absl::variant<Ed25519PublicKey, EcdsaP256PublicKey>;

// Nonce for obtaining deterministic ECDSA P-256 SHA-256 signatures. Taken from
// third_party/boringssl/src/crypto/fipsmodule/ecdsa/ecdsa_sign_tests.txt.
constexpr std::string_view kEcdsaP256SHA256NonceForTestingOnly =
    "36f853b5c54b1ec61588c9c6137eb56e7a708f09c57513093e4ecf6d739900e5";

cbor::Value CreateSignatureStackEntryAttributes(
    const PublicKey& public_key,
    IntegritySignatureErrorsForTesting errors_for_testing = {}) {
  std::vector<uint8_t> public_key_bytes =
      absl::visit(base::Overloaded{[](const auto& public_key) {
                    return base::ToVector(public_key.bytes());
                  }},
                  public_key);
  if (errors_for_testing.Has(
          IntegritySignatureErrorForTesting::kInvalidPublicKeyLength)) {
    public_key_bytes.push_back(42);
  }

  cbor::Value::MapValue attributes;

  if (!errors_for_testing.Has(IntegritySignatureErrorForTesting::
                                  kNoPublicKeySignatureStackEntryAttribute)) {
    if (errors_for_testing.Has(IntegritySignatureErrorForTesting::
                                   kMultipleValidPublicKeyAttributes)) {
      attributes.emplace(kEd25519PublicKeyAttributeName, public_key_bytes);

      attributes.emplace(
          kEcdsaP256PublicKeyAttributeName,
          WebBundleSigner::EcdsaP256KeyPair::CreateRandom().public_key.bytes());
    } else if (errors_for_testing.Has(
                   IntegritySignatureErrorForTesting::
                       kWrongSignatureStackEntryAttributeName)) {
      // Add a typo: "ee" instead of "ed".
      attributes.emplace("ee25519PublicKey", public_key_bytes);
    } else if (errors_for_testing.Has(
                   IntegritySignatureErrorForTesting::
                       kWrongSignatureStackEntryAttributeNameLength)) {
      attributes.emplace("ed25519", public_key_bytes);
    } else {
      attributes.emplace(
          absl::visit(
              base::Overloaded{[](const Ed25519PublicKey&) {
                                 return kEd25519PublicKeyAttributeName;
                               },
                               [](const EcdsaP256PublicKey&) {
                                 return kEcdsaP256PublicKeyAttributeName;
                               }},
              public_key),
          public_key_bytes);
    }
  }

  if (errors_for_testing.Has(IntegritySignatureErrorForTesting::
                                 kAdditionalSignatureStackEntryAttributes)) {
    attributes.emplace("kBinaryString", public_key_bytes);
    attributes.emplace("kTextString", "aaaaaaaaaaaaaaaaaaa");

    attributes.emplace("kZero", 0);

    attributes.emplace("kSimpleValue_true", true);
    attributes.emplace("kSimpleValue_false", false);

    // Integer values: one less than 24 & one large.
    attributes.emplace("kUnsignedInt_small", 5);
    attributes.emplace("kUnsignedInt", std::numeric_limits<int64_t>::max());

    // Negative integer values: one less than 24 (modulo) & one large.
    attributes.emplace("kNegativeInt_small", -12);
    attributes.emplace("kNegativeInt", std::numeric_limits<int64_t>::min());
  }

  if (errors_for_testing.Has(
          IntegritySignatureErrorForTesting::
              kSignatureStackEntryUnsupportedArrayAttribute)) {
    attributes.emplace("kArrayUnsupported", cbor::Value::ArrayValue());
  }

  if (errors_for_testing.Has(IntegritySignatureErrorForTesting::
                                 kSignatureStackEntryUnsupportedMapAttribute)) {
    attributes.emplace("kMapUnsupported", cbor::Value::MapValue());
  }

  return cbor::Value(attributes);
}

cbor::Value CreateSignatureStackEntry(
    const PublicKey& public_key,
    std::vector<uint8_t> signature,
    IntegritySignatureErrorsForTesting errors_for_testing = {}) {
  if (errors_for_testing.Has(
          IntegritySignatureErrorForTesting::kInvalidSignatureLength)) {
    signature.push_back(42);
  }

  cbor::Value::ArrayValue entry;
  entry.emplace_back(
      CreateSignatureStackEntryAttributes(public_key, errors_for_testing));
  entry.emplace_back(signature);

  if (errors_for_testing.Has(IntegritySignatureErrorForTesting::
                                 kAdditionalSignatureStackEntryElement)) {
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

std::vector<uint8_t> SignMessage(
    base::span<const uint8_t> message,
    const WebBundleSigner::EcdsaP256KeyPair& key_pair) {
  std::vector<uint8_t> signature = [&] {
    bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new());
    CHECK(ec_key);
    EC_KEY_set_group(ec_key.get(), EC_group_p256());
    CHECK_EQ(EC_KEY_oct2priv(ec_key.get(), key_pair.private_key.data(),
                             key_pair.private_key.size()),
             1);
    std::array<uint8_t, crypto::kSHA256Length> digest =
        crypto::SHA256Hash(message);

    // ECDSA signing with a fixed nonce is considered unsafe and is only
    // suitable for test scenarios.
    CHECK_IS_TEST();

    std::array<uint8_t, kEcdsaP256SHA256NonceForTestingOnly.size() / 2> nonce;
    CHECK(base::HexStringToSpan(kEcdsaP256SHA256NonceForTestingOnly, nonce));

    bssl::UniquePtr<ECDSA_SIG> sig(
        ECDSA_sign_with_nonce_and_leak_private_key_for_testing(
            digest.data(), digest.size(), ec_key.get(), nonce.data(),
            nonce.size()));
    CHECK(sig);

    uint8_t* signature_bytes;
    size_t signature_size;
    CHECK_EQ(ECDSA_SIG_to_bytes(&signature_bytes, &signature_size, sig.get()),
             1);
    bssl::UniquePtr<uint8_t> signature_bytes_deleter(signature_bytes);

    return std::vector<uint8_t>(signature_bytes,
                                signature_bytes + signature_size);
  }();

  if (key_pair.produce_invalid_signature) {
    signature[0] ^= 0xff;
  }
  return signature;
}

cbor::Value CreateIntegrityBlock(
    const cbor::Value::ArrayValue& signature_stack,
    const std::optional<WebBundleSigner::IntegrityBlockAttributes>&
        ib_attributes,
    WebBundleSigner::IntegrityBlockErrorsForTesting errors_for_testing) {
  cbor::Value::ArrayValue integrity_block;
  // magic bytes
  integrity_block.emplace_back(kIntegrityBlockMagicBytes);
  // version
  if (errors_for_testing.Has(IntegrityBlockErrorForTesting::kInvalidVersion)) {
    integrity_block.emplace_back(
        cbor::Value::BinaryValue({'1', 'p', '\0', '\0'}));  // Invalid.
    integrity_block.emplace_back(cbor::Value::MapValue{});
  } else if (ib_attributes) {
    // Presence of `ib_attributes` indicates integrity block v2.
    integrity_block.emplace_back(kIntegrityBlockV2VersionBytes);
    cbor::Value::MapValue attributes;
    attributes.emplace(web_package::kWebBundleIdAttributeName,
                       ib_attributes->web_bundle_id);
    integrity_block.emplace_back(std::move(attributes));
  } else if (errors_for_testing.Has(
                 IntegrityBlockErrorForTesting::kNoSignedWebBundleId)) {
    integrity_block.emplace_back(kIntegrityBlockV2VersionBytes);
    integrity_block.emplace_back(cbor::Value::MapValue{});
  } else if (errors_for_testing.Has(
                 IntegrityBlockErrorForTesting::kNoAttributes)) {
    integrity_block.emplace_back(kIntegrityBlockV2VersionBytes);
  } else {
    NOTREACHED()
        << "Absence of `ib_attributes` indicates integrity block v1, which "
           "shouldn't be used in tests.";
  }
  // signature stack
  integrity_block.emplace_back(signature_stack);
  if (errors_for_testing.Has(
          IntegrityBlockErrorForTesting::kInvalidIntegrityBlockStructure)) {
    integrity_block.emplace_back(signature_stack);
    integrity_block.emplace_back(signature_stack);
  }

  return cbor::Value(integrity_block);
}

cbor::Value CreateIntegrityBlockForBundle(
    base::span<const uint8_t> unsigned_bundle,
    const std::vector<WebBundleSigner::KeyPair>& key_pairs,
    const std::optional<WebBundleSigner::IntegrityBlockAttributes>&
        ib_attributes,
    WebBundleSigner::ErrorsForTesting errors_for_testing) {
  CHECK(errors_for_testing.signatures_errors.empty() ||
        errors_for_testing.signatures_errors.size() == key_pairs.size());
  auto use_signatures_errors = !errors_for_testing.signatures_errors.empty();

  // Calculate the SHA512 hash of the bundle.
  auto secure_hash = crypto::SecureHash::Create(crypto::SecureHash::SHA512);
  secure_hash->Update(unsigned_bundle.data(), unsigned_bundle.size());
  std::vector<uint8_t> unsigned_bundle_hash(secure_hash->GetHashLength());
  secure_hash->Finish(unsigned_bundle_hash.data(), unsigned_bundle_hash.size());

  std::vector<cbor::Value> signature_stack;
  for (size_t i = 0; i < key_pairs.size(); ++i) {
    const auto& key_pair = key_pairs[i];
    // Create an integrity block with an empty signature stack -- we don't need
    // signatures that depend on each other for now.
    std::optional<std::vector<uint8_t>> integrity_block = cbor::Writer::Write(
        CreateIntegrityBlock(/*signature_stack=*/{}, ib_attributes,
                             errors_for_testing.integrity_block_errors));

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

          IntegritySignatureErrorsForTesting errors;
          if (use_signatures_errors) {
            errors = errors_for_testing.signatures_errors[i];
          }

          signature_stack.push_back(CreateSignatureStackEntry(
              key_pair.public_key, SignMessage(payload_to_sign, key_pair),
              errors));
        }},
        key_pair);
  }

  return CreateIntegrityBlock(signature_stack, ib_attributes,
                              errors_for_testing.integrity_block_errors);
}

// If `web_bundle_id` is not provided explicitly, infer it from the first
// public key.
void FillIdAttributesIfPossibleAndNecessary(
    const std::vector<WebBundleSigner::KeyPair>& key_pairs,
    std::optional<WebBundleSigner::IntegrityBlockAttributes>& ib_attributes,
    const WebBundleSigner::IntegrityBlockErrorsForTesting& errors_for_testing) {
  if (ib_attributes || key_pairs.empty() ||
      errors_for_testing.Has(
          IntegrityBlockErrorForTesting::kNoSignedWebBundleId) ||
      errors_for_testing.Has(IntegrityBlockErrorForTesting::kNoAttributes)) {
    return;
  }
  ib_attributes = {.web_bundle_id = absl::visit(
                       [](const auto& key_pair) {
                         return SignedWebBundleId::CreateForPublicKey(
                                    key_pair.public_key)
                             .id();
                       },
                       key_pairs[0])};
}
}  // namespace

WebBundleSigner::ErrorsForTesting::ErrorsForTesting(
    IntegrityBlockErrorsForTesting bundle_errors,
    const std::vector<IntegritySignatureErrorsForTesting>& signatures_errors)
    : integrity_block_errors(std::move(bundle_errors)),
      signatures_errors(signatures_errors) {}

WebBundleSigner::ErrorsForTesting::ErrorsForTesting(
    const ErrorsForTesting& other) = default;
WebBundleSigner::ErrorsForTesting& WebBundleSigner::ErrorsForTesting::operator=(
    const ErrorsForTesting& other) = default;
WebBundleSigner::ErrorsForTesting::~ErrorsForTesting() = default;

std::vector<uint8_t> WebBundleSigner::SignBundle(
    base::span<const uint8_t> unsigned_bundle,
    const KeyPair& key_pair,
    ErrorsForTesting errors_for_testing) {
  return SignBundle(std::move(unsigned_bundle), {key_pair}, std::nullopt,
                    std::move(errors_for_testing));
}

std::vector<uint8_t> WebBundleSigner::SignBundle(
    base::span<const uint8_t> unsigned_bundle,
    const std::vector<KeyPair>& key_pairs,
    std::optional<IntegrityBlockAttributes> ib_attributes,
    ErrorsForTesting errors_for_testing) {
  CHECK(!key_pairs.empty() !=
        errors_for_testing.integrity_block_errors.Has(
            IntegrityBlockErrorForTesting::kEmptySignatureList))
      << "At least one signing key must be specified unless overriden by "
         "IntegrityBlockErrorForTesting::kEmptySignatureList.";

  FillIdAttributesIfPossibleAndNecessary(
      key_pairs, ib_attributes, errors_for_testing.integrity_block_errors);
  std::optional<std::vector<uint8_t>> integrity_block =
      cbor::Writer::Write(CreateIntegrityBlockForBundle(
          unsigned_bundle, key_pairs, ib_attributes, errors_for_testing));

  std::vector<uint8_t> signed_web_bundle;
  base::Extend(signed_web_bundle, std::move(*integrity_block));
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

// static
WebBundleSigner::EcdsaP256KeyPair
WebBundleSigner::EcdsaP256KeyPair::CreateRandom(
    bool produce_invalid_signature) {
  bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new());
  CHECK(ec_key);
  EC_KEY_set_group(ec_key.get(), EC_group_p256());
  CHECK_EQ(EC_KEY_generate_key(ec_key.get()), 1);

  std::array<uint8_t, EcdsaP256PublicKey::kLength> public_key;
  size_t export_length =
      EC_POINT_point2oct(EC_group_p256(), EC_KEY_get0_public_key(ec_key.get()),
                         POINT_CONVERSION_COMPRESSED, public_key.data(),
                         public_key.size(), /*ctx=*/nullptr);
  CHECK_EQ(export_length, EcdsaP256PublicKey::kLength);

  std::array<uint8_t, 32> private_key;
  CHECK_EQ(32u, EC_KEY_priv2oct(ec_key.get(), private_key.data(),
                                private_key.size()));

  return EcdsaP256KeyPair(public_key, private_key, produce_invalid_signature);
}

WebBundleSigner::EcdsaP256KeyPair::EcdsaP256KeyPair(
    base::span<const uint8_t, EcdsaP256PublicKey::kLength> public_key_bytes,
    base::span<const uint8_t, 32> private_key_bytes,
    bool produce_invalid_signature)
    : public_key(*EcdsaP256PublicKey::Create(public_key_bytes)),
      produce_invalid_signature(produce_invalid_signature) {
  base::ranges::copy(private_key_bytes, private_key.begin());
}

WebBundleSigner::EcdsaP256KeyPair::EcdsaP256KeyPair(
    const WebBundleSigner::EcdsaP256KeyPair&) = default;
WebBundleSigner::EcdsaP256KeyPair& WebBundleSigner::EcdsaP256KeyPair::operator=(
    const EcdsaP256KeyPair&) = default;

WebBundleSigner::EcdsaP256KeyPair::EcdsaP256KeyPair(
    EcdsaP256KeyPair&&) noexcept = default;
WebBundleSigner::EcdsaP256KeyPair& WebBundleSigner::EcdsaP256KeyPair::operator=(
    WebBundleSigner::EcdsaP256KeyPair&&) noexcept = default;

WebBundleSigner::EcdsaP256KeyPair::~EcdsaP256KeyPair() = default;

}  // namespace web_package
