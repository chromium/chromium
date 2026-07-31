// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
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
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AllDataAvailableWaiter {
 public:
  explicit AllDataAvailableWaiter(AccountPreviewDataServiceImpl* service)
      : service_(service) {
    service_->SetAllDataAvailableCallbackForTesting(base::BindOnce(
        &AllDataAvailableWaiter::OnAllDataAvailable, base::Unretained(this)));
  }

  ~AllDataAvailableWaiter() {
    // Clears the callback to avoid any unexpected callback after the waiter is
    // destroyed.
    service_->SetAllDataAvailableCallbackForTesting(base::OnceClosure());
  }

  bool is_all_data_available() const { return is_all_data_available_; }

  void Wait() {
    if (is_all_data_available_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  void OnAllDataAvailable() {
    is_all_data_available_ = true;
    run_loop_.Quit();
  }

  raw_ptr<AccountPreviewDataServiceImpl> service_;
  bool is_all_data_available_ = false;
  base::RunLoop run_loop_;
};

class AccountPreviewDataServiceTest : public testing::Test {
 public:
  AccountPreviewDataServiceTest()
      : identity_test_env_(&test_url_loader_factory_) {
    feature_list_.InitWithFeatures(
        {switches::kEnableAccountPreviewData,
         switches::kEnableAccountPreviewEntityPreviews,
         switches::kEnableAccountPreviewPreferredAccount},
        {});
  }

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
  base::test::ScopedFeatureList feature_list_;
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

TEST_F(AccountPreviewDataServiceTest,
       RemovesCachedDataAfterRefreshTokenRemoved) {
  MockSuccessfulFetch(&test_url_loader_factory_);

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(service_->GetAccountPreviewData(account_info.gaia).has_value());

  // Simulate IdentityManager completely forgetting about the account.
  // We can do this by removing it from IdentityTestEnv (which normally triggers
  // OnRefreshTokenRemovedForAccount automatically, but we can also check that
  // when the callback runs, the cache gets cleared).
  identity_test_env_.RemoveRefreshTokenForAccount(account_info.account_id);

  // Since RemoveRefreshTokenForAccount triggers the callback automatically,
  // the cache should have been cleared.
  EXPECT_FALSE(service_->GetAccountPreviewData(account_info.gaia).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       AddingAccountDoesNotTriggerRefreshForCachedAccount) {
  // Mock response and set callback for account1 fetch.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());

  // Make account1 available. This starts fetch for account1.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop.Run();

  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());

  // Make account2 available.
  // We do NOT mock any response for account1 (and it should not fetch it
  // anyway). We mock success for account2.
  MockSuccessfulFetch(&test_url_loader_factory_);

  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  // Fetch should only be triggered for the new account2, not the cached
  // account1.
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.gaia));
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.gaia));
}

TEST_F(AccountPreviewDataServiceTest,
       AddingAccountTriggersRefreshForUncachedAccount) {
  // 1. Mock fetch failure and set callback.
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());

  // Make account1 available so it starts fetch and fails.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop.Run();

  EXPECT_FALSE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.gaia));

  // 2. Make account2 available (triggers EnsureAllAccountsFetched()).
  // This should trigger fetch for both account1 and account2.
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account1.gaia));
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.gaia));
}

TEST_F(AccountPreviewDataServiceTest,
       RemovingAccountDoesNotTriggerRefreshForCachedAccount) {
  // Mock successful response and set callback for first fetch.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());

  // Make account1 available (starts fetch for account1).
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop1.Run();

  // Mock successful response and set callback for second fetch.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());

  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop2.Run();

  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_TRUE(service_->GetAccountPreviewData(account2.gaia).has_value());

  // Remove account1. This triggers EnsureAllAccountsFetched().
  identity_test_env_.RemoveRefreshTokenForAccount(account1.account_id);

  // account1 is cleared. No fetches should start for the remaining cached
  // account2.
  EXPECT_FALSE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.gaia));
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.gaia));
}

TEST_F(AccountPreviewDataServiceTest,
       RemovingAccountTriggersRefreshForUncachedAccount) {
  // 1. Make account1 available, fetch and cache it successfully.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop1.Run();
  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());

  // 2. Mock failure responses, set callback, and make account2 available.
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());

  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop2.Run();

  EXPECT_FALSE(service_->GetAccountPreviewData(account2.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.gaia));

  // Manually set account1 as the preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.gaia.ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // 3. Remove account1. This triggers EnsureAllAccountsFetched().
  identity_test_env_.RemoveRefreshTokenForAccount(account1.account_id);

  // account1 is cleared. Fetch should start for the remaining uncached
  // account2.
  EXPECT_FALSE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.gaia));
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.gaia));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(AccountPreviewDataServiceTest, OnAllFetchesCompleted) {
  AllDataAvailableWaiter waiter(service_.get());

  // 1. Make both accounts available. This starts stats fetch for both.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  // We should have 4 pending requests (2 for stats, 2 for previews).
  ASSERT_EQ(4, test_url_loader_factory_.NumPending());

  // 2. Resolve account1's fetch.
  base::RunLoop account1_completed_loop;
  service_->SetFetchCompleteCallbackForTesting(
      account1_completed_loop.QuitClosure());
  SimulateSuccessfulFetch(&test_url_loader_factory_);
  account1_completed_loop.Run();

  // account1 is cached, but account2 is still fetching (its stats fetch is
  // pending). The callback should NOT have been triggered.
  EXPECT_FALSE(waiter.is_all_data_available());
  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_FALSE(service_->GetAccountPreviewData(account2.gaia).has_value());

  // 3. Resolve account2's fetch.
  SimulateSuccessfulFetch(&test_url_loader_factory_);

  // Run the loop. Now that both fetches are completed, the callback should
  // trigger and quit the loop.
  waiter.Wait();
  EXPECT_TRUE(waiter.is_all_data_available());
  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_TRUE(service_->GetAccountPreviewData(account2.gaia).has_value());
}
#endif

TEST_F(AccountPreviewDataServiceTest, GetPreferredAccountForPromo) {
  // 1. Initially empty.
  {
    std::optional<AccountPreviewDataService::AccountPreviewPreference>
        preference = service_->GetPreferredAccountForPromo();
    EXPECT_FALSE(preference.has_value());
  }

  // Mock successful fetches.
  MockSuccessfulFetch(&test_url_loader_factory_);
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop all_data_available_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_data_available_loop.QuitClosure());

  // 2. Make accounts available.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  all_data_available_loop.Run();

  // 3. Verify it returns empty preference (since heuristic is to be
  // implemented).
  // TODO(crbug.com/530144650): When the heuristic is implemented, this test
  // should be updated to expect a non-empty preference.
  {
    std::optional<AccountPreviewDataService::AccountPreviewPreference>
        preference = service_->GetPreferredAccountForPromo();
    EXPECT_FALSE(preference.has_value());
  }
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

TEST_F(AccountPreviewDataServiceTest,
       DoesNotStartFetchIfAccountRemovedWhileWaitingForNetwork) {
  // 1. Start with network calls delayed so that StartFetch() is queued.
  network_delay_helper_->SetNetworkCallsDelayed(true);

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  service_->OnRefreshTokenUpdatedForAccount(account_info);

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.gaia));

  // 2. Remove the account while network calls are still delayed.
  identity_test_env_.RemoveRefreshTokenForAccount(account_info.account_id);

  // 3. Unblock network calls. The queued StartFetch() callback will run,
  // but should return early without starting a fetcher since the account was
  // removed.
  network_delay_helper_->SetNetworkCallsDelayed(false);

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.gaia));
  EXPECT_FALSE(service_->GetAccountPreviewData(account_info.gaia).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       RemovingAccountDuringActiveFetchCompletesBarrier) {
  // Make account1 and account2 available. This starts active fetches for both,
  // but we do NOT provide mock network responses yet, so they remain active.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account1.gaia));
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account2.gaia));

  // Set up a waiter for all fetches completed.
  base::RunLoop all_fetches_run_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_fetches_run_loop.QuitClosure());

  // Remove account1 while its fetch is still active.
  // This should run the barrier once, and erase the fetcher for account1.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.account_id);

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.gaia));

  // Since account1 was removed, mocking a successful fetch would trigger the
  // fetch for account2.
  MockSuccessfulFetch(&test_url_loader_factory_);

  // Wait for all data available (which triggers when the barrier runs to
  // completion).
  all_fetches_run_loop.Run();

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.gaia));
}

TEST_F(AccountPreviewDataServiceTest, RegularFetchOfAllAccountsResetsTimer) {
  // Pre-set the timer last update pref to a past time (e.g. 5 hours ago).
  base::Time past_time = base::Time::Now() - base::Hours(5);
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, past_time);

  // Trigger a fetch for the only available account.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  service_->OnRefreshTokenUpdatedForAccount(account_info);
  run_loop.Run();

  // Verify that the last update pref has been updated to the current time
  // because we fetched 1/1 accounts (all accounts).
  base::Time current_time =
      prefs_.GetTime(prefs::kAccountPreviewDataLastUpdatePref);
  EXPECT_GT(current_time, past_time);
  EXPECT_EQ(current_time, base::Time::Now());
}

TEST_F(AccountPreviewDataServiceTest,
       RegularFetchOfSubsetOfAccountsDoesNotResetTimer) {
  // Make account1 available and cached first, so it won't be refetched.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  MockSuccessfulFetch(&test_url_loader_factory_);

  {
    base::RunLoop run_loop;
    service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
    service_->OnRefreshTokenUpdatedForAccount(account1);
    run_loop.Run();
  }

  // Pre-set the timer last update pref to a past time (e.g. 5 hours ago).
  base::Time past_time = base::Time::Now() - base::Hours(5);
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, past_time);

  // Trigger a fetch for account2.
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  MockSuccessfulFetch(&test_url_loader_factory_);

  {
    base::RunLoop run_loop;
    service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
    service_->OnRefreshTokenUpdatedForAccount(account2);
    run_loop.Run();
  }

  // Verify that the last update pref was NOT updated because we only fetched
  // 1/2 accounts (subset of accounts).
  base::Time current_time =
      prefs_.GetTime(prefs::kAccountPreviewDataLastUpdatePref);
  EXPECT_EQ(current_time, past_time);
}

TEST_F(AccountPreviewDataServiceTest,
       RemovingNonPreferredAccountDoesNotTriggerRefresh) {
  // 1. Make account2 available and cache it successfully.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop2.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(account2.gaia).has_value());

  // 2. Make account1 available, and fail its fetch (so it remains uncached).
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop1.Run();

  ASSERT_FALSE(service_->GetAccountPreviewData(account1.gaia).has_value());

  // Manually set account1 as the preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.gaia.ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Verify that account1 is indeed preferred.
  std::optional<AccountPreviewDataService::AccountPreviewPreference>
      preferred_account = service_->GetPreferredAccountForPromo();
  ASSERT_TRUE(preferred_account.has_value());
  ASSERT_EQ(preferred_account->gaia_id, account1.gaia);

  // Now remove account2 (which is NOT the preferred account).
  // Because it is not the preferred account, OnRefreshTokenRemovedForAccount
  // should NOT trigger a new fetch cycle (i.e. it should NOT call
  // EnsureAllAccountsFetched()).
  // We verify this by asserting that no active fetcher is started for the
  // uncached account1.
  identity_test_env_.RemoveRefreshTokenForAccount(account2.account_id);

  EXPECT_FALSE(service_->GetAccountPreviewData(account2.gaia).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.gaia));
  EXPECT_FALSE(service_->GetAccountPreviewData(account1.gaia).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       DoesNotComputePreferredAccountWhenFeatureDisabled) {
  // Disable preferred account computation feature flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kEnableAccountPreviewPreferredAccount);

  // Force write a fake GAIA ID in prefs that is not present in signed-in
  // accounts.
  const GaiaId kFakeGaiaId("fake_preferred_gaia_id");
  base::DictValue dict;
  dict.Set("gaia_id", kFakeGaiaId.ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Sign in and trigger fetch with a different account.
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetAllDataAvailableCallbackForTesting(run_loop.QuitClosure());
  service_->OnRefreshTokenUpdatedForAccount(account);
  run_loop.Run();

  // Verify that the preferred account in prefs was NOT overwritten or
  // recomputed by OnAllFetchesCompleted() because the feature flag is disabled.
  std::optional<AccountPreviewDataService::AccountPreviewPreference>
      preference = service_->GetPreferredAccountForPromo();
  ASSERT_TRUE(preference.has_value());
  EXPECT_EQ(kFakeGaiaId, preference->gaia_id);
}

TEST_F(AccountPreviewDataServiceTest, ReadPreviewPreferenceFromPrefsDataTypes) {
  base::DictValue dict;
  dict.Set("gaia_id", "test_gaia_id");
  base::ListValue data_types_list;
  data_types_list.Append(syncer::DataTypeToStableIdentifier(syncer::BOOKMARKS));
  data_types_list.Append(-1);    // Negative invalid value.
  data_types_list.Append(9999);  // Unknown/invalid stable identifier.
  data_types_list.Append(syncer::DataTypeToStableIdentifier(syncer::PASSWORDS));
  dict.Set("data_types", std::move(data_types_list));

  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  std::optional<AccountPreviewDataService::AccountPreviewPreference>
      preference = service_->GetPreferredAccountForPromo();
  ASSERT_TRUE(preference.has_value());
  EXPECT_EQ(GaiaId("test_gaia_id"), preference->gaia_id);
  std::vector<syncer::DataType> expected_types = {syncer::BOOKMARKS,
                                                  syncer::PASSWORDS};
  EXPECT_EQ(expected_types, preference->preferred_data_types);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(AccountPreviewDataServiceTest, LogsFetchTriggerCause) {
  base::HistogramTester histograms;

  // 1. Trigger cause by token update (sign in / update).
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  run_loop1.Run();

  histograms.ExpectUniqueSample(
      "Signin.AccountPreview.AllFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenUpdated,
      1);
  histograms.ExpectUniqueSample(
      "Signin.AccountPreview.SuccessfulFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenUpdated,
      1);

  // 2. Trigger cause by token removal (sign out / removal).
  // Make account2 available and fail its fetch so it remains uncached.
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop_fail;
  service_->SetFetchCompleteCallbackForTesting(run_loop_fail.QuitClosure());
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop_fail.Run();

  // Manually set account_info as preferred account so removing it triggers a
  // refresh.
  base::DictValue dict;
  dict.Set("gaia_id", account_info.gaia.ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());
  identity_test_env_.RemoveRefreshTokenForAccount(account_info.account_id);
  run_loop2.Run();

  histograms.ExpectBucketCount(
      "Signin.AccountPreview.AllFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenRemoved,
      1);
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.SuccessfulFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenRemoved,
      1);

  // 3. Trigger cause by periodic refresh.
  // Fast forward the time by 24 hours.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop3;
  service_->SetFetchCompleteCallbackForTesting(run_loop3.QuitClosure());
  task_environment_.FastForwardBy(base::Hours(24));
  run_loop3.Run();

  histograms.ExpectBucketCount(
      "Signin.AccountPreview.AllFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kPeriodicRefresh, 1);
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.SuccessfulFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kPeriodicRefresh, 1);
}

TEST_F(AccountPreviewDataServiceTest, LogsTriggerCauseWithAllCachesAvailable) {
  base::HistogramTester histograms;

  // Make account available and cache it.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  run_loop.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(account_info.gaia).has_value());

  // Trigger a new update for the same account. Since it's already cached,
  // gaia_ids_to_fetch will be empty (all caches available).
  identity_test_env_.SetRefreshTokenForAccount(account_info.account_id);

  histograms.ExpectUniqueSample(
      "Signin.AccountPreview.TriggerCauseWithAllCachesAvailable",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenUpdated,
      1);
}
#endif

TEST_F(AccountPreviewDataServiceTest, LogsPercentAccountsToFetch) {
  base::HistogramTester histograms;

  // Make account1 available and cached first.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop1.Run();

  // Now make account2 available.
  // This starts a fetch. There are 2 accounts total (account1 and account2).
  // Only account2 needs to be fetched (since account1 is cached).
  // So percent accounts fetched is 1/2 = 50%.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop2.Run();

  // The first fetch was 1/1 = 100%. The second fetch was 1/2 = 50%.
  histograms.ExpectBucketCount("Signin.AccountPreview.PercentAccountsToFetch",
                               100, 1);
  histograms.ExpectBucketCount("Signin.AccountPreview.PercentAccountsToFetch",
                               50, 1);
}

TEST_F(AccountPreviewDataServiceTest,
       LogsNonPeriodicFetchesUntilNextPeriodicRefresh) {
  base::HistogramTester histograms;

  // 1. Initial state: count pref is 0.
  EXPECT_EQ(0,
            prefs_.GetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref));

  // 2. Perform 3 non-periodic fetches (making accounts available).
  for (int i = 0; i < 3; ++i) {
    MockSuccessfulFetch(&test_url_loader_factory_);
    base::RunLoop run_loop;
    service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
    identity_test_env_.MakeAccountAvailable(
        base::StrCat({"user", base::NumberToString(i), "@gmail.com"}));
    run_loop.Run();
  }

  // Count pref should be 3 now.
  EXPECT_EQ(3,
            prefs_.GetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref));

  // 3. Fast forward time by 24 hours to trigger periodic refresh.
  // Mock fetches for the 3 accounts since periodic refresh clears cache and
  // refetches them all.
  MockSuccessfulFetch(&test_url_loader_factory_);
  MockSuccessfulFetch(&test_url_loader_factory_);
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop_periodic;
  service_->SetAllDataAvailableCallbackForTesting(
      run_loop_periodic.QuitClosure());
  task_environment_.FastForwardBy(base::Hours(24));
  run_loop_periodic.Run();

  histograms.ExpectUniqueSample(
      "Signin.AccountPreview.NonPeriodicFetchesUntilNextPeriodicRefresh", 3, 1);
  EXPECT_EQ(0,
            prefs_.GetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref));
}

TEST_F(AccountPreviewDataServiceTest, PeriodicRefreshWithNoAccounts) {
  // 1. When non-periodic fetch count is 0 and 0 accounts are signed in.
  {
    base::HistogramTester histograms;
    EXPECT_EQ(
        0, prefs_.GetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref));

    // Fast-forward 24 hours to trigger periodic refresh with 0 accounts signed
    // in.
    task_environment_.FastForwardBy(base::Hours(24));

    // Count is 0, so NonPeriodicFetchesUntilNextPeriodicRefresh should NOT be
    // recorded.
    histograms.ExpectTotalCount(
        "Signin.AccountPreview.NonPeriodicFetchesUntilNextPeriodicRefresh", 0);
    EXPECT_EQ(
        0, prefs_.GetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref));
  }

  // 2. When non-periodic fetch count > 0 (e.g. from previous fetches), but
  // currently 0 accounts are signed in.
  {
    base::HistogramTester histograms;
    prefs_.SetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref, 2);

    // Fast-forward 24 hours to trigger periodic refresh with 0 accounts signed
    // in.
    task_environment_.FastForwardBy(base::Hours(24));

    // Count was 2 (> 0), so NonPeriodicFetchesUntilNextPeriodicRefresh should
    // be recorded with value 2.
    histograms.ExpectUniqueSample(
        "Signin.AccountPreview.NonPeriodicFetchesUntilNextPeriodicRefresh", 2,
        1);
    // Count pref should be cleared to 0.
    EXPECT_EQ(
        0, prefs_.GetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref));
  }
}

}  // namespace signin
