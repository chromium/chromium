// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/disconnect_tethering_request_sender_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/disconnect_tethering_operation.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

class FakeDisconnectTetheringOperation : public DisconnectTetheringOperation {
 public:
  FakeDisconnectTetheringOperation(
      cryptauth::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      BleConnectionManager* connection_manager)
      : DisconnectTetheringOperation(device_to_connect,
                                     device_sync_client,
                                     secure_channel_client,
                                     connection_manager) {}

  ~FakeDisconnectTetheringOperation() override = default;

  void NotifyFinished(bool success) {
    NotifyObserversOperationFinished(success);
  }

  cryptauth::RemoteDeviceRef GetRemoteDevice() {
    EXPECT_EQ(1u, remote_devices().size());
    return remote_devices()[0];
  }
};

class FakeDisconnectTetheringOperationFactory
    : public DisconnectTetheringOperation::Factory {
 public:
  FakeDisconnectTetheringOperationFactory() = default;
  virtual ~FakeDisconnectTetheringOperationFactory() = default;

  std::vector<FakeDisconnectTetheringOperation*>& created_operations() {
    return created_operations_;
  }

 protected:
  // DisconnectTetheringOperation::Factory:
  std::unique_ptr<DisconnectTetheringOperation> BuildInstance(
      cryptauth::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      BleConnectionManager* connection_manager) override {
    FakeDisconnectTetheringOperation* operation =
        new FakeDisconnectTetheringOperation(
            device_to_connect, device_sync_client, secure_channel_client,
            connection_manager);
    created_operations_.push_back(operation);
    return base::WrapUnique(operation);
  }

 private:
  std::vector<FakeDisconnectTetheringOperation*> created_operations_;
};

class FakeDisconnectTetheringRequestSenderObserver
    : public DisconnectTetheringRequestSender::Observer {
 public:
  FakeDisconnectTetheringRequestSenderObserver()
      : num_no_more_pending_requests_events_(0) {}

  ~FakeDisconnectTetheringRequestSenderObserver() override = default;

  void OnPendingDisconnectRequestsComplete() override {
    num_no_more_pending_requests_events_++;
  }

  uint32_t num_no_more_pending_requests_events() {
    return num_no_more_pending_requests_events_;
  }

 private:
  uint32_t num_no_more_pending_requests_events_;
};

}  // namespace

class DisconnectTetheringRequestSenderTest : public testing::Test {
 public:
  DisconnectTetheringRequestSenderTest()
      : test_devices_(cryptauth::CreateRemoteDeviceRefListForTest(2u)) {}
  ~DisconnectTetheringRequestSenderTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(features::kMultiDeviceApi);

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_ble_connection_manager_ = std::make_unique<FakeBleConnectionManager>();
    fake_tether_host_fetcher_ =
        std::make_unique<FakeTetherHostFetcher>(test_devices_);

    fake_operation_factory_ =
        std::make_unique<FakeDisconnectTetheringOperationFactory>();
    DisconnectTetheringOperation::Factory::SetInstanceForTesting(
        fake_operation_factory_.get());

    disconnect_tethering_request_sender_ =
        DisconnectTetheringRequestSenderImpl::Factory::NewInstance(
            fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
            fake_ble_connection_manager_.get(),
            fake_tether_host_fetcher_.get());

    fake_disconnect_tethering_request_sender_observer_ =
        std::make_unique<FakeDisconnectTetheringRequestSenderObserver>();
    disconnect_tethering_request_sender_->AddObserver(
        fake_disconnect_tethering_request_sender_observer_.get());
  }

  void TearDown() override {
    disconnect_tethering_request_sender_->RemoveObserver(
        fake_disconnect_tethering_request_sender_observer_.get());
  }

  void SendConcurrentRequestsToTwoDevices(bool first_operation_successful,
                                          bool second_operation_successful) {
    // Send requests to two devices concurrently.
    disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
        test_devices_[0].GetDeviceId());
    disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
        test_devices_[1].GetDeviceId());
    EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());
    EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                      ->num_no_more_pending_requests_events());

    ASSERT_EQ(2u, fake_operation_factory_->created_operations().size());
    EXPECT_EQ(
        test_devices_[0],
        fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
    EXPECT_EQ(
        test_devices_[1],
        fake_operation_factory_->created_operations()[1]->GetRemoteDevice());
    fake_operation_factory_->created_operations()[0]->NotifyFinished(
        first_operation_successful);
    EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());
    EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                      ->num_no_more_pending_requests_events());

    fake_operation_factory_->created_operations()[1]->NotifyFinished(
        second_operation_successful);
    EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
    EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                      ->num_no_more_pending_requests_events());
  }

  void CallSendRequestTwiceWithOneDevice(bool operation_successful) {
    disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
        test_devices_[0].GetDeviceId());
    disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
        test_devices_[0].GetDeviceId());
    EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());
    EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                      ->num_no_more_pending_requests_events());

    // When multiple concurrent attempts are made to send a request to the same
    // device, only one DisconnectTetheringOperation is created.
    ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
    EXPECT_EQ(
        test_devices_[0],
        fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
    fake_operation_factory_->created_operations()[0]->NotifyFinished(
        operation_successful);
    EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
    EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                      ->num_no_more_pending_requests_events());
  }

  const cryptauth::RemoteDeviceRefList test_devices_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::SecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;

  std::unique_ptr<FakeDisconnectTetheringOperationFactory>
      fake_operation_factory_;

  std::unique_ptr<DisconnectTetheringRequestSender>
      disconnect_tethering_request_sender_;
  std::unique_ptr<FakeDisconnectTetheringRequestSenderObserver>
      fake_disconnect_tethering_request_sender_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectTetheringRequestSenderTest);
};

TEST_F(DisconnectTetheringRequestSenderTest, SendRequest_Success) {
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());

  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      true /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
}

TEST_F(DisconnectTetheringRequestSenderTest, SendRequest_CannotFetchHost) {
  // Remove hosts from |fake_tether_host_fetcher_|; this will cause the fetcher
  // to return a null RemoteDevice.
  fake_tether_host_fetcher_->set_tether_hosts(cryptauth::RemoteDeviceRefList());

  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());

  EXPECT_TRUE(fake_operation_factory_->created_operations().empty());
  EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    MultipleRequestAttempts_Concurrent_DifferentDeviceId_BothOperationsSuccessful) {
  SendConcurrentRequestsToTwoDevices(true /* first_operation_successful */,
                                     true /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    MultipleRequestAttempts_Concurrent_DifferentDeviceId_BothOperationsFailed) {
  SendConcurrentRequestsToTwoDevices(false /* first_operation_successful */,
                                     false /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    MultipleRequestAttempts_Concurrent_DifferentDeviceId_FirstOperationSuccessful) {
  SendConcurrentRequestsToTwoDevices(true /* first_operation_successful */,
                                     false /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    MultipleRequestAttempts_Concurrent_DifferentDeviceId_SecondOperationSuccessful) {
  SendConcurrentRequestsToTwoDevices(false /* first_operation_successful */,
                                     true /* second_operation_successful */);
}

TEST_F(DisconnectTetheringRequestSenderTest,
       MultipleRequestAttempts_Concurrent_SameDeviceId_OperationSuccessful) {
  CallSendRequestTwiceWithOneDevice(true /* operation_successful */);
}

TEST_F(DisconnectTetheringRequestSenderTest,
       MultipleRequestAttempts_Concurrent_SameDeviceId_OperationFailed) {
  CallSendRequestTwiceWithOneDevice(false /* operation_successful */);
}

TEST_F(DisconnectTetheringRequestSenderTest,
       SendMultipleRequests_NotifyFinished) {
  // When multiple requests are sent, a new DisconnectTetheringOperation will be
  // created if the previous one has finished. This is true regardless of the
  // success of the previous operation.
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      true /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());

  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  ASSERT_EQ(2u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[1]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[1]->NotifyFinished(
      false /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(2u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());

  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  ASSERT_EQ(3u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[2]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[2]->NotifyFinished(
      true /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(3u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
}

}  // namespace tether

}  // namespace chromeos
