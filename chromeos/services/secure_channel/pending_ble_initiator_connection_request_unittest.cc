// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_ble_initiator_connection_request.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_pending_connection_request_delegate.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {
const char kTestFeature[] = "testFeature";
}  // namespace

class SecureChannelPendingBleInitiatorConnectionRequestTest
    : public testing::Test {
 protected:
  SecureChannelPendingBleInitiatorConnectionRequestTest() = default;
  ~SecureChannelPendingBleInitiatorConnectionRequestTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_pending_connection_request_delegate_ =
        std::make_unique<FakePendingConnectionRequestDelegate>();
    auto fake_client_connection_parameters =
        std::make_unique<FakeClientConnectionParameters>(kTestFeature);
    fake_client_connection_parameters_ =
        fake_client_connection_parameters.get();
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    pending_ble_initiator_request_ =
        PendingBleInitiatorConnectionRequest::Factory::Get()->BuildInstance(
            std::move(fake_client_connection_parameters),
            ConnectionPriority::kLow,
            fake_pending_connection_request_delegate_.get(), mock_adapter_);
  }

  const base::Optional<
      PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            pending_ble_initiator_request_->GetRequestId());
  }

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  GetConnectionAttemptFailureReason() {
    return fake_client_connection_parameters_->failure_reason();
  }

  void HandleConnectionFailure(BleInitiatorFailureType failure_type) {
    pending_ble_initiator_request_->HandleConnectionFailure(failure_type);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakePendingConnectionRequestDelegate>
      fake_pending_connection_request_delegate_;
  FakeClientConnectionParameters* fake_client_connection_parameters_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
      pending_ble_initiator_request_;

  DISALLOW_COPY_AND_ASSIGN(
      SecureChannelPendingBleInitiatorConnectionRequestTest);
};

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       HandleAuthenticationError) {
  HandleConnectionFailure(BleInitiatorFailureType::kAuthenticationError);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       HandleCouldNotGenerateAdvertisement) {
  HandleConnectionFailure(
      BleInitiatorFailureType::kCouldNotGenerateAdvertisement);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(
      mojom::ConnectionAttemptFailureReason::COULD_NOT_GENERATE_ADVERTISEMENT,
      *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       HandleGattErrors) {
  // Fail 5 times; no fatal error should occur.
  for (size_t i = 0; i < 5; ++i) {
    HandleConnectionFailure(BleInitiatorFailureType::kGattConnectionError);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail a 6th time; this should be a fatal error.
  HandleConnectionFailure(BleInitiatorFailureType::kGattConnectionError);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::GATT_CONNECTION_ERROR,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest, HandleTimeouts) {
  // Fail 2 times; no fatal error should occur.
  for (size_t i = 0; i < 2; ++i) {
    HandleConnectionFailure(
        BleInitiatorFailureType::kTimeoutContactingRemoteDevice);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail a 3rd time; this should be a fatal error.
  HandleConnectionFailure(
      BleInitiatorFailureType::kTimeoutContactingRemoteDevice);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::TIMEOUT_FINDING_DEVICE,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleInitiatorConnectionRequestTest,
       NonFailingErrors) {
  // Fail 5 times due to GATT errors; no fatal error should occur.
  for (size_t i = 0; i < 5; ++i) {
    HandleConnectionFailure(BleInitiatorFailureType::kGattConnectionError);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail 2 times due to timeouts; no fatal error should occur.
  for (size_t i = 0; i < 2; ++i) {
    HandleConnectionFailure(
        BleInitiatorFailureType::kTimeoutContactingRemoteDevice);
    EXPECT_FALSE(GetFailedConnectionReason());
    EXPECT_FALSE(GetConnectionAttemptFailureReason());
  }

  // Fail due to being interrupted by a higher-priority attempt; no fatal error
  // should occur.
  HandleConnectionFailure(
      BleInitiatorFailureType::kInterruptedByHigherPriorityConnectionAttempt);
  EXPECT_FALSE(GetFailedConnectionReason());
  EXPECT_FALSE(GetConnectionAttemptFailureReason());
}

}  // namespace secure_channel

}  // namespace chromeos
