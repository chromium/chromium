// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_ble_connection_request_base.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/secure_channel/fake_client_connection_parameters.h"
#include "chromeos/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/services/secure_channel/fake_pending_connection_request_delegate.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kTestReadableRequestTypeForLogging[] = "Test Request Type";
const char kTestFeature[] = "testFeature";
enum class TestFailureDetail { kTestFailureReason };

// Since PendingBleConnectionRequestBase is templatized, a concrete
// implementation is needed for its test.
class TestPendingBleConnectionRequestBase
    : public PendingBleConnectionRequestBase<TestFailureDetail> {
 public:
  TestPendingBleConnectionRequestBase(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
      : PendingBleConnectionRequestBase<TestFailureDetail>(
            std::move(client_connection_parameters),
            connection_priority,
            kTestReadableRequestTypeForLogging,
            delegate,
            std::move(bluetooth_adapter)) {}
  ~TestPendingBleConnectionRequestBase() override = default;

  // PendingConnectionRequest<TestFailureDetailType>:
  void HandleConnectionFailure(TestFailureDetail failure_detail) override {}
};

}  // namespace

class SecureChannelPendingBleConnectionRequestBaseTest : public testing::Test {
 protected:
  SecureChannelPendingBleConnectionRequestBaseTest() = default;
  ~SecureChannelPendingBleConnectionRequestBaseTest() override = default;

  void SetUp() override {
    auto fake_client_connection_parameters =
        std::make_unique<FakeClientConnectionParameters>(kTestFeature);
    fake_client_connection_parameters_ =
        fake_client_connection_parameters.get();

    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    fake_pending_connection_request_delegate_ =
        std::make_unique<FakePendingConnectionRequestDelegate>();

    test_pending_ble_connection_request_ =
        std::make_unique<TestPendingBleConnectionRequestBase>(
            std::move(fake_client_connection_parameters),
            ConnectionPriority::kLow,
            fake_pending_connection_request_delegate_.get(), mock_adapter_);

    EXPECT_TRUE(mock_adapter_->GetObservers().HasObserver(
        test_pending_ble_connection_request_.get()));
  }

  const base::Optional<
      PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            test_pending_ble_connection_request_->GetRequestId());
  }

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  GetConnectionAttemptFailureReason() const {
    return fake_client_connection_parameters_->failure_reason();
  }

  void SimulateAdapterPoweredChanged(bool powered) {
    test_pending_ble_connection_request_->AdapterPoweredChanged(
        mock_adapter_.get(), powered);
  }

  void SimulateAdapterPresentChanged(bool present) {
    test_pending_ble_connection_request_->AdapterPresentChanged(
        mock_adapter_.get(), present);
  }

 private:
  FakeClientConnectionParameters* fake_client_connection_parameters_;
  std::unique_ptr<FakePendingConnectionRequestDelegate>
      fake_pending_connection_request_delegate_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  std::unique_ptr<TestPendingBleConnectionRequestBase>
      test_pending_ble_connection_request_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelPendingBleConnectionRequestBaseTest);
};

TEST_F(SecureChannelPendingBleConnectionRequestBaseTest,
       HandleAdapterPoweredChanged) {
  // Turning the adapter on should do nothing.
  SimulateAdapterPoweredChanged(true /* powered */);
  EXPECT_FALSE(GetFailedConnectionReason());
  EXPECT_FALSE(GetConnectionAttemptFailureReason());

  // Turning the adapter off should trigger a failure.
  SimulateAdapterPoweredChanged(false /* powered */);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED,
            *GetConnectionAttemptFailureReason());
}

TEST_F(SecureChannelPendingBleConnectionRequestBaseTest,
       HandleAdapterPresentChanged) {
  // The adapter appearing should do nothing.
  SimulateAdapterPresentChanged(true /* present */);
  EXPECT_FALSE(GetFailedConnectionReason());
  EXPECT_FALSE(GetConnectionAttemptFailureReason());

  // The adapter disappearing should trigger a failure.
  SimulateAdapterPresentChanged(false /* present */);
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
  EXPECT_EQ(mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT,
            *GetConnectionAttemptFailureReason());
}

}  // namespace secure_channel

}  // namespace chromeos
