// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/secure_session_async_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace private_ai {

namespace {

// FakeOakSessionService stores callbacks and never executes them on purpose.
//
// Otherwise if callbacks are destroyed before disconnection, it leads
// to a crash.
class FakeOakSessionService : public mojom::OakSession {
 public:
  FakeOakSessionService() = default;
  ~FakeOakSessionService() override = default;

  mojo::Remote<mojom::OakSession> BindAndCreateRemote() {
    mojo::Remote<mojom::OakSession> remote;
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  // mojom::OakSession:
  void InitiateHandshake(InitiateHandshakeCallback callback) override {
    initiate_handshake_callback_ = std::move(callback);
  }
  void CompleteHandshake(HandshakeMessage message,
                         CompleteHandshakeCallback callback) override {
    complete_handshake_callback_ = std::move(callback);
  }
  void Encrypt(const std::vector<uint8_t>& data,
               EncryptCallback callback) override {
    encrypt_callback_ = std::move(callback);
  }
  void Decrypt(const std::vector<uint8_t>& data,
               DecryptCallback callback) override {
    decrypt_callback_ = std::move(callback);
  }

  void RunInitiateHandshakeCallback(HandshakeMessage message) {
    // Mojo service is not called instantly, therefore we have to wait here.
    CHECK(base::test::RunUntil(
        [&]() { return !initiate_handshake_callback_.is_null(); }));
    std::move(initiate_handshake_callback_).Run(std::move(message));
  }

  void RunCompleteHandshakeCallback(bool handshake_verified) {
    // Mojo service is not called instantly, therefore we have to wait here.
    CHECK(base::test::RunUntil(
        [&]() { return !complete_handshake_callback_.is_null(); }));
    std::move(complete_handshake_callback_).Run(handshake_verified);
  }

  void RunEncryptCallback(const std::optional<std::vector<uint8_t>>& data) {
    // Mojo service is not called instantly, therefore we have to wait here.
    CHECK(base::test::RunUntil([&]() { return !encrypt_callback_.is_null(); }));
    std::move(encrypt_callback_).Run(data);
  }

  void RunDecryptCallback(const std::optional<std::vector<uint8_t>>& data) {
    // Mojo service is not called instantly, therefore we have to wait here.
    CHECK(base::test::RunUntil([&]() { return !decrypt_callback_.is_null(); }));
    std::move(decrypt_callback_).Run(data);
  }

 private:
  mojo::Receiver<mojom::OakSession> receiver_{this};

  InitiateHandshakeCallback initiate_handshake_callback_;
  CompleteHandshakeCallback complete_handshake_callback_;
  EncryptCallback encrypt_callback_;
  DecryptCallback decrypt_callback_;
};

// This class tests that SecureSessionAsyncImpl handles gracefully when remote
// Mojo service is disconnected (e.g. due crash or memory pressure).
class SecureSessionAsyncImplTest : public ::testing::Test {
 public:
  SecureSessionAsyncImplTest()
      : fake_oak_session_service_(std::make_unique<FakeOakSessionService>()),
        secure_session_(SecureSessionAsyncImpl::CreateForTesting(
            fake_oak_session_service_->BindAndCreateRemote())) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<FakeOakSessionService> fake_oak_session_service_;
  std::unique_ptr<SecureSessionAsyncImpl> secure_session_;
};

TEST_F(SecureSessionAsyncImplTest, GetHandshakeMessageDisconnect) {
  base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
      future;
  secure_session_->GetHandshakeMessage(future.GetCallback());
  fake_oak_session_service_.reset();
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.InitiateHandshake", false, 1);
}

TEST_F(SecureSessionAsyncImplTest, ProcessHandshakeResponseDisconnect) {
  base::test::TestFuture<bool> future;

  // `HandshakeResponse` should be valid, otherwise inner Mojo service
  // will not be called.
  oak::session::v1::HandshakeResponse response;
  {
    auto* server_noise_msg = response.mutable_noise_handshake_message();
    uint8_t server_e_pub_bytes[kP256X962Length] = {0};  // Test key
    server_noise_msg->set_ephemeral_public_key(server_e_pub_bytes,
                                               sizeof(server_e_pub_bytes));
    server_noise_msg->set_ciphertext("corrupted ciphertext");
  }

  secure_session_->ProcessHandshakeResponse(response, future.GetCallback());
  fake_oak_session_service_.reset();
  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.CompleteHandshake", false, 1);
}

TEST_F(SecureSessionAsyncImplTest, EncryptDisconnect) {
  base::test::TestFuture<std::optional<oak::session::v1::EncryptedMessage>>
      future;
  secure_session_->Encrypt({}, future.GetCallback());
  fake_oak_session_service_.reset();
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.Encrypt", false, 1);
}

TEST_F(SecureSessionAsyncImplTest, DecryptDisconnect) {
  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
  secure_session_->Decrypt({}, future.GetCallback());
  fake_oak_session_service_.reset();
  EXPECT_FALSE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.Decrypt", false, 1);
}

TEST_F(SecureSessionAsyncImplTest, GetHandshakeMessageSuccess) {
  base::test::TestFuture<std::optional<oak::session::v1::HandshakeRequest>>
      future;
  secure_session_->GetHandshakeMessage(future.GetCallback());

  HandshakeMessage message(
      {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
       0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
       0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21,
       0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
       0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
       0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40},
      {0x01, 0x02, 0x03});
  fake_oak_session_service_->RunInitiateHandshakeCallback(std::move(message));

  EXPECT_TRUE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.InitiateHandshake", true, 1);
}

TEST_F(SecureSessionAsyncImplTest, ProcessHandshakeResponseSuccess) {
  base::test::TestFuture<bool> future;

  oak::session::v1::HandshakeResponse response;
  {
    auto* server_noise_msg = response.mutable_noise_handshake_message();
    uint8_t server_e_pub_bytes[kP256X962Length] = {0};  // Test key
    server_noise_msg->set_ephemeral_public_key(server_e_pub_bytes,
                                               sizeof(server_e_pub_bytes));
    server_noise_msg->set_ciphertext("valid ciphertext");
  }

  secure_session_->ProcessHandshakeResponse(response, future.GetCallback());

  fake_oak_session_service_->RunCompleteHandshakeCallback(true);

  EXPECT_TRUE(future.Get());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.CompleteHandshake", true, 1);
}

TEST_F(SecureSessionAsyncImplTest, EncryptSuccess) {
  base::test::TestFuture<std::optional<oak::session::v1::EncryptedMessage>>
      future;
  secure_session_->Encrypt({}, future.GetCallback());

  fake_oak_session_service_->RunEncryptCallback(
      std::vector<uint8_t>{'t', 'e', 's', 't'});

  EXPECT_TRUE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.Encrypt", true, 1);
}

TEST_F(SecureSessionAsyncImplTest, DecryptSuccess) {
  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
  secure_session_->Decrypt({}, future.GetCallback());

  fake_oak_session_service_->RunDecryptCallback(
      std::vector<uint8_t>{'t', 'e', 's', 't'});

  EXPECT_TRUE(future.Get().has_value());
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.OakSessionSandboxStability.Decrypt", true, 1);
}

}  // namespace

}  // namespace private_ai
