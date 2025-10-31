// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_session_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

namespace {

constexpr size_t kEphemeralPublicKeySize = 65;

// Helper class to simulate the server-side of a secure session. This class
// mirrors the functionality of `SecureSessionImpl` for the responder role in a
// Noise handshake, and is used for end-to-end testing.
class ServerSecureSession {
 public:
  ServerSecureSession() {
    // Initialize server Noise state for NN handshake.
    noise_.Init(Noise::HandshakeType::kNN);
    uint8_t prologue[1] = {0};
    noise_.MixHash(prologue);
  }

  // Processes the client's opening handshake message, generates a response,
  // and establishes session keys. A payload with a default empty value can be
  // included in the response for testing invalid handshake scenarios.
  std::optional<oak::session::v1::HandshakeResponse> ProcessHandshake(
      const oak::session::v1::HandshakeRequest& client_handshake_request,
      const std::vector<uint8_t>& payload = {}) {
    bssl::UniquePtr<EC_KEY> server_e_key(
        EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    const EC_GROUP* group = EC_KEY_get0_group(server_e_key.get());

    bssl::UniquePtr<EC_POINT> client_e_point;
    if (!ProcessClientRequest(client_handshake_request, group,
                              &client_e_point)) {
      return std::nullopt;
    }

    if (!EC_KEY_generate_key(server_e_key.get())) {
      return std::nullopt;
    }

    return GenerateHandshakeResponse(server_e_key.get(), client_e_point.get(),
                                     payload);
  }

  std::optional<Response> Decrypt(
      const oak::session::v1::EncryptedMessage& request) {
    if (!crypter_) {
      return std::nullopt;
    }
    std::string ciphertext_str = request.ciphertext();
    std::vector<uint8_t> ciphertext(ciphertext_str.begin(),
                                    ciphertext_str.end());
    return crypter_->Decrypt(ciphertext);
  }

  std::optional<oak::session::v1::EncryptedMessage> Encrypt(
      const Request& plaintext) {
    if (!crypter_) {
      return std::nullopt;
    }
    auto ciphertext = crypter_->Encrypt(plaintext);
    if (!ciphertext) {
      return std::nullopt;
    }
    oak::session::v1::EncryptedMessage response;
    response.set_ciphertext(ciphertext->data(), ciphertext->size());
    return response;
  }

 private:
  // Processes the client's handshake request and performs the initial part of
  // the Noise handshake protocol. Returns the client's ephemeral public key
  // point on success.
  bool ProcessClientRequest(
      const oak::session::v1::HandshakeRequest& request,
      const EC_GROUP* group,
      bssl::UniquePtr<EC_POINT>* out_client_e_point) {
    const auto& client_noise_msg = request.noise_handshake_message();
    std::vector<uint8_t> client_e_pub(
        client_noise_msg.ephemeral_public_key().begin(),
        client_noise_msg.ephemeral_public_key().end());
    std::vector<uint8_t> client_ciphertext(
        client_noise_msg.ciphertext().begin(),
        client_noise_msg.ciphertext().end());

    noise_.MixHash(client_e_pub);
    noise_.MixKey(client_e_pub);

    auto plaintext = noise_.DecryptAndHash(client_ciphertext);
    if (!plaintext.has_value() || !plaintext->empty()) {
      return false;
    }

    *out_client_e_point = bssl::UniquePtr<EC_POINT>(EC_POINT_new(group));
    if (!EC_POINT_oct2point(group, out_client_e_point->get(),
                            client_e_pub.data(), client_e_pub.size(),
                            nullptr)) {
      return false;
    }
    return true;
  }

  // Completes the handshake, generates the server's handshake response, and
  // establishes session keys.
  std::optional<oak::session::v1::HandshakeResponse> GenerateHandshakeResponse(
      EC_KEY* server_e_key,
      const EC_POINT* client_e_point,
      const std::vector<uint8_t>& payload) {
    const EC_GROUP* group = EC_KEY_get0_group(server_e_key);

    uint8_t server_e_pub_bytes[kEphemeralPublicKeySize] = {0};
    if (sizeof(server_e_pub_bytes) !=
        EC_POINT_point2oct(group, EC_KEY_get0_public_key(server_e_key),
                           POINT_CONVERSION_UNCOMPRESSED, server_e_pub_bytes,
                           sizeof(server_e_pub_bytes), nullptr)) {
      return std::nullopt;
    }

    noise_.MixHash(server_e_pub_bytes);
    noise_.MixKey(server_e_pub_bytes);

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

    oak::session::v1::HandshakeResponse server_handshake_response;
    auto* server_noise_msg =
        server_handshake_response.mutable_noise_handshake_message();
    server_noise_msg->set_ephemeral_public_key(
        server_e_pub_bytes, sizeof(server_e_pub_bytes));
    server_noise_msg->set_ciphertext(server_ciphertext.data(),
                                     server_ciphertext.size());
    return server_handshake_response;
  }

  Noise noise_;
  std::unique_ptr<Crypter> crypter_;
};

class SecureSessionImplTest : public ::testing::Test {
 protected:
  void PerformValidHandshake(ServerSecureSession& server_session) {
    auto client_handshake_request = client_session_.GetHandshakeMessage();
    ASSERT_TRUE(client_handshake_request.has_value());

    auto server_handshake_response =
        server_session.ProcessHandshake(client_handshake_request.value());
    ASSERT_TRUE(server_handshake_response.has_value());

    ASSERT_TRUE(client_session_.ProcessHandshakeResponse(
        server_handshake_response.value()));
  }

  SecureSessionImpl client_session_;
};

// End-to-end test of the handshake and encryption/decryption in both
// directions.
TEST_F(SecureSessionImplTest, HandshakeAndEncryptDecryptSucceeds) {
  ServerSecureSession server_session;
  PerformValidHandshake(server_session);

  // Test encryption and decryption from client to server.
  const Request client_plaintext = {1, 2, 3};
  auto encrypted_from_client = client_session_.Encrypt(client_plaintext);
  ASSERT_TRUE(encrypted_from_client.has_value());

  auto decrypted_by_server =
      server_session.Decrypt(encrypted_from_client.value());
  ASSERT_TRUE(decrypted_by_server.has_value());
  EXPECT_EQ(client_plaintext, decrypted_by_server.value());

  // Test encryption and decryption from server to client.
  const Request server_plaintext = {4, 5, 6};
  auto encrypted_from_server = server_session.Encrypt(server_plaintext);
  ASSERT_TRUE(encrypted_from_server.has_value());

  auto decrypted_by_client =
      client_session_.Decrypt(encrypted_from_server.value());
  ASSERT_TRUE(decrypted_by_client.has_value());
  EXPECT_EQ(server_plaintext, decrypted_by_client.value());
}

TEST_F(SecureSessionImplTest, GetHandshakeMessageSucceeds) {
  auto request = client_session_.GetHandshakeMessage();
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->has_noise_handshake_message());

  const auto& noise_msg = request->noise_handshake_message();
  EXPECT_EQ(noise_msg.ephemeral_public_key().size(), kEphemeralPublicKeySize);
  EXPECT_FALSE(noise_msg.ciphertext().empty());
}

TEST_F(SecureSessionImplTest, ProcessHandshakeResponseInvalidPeerKey) {
  auto request = client_session_.GetHandshakeMessage();
  ASSERT_TRUE(request.has_value());

  oak::session::v1::HandshakeResponse response;
  auto* noise_msg = response.mutable_noise_handshake_message();
  // Malform the key by providing an incorrect size.
  noise_msg->set_ephemeral_public_key("invalid key", 11);
  noise_msg->set_ciphertext("some ciphertext");

  EXPECT_FALSE(client_session_.ProcessHandshakeResponse(response));
}

TEST_F(SecureSessionImplTest, ProcessHandshakeResponseInvalidCiphertext) {
  auto client_handshake_request_opt = client_session_.GetHandshakeMessage();
  ASSERT_TRUE(client_handshake_request_opt.has_value());

  // Create a valid server response, but then corrupt the ciphertext.
  oak::session::v1::HandshakeResponse server_handshake_response;
  auto* server_noise_msg =
      server_handshake_response.mutable_noise_handshake_message();

  uint8_t server_e_pub_bytes[kEphemeralPublicKeySize] = {0};  // Test key
  server_noise_msg->set_ephemeral_public_key(
      server_e_pub_bytes, sizeof(server_e_pub_bytes));
  server_noise_msg->set_ciphertext("corrupted ciphertext");

  EXPECT_FALSE(
      client_session_.ProcessHandshakeResponse(server_handshake_response));
}

TEST_F(SecureSessionImplTest, EncryptBeforeHandshake) {
  const Request client_plaintext = {1, 2, 3};
  auto encrypted = client_session_.Encrypt(client_plaintext);
  EXPECT_FALSE(encrypted.has_value());
}

TEST_F(SecureSessionImplTest, DecryptBeforeHandshake) {
  oak::session::v1::EncryptedMessage encrypted_message;
  encrypted_message.set_ciphertext("some data");
  auto decrypted = client_session_.Decrypt(encrypted_message);
  EXPECT_FALSE(decrypted.has_value());
}

// Tests that ProcessHandshakeResponse fails if called before GetHandshakeMessage.
TEST_F(SecureSessionImplTest, ProcessHandshakeResponseWithoutHandshake) {
  oak::session::v1::HandshakeResponse response;
  EXPECT_FALSE(client_session_.ProcessHandshakeResponse(response));
}

// Tests that the handshake fails if the server's response includes a payload,
// which is not allowed in the NN handshake pattern.
TEST_F(SecureSessionImplTest, ProcessHandshakeResponseNonEmptyPlaintext) {
  auto client_handshake_request = client_session_.GetHandshakeMessage();
  ASSERT_TRUE(client_handshake_request.has_value());

  ServerSecureSession server_session;
  // Generate a server response with a non-empty payload, which is invalid for
  // the NN handshake pattern.
  auto server_handshake_response = server_session.ProcessHandshake(
      client_handshake_request.value(), {1, 2, 3});
  ASSERT_TRUE(server_handshake_response.has_value());

  // The client should reject the response because the decrypted payload is not
  // empty.
  EXPECT_FALSE(client_session_.ProcessHandshakeResponse(
      server_handshake_response.value()));
}

}  // namespace

}  // namespace legion
