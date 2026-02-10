// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_scheduler.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/service/device_statistics_request.h"
#include "components/sync/service/device_statistics_tracker.h"
#include "components/sync/test/fake_device_statistics_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class DeviceStatisticsSchedulerTest
    : public testing::Test,
      public DeviceStatisticsScheduler::Delegate {
 public:
  DeviceStatisticsSchedulerTest() {
    DeviceStatisticsScheduler::RegisterProfilePrefs(pref_service_.registry());
  }

  bool IsDeviceStatisticsMetricReportingEnabled() override { return true; }

  std::unique_ptr<DeviceStatisticsRequest> CreateDeviceStatisticsRequest(
      const CoreAccountInfo& account,
      const GURL& url) override {
    auto request = std::make_unique<FakeDeviceStatisticsRequest>();
    fake_requests_.emplace(account.gaia, request->GetWeakPtr());
    return request;
  }

  std::vector<std::string> GetCurrentDeviceCacheGuidsForDeviceStatistics()
      override {
    return std::vector<std::string>{"test_guid"};
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;

  base::flat_map<GaiaId, base::WeakPtr<FakeDeviceStatisticsRequest>>
      fake_requests_;
};

TEST_F(DeviceStatisticsSchedulerTest, DoesNotStartTrackerIfPrefIsRecent) {
  identity_test_env_.MakePrimaryAccountAvailable("primary@example.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.MakeAccountAvailable("secondary@example.com");

  pref_service_.SetTime("sync.device_statistics_timestamp", base::Time::Now());

  DeviceStatisticsScheduler scheduler(/*delegate=*/this, &pref_service_,
                                      identity_test_env_.identity_manager(),
                                      GURL("https://example.com/"));

  // Give the scheduler a chance to start the tracker.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(fake_requests_.empty());
}

TEST_F(DeviceStatisticsSchedulerTest, StartsTrackerIfPrefIsOld) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.MakeAccountAvailable("secondary@example.com");

  pref_service_.SetTime("sync.device_statistics_timestamp",
                        base::Time::Now() - base::Hours(25));

  DeviceStatisticsScheduler scheduler(/*delegate=*/this, &pref_service_,
                                      identity_test_env_.identity_manager(),
                                      GURL("https://example.com/"));

  // Give the scheduler a chance to start the tracker.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fake_requests_.size(), 2u);
}

}  // namespace

}  // namespace syncer
