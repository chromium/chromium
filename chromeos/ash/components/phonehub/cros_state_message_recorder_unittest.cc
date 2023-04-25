// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/cros_state_message_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::phonehub {
namespace {
constexpr auto kLatencyDelta = base::Milliseconds(500u);
}  // namespace

class CrosStateMessageRecorderTest : public testing::Test {
 protected:
  CrosStateMessageRecorderTest() = default;
  CrosStateMessageRecorderTest(const CrosStateMessageRecorderTest&) = delete;
  CrosStateMessageRecorderTest& operator=(const CrosStateMessageRecorderTest&) =
      delete;
  ~CrosStateMessageRecorderTest() override = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  CrosStateMessageRecorder recorder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(CrosStateMessageRecorderTest, RecordLatency) {
  recorder_.RecordCrosStateMessageSent();
  task_environment_.FastForwardBy(kLatencyDelta);
  recorder_.RecordPhoneStatusSnapShotReceived();
  histogram_tester_.ExpectTimeBucketCount(
      "PhoneHub.InitialPhoneStatusSnapshot.Latency", kLatencyDelta,
      /*expected_count=*/1);
  EXPECT_TRUE(recorder_.message_sent_timestamp_.is_null());
}

}  // namespace ash::phonehub
