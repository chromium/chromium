// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_operation.h"

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/fake_host_connection.h"
#include "chromeos/ash/components/tether/message_wrapper.h"
#include "chromeos/ash/components/tether/proto/tether.pb.h"
#include "chromeos/ash/components/timer_factory/fake_timer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::tether {

namespace {

constexpr base::TimeDelta kDisconnectTetheringRequestTime = base::Seconds(3);

// Used to verify the DisonnectTetheringOperation notifies the observer when
// appropriate.
class MockOperationObserver : public DisconnectTetheringOperation::Observer {
 public:
  MockOperationObserver() = default;

  MockOperationObserver(const MockOperationObserver&) = delete;
  MockOperationObserver& operator=(const MockOperationObserver&) = delete;

  ~MockOperationObserver() = default;

  MOCK_METHOD2(OnOperationFinished, void(const std::string&, bool));
};

}  // namespace

class DisconnectTetheringOperationTest : public testing::Test {
 public:
  DisconnectTetheringOperationTest(const DisconnectTetheringOperationTest&) =
      delete;
  DisconnectTetheringOperationTest& operator=(
      const DisconnectTetheringOperationTest&) = delete;

 protected:
  DisconnectTetheringOperationTest()
      : tether_host_(TetherHost(multidevice::CreateRemoteDeviceRefForTest())) {}

  void SetUp() override {
    fake_host_connection_factory_ =
        std::make_unique<FakeHostConnection::Factory>();

    operation_ = base::WrapUnique(new DisconnectTetheringOperation(
        tether_host_, fake_host_connection_factory_.get()));
    operation_->AddObserver(&mock_observer_);

    operation_->SetTimerFactoryForTest(
        std::make_unique<ash::timer_factory::FakeTimerFactory>());

    test_clock_.SetNow(base::Time::UnixEpoch());
    operation_->SetClockForTest(&test_clock_);
  }

  const TetherHost tether_host_;

  std::unique_ptr<FakeHostConnection::Factory> fake_host_connection_factory_;

  base::SimpleTestClock test_clock_;
  MockOperationObserver mock_observer_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<DisconnectTetheringOperation> operation_;
};

TEST_F(DisconnectTetheringOperationTest, TestSuccess) {
  // Setup the connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Verify that the Observer is called with success and the correct
  // parameters.
  EXPECT_CALL(mock_observer_, OnOperationFinished(tether_host_.GetDeviceId(),
                                                  true /* successful */));

  // Initialize the operation.
  operation_->Initialize();

  // Advance the clock in order to verify a non-zero request duration is
  // recorded and verified (below).
  test_clock_.Advance(kDisconnectTetheringRequestTime);

  fake_host_connection_factory_->GetActiveConnection(tether_host_.GetDeviceId())
      ->FinishSendingMessages();

  // Verify the request duration metric is recorded.
  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration",
      kDisconnectTetheringRequestTime, 1);
}

TEST_F(DisconnectTetheringOperationTest, TestFailure) {
  // Verify that the observer is called with failure and the correct parameters.
  EXPECT_CALL(mock_observer_, OnOperationFinished(tether_host_.GetDeviceId(),
                                                  false /* successful */));

  // Setup a connection to be returned.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Start the operation.
  operation_->Initialize();

  // Disconnect before allowing messages to send.
  fake_host_connection_factory_->GetActiveConnection(tether_host_.GetDeviceId())
      ->Close();

  histogram_tester_.ExpectTotalCount(
      "InstantTethering.Performance.DisconnectTetheringRequestDuration", 0);
}

// Tests that the DisonnectTetheringRequest message is sent to the remote device
// once the communication channel is connected and authenticated.
TEST_F(DisconnectTetheringOperationTest,
       DisconnectRequestSentOnceAuthenticated) {
  // Setup the connection.
  fake_host_connection_factory_->SetupConnectionAttempt(tether_host_);

  // Initialize the operation.
  operation_->Initialize();

  // Verify the DisconnectTetheringRequest message is sent.
  auto message_wrapper =
      std::make_unique<MessageWrapper>(DisconnectTetheringRequest());
  std::string expected_payload = message_wrapper->ToRawMessage();
  auto& sent_messages = fake_host_connection_factory_
                            ->GetActiveConnection(tether_host_.GetDeviceId())
                            ->sent_messages();
  EXPECT_EQ(1u, sent_messages.size());
  EXPECT_EQ(expected_payload, sent_messages[0].first->ToRawMessage());
}

}  // namespace ash::tether
