// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_CONSTANTS_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_CONSTANTS_H_

#include <array>
#include <cstdint>
#include <string_view>

namespace web_package {

// The V2 integrity block is a CBOR array with four entries:
//  * Magic Bytes
//  * Version
//  * Attributes
//  * Signature Stack
inline constexpr uint32_t kIntegrityBlockV2TopLevelArrayLength = 4;

inline constexpr std::array<uint8_t, 8> kIntegrityBlockMagicBytes = {
    // "🖋📦" magic bytes (in UTF-8)
    0xF0, 0x9F, 0x96, 0x8B, 0xF0, 0x9F, 0x93, 0xA6};

// [DEPRECATED, here only for error detecting] Version V1: "1b\0\0".
inline constexpr std::array<uint8_t, 4> kIntegrityBlockV1VersionBytes = {
    '1', 'b', 0x00, 0x00};

// Version V2: "2b\0\0".
inline constexpr std::array<uint8_t, 4> kIntegrityBlockV2VersionBytes = {
    '2', 'b', 0x00, 0x00};

// CBOR attribute name for web bundle ID.
inline constexpr std::string_view kWebBundleIdAttributeName = "webBundleId";

// CBOR attribute name for Ed25519 public keys.
inline constexpr std::string_view kEd25519PublicKeyAttributeName =
    "ed25519PublicKey";

// CBOR attribute name for ECDSA P-256 SHA-256 public keys.
inline constexpr std::string_view kEcdsaP256PublicKeyAttributeName =
    "ecdsaP256SHA256PublicKey";

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_CONSTANTS_H_
