// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/colocation/colocation_time_source.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

constexpr base::Time kInitialClockTime =
    base::Time::FromSecondsSinceUnixEpoch(1790000000);
constexpr base::Time kSyncedNetworkTime =
    base::Time::FromSecondsSinceUnixEpoch(1790003600);
constexpr base::TimeDelta kInitialTicksOffset = base::Days(1);

class ColocationTimeSourceTest : public testing::Test {
 protected:
  ColocationTimeSourceTest() {
    scoped_feature_list_.InitAndDisableFeature(
        network_time::kNetworkTimeServiceQuerying);
    network_time::NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    auto tracker_clock = std::make_unique<base::SimpleTestClock>();
    tracker_clock->SetNow(kInitialClockTime);
    auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
    tick_clock->Advance(kInitialTicksOffset);

    tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
        std::move(tracker_clock), std::move(tick_clock), &pref_service_,
        /*url_loader_factory=*/nullptr,
        network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);

    fallback_clock_.SetNow(kInitialClockTime);
  }

  void SyncNetworkTime(base::Time network_time) {
    tracker_->UpdateNetworkTime(network_time,
                                /*resolution=*/base::Milliseconds(50),
                                /*latency=*/base::TimeDelta(),
                                base::TimeTicks() + kInitialTicksOffset);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<network_time::NetworkTimeTracker> tracker_;
  base::SimpleTestClock fallback_clock_;
};

TEST_F(ColocationTimeSourceTest, ReturnsNetworkTimeWhenTrackerIsSynced) {
  SyncNetworkTime(kSyncedNetworkTime);
  EXPECT_EQ(GetColocationProofTime(tracker_.get(), &fallback_clock_),
            kSyncedNetworkTime);
}

TEST_F(ColocationTimeSourceTest, FallsBackToClockWhenTrackerHasNoSync) {
  EXPECT_EQ(GetColocationProofTime(tracker_.get(), &fallback_clock_),
            kInitialClockTime);
}

TEST_F(ColocationTimeSourceTest, FallsBackToClockWhenTrackerIsNull) {
  EXPECT_EQ(GetColocationProofTime(/*tracker=*/nullptr, &fallback_clock_),
            kInitialClockTime);
}

}  // namespace
}  // namespace browser_actuator
