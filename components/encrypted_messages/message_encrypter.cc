// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/encrypted_messages/message_encrypter.h"

#include <string_view>

#include "base/logging.h"
#include "base/strings/string_view_util.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "crypto/aead.h"
#include "crypto/kdf.h"
#include "crypto/kex.h"
#include "crypto/keypair.h"
#include "crypto/random.h"

namespace encrypted_messages {

namespace {

constexpr crypto::aead::Algorithm kAeadAlgorithm =
    crypto::aead::AES_128_CTR_HMAC_SHA256;

bool GetHkdfSubkeySecret(size_t subkey_length,
                         const crypto::keypair::PrivateKey& ours,
                         const crypto::keypair::PublicKey& theirs,
                         std::string_view hkdf_label,
                         std::vector<uint8_t>& secret) {
  uint8_t shared_secret[32];
  crypto::kex::X25519(theirs, ours, shared_secret);

  secret.resize(subkey_length);
  crypto::kdf::Hkdf(crypto::hash::kSha256, shared_secret, {},
                    base::as_byte_span(hkdf_label), secret);
  return true;
}

}  // namespace

bool EncryptSerializedMessage(base::span<const uint8_t, 32> server_public_key,
                              uint32_t server_public_key_version,
                              std::string_view hkdf_label,
                              const std::string& message,
                              EncryptedMessage* encrypted_message) {
  // Generate an ephemeral key pair to generate a shared secret.
  auto private_key = crypto::keypair::PrivateKey::GenerateX25519();
  auto public_key = private_key.ToX25519PublicKey();

  std::vector<uint8_t> key;
  if (!GetHkdfSubkeySecret(
          crypto::aead::KeySizeFor(kAeadAlgorithm), private_key,
          crypto::keypair::PublicKey::FromX25519PublicKey(server_public_key),
          hkdf_label, key)) {
    LOG(ERROR) << "Error getting subkey secret.";
    return false;
  }

  // Use an all-zero nonce because the key is random per-message.
  std::vector<uint8_t> nonce(crypto::aead::NonceSizeFor(kAeadAlgorithm), 0);

  std::vector<uint8_t> ciphertext = crypto::aead::Seal(
      kAeadAlgorithm, key, base::as_byte_span(message), nonce, {});

  encrypted_message->set_encrypted_message(ciphertext.data(),
                                           ciphertext.size());
  encrypted_message->set_server_public_key_version(server_public_key_version);
  encrypted_message->set_client_public_key(public_key.data(),
                                           public_key.size());
  encrypted_message->set_algorithm(
      EncryptedMessage::AEAD_ECDH_AES_128_CTR_HMAC_SHA256);
  return true;
}

// Used only by tests.
bool DecryptMessageForTesting(  // IN-TEST
    base::span<const uint8_t, 32> server_private_key,
    std::string_view hkdf_label,
    const EncryptedMessage& encrypted_message,
    std::string* decrypted_serialized_message) {
  auto client_public_key_span =
      base::as_byte_span(encrypted_message.client_public_key());
  if (client_public_key_span.size() != 32) {
    return false;
  }

  std::vector<uint8_t> key;
  if (!GetHkdfSubkeySecret(
          crypto::aead::KeySizeFor(kAeadAlgorithm),
          crypto::keypair::PrivateKey::FromX25519PrivateKey(server_private_key),
          crypto::keypair::PublicKey::FromX25519PublicKey(
              client_public_key_span.first<32u>()),
          hkdf_label, key)) {
    LOG(ERROR) << "Error getting subkey secret.";
    return false;
  }

  // Use an all-zero nonce because the key is random per-message.
  std::vector<uint8_t> nonce(crypto::aead::NonceSizeFor(kAeadAlgorithm), 0);

  auto plaintext = crypto::aead::Open(
      kAeadAlgorithm, key,
      base::as_byte_span(encrypted_message.encrypted_message()), nonce, {});
  if (!plaintext) {
    return false;
  }

  *decrypted_serialized_message = base::as_string_view(*plaintext);
  return true;
}

}  // namespace encrypted_messages
