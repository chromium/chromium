// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "components/web_package/signed_web_bundles/types.h"
#include "crypto/secure_hash.h"

namespace web_package::test {

namespace {

using IntegrityBlockErrorForTesting =
    WebBundleSigner::IntegrityBlockErrorForTesting;
using IntegritySignatureErrorForTesting =
    WebBundleSigner::IntegritySignatureErrorForTesting;
using IntegritySignatureErrorsForTesting =
    WebBundleSigner::IntegritySignatureErrorsForTesting;

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

      attributes.emplace(kEcdsaP256PublicKeyAttributeName,
                         EcdsaP256KeyPair::CreateRandom().public_key.bytes());
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
    const KeyPairs& key_pairs,
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
    const KeyPairs& key_pairs,
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
    const KeyPairs& key_pairs,
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

}  // namespace web_package::test
