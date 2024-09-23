// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_periodic_scheduler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestPrefName[] = "test_pref_name";
constexpr base::TimeDelta kTestRequestPeriod = base::Minutes(123);

}  // namespace

namespace ash::nearby {

class NearbyPeriodicSchedulerTest : public ::testing::Test {
 protected:
  NearbyPeriodicSchedulerTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {}

  ~NearbyPeriodicSchedulerTest() override = default;

  void SetUp() override {
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    pref_service_.registry()->RegisterDictionaryPref(kTestPrefName);
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    scheduler_ = std::make_unique<NearbyPeriodicScheduler>(
        kTestRequestPeriod, /*retry_failures=*/true,
        /*require_connectivity=*/true, kTestPrefName, &pref_service_,
        base::DoNothing(), Feature::NS, task_environment_.GetMockClock());
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  NearbyScheduler* scheduler() { return scheduler_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NearbyScheduler> scheduler_;
};

TEST_F(NearbyPeriodicSchedulerTest, PeriodicRequest) {
  // Set Now() to something nontrivial.
  FastForward(base::Days(100));

  // Immediately runs a first-time periodic request.
  scheduler()->Start();
  std::optional<base::TimeDelta> time_until_next_request =
      scheduler()->GetTimeUntilNextRequest();
  EXPECT_EQ(base::Seconds(0), scheduler()->GetTimeUntilNextRequest());
  FastForward(*time_until_next_request);
  scheduler()->HandleResult(/*success=*/true);
  EXPECT_EQ(Now(), scheduler()->GetLastSuccessTime());

  // Let 1 minute elapse since last success.
  base::TimeDelta elapsed_time = base::Minutes(1);
  FastForward(elapsed_time);

  EXPECT_EQ(kTestRequestPeriod - elapsed_time,
            scheduler()->GetTimeUntilNextRequest());
}

}  // namespace ash::nearby
