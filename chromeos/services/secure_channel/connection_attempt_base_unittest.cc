// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/connection_attempt_base.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/connection_attempt_details.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/connection_medium.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/fake_authenticated_channel.h"
#include "chromeos/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_connect_to_device_operation.h"
#include "chromeos/services/secure_channel/fake_connection_attempt_delegate.h"
#include "chromeos/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/services/secure_channel/fake_pending_connection_request.h"
#include "chromeos/services/secure_channel/pending_connection_request_delegate.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kTestRemoteDeviceId[] = "testRemoteDeviceId";
const char kTestLocalDeviceId[] = "testLocalDeviceId";

// Since ConnectionAttemptBase is templatized, a concrete implementation
// is needed for its test.
class TestConnectionAttempt
    : public ConnectionAttemptBase<BleInitiatorFailureType> {
 public:
  explicit TestConnectionAttempt(FakeConnectionAttemptDelegate* delegate)
      : ConnectionAttemptBase<BleInitiatorFailureType>(
            delegate,
            ConnectionAttemptDetails(kTestRemoteDeviceId,
                                     kTestLocalDeviceId,
                                     ConnectionMedium::kBluetoothLowEnergy,
                                     ConnectionRole::kListenerRole)) {}

  ~TestConnectionAttempt() override = default;

  FakeConnectToDeviceOperation<BleInitiatorFailureType>* fake_operation() {
    return fake_operation_;
  }

 private:
  // ConnectionAttemptBase<BleInitiatorFailureType>:
  std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
  CreateConnectToDeviceOperation(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectToDeviceOperation<
          BleInitiatorFailureType>::ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<BleInitiatorFailureType>::
          ConnectionFailedCallback& failure_callback) override {
    EXPECT_FALSE(fake_operation_);

    auto fake_operation =
        std::make_unique<FakeConnectToDeviceOperation<BleInitiatorFailureType>>(
            std::move(success_callback), failure_callback, connection_priority);
    fake_operation_ = fake_operation.get();

    return std::move(fake_operation);
  }

  FakeConnectToDeviceOperation<BleInitiatorFailureType>* fake_operation_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestConnectionAttempt);
};

}  // namespace

class SecureChannelConnectionAttemptBaseTest : public testing::Test {
 protected:
  SecureChannelConnectionAttemptBaseTest() = default;
  ~SecureChannelConnectionAttemptBaseTest() override = default;

  void SetUp() override {
    fake_authenticated_channel_ = std::make_unique<FakeAuthenticatedChannel>();

    fake_delegate_ = std::make_unique<FakeConnectionAttemptDelegate>();

    connection_attempt_ =
        std::make_unique<TestConnectionAttempt>(fake_delegate_.get());
  }

  void TearDown() override {
    // ExtractClientConnectionParameters() tests destroy |connection_attempt_|,
    // so no additional verifications should be performed.
    if (is_extract_client_data_test_)
      return;

    // If the operation did not complete successfully, the operation should be
    // canceled.
    bool should_operation_be_canceled_in_destructor =
        fake_delegate_->authenticated_channel() == nullptr;

    if (should_operation_be_canceled_in_destructor) {
      EXPECT_FALSE(fake_operation()->canceled());
      EXPECT_FALSE(was_operation_canceled_in_tear_down_);
      fake_operation()->set_cancel_callback(
          base::BindOnce(&SecureChannelConnectionAttemptBaseTest::
                             OnOperationCanceledInTeardown,
                         base::Unretained(this)));
    }

    connection_attempt_.reset();

    if (should_operation_be_canceled_in_destructor)
      EXPECT_TRUE(was_operation_canceled_in_tear_down_);
  }

  FakePendingConnectionRequest<BleInitiatorFailureType>* AddNewRequest(
      ConnectionPriority connection_priority) {
    auto request =
        std::make_unique<FakePendingConnectionRequest<BleInitiatorFailureType>>(
            connection_attempt_.get(), connection_priority);
    FakePendingConnectionRequest<BleInitiatorFailureType>* request_raw =
        request.get();
    active_requests_.insert(request_raw);

    ConnectionAttempt<BleInitiatorFailureType>* connection_attempt =
        connection_attempt_.get();
    connection_attempt->AddPendingConnectionRequest(std::move(request));

    return request_raw;
  }

  void FinishRequestWithoutConnection(
      FakePendingConnectionRequest<BleInitiatorFailureType>* request,
      PendingConnectionRequestDelegate::FailedConnectionReason reason) {
    request->NotifyRequestFinishedWithoutConnection(reason);
    EXPECT_EQ(1u, active_requests_.erase(request));
  }

  void FailOperation() {
    // Before failing the operation, check to see how many failure details each
    // request has been passed.
    std::unordered_map<base::UnguessableToken, size_t,
                       base::UnguessableTokenHash>
        id_to_num_details_map;
    for (const auto* request : active_requests_) {
      id_to_num_details_map[request->GetRequestId()] =
          request->handled_failure_details().size();
    }

    fake_operation()->OnFailedConnectionAttempt(
        BleInitiatorFailureType::kAuthenticationError);

    // Now, ensure that each active request had one additional failure detail
    // added, and verify that it was kAuthenticationError.
    for (const auto* request : active_requests_) {
      EXPECT_EQ(id_to_num_details_map[request->GetRequestId()] + 1,
                request->handled_failure_details().size());
      EXPECT_EQ(BleInitiatorFailureType::kAuthenticationError,
                request->handled_failure_details().back());
    }
  }

  void FinishOperationSuccessfully() {
    EXPECT_TRUE(fake_authenticated_channel_);
    auto* fake_authenticated_channel_raw = fake_authenticated_channel_.get();

    fake_operation()->OnSuccessfulConnectionAttempt(
        std::move(fake_authenticated_channel_));

    // |fake_delegate_|'s delegate should have received the
    // AuthenticatedChannel.
    EXPECT_TRUE(connection_attempt_->connection_attempt_details()
                    .CorrespondsToConnectionDetails(
                        *fake_delegate_->connection_details()));
    EXPECT_EQ(fake_authenticated_channel_raw,
              fake_delegate_->authenticated_channel());
  }

  void VerifyDelegateNotNotified() {
    EXPECT_FALSE(fake_delegate_->connection_details());
    EXPECT_FALSE(fake_delegate_->connection_attempt_details());
  }

  void VerifyDelegateNotifiedOfFailure() {
    // |fake_delegate_| should have received the failing attempt's ID but no
    // AuthenticatedChannel.
    EXPECT_EQ(connection_attempt_->connection_attempt_details(),
              fake_delegate_->connection_attempt_details());
    EXPECT_FALSE(fake_delegate_->connection_details());
    EXPECT_FALSE(fake_delegate_->authenticated_channel());
  }

  std::vector<std::unique_ptr<ClientConnectionParameters>>
  ExtractClientConnectionParameters() {
    is_extract_client_data_test_ = true;
    return ConnectionAttempt<BleInitiatorFailureType>::
        ExtractClientConnectionParameters(std::move(connection_attempt_));
  }

  FakeConnectToDeviceOperation<BleInitiatorFailureType>* fake_operation() {
    return connection_attempt_->fake_operation();
  }

 private:
  void OnOperationCanceledInTeardown() {
    was_operation_canceled_in_tear_down_ = true;
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakeConnectionAttemptDelegate> fake_delegate_;

  std::unique_ptr<FakeAuthenticatedChannel> fake_authenticated_channel_;
  std::set<FakePendingConnectionRequest<BleInitiatorFailureType>*>
      active_requests_;
  bool was_operation_canceled_in_tear_down_ = false;

  bool is_extract_client_data_test_ = false;

  std::unique_ptr<TestConnectionAttempt> connection_attempt_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelConnectionAttemptBaseTest);
};

TEST_F(SecureChannelConnectionAttemptBaseTest, SingleRequest_Success) {
  AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());
  FinishOperationSuccessfully();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, SingleRequest_Fails) {
  FakePendingConnectionRequest<BleInitiatorFailureType>* request =
      AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Fail the operation; the delegate should not have been notified since no
  // request has yet indicated failure.
  FailOperation();
  VerifyDelegateNotNotified();

  FinishRequestWithoutConnection(
      request,
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed);
  VerifyDelegateNotifiedOfFailure();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, SingleRequest_Canceled) {
  // Simulate the request being canceled.
  FakePendingConnectionRequest<BleInitiatorFailureType>* request =
      AddNewRequest(ConnectionPriority::kLow);
  FinishRequestWithoutConnection(
      request, PendingConnectionRequestDelegate::FailedConnectionReason::
                   kRequestCanceledByClient);
  VerifyDelegateNotifiedOfFailure();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, SingleRequest_FailThenSuccess) {
  AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Fail the operation; the delegate should not have been notified since no
  // request has yet indicated failure.
  FailOperation();
  VerifyDelegateNotNotified();

  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());
  FinishOperationSuccessfully();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, TwoRequests_Success) {
  AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Add a second request; the first operation should still be active.
  AddNewRequest(ConnectionPriority::kLow);

  FinishOperationSuccessfully();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, TwoRequests_Fails) {
  FakePendingConnectionRequest<BleInitiatorFailureType>* request1 =
      AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Add a second request.
  FakePendingConnectionRequest<BleInitiatorFailureType>* request2 =
      AddNewRequest(ConnectionPriority::kLow);

  // Fail the operation; the delegate should not have been notified since no
  // request has yet indicated failure.
  FailOperation();
  VerifyDelegateNotNotified();

  // Finish the first request; since a second request remains, the delegate
  // should not have been notified.
  FinishRequestWithoutConnection(
      request1,
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed);
  VerifyDelegateNotNotified();

  // Finish the second request, which should cause the delegate to be notified.
  FinishRequestWithoutConnection(
      request2,
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed);
  VerifyDelegateNotifiedOfFailure();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, TwoRequests_Canceled) {
  FakePendingConnectionRequest<BleInitiatorFailureType>* request1 =
      AddNewRequest(ConnectionPriority::kLow);
  FakePendingConnectionRequest<BleInitiatorFailureType>* request2 =
      AddNewRequest(ConnectionPriority::kLow);

  FinishRequestWithoutConnection(
      request1, PendingConnectionRequestDelegate::FailedConnectionReason::
                    kRequestCanceledByClient);
  VerifyDelegateNotNotified();

  FinishRequestWithoutConnection(
      request2, PendingConnectionRequestDelegate::FailedConnectionReason::
                    kRequestCanceledByClient);
  VerifyDelegateNotifiedOfFailure();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, TwoRequests_FailThenSuccess) {
  FakePendingConnectionRequest<BleInitiatorFailureType>* request1 =
      AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Fail the operation.
  FailOperation();
  VerifyDelegateNotNotified();
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Add a second request.
  AddNewRequest(ConnectionPriority::kLow);

  FailOperation();
  VerifyDelegateNotNotified();

  // Simulate the first request finishing due to failures; since a second
  // request remains, the delegate should not have been notified.
  FinishRequestWithoutConnection(
      request1,
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed);
  VerifyDelegateNotNotified();

  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());
  FinishOperationSuccessfully();
}

TEST_F(SecureChannelConnectionAttemptBaseTest, ManyRequests_UpdatePriority) {
  AddNewRequest(ConnectionPriority::kLow);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  // Add a medium-priority request. This should update the operation's priority
  // as well.
  FakePendingConnectionRequest<BleInitiatorFailureType>* request2 =
      AddNewRequest(ConnectionPriority::kMedium);
  EXPECT_EQ(ConnectionPriority::kMedium,
            fake_operation()->connection_priority());

  // Add a high-priority request and verify that the operation is updated.
  FakePendingConnectionRequest<BleInitiatorFailureType>* request3 =
      AddNewRequest(ConnectionPriority::kHigh);
  EXPECT_EQ(ConnectionPriority::kHigh, fake_operation()->connection_priority());

  // Remote the high-priority request; the operation should go back to medium.
  FinishRequestWithoutConnection(
      request3, PendingConnectionRequestDelegate::FailedConnectionReason::
                    kRequestCanceledByClient);
  EXPECT_EQ(ConnectionPriority::kMedium,
            fake_operation()->connection_priority());

  // Remote the medium-priority request; the operation should go back to low.
  FinishRequestWithoutConnection(
      request2, PendingConnectionRequestDelegate::FailedConnectionReason::
                    kRequestCanceledByClient);
  EXPECT_EQ(ConnectionPriority::kLow, fake_operation()->connection_priority());

  FinishOperationSuccessfully();
}

TEST_F(SecureChannelConnectionAttemptBaseTest,
       ExtractClientConnectionParameters) {
  FakePendingConnectionRequest<BleInitiatorFailureType>* request1 =
      AddNewRequest(ConnectionPriority::kLow);
  auto fake_parameters_1 =
      std::make_unique<FakeClientConnectionParameters>("request1Feature");
  auto* fake_parameters_1_raw = fake_parameters_1.get();
  request1->set_client_data_for_extraction(std::move(fake_parameters_1));

  FakePendingConnectionRequest<BleInitiatorFailureType>* request2 =
      AddNewRequest(ConnectionPriority::kLow);
  auto fake_parameters_2 =
      std::make_unique<FakeClientConnectionParameters>("request2Feature");
  auto* fake_parameters_2_raw = fake_parameters_2.get();
  request2->set_client_data_for_extraction(std::move(fake_parameters_2));

  auto extracted_client_data = ExtractClientConnectionParameters();
  EXPECT_EQ(2u, extracted_client_data.size());

  // The extracted client data may not be returned in the same order that as the
  // associated requests were added to |conenction_attempt_|, since
  // ConnectionAttemptBase internally utilizes a std::unordered_map. Sort the
  // data before making verifications to ensure correctness.
  std::sort(extracted_client_data.begin(), extracted_client_data.end(),
            [](const auto& ptr_1, const auto& ptr_2) {
              return ptr_1->feature() < ptr_2->feature();
            });

  EXPECT_EQ(fake_parameters_1_raw, extracted_client_data[0].get());
  EXPECT_EQ(fake_parameters_2_raw, extracted_client_data[1].get());
}

}  // namespace secure_channel

}  // namespace chromeos
