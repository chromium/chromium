// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_ID_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_ID_H_

#include <array>
#include <iosfwd>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_package {

// This class represents the ID of a Signed Web Bundle. There are currently two
// types of IDs:
//   * IDs used for development and testing (the so-called proxy mode);
//   * IDs based on an Ed25519 public key.
//   * IDs based on an ECDSA P-256 public key.
//
// IDs are base32-encoded (without padding), and then transformed to lowercase.
//
// New instances of this class can only be constructed via the static `Create`
// function, which will validate the format of the given ID. This means that you
// can assume that every instance of this class wraps a correctly formatted ID.
class SignedWebBundleId {
 private:
  static constexpr uint8_t kTypeSuffixLength = 3;

  // The key used in Proxy Mode is assumed to be similar in size to Ed25519
  // public key.
  static constexpr size_t kProxyModeKeyLength = 32;
  static_assert(kProxyModeKeyLength == Ed25519PublicKey::kLength);

  // The decoded ID is a concatenation of a public key and the corresponding
  // type suffix.
  static constexpr size_t kProxyModeDecodedIdLength =
      kProxyModeKeyLength + kTypeSuffixLength;
  static constexpr size_t kEd25519DecodedIdLength =
      Ed25519PublicKey::kLength + kTypeSuffixLength;
  static constexpr size_t kEcdsaP256DecodedIdLength =
      EcdsaP256PublicKey::kLength + kTypeSuffixLength;
  static_assert(kProxyModeDecodedIdLength == kEd25519DecodedIdLength);

  // The encoded ID is a lower ASCII base32-encoded concatenation of
  // a public key and the corresponding type suffix with the trailing padding
  // omitted.
  static constexpr size_t kEd25519EncodedIdLength =
      (kEd25519DecodedIdLength * 8 + 4) / 5;
  static constexpr size_t kProxyModeEncodedIdLength =
      (kProxyModeDecodedIdLength * 8 + 4) / 5;
  static constexpr size_t kEcdsaP256EncodedIdLength =
      (kEcdsaP256DecodedIdLength * 8 + 4) / 5;
  static_assert(kEd25519EncodedIdLength == kProxyModeEncodedIdLength);

  using TypeSuffix = std::array<uint8_t, kTypeSuffixLength>;

  static constexpr TypeSuffix kTypeProxyMode = {0x00, 0x00, 0x02};
  static constexpr TypeSuffix kTypeEd25519PublicKey = {0x00, 0x01, 0x02};
  static constexpr TypeSuffix kTypeEcdsaP256PublicKey = {0x00, 0x02, 0x02};

 public:
  enum class Type {
    // This is intended for use during development (the so-called proxy mode),
    // where instead of reading from a Signed Web Bundle, requests are proxied
    // to a development HTTP(s) server.
    kProxyMode,
    kEd25519PublicKey,
    kEcdsaP256PublicKey,
  };

  // Attempts to parse a a Signed Web Bundle ID, and returns an instance of
  // `SignedWebBundleId` if it works. If it doesn't, then the return value
  // contains an error message detailing the issue.
  static base::expected<SignedWebBundleId, std::string> Create(
      std::string_view base32_encoded_id);

  static SignedWebBundleId CreateForPublicKey(
      const Ed25519PublicKey& public_key);

  static SignedWebBundleId CreateForPublicKey(
      const EcdsaP256PublicKey& public_key);

  static SignedWebBundleId CreateForProxyMode(
      base::span<const uint8_t, kProxyModeKeyLength> data);

  static SignedWebBundleId CreateRandomForProxyMode();

  SignedWebBundleId(const SignedWebBundleId& other);
  SignedWebBundleId& operator=(const SignedWebBundleId& other);

  ~SignedWebBundleId();

  bool is_for_proxy_mode() const { return type_ == Type::kProxyMode; }

  Type type() const { return type_; }

  const std::string& id() const { return encoded_id_; }

  auto operator<=>(const SignedWebBundleId&) const = default;

  friend std::ostream& operator<<(std::ostream& os,
                                  const SignedWebBundleId& id);

 private:
  SignedWebBundleId(Type type, std::string_view encoded_id);

  Type type_;
  std::string encoded_id_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_ID_H_
