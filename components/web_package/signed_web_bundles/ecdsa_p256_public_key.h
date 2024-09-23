// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_PUBLIC_KEY_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_PUBLIC_KEY_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace web_package {

// Wrapper class around an ECDSA P-256 public key.
class EcdsaP256PublicKey {
 public:
  // The length of the compressed public key.
  static constexpr size_t kLength = 33;

  // Attempts to parse the bytes as an ECDSA public key. Returns an instance
  // of this class on success, and an error message on failure.
  static base::expected<EcdsaP256PublicKey, std::string> Create(
      base::span<const uint8_t> bytes);

  bool operator==(const EcdsaP256PublicKey&) const = default;

  base::span<const uint8_t, kLength> bytes() const { return *bytes_; }

  explicit EcdsaP256PublicKey(mojo::DefaultConstruct::Tag) {}

 private:
  explicit EcdsaP256PublicKey(std::array<uint8_t, kLength> bytes);

  // This field is `std::nullopt` only when the default constructor is used,
  // which only happens as part of mojom `StructTraits`. All methods of this
  // class can safely assume that this field is never `std::nullopt`.
  std::optional<std::array<uint8_t, kLength>> bytes_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_PUBLIC_KEY_H_
