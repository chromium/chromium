// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version_info/channel.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_metrics_id_allocator.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/account_preview_data_test_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/data_type.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AccountPreviewDataServiceTest : public testing::Test {
 public:
  AccountPreviewDataServiceTest()
      : identity_test_env_(&test_url_loader_factory_) {}

  void SetUp() override {
    AccountPreviewDataService::RegisterProfilePrefs(prefs_.registry());
    SigninPrefs::RegisterProfilePrefs(prefs_.registry());
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
    network_delay_helper_ = helper.get();
    service_ = std::make_unique<AccountPreviewDataServiceImpl>(
        identity_test_env_.identity_manager(), &prefs_,
        test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
        version_info::Channel::UNKNOWN, &profile_metrics_service_);
  }

  void TearDown() override {
    network_delay_helper_ = nullptr;
    service_.reset();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableAccountPreviewData};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple prefs_;
  IdentityTestEnvironment identity_test_env_;
  metrics::ProfileMetricsService profile_metrics_service_;
  raw_ptr<TestWaitForNetworkCallbackHelper> network_delay_helper_ = nullptr;
  std::unique_ptr<AccountPreviewDataServiceImpl> service_;
};

TEST_F(AccountPreviewDataServiceTest, EmptyInitially) {
  GaiaId id("some-gaia-id");
  std::optional<AccountPreviewData> data = service_->GetAccountPreviewData(id);
  EXPECT_FALSE(data.has_value());
}

TEST_F(AccountPreviewDataServiceTest, FetchesForPrimaryAccount) {
  AccountInfo primary_info = identity_test_env_.MakePrimaryAccountAvailable(
      "primary@gmail.com", ConsentLevel::kSignin);

  MockSuccessfulFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 10, .password_count = 20, .history_count = 30},
      {"google.com", "yahoo.com"});

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // Simulating OnRefreshTokenUpdatedForAccount for primary account
  service_->OnRefreshTokenUpdatedForAccount(primary_info);
  run_loop.Run();

  // It should trigger fetcher and save to memory cache with correct data
  std::optional<AccountPreviewData> data =
      service_->GetAccountPreviewData(primary_info.gaia);
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(10U, data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(20U, data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(30U, data->counts[syncer::HISTORY]);
  ASSERT_EQ(2U, data->password_domains.size());
  EXPECT_EQ("google.com", data->password_domains[0]);
  EXPECT_EQ("yahoo.com", data->password_domains[1]);
}

TEST_F(AccountPreviewDataServiceTest, RemovesCachedData) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  service_->OnRefreshTokenUpdatedForAccount(account_info);
  run_loop.Run();
  ASSERT_TRUE(service_->GetAccountPreviewData(account_info.gaia).has_value());

  service_->OnRefreshTokenRemovedForAccount(account_info.account_id);

  EXPECT_FALSE(service_->GetAccountPreviewData(account_info.gaia).has_value());
}

TEST_F(AccountPreviewDataServiceTest, PeriodicRefreshDefersUntilTokensLoaded) {
  // Destroy the service created in SetUp to prevent it from fetching when we
  // make the account available.
  network_delay_helper_ = nullptr;
  service_.reset();

  // Make an account available.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  // Simulate tokens not loaded yet.
  identity_test_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Clear the timer last update pref so that the recreated service's timer
  // fires immediately on startup.
  prefs_.ClearPref(prefs::kAccountPreviewDataLastUpdatePref);

  // Re-create the service. It will try to refresh on startup, but it should
  // defer because tokens are not loaded.
  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &prefs_,
      test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  // Verify that it did NOT fetch yet.
  EXPECT_FALSE(service_->GetAccountPreviewData(account_info.gaia).has_value());

  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // Simulate tokens loaded. This should trigger the deferred refresh.
  identity_test_env_.ReloadAccountsFromDisk();
  run_loop.Run();

  // Verify that it HAS fetched now.
  EXPECT_TRUE(service_->GetAccountPreviewData(account_info.gaia).has_value());
}

TEST_F(AccountPreviewDataServiceTest, NoFetchOnStartupIfTimerNotExpired) {
  // Destroy the service created in SetUp.
  network_delay_helper_ = nullptr;
  service_.reset();

  // Make an account available.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  // Simulate tokens not loaded yet.
  identity_test_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Set the timer last update pref to now, so the timer does NOT fire.
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  // Re-create the service.
  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &prefs_,
      test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  // Verify that it did NOT fetch yet.
  EXPECT_FALSE(service_->GetAccountPreviewData(account_info.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.gaia));

  // Mock successful fetch in case a fetch is incorrectly started.
  MockSuccessfulFetch(&test_url_loader_factory_);

  // Simulate tokens loaded.
  identity_test_env_.ReloadAccountsFromDisk();
  EXPECT_TRUE(identity_test_env_.identity_manager()->AreRefreshTokensLoaded());
  EXPECT_TRUE(identity_test_env_.identity_manager()->HasAccountWithRefreshToken(
      account_info.account_id));

  // Verify that it still did NOT fetch because the timer didn't fire and we
  // shouldn't fetch on startup token loading.
  EXPECT_FALSE(service_->GetAccountPreviewData(account_info.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.gaia));
}

TEST_F(AccountPreviewDataServiceTest, ClearsInvalidDataOnCookieUpdate) {
  // 1. Setup: Make two accounts available.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  // Mock successful fetches for both accounts.
  MockSuccessfulFetch(&test_url_loader_factory_);

  // Trigger fetches and wait for completion.
  {
    base::RunLoop run_loop;
    service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
    service_->OnRefreshTokenUpdatedForAccount(account1);
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
    service_->OnRefreshTokenUpdatedForAccount(account2);
    run_loop.Run();
  }

  // Verify both are present in cache.
  ASSERT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  ASSERT_TRUE(service_->GetAccountPreviewData(account2.gaia).has_value());

  // 2. Trigger: Set cookies to only contain account1 (removing account2).
  identity_test_env_.SetCookieAccounts({{account1.email, account1.gaia}});

  // 3. Assert: account2's data should be removed, account1's should remain.
  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_FALSE(service_->GetAccountPreviewData(account2.gaia).has_value());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AccountPreviewDataServiceTest,
       ClearsInvalidDataOnPrimaryAccountCleared) {
  // 1. Setup: Make a primary account available.
  AccountInfo primary_info = identity_test_env_.MakePrimaryAccountAvailable(
      "primary@gmail.com", ConsentLevel::kSignin);

  // Mock successful fetch.
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // Trigger fetch and wait for completion.
  service_->OnRefreshTokenUpdatedForAccount(primary_info);
  run_loop.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(primary_info.gaia).has_value());

  // 2. Trigger: Clear the primary account.
  identity_test_env_.ClearPrimaryAccount();

  // 3. Assert: Its data should be removed.
  EXPECT_FALSE(service_->GetAccountPreviewData(primary_info.gaia).has_value());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AccountPreviewDataServiceTest, QueuesFetchWhenOffline) {
  // 1. Start offline (network calls delayed).
  network_delay_helper_->SetNetworkCallsDelayed(true);

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  // Trigger fetch while offline.
  service_->OnRefreshTokenUpdatedForAccount(account_info);

  // Assert: No active fetcher was started.
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.gaia));

  // Mock successful fetch for when we go online.
  MockSuccessfulFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 5, .password_count = 10, .history_count = 15},
      {"example.com"});

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // 2. Go online. This should trigger the queued fetch.
  network_delay_helper_->SetNetworkCallsDelayed(false);
  run_loop.Run();

  // Assert: The queued fetch completed successfully and data was stored.
  std::optional<AccountPreviewData> data =
      service_->GetAccountPreviewData(account_info.gaia);
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(5U, data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(10U, data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(15U, data->counts[syncer::HISTORY]);
}

}  // namespace signin
