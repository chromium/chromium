// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/crypto/test_server_secure_session.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "components/private_ai/crypto/constants.h"
#include "components/private_ai/crypto/crypter.h"
#include "components/private_ai/crypto/noise.h"
#include "components/private_ai/crypto/secure_session_impl.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace private_ai {

TestServerSecureSession::TestServerSecureSession() {
  // Initialize server Noise state for NN handshake.
  noise_.Init(Noise::HandshakeType::kNN);
  uint8_t prologue[1] = {0};
  noise_.MixHash(prologue);
}

TestServerSecureSession::~TestServerSecureSession() = default;

std::optional<HandshakeMessage> TestServerSecureSession::ProcessHandshake(
    const HandshakeMessage& client_handshake_request,
    const std::vector<uint8_t>& payload) {
  bssl::UniquePtr<EC_KEY> server_e_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(server_e_key.get());

  bssl::UniquePtr<EC_POINT> client_e_point;
  if (!ProcessClientRequest(client_handshake_request, group, &client_e_point)) {
    return std::nullopt;
  }

  if (!EC_KEY_generate_key(server_e_key.get())) {
    return std::nullopt;
  }

  return GenerateHandshakeResponse(server_e_key.get(), client_e_point.get(),
                                   payload);
}

std::optional<std::vector<uint8_t>> TestServerSecureSession::Decrypt(
    const std::vector<uint8_t>& input) {
  if (!crypter_) {
    return std::nullopt;
  }
  return crypter_->Decrypt(input);
}

std::optional<std::vector<uint8_t>> TestServerSecureSession::Encrypt(
    const std::vector<uint8_t>& input) {
  if (!crypter_) {
    return std::nullopt;
  }
  return crypter_->Encrypt(input);
}

bool TestServerSecureSession::ProcessClientRequest(
    const HandshakeMessage& request,
    const EC_GROUP* group,
    bssl::UniquePtr<EC_POINT>* out_client_e_point) {
  noise_.MixHash(request.ephemeral_public_key);
  noise_.MixKey(request.ephemeral_public_key);

  auto decoded_msg = noise_.DecryptAndHash(request.ciphertext);
  if (!decoded_msg.has_value() || !decoded_msg->empty()) {
    return false;
  }

  *out_client_e_point = bssl::UniquePtr<EC_POINT>(EC_POINT_new(group));
  if (!EC_POINT_oct2point(group, out_client_e_point->get(),
                          request.ephemeral_public_key.data(),
                          request.ephemeral_public_key.size(), nullptr)) {
    return false;
  }
  return true;
}

std::optional<HandshakeMessage>
TestServerSecureSession::GenerateHandshakeResponse(
    EC_KEY* server_e_key,
    const EC_POINT* client_e_point,
    const std::vector<uint8_t>& payload) {
  const EC_GROUP* group = EC_KEY_get0_group(server_e_key);

  std::array<uint8_t, kP256X962Length> server_ephemeral_public_key;
  if (server_ephemeral_public_key.size() !=
      EC_POINT_point2oct(group, EC_KEY_get0_public_key(server_e_key),
                         POINT_CONVERSION_UNCOMPRESSED,
                         server_ephemeral_public_key.data(),
                         server_ephemeral_public_key.size(), nullptr)) {
    return std::nullopt;
  }

  noise_.MixHash(server_ephemeral_public_key);
  noise_.MixKey(server_ephemeral_public_key);

  uint8_t shared_key_ee[32] = {0};
  if (sizeof(shared_key_ee) !=
      ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), client_e_point,
                       server_e_key, nullptr)) {
    return std::nullopt;
  }
  noise_.MixKey(shared_key_ee);

  std::vector<uint8_t> server_ciphertext = noise_.EncryptAndHash(payload);

  auto [server_read_key, server_write_key] = noise_.traffic_keys();
  crypter_ = std::make_unique<Crypter>(server_read_key, server_write_key);

  return HandshakeMessage(std::move(server_ephemeral_public_key),
                          std::move(server_ciphertext));
}

}  // namespace private_ai
