// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_ECDSA_P256_KEY_PAIR_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_ECDSA_P256_KEY_PAIR_H_

#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"

namespace web_package::test {

struct EcdsaP256KeyPair {
  static EcdsaP256KeyPair CreateRandom(bool produce_invalid_signature = false);

  EcdsaP256KeyPair(
      base::span<const uint8_t, EcdsaP256PublicKey::kLength> public_key_bytes,
      base::span<const uint8_t, 32> private_key_bytes,
      bool produce_invalid_signature = false);

  EcdsaP256KeyPair(const EcdsaP256KeyPair&);
  EcdsaP256KeyPair& operator=(const EcdsaP256KeyPair&);

  EcdsaP256KeyPair(EcdsaP256KeyPair&&) noexcept;
  EcdsaP256KeyPair& operator=(EcdsaP256KeyPair&&) noexcept;

  ~EcdsaP256KeyPair();

  EcdsaP256PublicKey public_key;
  // We don't have a wrapper for private keys since they are only used in
  // tests.
  std::array<uint8_t, 32> private_key;
  bool produce_invalid_signature;
};

std::vector<uint8_t> SignMessage(base::span<const uint8_t> message,
                                 const EcdsaP256KeyPair& key_pair);

}  // namespace web_package::test

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_ECDSA_P256_KEY_PAIR_H_
