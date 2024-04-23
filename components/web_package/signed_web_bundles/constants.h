// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_CONSTANTS_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_CONSTANTS_H_

#include <string_view>

namespace web_package {

// CBOR attribute name for Ed25519 public keys.
inline constexpr std::string_view kEd25519PublicKeyAttributeName =
    "ed25519PublicKey";

// CBOR attribute name for ECDSA P-256 SHA-256 public keys.
inline constexpr std::string_view kEcdsaP256PublicKeyAttributeName =
    "ecdsaP256SHA256PublicKey";

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_CONSTANTS_H_
