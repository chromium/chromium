// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/keep_alive_operation.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NotNull;

namespace ash::tether {

namespace {

// Used to verify the KeepAliveOperation notifies the observer when appropriate.
class MockOperationObserver : public KeepAliveOperation::Observer {
 public:
  MockOperationObserver() = default;

  MockOperationObserver(const MockOperationObserver&) = delete;
  MockOperationObserver& operator=(const MockOperationObserver&) = delete;

  ~MockOperationObserver() = default;

  MOCK_METHOD1(OnOperationFinishedRaw, void(DeviceStatus*));

  void OnOperationFinished(
      std::unique_ptr<DeviceStatus> device_status) override {
    OnOperationFinishedRaw(device_status.get());
  }
};

}  // namespace

class KeepAliveOperationTest : public testing::Test {
 public:
  KeepAliveOperationTest(const KeepAliveOperationTest&) = delete;
  KeepAliveOperationTest& operator=(const KeepAliveOperationTest&) = delete;

 protected:
  KeepAliveOperationTest()
      : tether_host_(TetherHost(multidevice::CreateRemoteDeviceRefForTest())) {}

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();
    operation_ = base::WrapUnique(new KeepAliveOperation(
        tether_host_, fake_host_connection_factory_.get()));
    operation_->AddObserver(&mock_observer_);

    operation_->SetTimerFactoryForTest(
        std::make_unique<ash::timer_factory::FakeTimerFactory>());

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation_->SetClockForTest(&test_clock_);
  }

  const TetherHost tether_host_;

  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;
  std::unique_ptr<KeepAliveOperation> operation_;

  base::SimpleTestClock test_clock_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;
};

// Tests that the KeepAliveTickle message is sent to the remote device once the
// communication channel is connected and authenticated.
TEST_F(KeepAliveOperationTest, KeepAliveTickleSentOnceAuthenticated) {
  // Setup the connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Start the operation.
  operation_->Initialize();

  // Verify the KeepAliveTickle message is sent.
  auto message_wrapper = std::make_unique<MessageWrapper>(KeepAliveTickle());
  std::string expected_payload = message_wrapper->ToRawMessage();
  EXPECT_EQ(1u, fake_host_connection_factory_
                    ->GetActiveConnection(tether_host_.GetDeviceId())
                    ->sent_messages()
                    .size());
  EXPECT_EQ(expected_payload,
            fake_host_connection_factory_
                ->GetActiveConnection(tether_host_.GetDeviceId())
                ->sent_messages()[0]
                .first->ToRawMessage());
}

// Tests that observers are notified when the operation has completed, signified
// by the OnMessageReceived handler being called.
TEST_F(KeepAliveOperationTest, NotifiesObserversOnResponse) {
  DeviceStatus test_status = CreateDeviceStatusWithFakeFields();

  // Setup the connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Verify that the observer is called with the correct parameters.
  EXPECT_CALL(mock_observer_, OnOperationFinishedRaw(NotNull()))
      .WillOnce(Invoke([&test_status](DeviceStatus* status) {
        EXPECT_EQ(test_status.SerializeAsString(), status->SerializeAsString());
      }));

  // Start the operation.
  operation_->Initialize();

  KeepAliveTickleResponse response;
  response.mutable_device_status()->CopyFrom(test_status);
  std::unique_ptr<MessageWrapper> message(new MessageWrapper(response));
  operation_->OnMessageReceived(std::move(message));
}

TEST_F(KeepAliveOperationTest, RecordsResponseDuration) {
  static constexpr base::TimeDelta kKeepAliveTickleResponseTime =
      base::Seconds(3);

  EXPECT_CALL(mock_observer_, OnOperationFinishedRaw(_));

  // Setup the connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Initialize the operation.
  operation_->Initialize();

  // Advance the clock in order to verify a non-zero response duration is
  // recorded and verified (below).
  test_clock_.Advance(kKeepAliveTickleResponseTime);

  std::unique_ptr<MessageWrapper> message(
      new MessageWrapper(KeepAliveTickleResponse()));
  operation_->OnMessageReceived(std::move(message));

  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.KeepAliveTickleResponseDuration",
      kKeepAliveTickleResponseTime, 1);
}

}  // namespace ash::tether
