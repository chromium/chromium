// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/device_to_device_authenticator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/secure_channel/authenticator.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/device_to_device_responder_operations.h"
#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_context.h"
#include "chromeos/ash/services/secure_channel/session_keys.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

// The initiator's session public key in base64url form. Note that this is
// actually a serialized proto.
const char kInitiatorSessionPublicKeyBase64[] =
    "CAESRQogOlH8DgPMQu7eAt-b6yoTXcazG8mAl6SPC5Ds-LTULIcSIQDZDMqsoYRO4tNMej1FB"
    "El1sTiTiVDqrcGq-CkYCzDThw==";

// The initiator's session public key in base64url form. Note that this is
// actually a serialized proto.
const char kResponderSessionPublicKeyBase64[] =
    "CAESRgohAN9QYU5HySO14Gi9PDIClacBnC0C8wqPwXsNHUNG_vXlEiEAggzU80ZOd9DWuCBdp"
    "6bzpGcC-oj1yrwdVCHGg_yeaAQ=";

// Callback saving the result of ValidateHelloMessage().
void SaveValidateHelloMessageResult(bool* validated_out,
                                    std::string* public_key_out,
                                    bool validated,
                                    const std::string& public_key) {
  *validated_out = validated;
  *public_key_out = public_key;
}

// Connection implementation for testing.
class FakeConnection : public Connection {
 public:
  explicit FakeConnection(multidevice::RemoteDeviceRef remote_device)
      : Connection(remote_device), connection_blocked_(false) {}

  FakeConnection(const FakeConnection&) = delete;
  FakeConnection& operator=(const FakeConnection&) = delete;

  ~FakeConnection() override {}

  // Connection:
  void Connect() override { SetStatus(Connection::Status::CONNECTED); }
  void Disconnect() override { SetStatus(Connection::Status::DISCONNECTED); }
  std::string GetDeviceAddress() override { return std::string(); }

  using Connection::OnBytesReceived;

  void ClearMessageBuffer() { message_buffer_.clear(); }

  const std::vector<std::unique_ptr<WireMessage>>& message_buffer() {
    return message_buffer_;
  }

  void set_connection_blocked(bool connection_blocked) {
    connection_blocked_ = connection_blocked;
  }

  bool connection_blocked() { return connection_blocked_; }

 protected:
  // Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override {
    const WireMessage& message_alias = *message;
    message_buffer_.push_back(std::move(message));
    OnDidSendMessage(message_alias, !connection_blocked_);
  }

  void RegisterPayloadFileImpl(
      int64_t payload_id,
      mojom::PayloadFilesPtr payload_files,
      FileTransferUpdateCallback file_transfer_update_callback,
      base::OnceCallback<void(bool)> registration_result_callback) override {
    std::move(registration_result_callback).Run(/*success=*/false);
  }

 private:
  std::vector<std::unique_ptr<WireMessage>> message_buffer_;

  bool connection_blocked_;
};

// Harness for testing DeviceToDeviceAuthenticator.
class DeviceToDeviceAuthenticatorForTest : public DeviceToDeviceAuthenticator {
 public:
  DeviceToDeviceAuthenticatorForTest(
      Connection* connection,
      std::unique_ptr<multidevice::SecureMessageDelegate>
          secure_message_delegate)
      : DeviceToDeviceAuthenticator(connection,
                                    std::move(secure_message_delegate)),
        timer_(nullptr) {}

  DeviceToDeviceAuthenticatorForTest(
      const DeviceToDeviceAuthenticatorForTest&) = delete;
  DeviceToDeviceAuthenticatorForTest& operator=(
      const DeviceToDeviceAuthenticatorForTest&) = delete;

  ~DeviceToDeviceAuthenticatorForTest() override {}

  base::MockOneShotTimer* timer() { return timer_; }

 private:
  // DeviceToDeviceAuthenticator:
  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    auto timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    return timer;
  }

  // This instance is owned by the super class.
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> timer_;

  base::test::SingleThreadTaskEnvironment env_;
};

}  // namespace

class SecureChannelDeviceToDeviceAuthenticatorTest : public testing::Test {
 public:
  SecureChannelDeviceToDeviceAuthenticatorTest()
      : remote_device_(multidevice::CreateRemoteDeviceRefForTest()),
        connection_(remote_device_),
        secure_message_delegate_(new multidevice::FakeSecureMessageDelegate),
        authenticator_(&connection_,
                       base::WrapUnique(secure_message_delegate_.get())) {}

  SecureChannelDeviceToDeviceAuthenticatorTest(
      const SecureChannelDeviceToDeviceAuthenticatorTest&) = delete;
  SecureChannelDeviceToDeviceAuthenticatorTest& operator=(
      const SecureChannelDeviceToDeviceAuthenticatorTest&) = delete;

  ~SecureChannelDeviceToDeviceAuthenticatorTest() override {}

  void SetUp() override {
    // Set up the session asymmetric keys for both the local and remote devices.
    ASSERT_TRUE(
        base::Base64UrlDecode(kInitiatorSessionPublicKeyBase64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &local_session_public_key_));
    ASSERT_TRUE(
        base::Base64UrlDecode(kResponderSessionPublicKeyBase64,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &remote_session_public_key_));
    remote_session_private_key_ =
        secure_message_delegate_->GetPrivateKeyForPublicKey(
            remote_session_public_key_),

    secure_message_delegate_->set_next_public_key(local_session_public_key_);
    connection_.Connect();

    base::test::TestFuture<const std::string&> future;
    secure_message_delegate_->DeriveKey(remote_session_private_key_,
                                        local_session_public_key_,
                                        future.GetCallback());
    session_symmetric_key_ = future.Take();
  }

  // Begins authentication, and returns the [Hello] message sent from the local
  // device to the remote device.
  std::string BeginAuthentication() {
    authenticator_.Authenticate(base::BindOnce(
        &SecureChannelDeviceToDeviceAuthenticatorTest::OnAuthenticationResult,
        base::Unretained(this)));

    EXPECT_EQ(1u, connection_.message_buffer().size());
    std::string hello_message = connection_.message_buffer()[0]->payload();
    connection_.ClearMessageBuffer();

    bool validated = false;
    std::string local_session_public_key;
    DeviceToDeviceResponderOperations::ValidateHelloMessage(
        hello_message, remote_device_.persistent_symmetric_key(),
        secure_message_delegate_,
        base::BindOnce(&SaveValidateHelloMessageResult, &validated,
                       &local_session_public_key));

    EXPECT_TRUE(validated);
    EXPECT_EQ(local_session_public_key_, local_session_public_key);

    return hello_message;
  }

  // Simulate receiving a valid [Responder Auth] message from the remote device.
  std::string SimulateResponderAuth(const std::string& hello_message) {
    std::string remote_device_private_key =
        secure_message_delegate_->GetPrivateKeyForPublicKey(
            multidevice::kTestRemoteDevicePublicKey);

    base::test::TestFuture<const std::string&> future;
    DeviceToDeviceResponderOperations::CreateResponderAuthMessage(
        hello_message, remote_session_public_key_, remote_session_private_key_,
        remote_device_private_key, remote_device_.persistent_symmetric_key(),
        secure_message_delegate_, future.GetCallback());
    std::string responder_auth_message = future.Take();
    EXPECT_FALSE(responder_auth_message.empty());

    WireMessage wire_message(responder_auth_message,
                             Authenticator::kAuthenticationFeature);
    connection_.OnBytesReceived(wire_message.Serialize());

    return responder_auth_message;
  }

  void OnAuthenticationResult(Authenticator::Result result,
                              std::unique_ptr<SecureContext> secure_context) {
    secure_context_ = std::move(secure_context);
    OnAuthenticationResultProxy(result);
  }

  MOCK_METHOD1(OnAuthenticationResultProxy, void(Authenticator::Result result));

  // Contains information about the remote device.
  const multidevice::RemoteDeviceRef remote_device_;

  // Simulates the connection to the remote device.
  FakeConnection connection_;

  // The SecureMessageDelegate used by the authenticator.
  // Owned by |authenticator_|.
  raw_ptr<multidevice::FakeSecureMessageDelegate, DanglingUntriaged>
      secure_message_delegate_;

  // The DeviceToDeviceAuthenticator under test.
  DeviceToDeviceAuthenticatorForTest authenticator_;

  // The session keys in play during authentication.
  std::string local_session_public_key_;
  std::string remote_session_public_key_;
  std::string remote_session_private_key_;
  std::string session_symmetric_key_;

  // Stores the SecureContext returned after authentication succeeds.
  std::unique_ptr<SecureContext> secure_context_;
};

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest, AuthenticateSucceeds) {
  // Starts the authentication protocol and grab [Hello] message.
  std::string hello_message = BeginAuthentication();

  // Simulate receiving a valid [Responder Auth] from the remote device.
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::SUCCESS));
  std::string responder_auth_message = SimulateResponderAuth(hello_message);
  EXPECT_TRUE(secure_context_);

  // Validate the local device sends a valid [Initiator Auth] message.
  ASSERT_EQ(1u, connection_.message_buffer().size());
  std::string initiator_auth = connection_.message_buffer()[0]->payload();

  base::test::TestFuture<bool> future;
  DeviceToDeviceResponderOperations::ValidateInitiatorAuthMessage(
      initiator_auth, SessionKeys(session_symmetric_key_),
      remote_device_.persistent_symmetric_key(), responder_auth_message,
      secure_message_delegate_, future.GetCallback());
  ASSERT_TRUE(future.Get());
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest, ResponderRejectsHello) {
  std::string hello_message = BeginAuthentication();

  // If the responder could not validate the [Hello message], it essentially
  // sends random bytes back for privacy reasons.
  WireMessage wire_message(base::RandBytesAsString(300u),
                           Authenticator::kAuthenticationFeature);
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  connection_.OnBytesReceived(wire_message.Serialize());
  EXPECT_FALSE(secure_context_);
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest, ResponderAuthTimesOut) {
  // Starts the authentication protocol and grab [Hello] message.
  std::string hello_message = BeginAuthentication();
  ASSERT_TRUE(authenticator_.timer());
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  authenticator_.timer()->Fire();
  EXPECT_FALSE(secure_context_);
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest,
       DisconnectsWaitingForResponderAuth) {
  std::string hello_message = BeginAuthentication();
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::DISCONNECTED));
  connection_.Disconnect();
  EXPECT_FALSE(secure_context_);
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest, NotConnectedInitially) {
  connection_.Disconnect();
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::DISCONNECTED));
  authenticator_.Authenticate(base::BindOnce(
      &SecureChannelDeviceToDeviceAuthenticatorTest::OnAuthenticationResult,
      base::Unretained(this)));
  EXPECT_FALSE(secure_context_);
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest, FailToSendHello) {
  connection_.set_connection_blocked(true);
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  authenticator_.Authenticate(base::BindOnce(
      &SecureChannelDeviceToDeviceAuthenticatorTest::OnAuthenticationResult,
      base::Unretained(this)));
  EXPECT_FALSE(secure_context_);
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest, FailToSendInitiatorAuth) {
  std::string hello_message = BeginAuthentication();

  connection_.set_connection_blocked(true);
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::FAILURE));
  SimulateResponderAuth(hello_message);
  EXPECT_FALSE(secure_context_);
}

TEST_F(SecureChannelDeviceToDeviceAuthenticatorTest,
       SendMessagesAfterAuthenticationSuccess) {
  std::string hello_message = BeginAuthentication();
  EXPECT_CALL(*this,
              OnAuthenticationResultProxy(Authenticator::Result::SUCCESS));
  SimulateResponderAuth(hello_message);

  // Test that the authenticator is properly cleaned up after authentication
  // completes.
  WireMessage wire_message(base::RandBytesAsString(300u),
                           Authenticator::kAuthenticationFeature);
  connection_.SendMessage(std::make_unique<WireMessage>(
      base::RandBytesAsString(300u), Authenticator::kAuthenticationFeature));
  connection_.OnBytesReceived(wire_message.Serialize());
  connection_.SendMessage(std::make_unique<WireMessage>(
      base::RandBytesAsString(300u), Authenticator::kAuthenticationFeature));
  connection_.OnBytesReceived(wire_message.Serialize());
}

}  // namespace ash::secure_channel
