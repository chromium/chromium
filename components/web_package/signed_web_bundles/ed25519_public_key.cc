// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace web_package {

static_assert(Ed25519PublicKey::kLength == ED25519_PUBLIC_KEY_LEN);

Ed25519PublicKey::Ed25519PublicKey(const Ed25519PublicKey&) = default;
Ed25519PublicKey& Ed25519PublicKey::operator=(const Ed25519PublicKey&) =
    default;

Ed25519PublicKey::Ed25519PublicKey(Ed25519PublicKey&&) noexcept = default;
Ed25519PublicKey& Ed25519PublicKey::operator=(Ed25519PublicKey&&) noexcept =
    default;

Ed25519PublicKey::~Ed25519PublicKey() = default;

bool Ed25519PublicKey::operator==(const Ed25519PublicKey& other) const {
  return *bytes_ == *other.bytes_;
}

bool Ed25519PublicKey::operator!=(const Ed25519PublicKey& other) const {
  return !(*this == other);
}

base::expected<Ed25519PublicKey, std::string> Ed25519PublicKey::Create(
    base::span<const uint8_t> key) {
  auto sized_key = key.to_fixed_extent<kLength>();
  if (!sized_key) {
    return base::unexpected(base::StringPrintf(
        "The Ed25519 public key does not have the correct length. Expected %zu "
        "bytes, but received %zu bytes.",
        kLength, key.size()));
  }

  return Create(*sized_key);
}

Ed25519PublicKey Ed25519PublicKey::Create(
    base::span<const uint8_t, kLength> key) {
  std::array<uint8_t, kLength> bytes;
  base::ranges::copy(key, bytes.begin());

  return Ed25519PublicKey(std::move(bytes));
}

Ed25519PublicKey::Ed25519PublicKey(std::array<uint8_t, kLength> bytes)
    : bytes_(std::move(bytes)) {}

}  // namespace web_package
