// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

#include <ostream>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/base32/base32.h"
#include "crypto/random.h"

namespace web_package {

// static
base::expected<SignedWebBundleId, std::string> SignedWebBundleId::Create(
    std::string_view encoded_id) {
  if (encoded_id.size() != kEd25519EncodedIdLength &&
      encoded_id.size() != kEcdsaP256EncodedIdLength) {
    return base::unexpected(base::StringPrintf(
        "The signed web bundle ID must be exactly %zu "
        "characters long (for Ed25519) or %zu characters long (for ECDSA "
        "P-256), but was %zu characters long.",
        kEd25519EncodedIdLength, kEcdsaP256EncodedIdLength, encoded_id.size()));
  }

  for (const char c : encoded_id) {
    if (!(base::IsAsciiLower(c) || (c >= '2' && c <= '7'))) {
      return base::unexpected(
          "The signed web bundle ID must only contain lowercase ASCII "
          "characters and digits between 2 and 7 (without any padding).");
    }
  }

  // Base32 decode the ID as an array.
  const std::vector<uint8_t> decoded_id =
      base32::Base32Decode(base::ToUpperASCII(encoded_id));
  if (decoded_id.size() < kTypeSuffixLength) {
    return base::unexpected(
        "The signed web bundle ID could not be decoded from its base32 "
        "representation.");
  }

  auto type_suffix = base::span(decoded_id).last<kTypeSuffixLength>();
  if (base::ranges::equal(type_suffix, kTypeProxyMode)) {
    if (decoded_id.size() == kProxyModeDecodedIdLength) {
      return SignedWebBundleId(Type::kProxyMode, encoded_id);
    } else {
      return base::unexpected(base::StringPrintf(
          "A ProxyMode signed web bundle ID must be exactly %zu "
          "characters long, but was %zu characters long.",
          kProxyModeEncodedIdLength, encoded_id.size()));
    }
  }
  if (base::ranges::equal(type_suffix, kTypeEd25519PublicKey)) {
    if (decoded_id.size() == kEd25519DecodedIdLength) {
      return SignedWebBundleId(Type::kEd25519PublicKey, encoded_id);
    } else {
      return base::unexpected(base::StringPrintf(
          "An Ed25519 signed web bundle ID must be exactly %zu "
          "characters long, but was %zu characters long.",
          kEd25519EncodedIdLength, encoded_id.size()));
    }
  }
  if (base::ranges::equal(type_suffix, kTypeEcdsaP256PublicKey)) {
    if (decoded_id.size() == kEcdsaP256DecodedIdLength) {
      return SignedWebBundleId(Type::kEcdsaP256PublicKey, encoded_id);
    } else {
      return base::unexpected(base::StringPrintf(
          "An ECDSA P-256 signed web bundle ID must be exactly %zu "
          "characters long, but was %zu characters long.",
          kEcdsaP256EncodedIdLength, encoded_id.size()));
    }
  }
  return base::unexpected("The signed web bundle ID has an unknown type.");
}

// static
SignedWebBundleId SignedWebBundleId::CreateForPublicKey(
    const Ed25519PublicKey& public_key) {
  std::array<uint8_t, kEd25519DecodedIdLength> decoded_id;
  base::ranges::copy(public_key.bytes(), decoded_id.begin());
  base::ranges::copy(kTypeEd25519PublicKey,
                     decoded_id.end() - kTypeSuffixLength);

  auto encoded_id_uppercase = base32::Base32Encode(
      decoded_id, base32::Base32EncodePolicy::OMIT_PADDING);
  auto encoded_id = base::ToLowerASCII(encoded_id_uppercase);
  return SignedWebBundleId(Type::kEd25519PublicKey, encoded_id);
}

// static
SignedWebBundleId SignedWebBundleId::CreateForPublicKey(
    const EcdsaP256PublicKey& public_key) {
  std::array<uint8_t, kEcdsaP256DecodedIdLength> decoded_id;
  base::ranges::copy(public_key.bytes(), decoded_id.begin());
  base::ranges::copy(kTypeEcdsaP256PublicKey,
                     decoded_id.end() - kTypeSuffixLength);

  auto encoded_id_uppercase = base32::Base32Encode(
      decoded_id, base32::Base32EncodePolicy::OMIT_PADDING);
  auto encoded_id = base::ToLowerASCII(encoded_id_uppercase);
  return SignedWebBundleId(Type::kEcdsaP256PublicKey, encoded_id);
}

// static
SignedWebBundleId SignedWebBundleId::CreateForProxyMode(
    base::span<const uint8_t, kProxyModeKeyLength> data) {
  std::array<uint8_t, kProxyModeKeyLength + kTypeSuffixLength> decoded_id;
  base::ranges::copy(data, decoded_id.begin());
  base::ranges::copy(kTypeProxyMode, decoded_id.end() - kTypeSuffixLength);

  auto encoded_id_uppercase = base32::Base32Encode(
      decoded_id, base32::Base32EncodePolicy::OMIT_PADDING);
  auto encoded_id = base::ToLowerASCII(encoded_id_uppercase);
  return SignedWebBundleId(Type::kProxyMode, encoded_id);
}

// static
SignedWebBundleId SignedWebBundleId::CreateRandomForProxyMode() {
  std::array<uint8_t, kProxyModeKeyLength> random_bytes;
  crypto::RandBytes(random_bytes);
  return CreateForProxyMode(random_bytes);
}

SignedWebBundleId::SignedWebBundleId(Type type, std::string_view encoded_id)
    : type_(type), encoded_id_(encoded_id) {}

SignedWebBundleId::SignedWebBundleId(const SignedWebBundleId& other) = default;
SignedWebBundleId& SignedWebBundleId::operator=(
    const SignedWebBundleId& other) = default;

SignedWebBundleId::~SignedWebBundleId() = default;

std::ostream& operator<<(std::ostream& os, const SignedWebBundleId& id) {
  return os << id.id();
}

}  // namespace web_package
