// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_network_clock.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

using ::network_time::NetworkTimeTracker;

// The time source histogram name and bucket values.
constexpr std::string_view kTimeSourceHistogramName =
    "Variations.Headers.TimeSource";
constexpr int kLocalBucket = 1;
constexpr int kNetworkBucket = 2;

class VariationsNetworkClockTest : public ::testing::Test {
 public:
  VariationsNetworkClockTest() {
    CHECK(test_fixture_ == nullptr);
    test_fixture_ = this;
  }

  ~VariationsNetworkClockTest() override {
    CHECK(test_fixture_ == this);
    test_fixture_ = nullptr;
  }

  void SetUp() override {
    NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    // Override the system clock(s) and thread ticks.
    time_clock_overrides_ =
        std::make_unique<base::subtle::ScopedTimeClockOverrides>(
            &VariationsNetworkClockTest::Now,
            &VariationsNetworkClockTest::NowTicks,
            &VariationsNetworkClockTest::NowThreadTicks);

    // Create a NetworkTimeTracker with a `FETCHES_ON_DEMAND_ONLY` fetch
    // behavior, so that we can manually simulate network time updates.
    network_time_tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
        std::make_unique<base::DefaultClock>(),
        std::make_unique<base::DefaultTickClock>(), &pref_service_,
        /*url_loader_factory=*/nullptr,
        NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);

    // Create a VariationsNetworkClock using the NetworkTimeTracker.
    variations_network_clock_ =
        std::make_unique<VariationsNetworkClock>(network_time_tracker_.get());
  }

  // Simulates an update to the network time.
  void UpdateNetworkTime() {
    // Can not be smaller than 15, it's the NowFromSystemTime() resolution.
    constexpr base::TimeDelta resolution_ = base::Milliseconds(17);
    constexpr base::TimeDelta latency_ = base::Milliseconds(50);

    network_time_tracker_->UpdateNetworkTime(
        SimulatedLocalTime() - (latency_ / 2), resolution_, latency_,
        SimulatedLocalTimeTicks());
  }

  // Advances the simulated clock(s).
  void AdvanceClock(base::TimeDelta delta) {
    CHECK(delta >= base::TimeDelta());
    offset_ += delta;
  }

  // Offsets the simulated wall-clock by the given delta, leaving the remaining
  // clocks unchanged.
  void DivergeClock(base::TimeDelta divergence) { divergence_ = divergence; }

  // Returns the current estimated network time of `variations_network_clock_`.
  base::Time EstimatedNetworkTime() const {
    return variations_network_clock_->Now();
  }

  // Returns the current simulated local time. This is just an alias for
  // base::Time::Now() for readability. In turn, base::Time::Now() has been
  // overridden to call `VariationsNetworkClockTest::Now()`.
  base::Time SimulatedLocalTime() const { return base::Time::Now(); }

  // Returns the current simulated local time ticks. This is just an alias for
  // base::TimeTicks::Now() for readability. In turn, base::TimeTicks::Now()
  // has been overridden to call `VariationsNetworkClockTest::NowTicks()`.
  base::TimeTicks SimulatedLocalTimeTicks() const {
    return base::TimeTicks::Now();
  }

 protected:
  // Override for `base::Time::Now()`.
  static base::Time Now() {
    return base::Time() + test_fixture_->offset_ + test_fixture_->divergence_;
  }

  // Override for `base::TimeTicks::Now()`.
  static base::TimeTicks NowTicks() {
    return base::TimeTicks() + test_fixture_->offset_;
  }

  // Override for `base::ThreadTicks::Now()`.
  static base::ThreadTicks NowThreadTicks() {
    return base::ThreadTicks() + test_fixture_->offset_;
  }

  // The test fixture instance. This is used to access the simulated clock
  // offsets from the static methods above.
  static VariationsNetworkClockTest* test_fixture_;

  // The offset applied to all simulated clocks. Initialize to a non-zero value
  // to ensure that the simulated clocks are all non-zero.
  base::TimeDelta offset_{base::Days(365)};

  // The divergence applied to the simulated wall-clock. Used to simulate clock
  // sync loss.
  base::TimeDelta divergence_;

  // Remaining test state variables.
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_clock_overrides_;
  std::unique_ptr<NetworkTimeTracker> network_time_tracker_;
  std::unique_ptr<VariationsNetworkClock> variations_network_clock_;
};

VariationsNetworkClockTest* VariationsNetworkClockTest::test_fixture_ = nullptr;

TEST_F(VariationsNetworkClockTest, NetworkTimeUpdates) {
  // Having not received yet any simulated network time updates, the estimated
  // time should be equal to the simulated time (since it's falling back to the
  // local clock). The histogram tracking the time source should have a single
  // sample, indicating that local time was used.
  auto estimated_now = EstimatedNetworkTime();
  auto simulated_now = SimulatedLocalTime();
  EXPECT_EQ(estimated_now, simulated_now);
  histogram_tester_.ExpectBucketCount(kTimeSourceHistogramName,
                                      /*sample=*/kLocalBucket,
                                      /*expected_count=*/1);

  // Simulate a network time update. Now the estimator will start using the
  // simulated time for future estimates.
  UpdateNetworkTime();

  // The estimator should now be using the simulated network time, so the
  // estimated time should be equal to the simulated time. The histogram should
  // have a second sample, indicating that network time was used.
  estimated_now = EstimatedNetworkTime();
  simulated_now = SimulatedLocalTime();
  EXPECT_EQ(estimated_now, simulated_now);
  histogram_tester_.ExpectBucketCount(kTimeSourceHistogramName,
                                      /*sample=*/kNetworkBucket,
                                      /*expected_count=*/1);

  // Advance the simulated clock(s). The simulated tick clock will be used to
  // estimate the network time... so it should still match the simulated clock.
  AdvanceClock(base::Minutes(10));
  estimated_now = EstimatedNetworkTime();
  simulated_now = SimulatedLocalTime();
  EXPECT_EQ(estimated_now, simulated_now);
  histogram_tester_.ExpectBucketCount(kTimeSourceHistogramName,
                                      /*sample=*/kNetworkBucket,
                                      /*expected_count=*/2);

  // Simulate a clock divergence of one hour. The VariationsNetworkClock will
  // fall back to local clock until it receives a new update.
  DivergeClock(base::Hours(1));
  estimated_now = EstimatedNetworkTime();
  simulated_now = SimulatedLocalTime();
  EXPECT_EQ(estimated_now, simulated_now);
  histogram_tester_.ExpectBucketCount(kTimeSourceHistogramName,
                                      /*sample=*/kLocalBucket,
                                      /*expected_count=*/2);

  // Update the network time. The local clock divergence is still one hour, but
  // now that divergence is accounted for by the estimator.
  UpdateNetworkTime();
  AdvanceClock(base::Hours(2));
  estimated_now = EstimatedNetworkTime();
  simulated_now = SimulatedLocalTime();
  EXPECT_EQ(estimated_now, simulated_now);
  histogram_tester_.ExpectBucketCount(kTimeSourceHistogramName,
                                      /*sample=*/kNetworkBucket,
                                      /*expected_count=*/3);
}

}  // namespace
}  // namespace variations
