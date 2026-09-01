// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version_info/channel.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_test_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {
constexpr char kFetchStateHistogram[] = "Signin.AccountPreviewData.FetchState";
constexpr char kFetchHit429Histogram[] =
    "Signin.AccountPreviewData.FetchHit429";
constexpr char kFetchDurationSuccessHistogram[] =
    "Signin.AccountPreviewData.FetchDuration.Success";
constexpr char kFetchDurationFailureHistogram[] =
    "Signin.AccountPreviewData.FetchDuration.Failure";
constexpr char kFetchDurationTokenFailureHistogram[] =
    "Signin.AccountPreviewData.FetchDuration.TokenFailure";
}  // namespace

using FetchState = AccountPreviewDataFetcher::FetchState;

class AccountPreviewDataFetcherTest : public testing::Test {
 public:
  AccountPreviewDataFetcherTest() {
    feature_list_.InitWithFeatures(
        {switches::kEnableAccountPreviewData,
         switches::kEnableAccountPreviewEntityPreviews},
        {});
  }

  void SetUp() override {
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  IdentityTestEnvironment identity_test_env_{&test_url_loader_factory_};
  base::HistogramTester histogram_tester_;
};

TEST_F(AccountPreviewDataFetcherTest, Success) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  std::vector<DevicePreview> expected_devices = {
      {.cache_guid = "device_1",
       .last_updated = syncer::ProtoTimeToTime(123456789),
       .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
       .form_factor =
           sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP},
      {.cache_guid = "device_2",
       .last_updated = syncer::ProtoTimeToTime(987654321),
       .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_LINUX,
       .form_factor =
           sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP}};

  MockSuccessfulStatsFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 10, .password_count = 20, .history_count = 30});
  MockSuccessfulPreviewsFetch(&test_url_loader_factory_, expected_devices);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(10U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(20U, result_data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(30U, result_data->counts[syncer::HISTORY]);
  EXPECT_EQ(result_data->devices, expected_devices);

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, SuccessWithPreviewsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      switches::kEnableAccountPreviewEntityPreviews);

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 10, .password_count = 20, .history_count = 30});
  // We do NOT mock previews fetch.

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(10U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(20U, result_data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(30U, result_data->counts[syncer::HISTORY]);
  EXPECT_TRUE(result_data->devices.empty());

  // Verify that no previews request was even initiated (0 pending requests).
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 0);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 3);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, SuccessEmpty) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_);
  MockSuccessfulPreviewsFetch(&test_url_loader_factory_);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(0U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(0U, result_data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(0U, result_data->counts[syncer::HISTORY]);
  EXPECT_TRUE(result_data->devices.empty());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
  histogram_tester_.ExpectBucketCount(kFetchHit429Histogram, false, 1);
}

TEST_F(AccountPreviewDataFetcherTest, AccessTokenFailure) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  // Disable automatic token issuance and reject the request.
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info.GetAccountId(),
      GoogleServiceAuthError::FromServiceError("Service error"));

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationTokenFailureHistogram, 1);
}

TEST_F(AccountPreviewDataFetcherTest, StatsFailure) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockSuccessfulPreviewsFetch(
      &test_url_loader_factory_,
      {{.cache_guid = "device_1",
        .last_updated = syncer::ProtoTimeToTime(123456789),
        .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
        .form_factor =
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP}});

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_TRUE(result_data->counts.empty());
  ASSERT_EQ(1U, result_data->devices.size());
  EXPECT_EQ("device_1", result_data->devices[0].cache_guid);

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsEmptyResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

TEST_F(AccountPreviewDataFetcherTest, PreviewsFailure) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_, {.bookmark_count = 5});
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(5U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_TRUE(result_data->devices.empty());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewEmptyResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, StatsInvalidJson) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  test_url_loader_factory_.AddResponse(GetTestStatsUrl(), "{ invalid json }");
  MockSuccessfulPreviewsFetch(
      &test_url_loader_factory_,
      {{.cache_guid = "device_1",
        .last_updated = syncer::ProtoTimeToTime(123456789),
        .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
        .form_factor =
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP}});

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_TRUE(result_data->counts.empty());
  ASSERT_EQ(1U, result_data->devices.size());
  EXPECT_EQ("device_1", result_data->devices[0].cache_guid);

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, PreviewsInvalidJson) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_, {.password_count = 10});
  test_url_loader_factory_.AddResponse(GetTestPreviewsUrl(),
                                       "invalid json string");

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(10U, result_data->counts[syncer::PASSWORDS]);
  EXPECT_TRUE(result_data->devices.empty());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, BothRequestsFail) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsEmptyResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewEmptyResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithoutResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 1);
}

TEST_F(AccountPreviewDataFetcherTest, InvalidAccount) {
  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      GaiaId("invalid_gaia_id"), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(GaiaId("invalid_gaia_id"), gaia_id);
  EXPECT_FALSE(hit_429);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationSuccessHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationFailureHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFetchDurationTokenFailureHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, PreviewsInvalidCacheGuid) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_);

  std::string response_json = R"({
    "entitiesPreviews": [
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "lastUpdatedTimestamp": "123456789",
            "osType": 1,
            "deviceFormFactor": 1,
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      },
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": 12345,
            "lastUpdatedTimestamp": "123456789",
            "osType": 1,
            "deviceFormFactor": 1,
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      },
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "valid_device_guid",
            "lastUpdatedTimestamp": "123456789",
            "osType": 1,
            "deviceFormFactor": 1,
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      }
    ]
  })";
  test_url_loader_factory_.AddResponse(GetTestPreviewsUrl(), response_json);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  ASSERT_EQ(1U, result_data->devices.size());
  EXPECT_EQ("valid_device_guid", result_data->devices[0].cache_guid);
}

TEST_F(AccountPreviewDataFetcherTest, PreviewsInvalidFormFactorOrOsType) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_);

  std::string response_json = R"({
    "entitiesPreviews": [
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "device_1",
            "lastUpdatedTimestamp": "123456789",
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      }
    ]
  })";
  test_url_loader_factory_.AddResponse(GetTestPreviewsUrl(), response_json);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  ASSERT_EQ(1U, result_data->devices.size());
  EXPECT_EQ("device_1", result_data->devices[0].cache_guid);
  EXPECT_EQ(sync_pb::SyncEnums_OsType_OS_TYPE_UNSPECIFIED,
            result_data->devices[0].os_type);
  EXPECT_EQ(sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED,
            result_data->devices[0].form_factor);
}

TEST_F(AccountPreviewDataFetcherTest, FiltersNonChromeDevices) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_, {.password_count = 5});

  std::string response_json = R"({
    "entitiesPreviews": [
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "chrome_device",
            "lastUpdatedTimestamp": "123456789",
            "osType": 1,
            "deviceFormFactor": 1,
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      },
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "igsa_device",
            "lastUpdatedTimestamp": "123456789",
            "osType": 6,
            "deviceFormFactor": 2,
            "syncUserAgent": "iGSA/1.0",
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      },
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "non_chrome_device",
            "lastUpdatedTimestamp": "123456789",
            "osType": 2,
            "deviceFormFactor": 2,
            "googlePlayServicesVersionInfo": {
              "apkVersionName": "24.16.13"
            }
          }
        }
      }
    ]
  })";

  test_url_loader_factory_.AddResponse(GetTestPreviewsUrl(), response_json);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  ASSERT_EQ(1U, result_data->devices.size());
  EXPECT_EQ("chrome_device", result_data->devices[0].cache_guid);
}

TEST_F(AccountPreviewDataFetcherTest, FiltersCurrentDevice) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_, {.password_count = 5});

  std::string response_json = R"({
    "entitiesPreviews": [
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "other_device",
            "lastUpdatedTimestamp": "123456789",
            "osType": 1,
            "deviceFormFactor": 1,
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      },
      {
        "specificsPreview": {
          "deviceInfoPreview": {
            "cacheGuid": "current_device",
            "lastUpdatedTimestamp": "123456789",
            "osType": 1,
            "deviceFormFactor": 1,
            "chromeVersionInfo": {
              "versionNumber": "126.0.0.0"
            }
          }
        }
      }
    ]
  })";

  test_url_loader_factory_.AddResponse(GetTestPreviewsUrl(), response_json);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/
      base::flat_set<std::string>{"current_device"}, future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_FALSE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  ASSERT_EQ(1U, result_data->devices.size());
  EXPECT_EQ("other_device", result_data->devices[0].cache_guid);
}

TEST_F(AccountPreviewDataFetcherTest, Stats429Error) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  Mock429StatsFetch(&test_url_loader_factory_);
  MockSuccessfulPreviewsFetch(
      &test_url_loader_factory_,
      {{.cache_guid = "device_1",
        .last_updated = syncer::ProtoTimeToTime(123456789),
        .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
        .form_factor =
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP}});

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_TRUE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_TRUE(result_data->counts.empty());
  ASSERT_EQ(1U, result_data->devices.size());
  histogram_tester_.ExpectBucketCount(kFetchHit429Histogram, true, 1);
}

TEST_F(AccountPreviewDataFetcherTest, Previews429Error) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_, {.bookmark_count = 5});
  Mock429PreviewsFetch(&test_url_loader_factory_);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_TRUE(hit_429);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(5U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_TRUE(result_data->devices.empty());
  histogram_tester_.ExpectBucketCount(kFetchHit429Histogram, true, 1);
}

TEST_F(AccountPreviewDataFetcherTest, Both429Error) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  Mock429Fetch(&test_url_loader_factory_);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>, bool>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.GetGaiaId(), identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN,
      /*current_device_cache_guids=*/base::flat_set<std::string>(),
      future.GetCallback());
  fetcher->Start();

  auto [gaia_id, result_data, hit_429] = future.Take();
  EXPECT_EQ(account_info.GetGaiaId(), gaia_id);
  EXPECT_TRUE(hit_429);
  EXPECT_FALSE(result_data.has_value());
  histogram_tester_.ExpectBucketCount(kFetchHit429Histogram, true, 1);
}

}  // namespace signin
