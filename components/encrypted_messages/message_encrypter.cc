// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/encrypted_messages/message_encrypter.h"

#include <string_view>

#include "base/logging.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace encrypted_messages {

namespace {

bool GetHkdfSubkeySecret(size_t subkey_length,
                         const uint8_t* private_key,
                         const uint8_t* public_key,
                         std::string_view hkdf_label,
                         std::string* secret) {
  uint8_t shared_secret[X25519_SHARED_KEY_LEN];
  if (!X25519(shared_secret, private_key, public_key))
    return false;

  std::string_view hkdf_input(reinterpret_cast<char*>(shared_secret),
                              sizeof(shared_secret));
  *secret = crypto::HkdfSha256(hkdf_input, "", hkdf_label, subkey_length);
  return true;
}

}  // namespace

bool EncryptSerializedMessage(const uint8_t* server_public_key,
                              uint32_t server_public_key_version,
                              std::string_view hkdf_label,
                              const std::string& message,
                              EncryptedMessage* encrypted_message) {
  // Generate an ephemeral key pair to generate a shared secret.
  uint8_t public_key[X25519_PUBLIC_VALUE_LEN];
  uint8_t private_key[X25519_PRIVATE_KEY_LEN];

  crypto::RandBytes(private_key);
  X25519_public_from_private(public_key, private_key);

  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  std::string key;
  if (!GetHkdfSubkeySecret(aead.KeyLength(), private_key,
                           reinterpret_cast<const uint8_t*>(server_public_key),
                           hkdf_label, &key)) {
    LOG(ERROR) << "Error getting subkey secret.";
    return false;
  }
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), '\0');

  std::string ciphertext;
  if (!aead.Seal(message, nonce, std::string(), &ciphertext)) {
    LOG(ERROR) << "Error sealing message.";
    return false;
  }

  encrypted_message->set_encrypted_message(ciphertext);
  encrypted_message->set_server_public_key_version(server_public_key_version);
  encrypted_message->set_client_public_key(reinterpret_cast<char*>(public_key),
                                           sizeof(public_key));
  encrypted_message->set_algorithm(
      EncryptedMessage::AEAD_ECDH_AES_128_CTR_HMAC_SHA256);
  return true;
}

// Used only by tests.
bool DecryptMessageForTesting(const uint8_t server_private_key[32],
                              std::string_view hkdf_label,
                              const EncryptedMessage& encrypted_message,
                              std::string* decrypted_serialized_message) {
  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  std::string key;
  if (!GetHkdfSubkeySecret(aead.KeyLength(), server_private_key,
                           reinterpret_cast<const uint8_t*>(
                               encrypted_message.client_public_key().data()),
                           hkdf_label, &key)) {
    LOG(ERROR) << "Error getting subkey secret.";
    return false;
  }
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), 0);

  return aead.Open(encrypted_message.encrypted_message(), nonce, std::string(),
                   decrypted_serialized_message);
}

}  // namespace encrypted_messages
