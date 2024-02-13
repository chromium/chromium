// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/client_connection_parameters_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/secure_channel/fake_channel.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_structured_metrics_logger.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestFeature[] = "testFeature";

}  // namespace

class SecureChannelClientConnectionParametersImplTest : public testing::Test {
 public:
  SecureChannelClientConnectionParametersImplTest(
      const SecureChannelClientConnectionParametersImplTest&) = delete;
  SecureChannelClientConnectionParametersImplTest& operator=(
      const SecureChannelClientConnectionParametersImplTest&) = delete;

 protected:
  SecureChannelClientConnectionParametersImplTest() = default;
  ~SecureChannelClientConnectionParametersImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_connection_delegate_ = std::make_unique<FakeConnectionDelegate>();
    fake_secure_channel_structured_metrics_logger_ =
        std::make_unique<FakeSecureChannelStructuredMetricsLogger>();
    auto fake_connection_delegate_remote =
        fake_connection_delegate_->GenerateRemote();
    auto fake_secure_channel_structured_metrics_logger_remote =
        fake_secure_channel_structured_metrics_logger_->GenerateRemote();

    client_connection_parameters_ =
        ClientConnectionParametersImpl::Factory::Create(
            kTestFeature, std::move(fake_connection_delegate_remote),
            std::move(fake_secure_channel_structured_metrics_logger_remote));

    fake_observer_ = std::make_unique<FakeClientConnectionParametersObserver>();
    client_connection_parameters_->AddObserver(fake_observer_.get());
  }

  void TearDown() override {
    client_connection_parameters_->RemoveObserver(fake_observer_.get());
  }

  void DisconnectConnectionDelegateRemote() {
    base::RunLoop run_loop;
    fake_observer_->set_closure_for_next_callback(run_loop.QuitClosure());
    fake_connection_delegate_->DisconnectGeneratedRemotes();
    fake_secure_channel_structured_metrics_logger_->UnbindReceiver();
    run_loop.Run();
  }

  void CallOnConnection(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver,
      mojo::PendingReceiver<mojom::NearbyConnectionStateListener>
          nearby_connection_state_listener_receiver) {
    base::RunLoop run_loop;
    fake_connection_delegate_->set_closure_for_next_delegate_callback(
        run_loop.QuitClosure());
    client_connection_parameters_->SetConnectionSucceeded(
        std::move(channel), std::move(message_receiver_receiver),
        std::move(nearby_connection_state_listener_receiver));
    run_loop.Run();
  }

  void CallOnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) {
    base::RunLoop run_loop;
    fake_connection_delegate_->set_closure_for_next_delegate_callback(
        run_loop.QuitClosure());
    client_connection_parameters_->SetConnectionAttemptFailed(reason);
    run_loop.Run();
  }

  void VerifyStatus(bool expected_to_be_waiting_for_response,
                    bool expected_to_be_canceled) {
    EXPECT_EQ(expected_to_be_waiting_for_response,
              client_connection_parameters_->IsClientWaitingForResponse());
    EXPECT_EQ(expected_to_be_canceled,
              fake_observer_->has_connection_request_been_canceled());
  }

  const FakeConnectionDelegate* fake_connection_delegate() {
    return fake_connection_delegate_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeConnectionDelegate> fake_connection_delegate_;

  std::unique_ptr<FakeSecureChannelStructuredMetricsLogger>
      fake_secure_channel_structured_metrics_logger_;

  std::unique_ptr<FakeClientConnectionParametersObserver> fake_observer_;

  std::unique_ptr<ClientConnectionParameters> client_connection_parameters_;
};

TEST_F(SecureChannelClientConnectionParametersImplTest,
       ConnectionDelegateDisconnected) {
  DisconnectConnectionDelegateRemote();
  VerifyStatus(false /* expected_to_be_waiting_for_response */,
               true /* expected_to_be_canceled */);
}

TEST_F(SecureChannelClientConnectionParametersImplTest, OnConnection) {
  auto fake_channel = std::make_unique<FakeChannel>();
  mojo::PendingRemote<mojom::MessageReceiver> message_receiver_remote;
  mojo::PendingRemote<mojom::NearbyConnectionStateListener>
      nearby_connection_state_listener_remote;

  CallOnConnection(
      fake_channel->GenerateRemote(),
      message_receiver_remote.InitWithNewPipeAndPassReceiver(),
      nearby_connection_state_listener_remote.InitWithNewPipeAndPassReceiver());
  VerifyStatus(false /* expected_to_be_waiting_for_response */,
               false /* expected_to_be_canceled */);

  EXPECT_TRUE(fake_connection_delegate()->channel());
  EXPECT_TRUE(fake_connection_delegate()->message_receiver_receiver());
  EXPECT_TRUE(
      fake_connection_delegate()->nearby_connection_state_listener_receiver());
}

TEST_F(SecureChannelClientConnectionParametersImplTest, OnConnectionFailed) {
  const mojom::ConnectionAttemptFailureReason kTestReason =
      mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR;

  CallOnConnectionAttemptFailure(kTestReason);
  VerifyStatus(false /* expected_to_be_waiting_for_response */,
               false /* expected_to_be_canceled */);

  EXPECT_EQ(kTestReason,
            *fake_connection_delegate()->connection_attempt_failure_reason());
}

}  // namespace ash::secure_channel
