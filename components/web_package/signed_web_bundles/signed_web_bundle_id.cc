// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "components/base32/base32.h"

namespace web_package {

namespace {

constexpr uint8_t kTypeSuffixLength = 3;

constexpr uint8_t kTypeDevelopment[] = {0x00, 0x00, 0x02};
constexpr uint8_t kTypeEd25519PublicKey[] = {0x00, 0x01, 0x02};

static_assert(std::size(kTypeDevelopment) == kTypeSuffixLength);
static_assert(std::size(kTypeEd25519PublicKey) == kTypeSuffixLength);

}  // namespace

base::expected<SignedWebBundleId, std::string> SignedWebBundleId::Create(
    base::StringPiece encoded_id) {
  if (encoded_id.size() != kEncodedIdLength) {
    return base::unexpected(
        base::StringPrintf("The signed web bundle ID must be exactly %zu "
                           "characters long, but was %zu characters long.",
                           kEncodedIdLength, encoded_id.size()));
  }

  for (const char c : encoded_id) {
    if (!(base::IsAsciiLower(c) || (c >= '2' && c <= '7'))) {
      return base::unexpected(
          "The signed web bundle ID must only contain lowercase ASCII "
          "characters and digits between 2 and 7 (without any padding).");
    }
  }

  // Base32 decode the ID and convert it into an array.
  const std::string decoded_id_string =
      base32::Base32Decode(base::ToUpperASCII(encoded_id));
  if (decoded_id_string.size() != kDecodedIdLength) {
    return base::unexpected(
        "The signed web bundle ID could not be decoded from its base32 "
        "representation.");
  }
  std::array<uint8_t, kDecodedIdLength> decoded_id;
  base::ranges::copy(decoded_id_string, decoded_id.begin());

  auto type_suffix = base::make_span(decoded_id).last<kTypeSuffixLength>();
  if (base::ranges::equal(type_suffix, kTypeDevelopment)) {
    return SignedWebBundleId(Type::kDevelopment, encoded_id,
                             std::move(decoded_id));
  }
  if (base::ranges::equal(type_suffix, kTypeEd25519PublicKey)) {
    return SignedWebBundleId(Type::kEd25519PublicKey, encoded_id,
                             std::move(decoded_id));
  }
  return base::unexpected("The signed web bundle ID has an unknown type.");
}

SignedWebBundleId::SignedWebBundleId(
    Type type,
    base::StringPiece encoded_id,
    std::array<uint8_t, kDecodedIdLength> decoded_id)
    : type_(type),
      encoded_id_(encoded_id),
      decoded_id_(std::move(decoded_id)) {}

SignedWebBundleId::SignedWebBundleId(const SignedWebBundleId& other) = default;

SignedWebBundleId::~SignedWebBundleId() = default;

Ed25519PublicKey SignedWebBundleId::GetEd25519PublicKey() const {
  CHECK_EQ(type(), Type::kEd25519PublicKey);

  return Ed25519PublicKey::Create(
      base::make_span(decoded_id_)
          .first<kDecodedIdLength - kTypeSuffixLength>());
}

}  // namespace web_package
