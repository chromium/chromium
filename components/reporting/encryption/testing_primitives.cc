// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/encryption/testing_primitives.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "base/strings/string_piece.h"
#include "crypto/aead.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"

#include "components/reporting/encryption/primitives.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Ne;

namespace reporting {
namespace test {

void GenerateEncryptionKeyPair(uint8_t private_key[kKeySize],
                               uint8_t public_value[kKeySize]) {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();

  X25519_keypair(public_value, private_key);
}

void RestoreSharedSecret(const uint8_t private_key[kKeySize],
                         const uint8_t peer_public_value[kKeySize],
                         uint8_t shared_secret[kKeySize]) {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();

  ASSERT_TRUE(X25519(shared_secret, private_key, peer_public_value));
}

void PerformSymmetricDecryption(const uint8_t symmetric_key[kKeySize],
                                std::string_view input_data,
                                std::string* output_data) {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();

  // Decrypt the data with symmetric key using AEAD interface.
  crypto::Aead aead(crypto::Aead::CHACHA20_POLY1305);
  CHECK_EQ(aead.KeyLength(), kKeySize);

  // Use the symmetric key for data decryption.
  aead.Init(base::make_span(symmetric_key, kKeySize));

  // Get nonce at the head of input_data.
  CHECK_EQ(aead.NonceLength(), kNonceSize);
  std::string_view nonce = input_data.substr(0, kNonceSize);

  // Decrypt collected record.
  std::string decrypted;
  ASSERT_TRUE(aead.Open(input_data.substr(kNonceSize), nonce, std::string(),
                        output_data));
}

void GenerateSigningKeyPair(uint8_t private_key[kSignKeySize],
                            uint8_t public_value[kKeySize]) {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();

  ED25519_keypair(public_value, private_key);
}

void SignMessage(const uint8_t signing_key[kSignKeySize],
                 std::string_view message,
                 uint8_t signature[kSignatureSize]) {
  // Make sure OpenSSL is initialized, in order to avoid data races later.
  crypto::EnsureOpenSSLInit();

  ASSERT_THAT(
      ED25519_sign(signature, reinterpret_cast<const uint8_t*>(message.data()),
                   message.size(), signing_key),
      Eq(1));
}

}  // namespace test
}  // namespace reporting
