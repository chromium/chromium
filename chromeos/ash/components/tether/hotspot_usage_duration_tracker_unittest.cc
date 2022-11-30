// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/hotspot_usage_duration_tracker.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

const char kTestDeviceId[] = "testDeviceId";
const char kTestTetherNetworkGuid[] = "testTetherNetworkGuid";
const char kTestWifiNetworkGuid[] = "testWifiNetworkGuid";

constexpr const base::TimeDelta kTestDuration = base::Seconds(5);

}  // namespace

class HotspotUsageDurationTrackerTest : public testing::Test {
 public:
  HotspotUsageDurationTrackerTest(const HotspotUsageDurationTrackerTest&) =
      delete;
  HotspotUsageDurationTrackerTest& operator=(
      const HotspotUsageDurationTrackerTest&) = delete;

 protected:
  HotspotUsageDurationTrackerTest() = default;
  ~HotspotUsageDurationTrackerTest() override = default;

  void SetUp() override {
    fake_active_host_ = std::make_unique<FakeActiveHost>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(base::Time::UnixEpoch());

    tracker_ = std::make_unique<HotspotUsageDurationTracker>(
        fake_active_host_.get(), test_clock_.get());
  }

  void AdvanceClock() { test_clock_->Advance(kTestDuration); }

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<HotspotUsageDurationTracker> tracker_;
};

TEST_F(HotspotUsageDurationTrackerTest, TestNoConnection) {
  // Attempt a connection but fail to connect. No duration should be logged.
  fake_active_host_->SetActiveHostConnecting(kTestDeviceId,
                                             kTestTetherNetworkGuid);
  AdvanceClock();
  fake_active_host_->SetActiveHostDisconnected();
  histogram_tester_.ExpectTotalCount("InstantTethering.HotspotUsageDuration",
                                     0 /* count */);

  AdvanceClock();

  // Fail one more attempt.
  fake_active_host_->SetActiveHostConnecting(kTestDeviceId,
                                             kTestTetherNetworkGuid);
  AdvanceClock();
  fake_active_host_->SetActiveHostDisconnected();
  histogram_tester_.ExpectTotalCount("InstantTethering.HotspotUsageDuration",
                                     0 /* count */);
}

TEST_F(HotspotUsageDurationTrackerTest, TestConnections) {
  // Connect, have some time pass, then disconnect. Metric should have been
  // logged.
  fake_active_host_->SetActiveHostConnecting(kTestDeviceId,
                                             kTestTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(
      kTestDeviceId, kTestTetherNetworkGuid, kTestWifiNetworkGuid);
  AdvanceClock();
  fake_active_host_->SetActiveHostDisconnected();
  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.HotspotUsageDuration", kTestDuration, 1 /* count */);

  AdvanceClock();

  // Try one more time, advancing the clock for twice as long.
  fake_active_host_->SetActiveHostConnecting(kTestDeviceId,
                                             kTestTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(
      kTestDeviceId, kTestTetherNetworkGuid, kTestWifiNetworkGuid);
  AdvanceClock();
  AdvanceClock();
  fake_active_host_->SetActiveHostDisconnected();
  histogram_tester_.ExpectTimeBucketCount(
      "InstantTethering.HotspotUsageDuration", 2 * kTestDuration,
      1 /* count */);
}

TEST_F(HotspotUsageDurationTrackerTest, TestErrorCondition) {
  fake_active_host_->SetActiveHostConnecting(kTestDeviceId,
                                             kTestTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(
      kTestDeviceId, kTestTetherNetworkGuid, kTestWifiNetworkGuid);
  AdvanceClock();

  // Transition from CONNECTED to CONNECTING. This should never actually happen,
  // so we can assume that some error occurred. No metric should be logged.
  fake_active_host_->SetActiveHostConnecting(kTestDeviceId,
                                             kTestTetherNetworkGuid);
  histogram_tester_.ExpectTotalCount("InstantTethering.HotspotUsageDuration",
                                     0 /* count */);
}

}  // namespace tether

}  // namespace ash
