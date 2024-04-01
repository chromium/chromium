// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/common/scheduling/nearby_on_demand_scheduler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestPrefName[] = "test_pref_name";

}  // namespace

namespace ash::nearby {

class NearbyOnDemandSchedulerTest : public ::testing::Test {
 protected:
  NearbyOnDemandSchedulerTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {}

  ~NearbyOnDemandSchedulerTest() override = default;

  void SetUp() override {
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    pref_service_.registry()->RegisterDictionaryPref(kTestPrefName);
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);

    scheduler_ = std::make_unique<NearbyOnDemandScheduler>(
        /*retry_failures=*/true, /*require_connectivity=*/true, kTestPrefName,
        &pref_service_, base::DoNothing(), Feature::NS,
        task_environment_.GetMockClock());
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

TEST_F(NearbyOnDemandSchedulerTest, NoRecurringRequest) {
  scheduler()->Start();
  EXPECT_FALSE(scheduler()->GetTimeUntilNextRequest());
}

}  // namespace ash::nearby
