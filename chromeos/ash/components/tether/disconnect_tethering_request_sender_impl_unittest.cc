// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_request_sender_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/disconnect_tethering_operation.h"
#include "chromeos/ash/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

class FakeDisconnectTetheringOperation : public DisconnectTetheringOperation {
 public:
  FakeDisconnectTetheringOperation(
      const TetherHost& tether_host,
      raw_ptr<HostConnection::Factory> host_connection_factory)
      : DisconnectTetheringOperation(tether_host, host_connection_factory),
        tether_host_(tether_host) {}

  ~FakeDisconnectTetheringOperation() override = default;

  void NotifyFinished(bool success) {
    NotifyObserversOperationFinished(success);
  }

  std::string GetDeviceId() { return tether_host_.GetDeviceId(); }

 private:
  TetherHost tether_host_;
};

class FakeDisconnectTetheringOperationFactory
    : public DisconnectTetheringOperation::Factory {
 public:
  FakeDisconnectTetheringOperationFactory() = default;
  ~FakeDisconnectTetheringOperationFactory() override = default;

  std::vector<raw_ptr<FakeDisconnectTetheringOperation, VectorExperimental>>&
  created_operations() {
    return created_operations_;
  }

 protected:
  // DisconnectTetheringOperation::Factory:
  std::unique_ptr<DisconnectTetheringOperation> CreateInstance(
      const TetherHost& tether_host,
      raw_ptr<HostConnection::Factory> host_connection_factory) override {
    FakeDisconnectTetheringOperation* operation =
        new FakeDisconnectTetheringOperation(tether_host,
                                             host_connection_factory);
    created_operations_.push_back(operation);
    return base::WrapUnique(operation);
  }

 private:
  std::vector<raw_ptr<FakeDisconnectTetheringOperation, VectorExperimental>>
      created_operations_;
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
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(2u)) {}

  DisconnectTetheringRequestSenderTest(
      const DisconnectTetheringRequestSenderTest&) = delete;
  DisconnectTetheringRequestSenderTest& operator=(
      const DisconnectTetheringRequestSenderTest&) = delete;

  ~DisconnectTetheringRequestSenderTest() override = default;

  void SetUp() override {
    fake_tether_host_fetcher_ =
        std::make_unique<FakeTetherHostFetcher>(test_devices_[0]);
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();

    fake_operation_factory_ =
        std::make_unique<FakeDisconnectTetheringOperationFactory>();
    DisconnectTetheringOperation::Factory::SetFactoryForTesting(
        fake_operation_factory_.get());

    disconnect_tethering_request_sender_ =
        DisconnectTetheringRequestSenderImpl::Factory::Create(
            fake_host_connection_factory_.get(),
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
    EXPECT_EQ(test_devices_[0].GetDeviceId(),
              fake_operation_factory_->created_operations()[0]->GetDeviceId());
    EXPECT_EQ(test_devices_[1].GetDeviceId(),
              fake_operation_factory_->created_operations()[1]->GetDeviceId());
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

    // When multiple concurrent attempts are made to send a request to the
    // same device, only one DisconnectTetheringOperation is created.
    ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
    EXPECT_EQ(test_devices_[0].GetDeviceId(),
              fake_operation_factory_->created_operations()[0]->GetDeviceId());
    fake_operation_factory_->created_operations()[0]->NotifyFinished(
        operation_successful);
    EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
    EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                      ->num_no_more_pending_requests_events());
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;

  std::unique_ptr<FakeDisconnectTetheringOperationFactory>
      fake_operation_factory_;

  std::unique_ptr<DisconnectTetheringRequestSender>
      disconnect_tethering_request_sender_;
  std::unique_ptr<FakeDisconnectTetheringRequestSenderObserver>
      fake_disconnect_tethering_request_sender_observer_;
};

TEST_F(DisconnectTetheringRequestSenderTest, DISABLED_SendRequest_Success) {
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());

  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_operation_factory_->created_operations()[0]->GetDeviceId());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      true /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
}

TEST_F(DisconnectTetheringRequestSenderTest,
       DISABLED_SendRequest_CannotFetchHost) {
  // Remove hosts from |fake_tether_host_fetcher_|; this will cause the
  // fetcher to return a null RemoteDevice.
  fake_tether_host_fetcher_->SetTetherHost(std::nullopt);

  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());

  EXPECT_TRUE(fake_operation_factory_->created_operations().empty());
  EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    DISABLED_MultipleRequestAttempts_Concurrent_DifferentDeviceId_BothOperationsSuccessful) {
  SendConcurrentRequestsToTwoDevices(true /* first_operation_successful */,
                                     true /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    DISABLED_MultipleRequestAttempts_Concurrent_DifferentDeviceId_BothOperationsFailed) {
  SendConcurrentRequestsToTwoDevices(false /* first_operation_successful */,
                                     false /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    DISABLED_MultipleRequestAttempts_Concurrent_DifferentDeviceId_FirstOperationSuccessful) {
  SendConcurrentRequestsToTwoDevices(true /* first_operation_successful */,
                                     false /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    DISABLED_MultipleRequestAttempts_Concurrent_DifferentDeviceId_SecondOperationSuccessful) {
  SendConcurrentRequestsToTwoDevices(false /* first_operation_successful */,
                                     true /* second_operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    DISABLED_MultipleRequestAttempts_Concurrent_SameDeviceId_OperationSuccessful) {
  CallSendRequestTwiceWithOneDevice(true /* operation_successful */);
}

TEST_F(
    DisconnectTetheringRequestSenderTest,
    DISABLED_MultipleRequestAttempts_Concurrent_SameDeviceId_OperationFailed) {
  CallSendRequestTwiceWithOneDevice(false /* operation_successful */);
}

TEST_F(DisconnectTetheringRequestSenderTest,
       DISABLED_SendMultipleRequests_NotifyFinished) {
  // When multiple requests are sent, a new DisconnectTetheringOperation will
  // be created if the previous one has finished. This is true regardless of
  // the success of the previous operation.
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  EXPECT_TRUE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(0u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_operation_factory_->created_operations()[0]->GetDeviceId());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      true /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(1u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());

  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  ASSERT_EQ(2u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_operation_factory_->created_operations()[1]->GetDeviceId());
  fake_operation_factory_->created_operations()[1]->NotifyFinished(
      false /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(2u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());

  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      test_devices_[0].GetDeviceId());
  ASSERT_EQ(3u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_operation_factory_->created_operations()[2]->GetDeviceId());
  fake_operation_factory_->created_operations()[2]->NotifyFinished(
      true /* success */);
  EXPECT_FALSE(disconnect_tethering_request_sender_->HasPendingRequests());
  EXPECT_EQ(3u, fake_disconnect_tethering_request_sender_observer_
                    ->num_no_more_pending_requests_events());
}

}  // namespace tether

}  // namespace ash
