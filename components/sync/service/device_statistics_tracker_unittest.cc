// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_tracker.h"

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/device_statistics_request.h"
#include "components/sync/test/fake_device_statistics_request.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

constexpr char kThisDeviceCacheGuid[] = "this_device_guid";

class DeviceStatisticsTrackerTest : public testing::Test {
 public:
  DeviceStatisticsTrackerTest() = default;

  std::unique_ptr<DeviceStatisticsRequest> CreateRequest(
      const CoreAccountInfo& account,
      const GURL& url) {
    auto request = std::make_unique<FakeDeviceStatisticsRequest>();
    fake_requests_.emplace(account.gaia, request->GetWeakPtr());
    return request;
  }

  DeviceStatisticsTracker::RequestFactory CreateRequestFactory() {
    return base::BindRepeating(&DeviceStatisticsTrackerTest::CreateRequest,
                               base::Unretained(this));
  }

  sync_pb::SyncEntity CreateDeviceInfo(std::string_view cache_guid,
                                       sync_pb::SyncEnums_OsType platform,
                                       bool history_opt_in) {
    const base::Time now = base::Time::Now();

    sync_pb::SyncEntity entity;
    entity.set_ctime(syncer::TimeToProtoTime(now - base::Days(7)));
    entity.set_mtime(syncer::TimeToProtoTime(now - base::Days(1)));

    sync_pb::DeviceInfoSpecifics& device =
        *entity.mutable_specifics()->mutable_device_info();
    device.set_cache_guid(cache_guid);
    device.set_os_type(platform);
    if (history_opt_in) {
      device.mutable_invalidation_fields()->add_interested_data_type_ids(
          sync_pb::EntitySpecifics::kHistoryDeleteDirectiveFieldNumber);
    }
    device.mutable_chrome_version_info();
    device.set_last_updated_timestamp(
        syncer::TimeToProtoTime(now - base::Days(1)));

    return entity;
  }

  std::vector<sync_pb::SyncEntity> CreateDeviceInfosWithPlatforms(
      const std::vector<sync_pb::SyncEnums_OsType>& platforms) {
    std::vector<bool> history_opt_ins(platforms.size(), false);
    return CreateDeviceInfos(platforms, history_opt_ins);
  }

  std::vector<sync_pb::SyncEntity> CreateDeviceInfosWithHistoryOptIns(
      const std::vector<bool>& history_opt_ins) {
    std::vector<sync_pb::SyncEnums_OsType> platforms(
        history_opt_ins.size(), sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS);
    return CreateDeviceInfos(platforms, history_opt_ins);
  }

  std::vector<sync_pb::SyncEntity> CreateDeviceInfos(
      const std::vector<sync_pb::SyncEnums_OsType>& platforms,
      const std::vector<bool>& history_opt_ins) {
    CHECK_EQ(platforms.size(), history_opt_ins.size());
    std::vector<sync_pb::SyncEntity> entities;
    for (size_t i = 0; i < platforms.size(); ++i) {
      entities.push_back(
          CreateDeviceInfo("test_guid_" + base::NumberToString(i), platforms[i],
                           history_opt_ins[i]));
    }
    return entities;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;

  base::flat_map<GaiaId, base::WeakPtr<FakeDeviceStatisticsRequest>>
      fake_requests_;
};

TEST_F(DeviceStatisticsTrackerTest, RecordsOutcomeWhenNoAccounts) {
  identity_test_env_.WaitForRefreshTokensLoaded();

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 0u);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectTotalCount(
      "Sync.DeviceStatistics.RequestsStartedCount", 0);
  histogram_tester.ExpectTotalCount(
      "Sync.DeviceStatistics.RequestsCompletedSuccess", 0);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::kNoAccounts,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenPrimarySucceedsAndSecondaryFails) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess({});
  fake_requests_[secondary.gaia]->SimulateFailure();
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::
          kPrimarySucceededButNonPrimaryFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryNo,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsNoOutcomeWhenPrimaryFailsAndSecondarySucceeds) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateFailure();
  fake_requests_[secondary.gaia]->SimulateSuccess({});
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::
          kPrimaryFailedButNonPrimarySucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("Sync.DeviceStatistics.Outcome.Overall", 0);
}

TEST_F(DeviceStatisticsTrackerTest, RecordsNoOutcomeWhenAllRequestsFail) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateFailure();
  fake_requests_[secondary.gaia]->SimulateFailure();
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/DeviceStatisticsTracker::RequestsCompletedSuccess::kAllFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("Sync.DeviceStatistics.Outcome.Overall", 0);
}

// On ChromeOS, the primary account cannot change.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(DeviceStatisticsTrackerTest, RecordsNoOutcomeWhenPrimaryAccountChanges) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);

  identity_test_env_.ClearPrimaryAccount();
  identity_test_env_.MakePrimaryAccountAvailable("another@example.com",
                                                 signin::ConsentLevel::kSignin);

  fake_requests_[primary.gaia]->SimulateSuccess({});
  fake_requests_[secondary.gaia]->SimulateSuccess({});
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::
          kPrimaryAccountChangedOrRemoved,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount("Sync.DeviceStatistics.Outcome.Overall", 0);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(DeviceStatisticsTrackerTest, RecordsOutcomeWhenPrimaryHasOtherDevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(CreateDeviceInfosWithPlatforms(
      {sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS}));
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryYesNonPrimaryNA,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenPrimaryHasNoOtherDevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess({});
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryNA,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenBothPrimaryAndSecondaryHaveOtherDevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess(CreateDeviceInfosWithPlatforms(
      {sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS}));
  fake_requests_[secondary.gaia]->SimulateSuccess(
      CreateDeviceInfosWithPlatforms({sync_pb::SyncEnums_OsType_OS_TYPE_MAC}));
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryYesNonPrimaryYes,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenOnlyPrimaryHasOtherDevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess(CreateDeviceInfosWithPlatforms(
      {sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS}));
  fake_requests_[secondary.gaia]->SimulateSuccess({});
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryYesNonPrimaryNo,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenOnlySecondaryHasOtherDevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess({});
  fake_requests_[secondary.gaia]->SimulateSuccess(
      CreateDeviceInfosWithPlatforms({sync_pb::SyncEnums_OsType_OS_TYPE_MAC}));
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryYes,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, RecordsOutcomeWhenNobodyHasOtherDevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess({});
  fake_requests_[secondary.gaia]->SimulateSuccess({});
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryNo,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenPrimaryDoesNotExistAndSecondarySucceeds) {
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[secondary.gaia]->SimulateSuccess({});
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/1,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::kAllSucceeded,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNANonPrimaryNo,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsOutcomeWhenPrimaryDoesNotExistAndSecondaryMixed) {
  AccountInfo secondary1 =
      identity_test_env_.MakeAccountAvailable("secondary1@example.com");
  AccountInfo secondary2 =
      identity_test_env_.MakeAccountAvailable("secondary2@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[secondary1.gaia]->SimulateSuccess({});
  fake_requests_[secondary2.gaia]->SimulateFailure();
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsStartedCount", /*sample=*/2,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.RequestsCompletedSuccess",
      /*sample=*/
      DeviceStatisticsTracker::RequestsCompletedSuccess::
          kPrimaryNAAndSomeNonPrimaryFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNANonPrimaryNo,
      /*expected_bucket_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, ExcludesCurrentDevice) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  constexpr char kThisDeviceSecondaryCacheGuid[] = "this_device_guid2";

  DeviceStatisticsTracker tracker(
      identity_test_env_.identity_manager(), GURL("https://example.com/"),
      CreateRequestFactory(),
      {kThisDeviceCacheGuid, kThisDeviceSecondaryCacheGuid});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  // The primary account only has the current device.
  std::vector<sync_pb::SyncEntity> primary_device_infos{CreateDeviceInfo(
      kThisDeviceCacheGuid, sync_pb::SyncEnums_OsType_OS_TYPE_MAC, false)};
  // The secondary account has one entry for this device (from being previously
  // signed in / primary) plus one other device.
  std::vector<sync_pb::SyncEntity> secondary_device_infos{
      CreateDeviceInfo(kThisDeviceSecondaryCacheGuid,
                       sync_pb::SyncEnums_OsType_OS_TYPE_MAC, false),
      CreateDeviceInfo("some_other_guid", sync_pb::SyncEnums_OsType_OS_TYPE_MAC,
                       false)};

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess(primary_device_infos);
  fake_requests_[secondary.gaia]->SimulateSuccess(secondary_device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.Overall",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryYes,
      /*expected_bucket_count=*/1);

  // For both primary and non-primary account, the current device was not
  // counted.
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.NumberOfAdditionalClients",
      /*sample=*/0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "NumberOfAdditionalClients",
      /*sample=*/1,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, RecordsOtherPlatformsMetrics) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary =
      identity_test_env_.MakeAccountAvailable("secondary@example.com");

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 2u);
  fake_requests_[primary.gaia]->SimulateSuccess(
      CreateDeviceInfosWithPlatforms({sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
                                      sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
                                      sync_pb::SyncEnums_OsType_OS_TYPE_MAC}));
  fake_requests_[secondary.gaia]->SimulateSuccess(
      CreateDeviceInfosWithPlatforms(
          {sync_pb::SyncEnums_OsType_OS_TYPE_IOS,
           sync_pb::SyncEnums_OsType_OS_TYPE_LINUX}));
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.NumberOfAdditionalClients",
      /*sample=*/3,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.PlatformOfAdditionalClient",
      DeviceStatisticsTracker::Platform::kWindows,
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.PlatformOfAdditionalClient",
      DeviceStatisticsTracker::Platform::kMac,
      /*expected_count=*/1);

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "NumberOfAdditionalClients",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "PlatformOfAdditionalClient",
      DeviceStatisticsTracker::Platform::kIOS,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "PlatformOfAdditionalClient",
      DeviceStatisticsTracker::Platform::kLinux,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsHistoryMetricsWhenBothThisAndOtherDevicesOptedIn) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(
      identity_test_env_.identity_manager(), GURL("https://example.com/"),
      CreateRequestFactory(), {kThisDeviceCacheGuid});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  std::vector<sync_pb::SyncEntity> device_infos =
      CreateDeviceInfosWithHistoryOptIns({true, false, true});
  device_infos.push_back(CreateDeviceInfo(
      kThisDeviceCacheGuid, sync_pb::SyncEnums_OsType_OS_TYPE_MAC, true));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients",
      /*sample=*/3,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn",
      DeviceStatisticsTracker::HistoryOptInSummary::
          kThisDeviceYesOtherDevicesYes,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest,
       RecordsHistoryMetricsWhenOnlyOtherDevicesOptedIn) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(
      identity_test_env_.identity_manager(), GURL("https://example.com/"),
      CreateRequestFactory(), {kThisDeviceCacheGuid});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  std::vector<sync_pb::SyncEntity> device_infos =
      CreateDeviceInfosWithHistoryOptIns({true, false, true});
  device_infos.push_back(CreateDeviceInfo(
      kThisDeviceCacheGuid, sync_pb::SyncEnums_OsType_OS_TYPE_MAC, false));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients",
      /*sample=*/3,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn",
      DeviceStatisticsTracker::HistoryOptInSummary::
          kThisDeviceNoOtherDevicesYes,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, RecordsHistoryMetricsWhenNoDevicesOptedIn) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(
      identity_test_env_.identity_manager(), GURL("https://example.com/"),
      CreateRequestFactory(), {kThisDeviceCacheGuid});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  std::vector<sync_pb::SyncEntity> device_infos =
      CreateDeviceInfosWithHistoryOptIns({false, false});
  device_infos.push_back(CreateDeviceInfo(
      kThisDeviceCacheGuid, sync_pb::SyncEnums_OsType_OS_TYPE_MAC, false));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn",
      /*sample=*/0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn",
      DeviceStatisticsTracker::HistoryOptInSummary::kThisDeviceNoOtherDevicesNo,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, DedupesByActivityTimeRange) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 1u);
  std::vector<sync_pb::SyncEntity> entities = CreateDeviceInfosWithPlatforms(
      {sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
       sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
       sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS});
  // Give all the entities non-overlapping usage time ranges. This means they
  // likely all represent the same device, just with different cache GUIDs (i.e.
  // the user removed and re-added the same account on the same device).
  const base::Time now = base::Time::Now();
  entities[0].set_ctime(syncer::TimeToProtoTime(now - base::Days(7)));
  entities[0].set_mtime(syncer::TimeToProtoTime(now - base::Days(6)));
  entities[1].set_ctime(syncer::TimeToProtoTime(now - base::Days(5)));
  entities[1].set_mtime(syncer::TimeToProtoTime(now - base::Days(4)));
  entities[2].set_ctime(syncer::TimeToProtoTime(now - base::Days(3)));
  entities[2].set_mtime(syncer::TimeToProtoTime(now - base::Days(2)));
  fake_requests_[primary.gaia]->SimulateSuccess(entities);
  EXPECT_TRUE(future.Wait());

  // Since the activity time ranges were non-overlapping, the three DeviceInfos
  // should have been deduped into a single device.
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.PlatformOfAdditionalClient",
      DeviceStatisticsTracker::Platform::kWindows,
      /*expected_count=*/1);
}

}  // namespace

}  // namespace syncer
