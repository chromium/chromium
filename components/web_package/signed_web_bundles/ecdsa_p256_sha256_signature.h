// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_SHA256_SIGNATURE_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_SHA256_SIGNATURE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace web_package {

// Wrapper class around an ECDSA P-256 SHA-256 signature.
class EcdsaP256SHA256Signature {
 public:
  // Attempt to convert the provided bytes into an ECDSA P-256 signature,
  // returning a string describing the error on failure.
  static base::expected<EcdsaP256SHA256Signature, std::string> Create(
      base::span<const uint8_t> bytes);

  explicit EcdsaP256SHA256Signature(mojo::DefaultConstruct::Tag);

  EcdsaP256SHA256Signature(const EcdsaP256SHA256Signature&);
  EcdsaP256SHA256Signature(EcdsaP256SHA256Signature&&);
  EcdsaP256SHA256Signature& operator=(const EcdsaP256SHA256Signature&);
  EcdsaP256SHA256Signature& operator=(EcdsaP256SHA256Signature&&);

  ~EcdsaP256SHA256Signature();

  bool operator==(const EcdsaP256SHA256Signature& other) const = default;

  [[nodiscard]] bool Verify(base::span<const uint8_t> message,
                            const EcdsaP256PublicKey& public_key) const;

  base::span<const uint8_t> bytes() const { return *bytes_; }


 private:
  explicit EcdsaP256SHA256Signature(std::vector<uint8_t> bytes);

  // This field is `std::nullopt` only when the default constructor is used,
  // which only happens as part of mojom `StructTraits`. All methods of this
  // class can safely assume that this field is never `std::nullopt`.
  std::optional<std::vector<uint8_t>> bytes_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_SHA256_SIGNATURE_H_
