// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_GAIA_ID_HASH_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_GAIA_ID_HASH_H_

#include <compare>
#include <string>
#include <string_view>

namespace signin {

// Represents the hash of the Gaia ID of a signed-in user. This is useful for
// storing Gaia-keyed information without storing sensitive information (like
// the email address).
// Note: the hashed GaiaId here is not cryptographically-secure and should still
// be treated as a PII.
class GaiaIdHash {
 public:
  static GaiaIdHash FromGaiaId(std::string_view gaia_id);
  // |gaia_id_hash| is a string representing the binary hash of the gaia id. If
  // the input isn't of length crypto::kSHA256Length, it returns an invalid
  // GaiaIdHash object.
  static GaiaIdHash FromBinary(std::string gaia_id_hash);
  // If |gaia_id_base64_hash| isn't well-formed Base64 string, or doesn't decode
  // to a hash of the correct length, it returns an invalid GaiaIdHash object.
  static GaiaIdHash FromBase64(std::string_view gaia_id_base64_hash);

  GaiaIdHash();
  GaiaIdHash(const GaiaIdHash& other);
  GaiaIdHash& operator=(const GaiaIdHash& form);
  GaiaIdHash(GaiaIdHash&& other);
  GaiaIdHash& operator=(GaiaIdHash&& form);
  ~GaiaIdHash();

  std::string ToBinary() const;
  std::string ToBase64() const;

  bool IsValid() const;

  friend std::strong_ordering operator<=>(const GaiaIdHash&,
                                          const GaiaIdHash&) = default;
  friend bool operator==(const GaiaIdHash&, const GaiaIdHash&) = default;

 private:
  explicit GaiaIdHash(std::string gaia_id_hash);
  std::string gaia_id_hash_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_GAIA_ID_HASH_H_
