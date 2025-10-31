// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_session_impl.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

namespace {
// Length of a P-256 public key in uncompressed X9.62 format.
constexpr size_t kP256X962Length = 65;
}  // namespace

SecureSessionImpl::SecureSessionImpl() = default;

SecureSessionImpl::~SecureSessionImpl() = default;

std::optional<oak::session::v1::HandshakeRequest>
SecureSessionImpl::GetHandshakeMessage() {
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

  std::vector<uint8_t> ciphertext_request = noise_->EncryptAndHash({});

  oak::session::v1::HandshakeRequest handshake_request;
  auto* noise_message = handshake_request.mutable_noise_handshake_message();
  noise_message->set_ephemeral_public_key(ephemeral_public_key_bytes,
                                          sizeof(ephemeral_public_key_bytes));
  noise_message->set_ciphertext(ciphertext_request.data(),
                                ciphertext_request.size());

  return handshake_request;
}

bool SecureSessionImpl::ProcessHandshakeResponse(
    const oak::session::v1::HandshakeResponse& response) {
  if (!noise_) {
    DLOG(ERROR) << "Handshake not initiated.";
    return false;
  }

  const auto& noise_response = response.noise_handshake_message();
  std::vector<uint8_t> e(
      noise_response.ephemeral_public_key().begin(),
      noise_response.ephemeral_public_key().end());

  bssl::UniquePtr<EC_POINT> peer_point(
      EC_POINT_new(EC_KEY_get0_group(ephemeral_key_.get())));
  uint8_t shared_key_ee[32];
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  if (!EC_POINT_oct2point(group, peer_point.get(), e.data(), e.size(),
                          /*ctx=*/nullptr) ||
      ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), peer_point.get(),
                       ephemeral_key_.get(),
                       /*kdf=*/nullptr) != sizeof(shared_key_ee)) {
    DLOG(ERROR) << "Peer's P-256 point not on curve.";
    return false;
  }

  noise_->MixHash(e);
  noise_->MixKey(e);
  noise_->MixKey(shared_key_ee);

  std::vector<uint8_t> ciphertext_response(
      noise_response.ciphertext().begin(),
      noise_response.ciphertext().end());

  auto plaintext = noise_->DecryptAndHash(ciphertext_response);
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

std::optional<oak::session::v1::EncryptedMessage> SecureSessionImpl::Encrypt(
    const Request& data) {
  if (!crypter_) {
    DLOG(ERROR) << "Crypter not initialized. Handshake must be completed.";
    return std::nullopt;
  }

  auto encrypted_data = crypter_->Encrypt(data);
  if (!encrypted_data) {
    DLOG(ERROR) << "Encryption failed.";
    return std::nullopt;
  }

  oak::session::v1::EncryptedMessage encrypted_message;
  encrypted_message.set_ciphertext(encrypted_data->data(),
                                   encrypted_data->size());
  return encrypted_message;
}

std::optional<Response> SecureSessionImpl::Decrypt(
    const oak::session::v1::EncryptedMessage& data) {
  if (!crypter_) {
    DLOG(ERROR) << "Crypter not initialized. Handshake must be completed.";
    return std::nullopt;
  }
  std::vector<uint8_t> encrypted_response(data.ciphertext().begin(),
                                          data.ciphertext().end());

  return crypter_->Decrypt(encrypted_response);
}

}  // namespace legion
