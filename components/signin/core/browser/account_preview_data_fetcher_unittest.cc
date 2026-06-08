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
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {
constexpr char kFetchStateHistogram[] = "Signin.AccountPreviewData.FetchState";
}  // namespace

using FetchState = AccountPreviewDataFetcher::FetchState;

class AccountPreviewDataFetcherTest : public testing::Test {
 public:
  AccountPreviewDataFetcherTest() = default;

  void SetUp() override {
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableAccountPreviewData};
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  IdentityTestEnvironment identity_test_env_{&test_url_loader_factory_};
  base::HistogramTester histogram_tester_;
};

TEST_F(AccountPreviewDataFetcherTest, Success) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 10, .password_count = 20, .history_count = 30});
  MockSuccessfulPreviewsFetch(&test_url_loader_factory_,
                              {"google.com", "yahoo.com"});

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(10U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(20U, result_data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(30U, result_data->counts[syncer::HISTORY]);
  ASSERT_EQ(2U, result_data->password_domains.size());
  EXPECT_EQ("google.com", result_data->password_domains[0]);
  EXPECT_EQ("yahoo.com", result_data->password_domains[1]);

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

TEST_F(AccountPreviewDataFetcherTest, SuccessEmpty) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_);
  MockSuccessfulPreviewsFetch(&test_url_loader_factory_);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  ASSERT_TRUE(result_data.has_value());
  EXPECT_EQ(0U, result_data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(0U, result_data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(0U, result_data->counts[syncer::HISTORY]);
  EXPECT_TRUE(result_data->password_domains.empty());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

TEST_F(AccountPreviewDataFetcherTest, AccessTokenFailure) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  // Disable automatic token issuance and reject the request.
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info.account_id,
      GoogleServiceAuthError::FromServiceError("Service error"));

  ASSERT_TRUE(future.IsReady());
  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 0);
}

TEST_F(AccountPreviewDataFetcherTest, StatsFailure) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockSuccessfulPreviewsFetch(&test_url_loader_factory_);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsEmptyResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 0);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithoutResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

TEST_F(AccountPreviewDataFetcherTest, PreviewsFailure) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewEmptyResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 0);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithoutResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

TEST_F(AccountPreviewDataFetcherTest, StatsInvalidJson) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  test_url_loader_factory_.AddResponse(kTestStatsUrl, "{ invalid json }");
  MockSuccessfulPreviewsFetch(&test_url_loader_factory_);

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithoutResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

TEST_F(AccountPreviewDataFetcherTest, PreviewsInvalidJson) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulStatsFetch(&test_url_loader_factory_);
  test_url_loader_factory_.AddResponse(kTestPreviewsUrl, "{ invalid json }");

  base::test::TestFuture<const GaiaId&, std::optional<AccountPreviewData>>
      future;
  auto fetcher = std::make_unique<AccountPreviewDataFetcher>(
      account_info.gaia, identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      version_info::Channel::UNKNOWN, future.GetCallback());

  auto [gaia_id, result_data] = future.Take();
  EXPECT_EQ(account_info.gaia, gaia_id);
  EXPECT_FALSE(result_data.has_value());

  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kRequested, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kStatisticsHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kEntityPreviewHasResult, 1);
  histogram_tester_.ExpectBucketCount(kFetchStateHistogram,
                                      FetchState::kCompletedWithoutResults, 1);
  histogram_tester_.ExpectTotalCount(kFetchStateHistogram, 4);
}

}  // namespace signin
