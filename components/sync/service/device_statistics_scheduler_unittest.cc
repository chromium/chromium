// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_scheduler.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
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
  base::test::ScopedFeatureList features_{kSyncRecordDeviceStatisticsMetrics};
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
  task_environment_.FastForwardBy(
      kSyncRecordDeviceStatisticsMetricsDelay.Get());

  EXPECT_TRUE(fake_requests_.empty());
}

TEST_F(DeviceStatisticsSchedulerTest, StartsTrackerIfPrefIsUnset) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.MakeAccountAvailable("secondary@example.com");

  ASSERT_FALSE(pref_service_.HasPrefPath("sync.device_statistics_timestamp"));

  DeviceStatisticsScheduler scheduler(/*delegate=*/this, &pref_service_,
                                      identity_test_env_.identity_manager(),
                                      GURL("https://example.com/"));

  // Give the scheduler a chance to start the tracker.
  task_environment_.FastForwardBy(
      kSyncRecordDeviceStatisticsMetricsDelay.Get());

  EXPECT_EQ(fake_requests_.size(), 2u);
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
  task_environment_.FastForwardBy(
      kSyncRecordDeviceStatisticsMetricsDelay.Get());

  EXPECT_EQ(fake_requests_.size(), 2u);
}

TEST_F(DeviceStatisticsSchedulerTest, WaitsForRefreshTokensLoaded) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);
  identity_test_env_.MakeAccountAvailable("secondary@example.com");

  pref_service_.SetTime("sync.device_statistics_timestamp",
                        base::Time::Now() - base::Hours(25));

  // At the time the DeviceStatisticsScheduler is created, the refresh tokens
  // haven't been loaded yet.
  identity_test_env_.ResetToAccountsNotYetLoadedFromDiskState();

  DeviceStatisticsScheduler scheduler(/*delegate=*/this, &pref_service_,
                                      identity_test_env_.identity_manager(),
                                      GURL("https://example.com/"));

  // Give the scheduler a chance to start the tracker.
  task_environment_.FastForwardBy(
      kSyncRecordDeviceStatisticsMetricsDelay.Get());

  // Since the refresh tokens aren't loaded, no requests should've been sent
  // yet.
  EXPECT_EQ(fake_requests_.size(), 0u);

  // Once the refresh tokens are loaded, requests should be sent immediately (in
  // a posted task).
  identity_test_env_.ReloadAccountsFromDisk();
  identity_test_env_.WaitForRefreshTokensLoaded();
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_EQ(fake_requests_.size(), 2u);
}

TEST_F(DeviceStatisticsSchedulerTest, StartsTrackerPeriodically) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  // Fast-forward to 13:00 (on the following day, to ensure the time is moving
  // forward).
  const base::Time start =
      base::Time::Now().LocalMidnight() + base::Hours(24 + 13);
  task_environment_.FastForwardBy(start - base::Time::Now());

  pref_service_.SetTime("sync.device_statistics_timestamp",
                        base::Time::Now() - base::Hours(25));

  DeviceStatisticsScheduler scheduler(this, &pref_service_,
                                      identity_test_env_.identity_manager(),
                                      GURL("https://example.com/"));

  // Since the last metrics emission was on the previous day, the new run should
  // start as soon as the startup delay passes.
  task_environment_.FastForwardBy(
      kSyncRecordDeviceStatisticsMetricsDelay.Get());

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess({});
  fake_requests_.clear();

  // The periodic recording triggers at noon on every day. The original
  // recording happened at 13:00, so 24 hours later another recording should've
  // been started.
  task_environment_.FastForwardBy(base::Hours(24));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess({});
  fake_requests_.clear();

  // Another 10 hours later, it'll be 23:00 - not recording time yet.
  task_environment_.FastForwardBy(base::Hours(10));

  EXPECT_EQ(fake_requests_.size(), 0u);

  // By 13:00 on the next day, recording should've started again.
  task_environment_.FastForwardBy(base::Hours(14));
  EXPECT_EQ(fake_requests_.size(), 1u);
}

}  // namespace

}  // namespace syncer
