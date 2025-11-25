// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/crypto/secure_session_impl.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/legion/crypto/constants.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace legion {

HandshakeMessage::HandshakeMessage(std::vector<uint8_t> ephemeral_public_key,
                                   std::vector<uint8_t> ciphertext)
    : ephemeral_public_key(ephemeral_public_key), ciphertext(ciphertext) {}

HandshakeMessage::~HandshakeMessage() = default;

HandshakeMessage::HandshakeMessage(HandshakeMessage&&) = default;

HandshakeMessage& HandshakeMessage::operator=(HandshakeMessage&&) = default;

SecureSessionImpl::SecureSessionImpl() = default;

SecureSessionImpl::~SecureSessionImpl() = default;

HandshakeMessage SecureSessionImpl::GetHandshakeMessage() {
  noise_.emplace();
  noise_->Init(Noise::HandshakeType::kNN);
  uint8_t prologue[1];
  prologue[0] = 0;
  noise_->MixHash(prologue);

  ephemeral_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  CHECK(EC_KEY_generate_key(ephemeral_key_.get()));
  uint8_t ephemeral_public_key_bytes[kP256X962Length];
  CHECK_EQ(sizeof(ephemeral_public_key_bytes),
           EC_POINT_point2oct(
               group, EC_KEY_get0_public_key(ephemeral_key_.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_public_key_bytes,
               sizeof(ephemeral_public_key_bytes), /*ctx=*/nullptr));
  noise_->MixHash(ephemeral_public_key_bytes);
  noise_->MixKey(ephemeral_public_key_bytes);

  std::vector<uint8_t> ephemeral_public_key(
      std::begin(ephemeral_public_key_bytes),
      std::end(ephemeral_public_key_bytes));
  std::vector<uint8_t> ciphertext_request = noise_->EncryptAndHash({});

  return HandshakeMessage(std::move(ephemeral_public_key),
                          std::move(ciphertext_request));
}

bool SecureSessionImpl::ProcessHandshakeResponse(
    const HandshakeMessage& response) {
  if (!noise_) {
    DLOG(ERROR) << "Handshake not initiated.";
    return false;
  }

  bssl::UniquePtr<EC_POINT> peer_point(
      EC_POINT_new(EC_KEY_get0_group(ephemeral_key_.get())));
  uint8_t shared_key_ee[32];
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  if (!EC_POINT_oct2point(group, peer_point.get(),
                          response.ephemeral_public_key.data(),
                          response.ephemeral_public_key.size(),
                          /*ctx=*/nullptr) ||
      ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), peer_point.get(),
                       ephemeral_key_.get(),
                       /*kdf=*/nullptr) != sizeof(shared_key_ee)) {
    DLOG(ERROR) << "Peer's P-256 point not on curve.";
    return false;
  }

  noise_->MixHash(response.ephemeral_public_key);
  noise_->MixKey(response.ephemeral_public_key);
  noise_->MixKey(shared_key_ee);

  auto plaintext = noise_->DecryptAndHash(response.ciphertext);
  if (!plaintext || !plaintext->empty()) {
    DLOG(ERROR) << "Invalid handshake message: " << plaintext.has_value();
    return false;
  }
  auto [write_key, read_key] = noise_->traffic_keys();
  crypter_ = std::make_unique<Crypter>(read_key, write_key);
  noise_.reset();
  ephemeral_key_.reset();

  return true;
}

std::optional<std::vector<uint8_t>> SecureSessionImpl::Encrypt(
    const std::vector<uint8_t>& input) {
  if (!crypter_) {
    DLOG(ERROR) << "Crypter not initialized. Handshake must be completed.";
    return std::nullopt;
  }

  auto output = crypter_->Encrypt(input);
  if (!output) {
    DLOG(ERROR) << "Encryption failed.";
    return std::nullopt;
  }

  return output;
}

std::optional<std::vector<uint8_t>> SecureSessionImpl::Decrypt(
    const std::vector<uint8_t>& input) {
  if (!crypter_) {
    DLOG(ERROR) << "Crypter not initialized. Handshake must be completed.";
    return std::nullopt;
  }
  return crypter_->Decrypt(input);
}

void SecureSessionImpl::set_crypter_for_testing(
    std::unique_ptr<Crypter> crypter) {
  crypter_ = std::move(crypter);
}

}  // namespace legion
