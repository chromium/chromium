// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_ED25519_KEY_PAIR_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_ED25519_KEY_PAIR_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_package::test {

struct Ed25519KeyPair {
  static Ed25519KeyPair CreateRandom(bool produce_invalid_signature = false);

  Ed25519KeyPair(
      base::span<const uint8_t, Ed25519PublicKey::kLength> public_key_bytes,
      base::span<const uint8_t, 64> private_key_bytes,
      bool produce_invalid_signature = false);
  Ed25519KeyPair(const Ed25519KeyPair&);
  Ed25519KeyPair& operator=(const Ed25519KeyPair&);

  Ed25519KeyPair(Ed25519KeyPair&&) noexcept;
  Ed25519KeyPair& operator=(Ed25519KeyPair&&) noexcept;

  ~Ed25519KeyPair();

  Ed25519PublicKey public_key;
  // We don't have a wrapper for private keys since they are only used in
  // tests.
  std::array<uint8_t, 64> private_key;
  bool produce_invalid_signature;
};
std::vector<uint8_t> SignMessage(base::span<const uint8_t> message,
                                 const Ed25519KeyPair& key_pair);

}  // namespace web_package::test

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_ED25519_KEY_PAIR_H_
