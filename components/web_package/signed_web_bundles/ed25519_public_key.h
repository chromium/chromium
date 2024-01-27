// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_PUBLIC_KEY_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_PUBLIC_KEY_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace web_package {

// This class wraps an Ed25519 public key. New instances must be created via the
// static `Create` function, which will validate the length of the key before
// creating a new instance. This guarantees that an instance of this class
// always contains a public key of the correct length. This makes the key safe
// to use with functions like BoringSSL's `ED25519_sign`. Note that the public
// key might still be invalid, even though it has the correct length. This will
// be checked and caught by BoringSSL when trying to use the key.
class Ed25519PublicKey {
 public:
  static constexpr size_t kLength = 32;

  // Attempts to parse the bytes as an Ed25519 public key. Returns an instance
  // of this class on success, and an error message on failure.
  static base::expected<Ed25519PublicKey, std::string> Create(
      base::span<const uint8_t> key);

  // Constructs an instance of this class from the provided bytes.
  static Ed25519PublicKey Create(base::span<const uint8_t, kLength> key);

  Ed25519PublicKey(const Ed25519PublicKey&);
  Ed25519PublicKey& operator=(const Ed25519PublicKey&);

  Ed25519PublicKey(Ed25519PublicKey&&) noexcept;
  Ed25519PublicKey& operator=(Ed25519PublicKey&&) noexcept;

  ~Ed25519PublicKey();

  bool operator==(const Ed25519PublicKey&) const;
  bool operator!=(const Ed25519PublicKey&) const;

  const std::array<uint8_t, kLength>& bytes() const { return *bytes_; }

  explicit Ed25519PublicKey(mojo::DefaultConstruct::Tag) {}

 private:
  FRIEND_TEST_ALL_PREFIXES(StructTraitsTest, Ed25519PublicKey);

  Ed25519PublicKey() = default;

  explicit Ed25519PublicKey(std::array<uint8_t, kLength> bytes);

  // This field is `std::nullopt` only when the default constructor is used,
  // which only happens as part of mojom `StructTraits`. All methods of this
  // class can safely assume that this field is never `std::nullopt` and should
  // `CHECK` if it is.
  std::optional<std::array<uint8_t, kLength>> bytes_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_PUBLIC_KEY_H_
