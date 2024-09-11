// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_SIGNATURE_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_SIGNATURE_H_

#include <array>
#include <optional>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace web_package {

// Wrapper class around an Ed25519 signature.
class Ed25519Signature {
 public:
  static constexpr size_t kLength = 64;

  // Attempt to convert the provided bytes into an Ed25519 signature, returning
  // a string describing the error on failure.
  static base::expected<Ed25519Signature, std::string> Create(
      base::span<const uint8_t> bytes);

  static Ed25519Signature Create(base::span<const uint8_t, kLength> bytes);

  explicit Ed25519Signature(mojo::DefaultConstruct::Tag) {}
  Ed25519Signature(const Ed25519Signature&) = default;
  Ed25519Signature(Ed25519Signature&&) = default;
  Ed25519Signature& operator=(const Ed25519Signature&) = default;
  Ed25519Signature& operator=(Ed25519Signature&&) = default;

  bool operator==(const Ed25519Signature& other) const;
  bool operator!=(const Ed25519Signature& other) const;

  [[nodiscard]] bool Verify(base::span<const uint8_t> message,
                            const Ed25519PublicKey& public_key) const;

  const std::array<uint8_t, kLength>& bytes() const { return *bytes_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(StructTraitsTest, Ed25519Signature);

  Ed25519Signature() = default;
  explicit Ed25519Signature(std::array<uint8_t, kLength>& bytes);

  // This field is `std::nullopt` only when the default constructor is used,
  // which only happens as part of mojom `StructTraits`. All methods of this
  // class can safely assume that this field is never `std::nullopt` and should
  // `CHECK` if it is.
  std::optional<std::array<uint8_t, kLength>> bytes_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_SIGNATURE_H_
