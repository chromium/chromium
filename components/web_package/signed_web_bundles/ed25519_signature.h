// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_SIGNATURE_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_SIGNATURE_H_

#include <array>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

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

  [[nodiscard]] bool Verify(base::span<const uint8_t> message,
                            const Ed25519PublicKey& public_key) const;

  const std::array<uint8_t, kLength>& bytes() const { return bytes_; }

 private:
  explicit Ed25519Signature(std::array<uint8_t, kLength>& bytes);

  const std::array<uint8_t, kLength> bytes_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ED25519_SIGNATURE_H_
