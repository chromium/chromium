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

  sync_pb::SyncEnums_DeviceFormFactor GetDefaultFormFactor(
      sync_pb::SyncEnums_OsType os_type) const {
    switch (os_type) {
      case sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS:
      case sync_pb::SyncEnums_OsType_OS_TYPE_MAC:
      case sync_pb::SyncEnums_OsType_OS_TYPE_LINUX:
      case sync_pb::SyncEnums_OsType_OS_TYPE_CHROME_OS_ASH:
        return sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;
      case sync_pb::SyncEnums_OsType_OS_TYPE_ANDROID:
      case sync_pb::SyncEnums_OsType_OS_TYPE_IOS:
        return sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;
      case sync_pb::SyncEnums_OsType_OS_TYPE_CHROME_OS_LACROS:
      case sync_pb::SyncEnums_OsType_OS_TYPE_FUCHSIA:
      case sync_pb::SyncEnums_OsType_OS_TYPE_UNSPECIFIED:
        return sync_pb::
            SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED;
    }
    NOTREACHED();
  }

  sync_pb::SyncEntity CreateDeviceInfo(
      std::string_view cache_guid,
      sync_pb::SyncEnums_OsType platform,
      bool history_opt_in,
      std::optional<sync_pb::SyncEnums_DeviceFormFactor> form_factor =
          std::nullopt) {
    const base::Time now = base::Time::Now();

    sync_pb::SyncEntity entity;
    entity.set_ctime(syncer::TimeToProtoTime(now - base::Days(7)));
    entity.set_mtime(syncer::TimeToProtoTime(now - base::Days(1)));

    sync_pb::DeviceInfoSpecifics& device =
        *entity.mutable_specifics()->mutable_device_info();
    device.set_cache_guid(cache_guid);
    device.set_os_type(platform);
    device.set_device_form_factor(
        form_factor.value_or(GetDefaultFormFactor(platform)));
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
    std::vector<sync_pb::SyncEnums_OsType> platforms(history_opt_ins.size(),
                                                     GetOtherOsType());
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

  sync_pb::SyncEnums_OsType GetLocalOsType() const {
#if BUILDFLAG(IS_WIN)
    return sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS;
#elif BUILDFLAG(IS_MAC)
    return sync_pb::SyncEnums_OsType_OS_TYPE_MAC;
#elif BUILDFLAG(IS_LINUX)
    return sync_pb::SyncEnums_OsType_OS_TYPE_LINUX;
#elif BUILDFLAG(IS_CHROMEOS)
    return sync_pb::SyncEnums_OsType_OS_TYPE_CHROME_OS_ASH;
#elif BUILDFLAG(IS_ANDROID)
    return sync_pb::SyncEnums_OsType_OS_TYPE_ANDROID;
#elif BUILDFLAG(IS_IOS)
    return sync_pb::SyncEnums_OsType_OS_TYPE_IOS;
#else
    return sync_pb::SyncEnums_OsType_OS_TYPE_UNSPECIFIED;
#endif
  }

  sync_pb::SyncEnums_OsType GetOtherOsType() const {
#if BUILDFLAG(IS_LINUX)
    return sync_pb::SyncEnums_OsType_OS_TYPE_MAC;
#else
    return sync_pb::SyncEnums_OsType_OS_TYPE_LINUX;
#endif
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
  histogram_tester.ExpectTotalCount("Sync.DeviceStatistics.Outcome.Overall2",
                                    0);
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
  histogram_tester.ExpectTotalCount("Sync.DeviceStatistics.Outcome.Overall2",
                                    0);
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
  histogram_tester.ExpectTotalCount("Sync.DeviceStatistics.Outcome.Overall2",
                                    0);
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
      "Sync.DeviceStatistics.Outcome.Overall2",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryNA,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      /*sample=*/
      DeviceStatisticsTracker::MultiDeviceReadiness::kSingleDevice,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      /*sample=*/
      DeviceStatisticsTracker::MultiPlatformReadiness::kSinglePlatform,
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
      "Sync.DeviceStatistics.Outcome.Overall2",
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
      "Sync.DeviceStatistics.Outcome.Overall2",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNANonPrimaryNo,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      /*sample=*/
      DeviceStatisticsTracker::MultiDeviceReadiness::kSignedOut,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      /*sample=*/
      DeviceStatisticsTracker::MultiPlatformReadiness::kSignedOut,
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
      "Sync.DeviceStatistics.Outcome.Overall2",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNANonPrimaryNo,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      /*sample=*/
      DeviceStatisticsTracker::MultiDeviceReadiness::kSignedOut,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      /*sample=*/
      DeviceStatisticsTracker::MultiPlatformReadiness::kSignedOut,
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
      "Sync.DeviceStatistics.Outcome.Overall2",
      /*sample=*/
      DeviceStatisticsTracker::AccountsHaveOtherDevicesSummary::
          kPrimaryNoNonPrimaryYes,
      /*expected_bucket_count=*/1);

  // For both primary and non-primary account, the current device was not
  // counted.
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.NumberOfAdditionalClients2",
      /*sample=*/0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "NumberOfAdditionalClients2",
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
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.NumberOfAdditionalClients2",
      /*sample=*/3,
      /*expected_count=*/1);

  int primary_expected_platforms = 2;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  --primary_expected_platforms;
#endif
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalPlatforms2",
      /*sample=*/primary_expected_platforms,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformOfAdditionalClient2",
      DeviceStatisticsTracker::Platform::kWindows,
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kWindowsDesktop,
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformOfAdditionalClient2",
      DeviceStatisticsTracker::Platform::kMac,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kMacDesktop,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      DeviceStatisticsTracker::MultiDeviceReadiness::kMultiDeviceWithoutHistory,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      DeviceStatisticsTracker::MultiPlatformReadiness::
          kMultiPlatformWithoutHistory,
      /*expected_count=*/1);

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "NumberOfAdditionalClients2",
      /*sample=*/2,
      /*expected_count=*/1);

  int non_primary_expected_platforms = 2;
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_LINUX)
  --non_primary_expected_platforms;
#endif
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "NumberOfAdditionalPlatforms2",
      /*sample=*/non_primary_expected_platforms,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "PlatformOfAdditionalClient2",
      DeviceStatisticsTracker::Platform::kIOS,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kIOSPhone,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "PlatformOfAdditionalClient2",
      DeviceStatisticsTracker::Platform::kLinux,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.NonPrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kLinuxDesktop,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, RecordsMultiPlatformHistoryOptInMetrics) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(
      identity_test_env_.identity_manager(), GURL("https://example.com/"),
      CreateRequestFactory(), {kThisDeviceCacheGuid});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 1u);
  // Create 3 other devices:
  // - Windows, opted in
  // - Mac, not opted in
  // - Mac, opted in
  // Total other platforms: 2, or 1 if on Windows/Mac.
  // Total other platforms opted in: 2, or 1 if on Windows/Mac.
  std::vector<sync_pb::SyncEntity> device_infos =
      CreateDeviceInfos({sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
                         sync_pb::SyncEnums_OsType_OS_TYPE_MAC,
                         sync_pb::SyncEnums_OsType_OS_TYPE_MAC},
                        {true, false, true});
  // The local device is also opted in.
  device_infos.push_back(
      CreateDeviceInfo(kThisDeviceCacheGuid, GetLocalOsType(), true));

  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  int expected_other_platforms = 2;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  --expected_other_platforms;
#endif

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalPlatforms2",
      /*sample=*/expected_other_platforms,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalPlatformsWithHistoryOptIn2",
      /*sample=*/expected_other_platforms,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "HistoryOptInMultiPlatform2",
      DeviceStatisticsTracker::HistoryOptInPlatformsSummary::
          kThisPlatformYesOtherPlatformsYes,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      DeviceStatisticsTracker::MultiDeviceReadiness::kMultiDeviceWithHistory,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      DeviceStatisticsTracker::MultiPlatformReadiness::
          kMultiPlatformWithHistory,
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
  device_infos.push_back(
      CreateDeviceInfo(kThisDeviceCacheGuid, GetLocalOsType(), true));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients2",
      /*sample=*/3,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn2",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn2",
      DeviceStatisticsTracker::HistoryOptInDevicesSummary::
          kThisDeviceYesOtherDevicesYes,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      DeviceStatisticsTracker::MultiDeviceReadiness::kMultiDeviceWithHistory,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      DeviceStatisticsTracker::MultiPlatformReadiness::
          kMultiPlatformWithHistory,
      /*expected_count=*/1);
}

// This test doesn't work on Fuchsia, because Fuchsia is not among the platforms
// recognized/tracked by the DeviceStatisticsTracker.
#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(DeviceStatisticsTrackerTest,
       RecordsHistoryMetricsWhenThisPlatformButNotThisDeviceOptedIn) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(
      identity_test_env_.identity_manager(), GURL("https://example.com/"),
      CreateRequestFactory(), {kThisDeviceCacheGuid});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  // Three devices:
  // 1. Current device (local platform), not opted in.
  // 2. Another device on the SAME platform, opted in.
  // 3. Another device on a DIFFERENT platform, not opted in.
  std::vector<sync_pb::SyncEntity> device_infos;
  device_infos.push_back(
      CreateDeviceInfo(kThisDeviceCacheGuid, GetLocalOsType(), false));
  device_infos.push_back(
      CreateDeviceInfo("other_device_same_os", GetLocalOsType(), true));
  device_infos.push_back(
      CreateDeviceInfo("other_device_diff_os", GetOtherOsType(), false));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients2",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn2",
      /*sample=*/1,
      /*expected_count=*/1);

  // For DEVICES summary: The current device is NOT opted in, but another device
  // IS.
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn2",
      DeviceStatisticsTracker::HistoryOptInDevicesSummary::
          kThisDeviceNoOtherDevicesYes,
      /*expected_count=*/1);

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalPlatforms2",
      /*sample=*/1,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalPlatformsWithHistoryOptIn2",
      /*sample=*/0,
      /*expected_count=*/1);

  // For PLATFORMS summary: A device on the local platform IS opted in, but no
  // other platform is opted in.
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptInMultiPlatform2",
      DeviceStatisticsTracker::HistoryOptInPlatformsSummary::
          kThisPlatformYesOtherPlatformsNo,
      /*expected_count=*/1);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

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
  device_infos.push_back(
      CreateDeviceInfo(kThisDeviceCacheGuid, GetLocalOsType(), false));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients2",
      /*sample=*/3,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn2",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn2",
      DeviceStatisticsTracker::HistoryOptInDevicesSummary::
          kThisDeviceNoOtherDevicesYes,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      DeviceStatisticsTracker::MultiDeviceReadiness::kMultiDeviceWithoutHistory,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      DeviceStatisticsTracker::MultiPlatformReadiness::
          kMultiPlatformWithoutHistory,
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
  device_infos.push_back(
      CreateDeviceInfo(kThisDeviceCacheGuid, GetLocalOsType(), false));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClients2",
      /*sample=*/2,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "NumberOfAdditionalClientsWithHistoryOptIn2",
      /*sample=*/0,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.HistoryOptIn2",
      DeviceStatisticsTracker::HistoryOptInDevicesSummary::
          kThisDeviceNoOtherDevicesNo,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiDeviceReadiness2",
      DeviceStatisticsTracker::MultiDeviceReadiness::kMultiDeviceWithoutHistory,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.MultiPlatformReadiness2",
      DeviceStatisticsTracker::MultiPlatformReadiness::
          kMultiPlatformWithoutHistory,
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
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformOfAdditionalClient2",
      DeviceStatisticsTracker::Platform::kWindows,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, ExcludesIGSADevices) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  ASSERT_EQ(fake_requests_.size(), 1u);
  std::vector<sync_pb::SyncEntity> entities =
      CreateDeviceInfosWithPlatforms({sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
                                      sync_pb::SyncEnums_OsType_OS_TYPE_IOS});
  // One device has an "iGSA" user agent.
  entities[1].mutable_specifics()->mutable_device_info()->set_sync_user_agent(
      "iGSA IOS-PHONE 145.0.7632.153 (007368903b9211f2773672f5072b67f9b2afc409-"
      "refs/branch-heads/7632@{#3240}) channel(stable)");
  fake_requests_[primary.gaia]->SimulateSuccess(entities);
  EXPECT_TRUE(future.Wait());

  // Only the first device should have been counted.
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount.NumberOfAdditionalClients2",
      /*sample=*/1,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformOfAdditionalClient2",
      DeviceStatisticsTracker::Platform::kWindows,
      /*expected_count=*/1);
}

TEST_F(DeviceStatisticsTrackerTest, RecordsPlatformAndFormFactorMetrics) {
  AccountInfo primary = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  base::HistogramTester histogram_tester;

  DeviceStatisticsTracker tracker(identity_test_env_.identity_manager(),
                                  GURL("https://example.com/"),
                                  CreateRequestFactory(), {"test_guid"});

  base::test::TestFuture<void> future;
  tracker.Start(future.GetCallback());

  std::vector<sync_pb::SyncEntity> device_infos;

  // Add a Windows Desktop device.
  device_infos.push_back(
      CreateDeviceInfo("guid1", sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
                       false, sync_pb::SyncEnums::DEVICE_FORM_FACTOR_DESKTOP));

  // Add an Android Tablet device.
  device_infos.push_back(
      CreateDeviceInfo("guid2", sync_pb::SyncEnums_OsType_OS_TYPE_ANDROID,
                       false, sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TABLET));

  // Add a ChromeOS Tablet device.
  device_infos.push_back(
      CreateDeviceInfo("guid4", sync_pb::SyncEnums_OsType_OS_TYPE_CHROME_OS_ASH,
                       false, sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TABLET));

  // Add an iOS Phone device.
  device_infos.push_back(
      CreateDeviceInfo("guid5", sync_pb::SyncEnums_OsType_OS_TYPE_IOS, false,
                       sync_pb::SyncEnums::DEVICE_FORM_FACTOR_PHONE));

  // Add a Linux "Other" device (e.g. TV).
  device_infos.push_back(
      CreateDeviceInfo("guid6", sync_pb::SyncEnums_OsType_OS_TYPE_LINUX, false,
                       sync_pb::SyncEnums::DEVICE_FORM_FACTOR_TV));

  ASSERT_EQ(fake_requests_.size(), 1u);
  fake_requests_[primary.gaia]->SimulateSuccess(device_infos);
  EXPECT_TRUE(future.Wait());

  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kWindowsDesktop,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kAndroidTablet,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kChromeOSTablet,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kIOSPhone,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Sync.DeviceStatistics.Outcome.PrimaryAccount."
      "PlatformAndFormFactorOfAdditionalClient2",
      DeviceStatisticsTracker::PlatformAndFormFactor::kLinuxOther,
      /*expected_count=*/1);
}

}  // namespace

}  // namespace syncer
