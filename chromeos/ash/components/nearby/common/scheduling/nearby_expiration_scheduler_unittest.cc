// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_expiration_scheduler.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestPrefName[] = "test_pref_name";

constexpr base::TimeDelta kTestInitialNow = base::Days(100);
constexpr base::TimeDelta kTestExpirationTimeFromInitalNow = base::Minutes(123);

}  // namespace

namespace ash::nearby {

class NearbyExpirationSchedulerTest : public ::testing::Test {
 protected:
  NearbyExpirationSchedulerTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {}

  ~NearbyExpirationSchedulerTest() override = default;

  void SetUp() override {
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    FastForward(kTestInitialNow);
    expiration_time_ = Now() + kTestExpirationTimeFromInitalNow;

    pref_service_.registry()->RegisterDictionaryPref(kTestPrefName);
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    scheduler_ = std::make_unique<NearbyExpirationScheduler>(
        base::BindRepeating(
            &NearbyExpirationSchedulerTest::TestExpirationTimeFunctor,
            base::Unretained(this)),
        /*retry_failures=*/true, /*require_connectivity=*/true, kTestPrefName,
        &pref_service_, base::DoNothing(), Feature::NS,
        task_environment_.GetMockClock());
  }

  std::optional<base::Time> TestExpirationTimeFunctor() {
    return expiration_time_;
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  std::optional<base::Time> expiration_time_;
  NearbyScheduler* scheduler() { return scheduler_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<NearbyScheduler> scheduler_;
};

TEST_F(NearbyExpirationSchedulerTest, ExpirationRequest) {
  scheduler()->Start();

  // Let 5 minutes elapse since the start time just to make sure the time to the
  // next request only depends on the expiration time and the current time.
  FastForward(base::Minutes(5));

  EXPECT_EQ(*expiration_time_ - Now(), scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbyExpirationSchedulerTest, Reschedule) {
  scheduler()->Start();
  FastForward(base::Minutes(5));

  base::TimeDelta initial_expected_time_until_next_request =
      *expiration_time_ - Now();
  EXPECT_EQ(initial_expected_time_until_next_request,
            scheduler()->GetTimeUntilNextRequest());

  // The expiration time suddenly changes.
  expiration_time_ = *expiration_time_ + base::Days(2);
  scheduler()->Reschedule();
  EXPECT_EQ(initial_expected_time_until_next_request + base::Days(2),
            scheduler()->GetTimeUntilNextRequest());
}

TEST_F(NearbyExpirationSchedulerTest, NullExpirationTime) {
  expiration_time_.reset();
  scheduler()->Start();
  EXPECT_EQ(std::nullopt, scheduler()->GetTimeUntilNextRequest());
}

}  // namespace ash::nearby
