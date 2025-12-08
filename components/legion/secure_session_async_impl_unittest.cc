// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_session_async_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/legion/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

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

  std::unique_ptr<FakeOakSessionService> fake_oak_session_service_;
  std::unique_ptr<SecureSessionAsyncImpl> secure_session_;
};

TEST_F(SecureSessionAsyncImplTest, EncryptDisconnect) {
  base::test::TestFuture<std::optional<oak::session::v1::EncryptedMessage>>
      future;
  secure_session_->Encrypt({}, future.GetCallback());
  fake_oak_session_service_.reset();
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(SecureSessionAsyncImplTest, DecryptDisconnect) {
  base::test::TestFuture<const std::optional<std::vector<uint8_t>>&> future;
  secure_session_->Decrypt({}, future.GetCallback());
  fake_oak_session_service_.reset();
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace

}  // namespace legion
