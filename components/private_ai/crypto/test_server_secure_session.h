// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_CRYPTO_TEST_SERVER_SECURE_SESSION_H_
#define COMPONENTS_PRIVATE_AI_CRYPTO_TEST_SERVER_SECURE_SESSION_H_

#include <memory>
#include <optional>
#include <vector>

#include "components/private_ai/crypto/crypter.h"
#include "components/private_ai/crypto/noise.h"
#include "components/private_ai/crypto/secure_session_impl.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace private_ai {

// Helper class to simulate the server-side of a secure session. This class
// mirrors the functionality of `SecureSessionImpl` for the responder role in a
// Noise handshake, and is used for end-to-end testing.
class TestServerSecureSession {
 public:
  TestServerSecureSession();
  ~TestServerSecureSession();

  // Processes the client's opening handshake message, generates a response,
  // and establishes session keys. A payload with a default empty value can be
  // included in the response for testing invalid handshake scenarios.
  std::optional<HandshakeMessage> ProcessHandshake(
      const HandshakeMessage& client_handshake_request,
      const std::vector<uint8_t>& payload = {});

  std::optional<std::vector<uint8_t>> Decrypt(
      const std::vector<uint8_t>& input);

  std::optional<std::vector<uint8_t>> Encrypt(
      const std::vector<uint8_t>& input);

 private:
  // Processes the client's handshake request and performs the initial part of
  // the Noise handshake protocol. Returns the client's ephemeral public key
  // point on success.
  bool ProcessClientRequest(const HandshakeMessage& request,
                            const EC_GROUP* group,
                            bssl::UniquePtr<EC_POINT>* out_client_e_point);

  // Completes the handshake, generates the server's handshake response, and
  // establishes session keys.
  std::optional<HandshakeMessage> GenerateHandshakeResponse(
      EC_KEY* server_e_key,
      const EC_POINT* client_e_point,
      const std::vector<uint8_t>& payload);

  Noise noise_;
  std::unique_ptr<Crypter> crypter_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_CRYPTO_TEST_SERVER_SECURE_SESSION_H_
