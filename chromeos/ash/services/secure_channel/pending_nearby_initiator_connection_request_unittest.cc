// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/pending_nearby_initiator_connection_request.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/ash/services/secure_channel/fake_pending_connection_request_delegate.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const char kTestFeature[] = "testFeature";

}  // namespace

class SecureChannelPendingNearbyInitiatorConnectionRequestTest
    : public testing::Test {
 protected:
  SecureChannelPendingNearbyInitiatorConnectionRequestTest() = default;
  SecureChannelPendingNearbyInitiatorConnectionRequestTest(
      const SecureChannelPendingNearbyInitiatorConnectionRequestTest&) = delete;
  SecureChannelPendingNearbyInitiatorConnectionRequestTest& operator=(
      const SecureChannelPendingNearbyInitiatorConnectionRequestTest&) = delete;
  ~SecureChannelPendingNearbyInitiatorConnectionRequestTest() override =
      default;

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

    pending_nearby_initiator_request_ =
        PendingNearbyInitiatorConnectionRequest::Factory::Create(
            std::move(fake_client_connection_parameters),
            ConnectionPriority::kLow,
            fake_pending_connection_request_delegate_.get(), mock_adapter_);

    EXPECT_TRUE(mock_adapter_->GetObservers().HasObserver(GetRequest()));
  }

  const std::optional<PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            pending_nearby_initiator_request_->GetRequestId());
  }

  const std::optional<mojom::ConnectionAttemptFailureReason>&
  GetConnectionAttemptFailureReason() {
    return fake_client_connection_parameters_->failure_reason();
  }

  void HandleConnectionFailure(NearbyInitiatorFailureType failure_type) {
    pending_nearby_initiator_request_->HandleConnectionFailure(failure_type);
  }

  void SimulateAdapterPoweredChanged(bool powered) {
    GetRequest()->AdapterPoweredChanged(mock_adapter_.get(), powered);
  }

  void SimulateAdapterPresentChanged(bool present) {
    GetRequest()->AdapterPresentChanged(mock_adapter_.get(), present);
  }

 private:
  PendingNearbyInitiatorConnectionRequest* GetRequest() {
    return static_cast<PendingNearbyInitiatorConnectionRequest*>(
        pending_nearby_initiator_request_.get());
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<FakePendingConnectionRequestDelegate>
      fake_pending_connection_request_delegate_;
  raw_ptr<FakeClientConnectionParameters, DanglingUntriaged>
      fake_client_connection_parameters_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<PendingConnectionRequest<NearbyInitiatorFailureType>>
      pending_nearby_initiator_request_;
};

TEST_F(SecureChannelPendingNearbyInitiatorConnectionRequestTest,
       HandleAdapterPoweredChanged) {
  // Turning the adapter on should do nothing.
  SimulateAdapterPoweredChanged(/*powered=*/true);
  EXPECT_FALSE(GetFailedConnectionReason());
  EXPECT_FALSE(GetConnectionAttemptFailureReason());

  // Turning the adapter off should trigger a failure.
  SimulateAdapterPoweredChanged(/*powered=*/false);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingNearbyInitiatorConnectionRequestTest,
       HandleAdapterPresentChanged) {
  // The adapter appearing should do nothing.
  SimulateAdapterPresentChanged(/*present=*/true);
  EXPECT_FALSE(GetFailedConnectionReason());
  EXPECT_FALSE(GetConnectionAttemptFailureReason());

  // The adapter disappearing should trigger a failure.
  SimulateAdapterPresentChanged(/*present=*/false);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingNearbyInitiatorConnectionRequestTest,
       HandleAuthenticationError) {
  HandleConnectionFailure(NearbyInitiatorFailureType::kAuthenticationError);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::AUTHENTICATION_ERROR,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingNearbyInitiatorConnectionRequestTest,
       HandleConnectivityError) {
  HandleConnectionFailure(NearbyInitiatorFailureType::kConnectivityError);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::NEARBY_CONNECTION_ERROR,
            *GetConnectionAttemptFailureReason());
}

}  // namespace ash::secure_channel
