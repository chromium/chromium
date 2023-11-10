// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_ID_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_ID_H_

#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_package {

// This class represents the ID of a Signed Web Bundle. There are currently two
// types of IDs: IDs used for development and testing, and IDs based on an
// Ed25519 public key.
// IDs are base32-encoded (without padding), and then transformed to lowercase.
//
// New instances of this class can only be constructed via the static `Create`
// function, which will validate the format of the given ID. This means that you
// can assume that every instance of this class wraps a correctly formatted ID.
class SignedWebBundleId {
 private:
  static constexpr size_t kEncodedIdLength = 56;
  static constexpr size_t kDecodedIdLength = 35;
  static constexpr uint8_t kTypeSuffixLength = 3;

  static constexpr uint8_t kTypeDevelopment[] = {0x00, 0x00, 0x02};
  static constexpr uint8_t kTypeEd25519PublicKey[] = {0x00, 0x01, 0x02};

  static_assert(std::size(kTypeDevelopment) ==
                SignedWebBundleId::kTypeSuffixLength);
  static_assert(std::size(kTypeEd25519PublicKey) ==
                SignedWebBundleId::kTypeSuffixLength);

 public:
  enum class Type {
    // This is intended for use during development, where a Web Bundle might not
    // be signed with a real key and instead uses a fake development-only ID.
    kDevelopment,
    kEd25519PublicKey,
  };

  // Attempts to parse a a Signed Web Bundle ID, and returns an instance of
  // `SignedWebBundleId` if it works. If it doesn't, then the return value
  // contains an error message detailing the issue.
  static base::expected<SignedWebBundleId, std::string> Create(
      base::StringPiece base32_encoded_id);

  static SignedWebBundleId CreateForEd25519PublicKey(
      Ed25519PublicKey public_key);

  static SignedWebBundleId CreateForDevelopment(
      base::span<const uint8_t, kDecodedIdLength - kTypeSuffixLength> data);

  static SignedWebBundleId CreateRandomForDevelopment(
      base::RepeatingCallback<void(void*, size_t)> random_generator =
          GetDefaultRandomGenerator());

  SignedWebBundleId(const SignedWebBundleId& other);

  ~SignedWebBundleId();

  Type type() const { return type_; }

  const std::string& id() const { return encoded_id_; }

  bool operator<(const SignedWebBundleId& other) const {
    return decoded_id_ < other.decoded_id_;
  }

  bool operator==(const SignedWebBundleId& other) const {
    return decoded_id_ == other.decoded_id_;
  }

  bool operator!=(const SignedWebBundleId& other) const {
    return !(*this == other);
  }

 private:
  SignedWebBundleId(Type type,
                    base::StringPiece encoded_id,
                    base::span<const uint8_t, kDecodedIdLength> decoded_id);

  Type type_;
  std::string encoded_id_;
  std::array<uint8_t, kDecodedIdLength> decoded_id_;

  static base::RepeatingCallback<void(void*, size_t)>
  GetDefaultRandomGenerator();
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNED_WEB_BUNDLE_ID_H_
