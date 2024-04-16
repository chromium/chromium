// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_UTILS_H_

#include <cstdint>

#include "base/containers/span.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"

namespace web_package::internal {

// Checks whether the given `public_key` constitutes a valid ECDSA P-256 public
// key.
bool IsValidEcdsaP256PublicKey(
    base::span<const uint8_t, EcdsaP256PublicKey::kLength> public_key);

// Checks whether the given `signature` corresponds to a SHA-256 hash of
// `message` signed by the provided `public_key`.
bool VerifyMessageSignedWithEcdsaP256SHA256(
    base::span<const uint8_t> message,
    base::span<const uint8_t> signature,
    const EcdsaP256PublicKey& public_key);

}  // namespace web_package::internal

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_ECDSA_P256_UTILS_H_
