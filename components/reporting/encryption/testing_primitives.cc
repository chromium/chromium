// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/testing_primitives.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/check_op.h"
#include "base/strings/string_view_util.h"
#include "components/reporting/encryption/primitives.h"
#include "crypto/aead.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Ne;

namespace reporting {
namespace test {

void GenerateEncryptionKeyPair(base::span<uint8_t, kKeySize> private_key,
                               base::span<uint8_t, kKeySize> public_value) {
  X25519_keypair(public_value.data(), private_key.data());
}

// TODO(https://issues.chromium.org/issues/431824286): use crypto/keyexchange
void RestoreSharedSecret(base::span<const uint8_t, kKeySize> private_key,
                         base::span<const uint8_t, kKeySize> peer_public_value,
                         base::span<uint8_t, kKeySize> shared_secret) {
  ASSERT_TRUE(X25519(shared_secret.data(), private_key.data(),
                     peer_public_value.data()));
}

void PerformSymmetricDecryption(base::span<const uint8_t, kKeySize> key,
                                std::string_view input_data,
                                std::string* output_data) {
  // Decrypt the data with symmetric key using AEAD interface.
  CHECK_EQ(kKeySize, crypto::aead::KeySizeFor(crypto::aead::CHACHA20_POLY1305));
  CHECK_EQ(kNonceSize,
           crypto::aead::NonceSizeFor(crypto::aead::CHACHA20_POLY1305));

  // Get nonce at the head of input_data.
  std::string_view nonce = input_data.substr(0, kNonceSize);

  // Decrypt collected record.
  std::optional<std::vector<uint8_t>> decrypted =
      crypto::aead::Open(crypto::aead::CHACHA20_POLY1305, key,
                         base::as_byte_span(input_data.substr(kNonceSize)),
                         base::as_byte_span(nonce), /*associated_data=*/{});

  ASSERT_TRUE(decrypted.has_value());
  *output_data = std::string(base::as_string_view(*decrypted));
}

void GenerateSigningKeyPair(base::span<uint8_t, kSignKeySize> private_key,
                            base::span<uint8_t, kKeySize> public_value) {
  ED25519_keypair(public_value.data(), private_key.data());
}

void SignMessage(base::span<const uint8_t, kSignKeySize> signing_key,
                 std::string_view message,
                 base::span<uint8_t, kSignatureSize> signature) {
  ASSERT_THAT(ED25519_sign(signature.data(),
                           reinterpret_cast<const uint8_t*>(message.data()),
                           message.size(), signing_key.data()),
              Eq(1));
}

}  // namespace test
}  // namespace reporting
