// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_session_completion_logger.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

class TetherSessionCompletionLoggerTest : public testing::Test {
 public:
  TetherSessionCompletionLoggerTest(const TetherSessionCompletionLoggerTest&) =
      delete;
  TetherSessionCompletionLoggerTest& operator=(
      const TetherSessionCompletionLoggerTest&) = delete;

 protected:
  TetherSessionCompletionLoggerTest() = default;
  ~TetherSessionCompletionLoggerTest() override = default;

  void SetUp() override {
    logger_ = std::make_unique<TetherSessionCompletionLogger>();
  }

  void TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason
          expected_session_completion_reason) {
    logger_->RecordTetherSessionCompletion(expected_session_completion_reason);
    histogram_tester_.ExpectUniqueSample(
        "InstantTethering.SessionCompletionReason",
        expected_session_completion_reason, 1);
  }

  std::unique_ptr<TetherSessionCompletionLogger> logger_;

  base::HistogramTester histogram_tester_;
};

TEST_F(TetherSessionCompletionLoggerTest, TestOther) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::OTHER);
}

TEST_F(TetherSessionCompletionLoggerTest, TestUserDisconnected) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          USER_DISCONNECTED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestConnectionDropped) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          CONNECTION_DROPPED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestUserLoggedOut) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::USER_LOGGED_OUT);
}

TEST_F(TetherSessionCompletionLoggerTest, TestUserClosedLid) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::USER_CLOSED_LID);
}

TEST_F(TetherSessionCompletionLoggerTest, TestBluetoothDisabled) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          BLUETOOTH_DISABLED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestCellularDisabled) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          CELLULAR_DISABLED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestWiFiDisabled) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::WIFI_DISABLED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestBluetoothControllerDisappeared) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          BLUETOOTH_CONTROLLER_DISAPPEARED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestMultiDeviceHostUnverified) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          MULTIDEVICE_HOST_UNVERIFIED);
}

TEST_F(TetherSessionCompletionLoggerTest, TestBetterTogetherSuiteDisabled) {
  TestSessionCompletionReasonRecorded(
      TetherSessionCompletionLogger::SessionCompletionReason::
          BETTER_TOGETHER_SUITE_DISABLED);
}

}  // namespace tether

}  // namespace ash
