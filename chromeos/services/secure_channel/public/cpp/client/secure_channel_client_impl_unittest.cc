// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/services/secure_channel/fake_channel.h"
#include "chromeos/services/secure_channel/fake_secure_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_connection_attempt.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const size_t kNumTestDevices = 5u;

class FakeSecureChannelInitializerFactory
    : public SecureChannelInitializer::Factory {
 public:
  explicit FakeSecureChannelInitializerFactory(
      std::unique_ptr<FakeSecureChannel> fake_secure_channel)
      : fake_secure_channel_(std::move(fake_secure_channel)) {}

  ~FakeSecureChannelInitializerFactory() override = default;

  // SecureChannelInitializer::Factory:
  std::unique_ptr<SecureChannelBase> CreateInstance(
      scoped_refptr<base::TaskRunner> task_runner) override {
    EXPECT_TRUE(fake_secure_channel_);
    return std::move(fake_secure_channel_);
  }

 private:
  std::unique_ptr<FakeSecureChannel> fake_secure_channel_;
};

class FakeConnectionAttemptFactory : public ConnectionAttemptImpl::Factory {
 public:
  FakeConnectionAttemptFactory() = default;
  ~FakeConnectionAttemptFactory() override = default;

  // ConnectionAttemptImpl::Factory:
  std::unique_ptr<ConnectionAttemptImpl> CreateInstance() override {
    return std::make_unique<FakeConnectionAttempt>();
  }
};

class FakeClientChannelImplFactory : public ClientChannelImpl::Factory {
 public:
  FakeClientChannelImplFactory() = default;
  ~FakeClientChannelImplFactory() override = default;

  ClientChannel* last_client_channel_created() {
    return last_client_channel_created_;
  }

  // ClientChannelImpl::Factory:
  std::unique_ptr<ClientChannel> CreateInstance(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver)
      override {
    auto client_channel = std::make_unique<FakeClientChannel>();
    last_client_channel_created_ = client_channel.get();
    return client_channel;
  }

 private:
  ClientChannel* last_client_channel_created_;
};

class TestConnectionAttemptDelegate : public ConnectionAttempt::Delegate {
 public:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override {
    last_connection_attempt_failure_reason_ = reason;
  }

  void OnConnection(std::unique_ptr<ClientChannel> channel) override {
    client_channels_.push_back(std::move(channel));
  }

  base::Optional<mojom::ConnectionAttemptFailureReason>
  last_connection_attempt_failure_reason() {
    return last_connection_attempt_failure_reason_;
  }

  std::vector<std::unique_ptr<ClientChannel>>& client_channels() {
    return client_channels_;
  }

 private:
  base::Optional<mojom::ConnectionAttemptFailureReason>
      last_connection_attempt_failure_reason_;
  std::vector<std::unique_ptr<ClientChannel>> client_channels_;
};

}  // namespace

class SecureChannelClientImplTest : public testing::Test {
 protected:
  SecureChannelClientImplTest()
      : test_remote_device_list_(
            multidevice::CreateRemoteDeviceListForTest(kNumTestDevices)),
        test_remote_device_ref_list_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}

  // testing::Test:
  void SetUp() override {
    auto fake_secure_channel = std::make_unique<FakeSecureChannel>();
    fake_secure_channel_ = fake_secure_channel.get();
    fake_secure_channel_initializer_factory_ =
        std::make_unique<FakeSecureChannelInitializerFactory>(
            std::move(fake_secure_channel));
    SecureChannelInitializer::Factory::SetFactoryForTesting(
        fake_secure_channel_initializer_factory_.get());

    fake_connection_attempt_factory_ =
        std::make_unique<FakeConnectionAttemptFactory>();
    ConnectionAttemptImpl::Factory::SetFactoryForTesting(
        fake_connection_attempt_factory_.get());

    fake_client_channel_impl_factory_ =
        std::make_unique<FakeClientChannelImplFactory>();
    ClientChannelImpl::Factory::SetFactoryForTesting(
        fake_client_channel_impl_factory_.get());

    test_connection_attempt_delegate_ =
        std::make_unique<TestConnectionAttemptDelegate>();

    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    service_ = SecureChannelInitializer::Factory::Create(test_task_runner_);

    mojo::PendingRemote<mojom::SecureChannel> channel;
    service_->BindReceiver(channel.InitWithNewPipeAndPassReceiver());
    client_ = SecureChannelClientImpl::Factory::Create(std::move(channel),
                                                       test_task_runner_);
  }

  void TearDown() override {
    SecureChannelInitializer::Factory::SetFactoryForTesting(nullptr);
  }

  std::unique_ptr<FakeConnectionAttempt> CallListenForConnectionFromDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    auto connection_attempt = client_->ListenForConnectionFromDevice(
        device_to_connect, local_device, feature, connection_priority);
    auto fake_connection_attempt = base::WrapUnique(
        static_cast<FakeConnectionAttempt*>(connection_attempt.release()));
    fake_connection_attempt->SetDelegate(
        test_connection_attempt_delegate_.get());

    test_task_runner_->RunUntilIdle();

    SendPendingMojoMessages();

    return fake_connection_attempt;
  }

  std::unique_ptr<FakeConnectionAttempt> CallInitiateConnectionToDevice(
      multidevice::RemoteDeviceRef device_to_connect,
      multidevice::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) {
    auto connection_attempt = client_->InitiateConnectionToDevice(
        device_to_connect, local_device, feature, connection_priority);
    auto fake_connection_attempt = base::WrapUnique(
        static_cast<FakeConnectionAttempt*>(connection_attempt.release()));
    fake_connection_attempt->SetDelegate(
        test_connection_attempt_delegate_.get());

    test_task_runner_->RunUntilIdle();

    SendPendingMojoMessages();

    return fake_connection_attempt;
  }

  void SendPendingMojoMessages() {
    static_cast<SecureChannelClientImpl*>(client_.get())->FlushForTesting();
  }

  base::test::TaskEnvironment task_environment_;

  FakeSecureChannel* fake_secure_channel_;
  std::unique_ptr<FakeSecureChannelInitializerFactory>
      fake_secure_channel_initializer_factory_;
  std::unique_ptr<FakeConnectionAttemptFactory>
      fake_connection_attempt_factory_;
  std::unique_ptr<FakeClientChannelImplFactory>
      fake_client_channel_impl_factory_;
  std::unique_ptr<TestConnectionAttemptDelegate>
      test_connection_attempt_delegate_;
  std::unique_ptr<SecureChannelBase> service_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  std::unique_ptr<SecureChannelClient> client_;

  const multidevice::RemoteDeviceList test_remote_device_list_;
  const multidevice::RemoteDeviceRefList test_remote_device_ref_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelClientImplTest);
};

TEST_F(SecureChannelClientImplTest, TestInitiateConnectionToDevice) {
  auto fake_connection_attempt = CallInitiateConnectionToDevice(
      test_remote_device_ref_list_[1], test_remote_device_ref_list_[0],
      "feature", ConnectionPriority::kLow);

  base::RunLoop run_loop;

  fake_connection_attempt->set_on_connection_callback(run_loop.QuitClosure());

  auto fake_channel = std::make_unique<FakeChannel>();
  mojo::PendingRemote<mojom::MessageReceiver> message_receiver_remote;

  fake_secure_channel_->delegate_from_last_initiate_call()->OnConnection(
      fake_channel->GenerateRemote(),
      message_receiver_remote.InitWithNewPipeAndPassReceiver());

  run_loop.Run();

  EXPECT_EQ(fake_client_channel_impl_factory_->last_client_channel_created(),
            test_connection_attempt_delegate_->client_channels()[0].get());
}

TEST_F(SecureChannelClientImplTest, TestInitiateConnectionToDevice_Failure) {
  auto fake_connection_attempt = CallInitiateConnectionToDevice(
      test_remote_device_ref_list_[1], test_remote_device_ref_list_[0],
      "feature", ConnectionPriority::kLow);

  base::RunLoop run_loop;

  fake_connection_attempt->set_on_connection_attempt_failure_callback(
      run_loop.QuitClosure());

  fake_secure_channel_->delegate_from_last_initiate_call()
      ->OnConnectionAttemptFailure(
          mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR);

  run_loop.Run();

  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR,
            test_connection_attempt_delegate_
                ->last_connection_attempt_failure_reason());
}

TEST_F(SecureChannelClientImplTest, TestListenForConnectionFromDevice) {
  auto fake_connection_attempt = CallListenForConnectionFromDevice(
      test_remote_device_ref_list_[1], test_remote_device_ref_list_[0],
      "feature", ConnectionPriority::kLow);

  base::RunLoop run_loop;

  fake_connection_attempt->set_on_connection_callback(run_loop.QuitClosure());

  auto fake_channel = std::make_unique<FakeChannel>();
  mojo::PendingRemote<mojom::MessageReceiver> message_receiver_remote;

  fake_secure_channel_->delegate_from_last_listen_call()->OnConnection(
      fake_channel->GenerateRemote(),
      message_receiver_remote.InitWithNewPipeAndPassReceiver());

  run_loop.Run();

  EXPECT_EQ(fake_client_channel_impl_factory_->last_client_channel_created(),
            test_connection_attempt_delegate_->client_channels()[0].get());
}

TEST_F(SecureChannelClientImplTest, TestListenForConnectionFromDevice_Failure) {
  auto fake_connection_attempt = CallListenForConnectionFromDevice(
      test_remote_device_ref_list_[1], test_remote_device_ref_list_[0],
      "feature", ConnectionPriority::kLow);

  base::RunLoop run_loop;

  fake_connection_attempt->set_on_connection_attempt_failure_callback(
      run_loop.QuitClosure());

  fake_secure_channel_->delegate_from_last_listen_call()
      ->OnConnectionAttemptFailure(
          mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR);

  run_loop.Run();

  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR,
            test_connection_attempt_delegate_
                ->last_connection_attempt_failure_reason());
}

TEST_F(SecureChannelClientImplTest, TestMultipleConnections) {
  auto fake_connection_attempt_1 = CallInitiateConnectionToDevice(
      test_remote_device_ref_list_[1], test_remote_device_ref_list_[0],
      "feature", ConnectionPriority::kLow);
  base::RunLoop run_loop_1;
  fake_connection_attempt_1->set_on_connection_callback(
      run_loop_1.QuitClosure());
  auto fake_channel_1 = std::make_unique<FakeChannel>();
  mojo::PendingRemote<mojom::MessageReceiver> message_receiver_remote_1;
  fake_secure_channel_->delegate_from_last_initiate_call()->OnConnection(
      fake_channel_1->GenerateRemote(),
      message_receiver_remote_1.InitWithNewPipeAndPassReceiver());
  run_loop_1.Run();

  ClientChannel* client_channel_1 =
      test_connection_attempt_delegate_->client_channels()[0].get();
  EXPECT_EQ(fake_client_channel_impl_factory_->last_client_channel_created(),
            client_channel_1);

  auto fake_connection_attempt_2 = CallListenForConnectionFromDevice(
      test_remote_device_ref_list_[2], test_remote_device_ref_list_[0],
      "feature", ConnectionPriority::kLow);
  base::RunLoop run_loop_2;
  fake_connection_attempt_2->set_on_connection_callback(
      run_loop_2.QuitClosure());
  auto fake_channel_2 = std::make_unique<FakeChannel>();
  mojo::PendingRemote<mojom::MessageReceiver> message_receiver_remote_2;
  fake_secure_channel_->delegate_from_last_listen_call()->OnConnection(
      fake_channel_2->GenerateRemote(),
      message_receiver_remote_2.InitWithNewPipeAndPassReceiver());
  run_loop_2.Run();

  ClientChannel* client_channel_2 =
      test_connection_attempt_delegate_->client_channels()[1].get();
  EXPECT_EQ(fake_client_channel_impl_factory_->last_client_channel_created(),
            client_channel_2);

  EXPECT_NE(client_channel_1, client_channel_2);
}

}  // namespace secure_channel

}  // namespace chromeos
