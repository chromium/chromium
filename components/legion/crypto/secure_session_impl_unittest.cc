// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/crypto/secure_session_impl.h"

#include "components/legion/crypto/constants.h"
#include "components/legion/crypto/test_server_secure_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace legion {

namespace {

class SecureSessionImplTest : public ::testing::Test {
 protected:
  void PerformValidHandshake(TestServerSecureSession& server_session) {
    auto client_handshake_request = client_session_.GetHandshakeMessage();

    auto server_handshake_response =
        server_session.ProcessHandshake(client_handshake_request);
    ASSERT_TRUE(server_handshake_response.has_value());

    ASSERT_TRUE(client_session_.ProcessHandshakeResponse(
        server_handshake_response.value()));
  }

  SecureSessionImpl client_session_;
};

// End-to-end test of the handshake and encryption/decryption in both
// directions.
TEST_F(SecureSessionImplTest, HandshakeAndEncryptDecryptSucceeds) {
  TestServerSecureSession server_session;
  PerformValidHandshake(server_session);

  // Test encryption and decryption from client to server.
  const std::vector<uint8_t> client_plaintext = {1, 2, 3};
  auto encrypted_from_client = client_session_.Encrypt(client_plaintext);
  ASSERT_TRUE(encrypted_from_client.has_value());

  auto decrypted_by_server =
      server_session.Decrypt(encrypted_from_client.value());
  ASSERT_TRUE(decrypted_by_server.has_value());
  EXPECT_EQ(client_plaintext, decrypted_by_server.value());

  // Test encryption and decryption from server to client.
  const std::vector<uint8_t> server_plaintext = {4, 5, 6};
  auto encrypted_from_server = server_session.Encrypt(server_plaintext);
  ASSERT_TRUE(encrypted_from_server.has_value());

  auto decrypted_by_client =
      client_session_.Decrypt(encrypted_from_server.value());
  ASSERT_TRUE(decrypted_by_client.has_value());
  EXPECT_EQ(server_plaintext, decrypted_by_client.value());
}

TEST_F(SecureSessionImplTest, GetHandshakeMessageSucceeds) {
  auto request = client_session_.GetHandshakeMessage();
  EXPECT_EQ(request.ephemeral_public_key.size(), kP256X962Length);
  EXPECT_FALSE(request.ciphertext.empty());
}

TEST_F(SecureSessionImplTest, ProcessHandshakeResponseInvalidPeerKey) {
  // Though the result is not used, it's important to call GetHandshakeMessage()
  // before ProcessHandshakeResponse().
  client_session_.GetHandshakeMessage();

  constexpr char kInvalidKey[] = "invalid key";
  constexpr char kCiphertext[] = "some ciphertext";

  // Malform the key by providing an incorrect size.
  HandshakeMessage response(
      std::vector<uint8_t>(std::begin(kInvalidKey), std::end(kInvalidKey)),
      std::vector<uint8_t>(std::begin(kCiphertext), std::end(kCiphertext)));

  EXPECT_FALSE(client_session_.ProcessHandshakeResponse(response));
}

TEST_F(SecureSessionImplTest, ProcessHandshakeResponseInvalidCiphertext) {
  // Though the result is not used, it's important to call GetHandshakeMessage()
  // before ProcessHandshakeResponse().
  client_session_.GetHandshakeMessage();

  uint8_t server_e_pub_bytes[kP256X962Length] = {0};  // Test key

  constexpr char kCorruptedCiphertext[] = "corrupted ciphertext";

  // Create a valid server response, but then corrupt the ciphertext.
  HandshakeMessage server_handshake_response(
      std::vector(std::begin(server_e_pub_bytes), std::end(server_e_pub_bytes)),
      std::vector<uint8_t>(std::begin(kCorruptedCiphertext),
                           std::end(kCorruptedCiphertext)));

  EXPECT_FALSE(
      client_session_.ProcessHandshakeResponse(server_handshake_response));
}

TEST_F(SecureSessionImplTest, EncryptBeforeHandshake) {
  const std::vector<uint8_t> client_plaintext = {1, 2, 3};

  auto encrypted = client_session_.Encrypt(client_plaintext);
  EXPECT_FALSE(encrypted.has_value());
}

TEST_F(SecureSessionImplTest, DecryptBeforeHandshake) {
  const std::vector<uint8_t> response = {1, 2, 3};

  auto decrypted = client_session_.Decrypt(response);
  EXPECT_FALSE(decrypted.has_value());
}

// Tests that ProcessHandshakeResponse fails if called before
// GetHandshakeMessage.
TEST_F(SecureSessionImplTest, ProcessHandshakeResponseWithoutHandshake) {
  HandshakeMessage response({}, {});

  EXPECT_FALSE(client_session_.ProcessHandshakeResponse(response));
}

// Tests that the handshake fails if the server's response includes a payload,
// which is not allowed in the NN handshake pattern.
TEST_F(SecureSessionImplTest, ProcessHandshakeResponseNonEmptyPlaintext) {
  auto client_handshake_request = client_session_.GetHandshakeMessage();

  TestServerSecureSession server_session;
  // Generate a server response with a non-empty payload, which is invalid for
  // the NN handshake pattern.
  auto server_handshake_response =
      server_session.ProcessHandshake(client_handshake_request, {1, 2, 3});
  ASSERT_TRUE(server_handshake_response.has_value());

  // The client should reject the response because the decrypted payload is not
  // empty.
  EXPECT_FALSE(client_session_.ProcessHandshakeResponse(
      server_handshake_response.value()));
}

}  // namespace

}  // namespace legion
