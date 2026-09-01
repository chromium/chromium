// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include "base/functional/callback_forward.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/version_info/channel.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
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
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

using AccountPreviewPreference =
    AccountPreviewDataService::AccountPreviewPreference;

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
         switches::kEnableAccountPreviewPreferredAccount
#if BUILDFLAG(IS_ANDROID)
         ,
         switches::kEnableAccountPreviewUseAppAccount
#endif
        },
        {});
  }

  void SetUp() override {
    AccountPreviewDataService::RegisterProfilePrefs(prefs_.registry());
    SigninPrefs::RegisterProfilePrefs(prefs_.registry());
    local_state_.registry()->RegisterStringPref(
        prefs::kGoogleServicesUsernamePattern, std::string());
    prefs_.registry()->RegisterBooleanPref(prefs::kSigninAllowed, true);
    prefs_.SetBoolean(prefs::kSigninAllowed, true);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
    network_delay_helper_ = helper.get();
    service_ = std::make_unique<AccountPreviewDataServiceImpl>(
        identity_test_env_.identity_manager(), &sync_service_, &local_state_,
        &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(),
        std::move(helper), version_info::Channel::UNKNOWN,
        &profile_metrics_service_);
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
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple prefs_;
  IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  metrics::ProfileMetricsService profile_metrics_service_;
  raw_ptr<TestWaitForNetworkCallbackHelper> network_delay_helper_ = nullptr;
  std::unique_ptr<AccountPreviewDataServiceImpl> service_;
};

TEST_F(AccountPreviewDataServiceTest, EmptyInitially) {
  GaiaId id("some-gaia-id");
  std::optional<AccountPreviewData> data = service_->GetAccountPreviewData(id);
  EXPECT_FALSE(data.has_value());
}

TEST_F(AccountPreviewDataServiceTest, SigninDisallowed) {
  prefs_.SetBoolean(prefs::kSigninAllowed, false);
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("primary@gmail.com");

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.GetGaiaId()));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->GetPreferredAccountForPromo().has_value());
}

TEST_F(AccountPreviewDataServiceTest, FetchesForPrimaryAccount) {
  AccountInfo primary_info = identity_test_env_.MakePrimaryAccountAvailable(
      "primary@gmail.com", ConsentLevel::kSignin);

  MockSuccessfulFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 10, .password_count = 20, .history_count = 30},
      {{.cache_guid = "device_1",
        .last_updated = syncer::ProtoTimeToTime(123456789),
        .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
        .form_factor =
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP},
       {.cache_guid = "device_2",
        .last_updated = syncer::ProtoTimeToTime(987654321),
        .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_LINUX,
        .form_factor =
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP}});

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // Simulating OnRefreshTokenUpdatedForAccount for primary account
  service_->OnRefreshTokenUpdatedForAccount(primary_info);
  run_loop.Run();

  // It should trigger fetcher and save to memory cache with correct data
  std::optional<AccountPreviewData> data =
      service_->GetAccountPreviewData(primary_info.GetGaiaId());
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(10U, data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(20U, data->counts[syncer::PASSWORDS]);
  EXPECT_EQ(30U, data->counts[syncer::HISTORY]);
  ASSERT_EQ(2U, data->devices.size());
  EXPECT_EQ("device_1", data->devices[0].cache_guid);
  EXPECT_EQ(syncer::ProtoTimeToTime(123456789), data->devices[0].last_updated);
  EXPECT_EQ(sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
            data->devices[0].os_type);
  EXPECT_EQ(sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP,
            data->devices[0].form_factor);
  EXPECT_EQ("device_2", data->devices[1].cache_guid);
  EXPECT_EQ(syncer::ProtoTimeToTime(987654321), data->devices[1].last_updated);
  EXPECT_EQ(sync_pb::SyncEnums_OsType_OS_TYPE_LINUX, data->devices[1].os_type);
  EXPECT_EQ(sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP,
            data->devices[1].form_factor);
}

TEST_F(AccountPreviewDataServiceTest, RemovesCachedData) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  service_->OnRefreshTokenUpdatedForAccount(account_info);
  run_loop.Run();
  ASSERT_TRUE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());

  service_->OnRefreshTokenRemovedForAccount(account_info.GetAccountId());

  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       RemovesCachedDataAfterRefreshTokenRemoved) {
  MockSuccessfulFetch(&test_url_loader_factory_);

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());

  // Simulate IdentityManager completely forgetting about the account.
  // We can do this by removing it from IdentityTestEnv (which normally triggers
  // OnRefreshTokenRemovedForAccount automatically, but we can also check that
  // when the callback runs, the cache gets cleared).
  identity_test_env_.RemoveRefreshTokenForAccount(account_info.GetAccountId());

  // Since RemoveRefreshTokenForAccount triggers the callback automatically,
  // the cache should have been cleared.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
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

  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());

  // Make account2 available.
  // We do NOT mock any response for account1 (and it should not fetch it
  // anyway). We mock success for account2.
  MockSuccessfulFetch(&test_url_loader_factory_);

  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  // Fetch should only be triggered for the new account2, not the cached
  // account1.
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
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

  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));

  // 2. Make account2 available (triggers EnsureAllAccountsFetched()).
  // This should trigger fetch for both account1 and account2.
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
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

  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // Remove account1. This triggers EnsureAllAccountsFetched().
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());

  // account1 is cleared. No fetches should start for the remaining cached
  // account2.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
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
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());

  // 2. Mock failure responses, set callback, and make account2 available.
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());

  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop2.Run();

  EXPECT_FALSE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // Manually set account1 as the preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  // 3. Remove account1. This triggers EnsureAllAccountsFetched() and clears the
  // preferred account preference.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());

  // Preferred account pref should be cleared now since account1 was preferred.
  EXPECT_EQ(service_->GetPreferredAccountForPromo(), std::nullopt);

  // account1 is cleared. Fetch should start for the remaining uncached
  // account2.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
}

#if !BUILDFLAG(IS_CHROMEOS)
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
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // 3. Resolve account2's fetch.
  SimulateSuccessfulFetch(&test_url_loader_factory_);

  // Run the loop. Now that both fetches are completed, the callback should
  // trigger and quit the loop.
  waiter.Wait();
  EXPECT_TRUE(waiter.is_all_data_available());
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest, GetPreferredAccountForPromo) {
  // 1. Initially empty.
  EXPECT_EQ(service_->GetPreferredAccountForPromo(), std::nullopt);

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
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  identity_test_env_.SetCookieAccounts(
      {{std::string(account1.GetEmail()), account1.GetGaiaId()},
       {std::string(account2.GetEmail()), account2.GetGaiaId()}});
#endif

  all_data_available_loop.Run();

  // 3. Verify preferred account is computed.
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreferredAccountForPromoRespectsUsernamePatternPolicy) {
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern, "*@gmail.com");

  MockSuccessfulFetch(&test_url_loader_factory_);
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop all_data_available_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_data_available_loop.QuitClosure());

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@example.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  identity_test_env_.SetCookieAccounts(
      {{account1.email, account1.gaia}, {account2.email, account2.gaia}});
#endif

  all_data_available_loop.Run();

  // account1@example.com is disallowed by pattern *@gmail.com, so account2
  // should be preferred even though account1 was added first.
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account2.gaia)));
}
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(AccountPreviewDataServiceTest,
       GetPreferredAccountForPromoRespectsDefaultAccountOrderCookieJar) {
  AllDataAvailableWaiter waiter(service_.get());

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  // On Desktop, specify account2 first in the cookie jar.
  identity_test_env_.SetCookieAccounts(
      {{std::string(account2.GetEmail()), account2.GetGaiaId()},
       {std::string(account1.GetEmail()), account1.GetGaiaId()}});

  // Resolve pending fetches for both accounts.
  SimulateSuccessfulFetch(&test_url_loader_factory_);
  SimulateSuccessfulFetch(&test_url_loader_factory_);

  waiter.Wait();

  // With both accounts having identical preview data, the tie is broken in
  // favor of the default promo account from cookie jar (account2).
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account2.GetGaiaId())));
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_F(AccountPreviewDataServiceTest,
       GetPreferredAccountForPromoRespectsDefaultAccountOrderDeviceOrder) {
  AllDataAvailableWaiter waiter(service_.get());

  // On Mobile, make account2 available first so it becomes the default device
  // account.
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");

  // Resolve pending fetches for both accounts.
  SimulateSuccessfulFetch(&test_url_loader_factory_);
  SimulateSuccessfulFetch(&test_url_loader_factory_);

  waiter.Wait();

  // With both accounts having identical preview data, the tie is broken in
  // favor of the first signed-in/default device account (account2).
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account2.GetGaiaId())));
}
#endif

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
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  // Verify that it did NOT fetch yet.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());

  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // Simulate tokens loaded. This should trigger the deferred refresh.
  identity_test_env_.ReloadAccountsFromDisk();
  run_loop.Run();

  // Verify that it HAS fetched now.
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
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
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  // Verify that it did NOT fetch yet.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.GetGaiaId()));

  // Mock successful fetch in case a fetch is incorrectly started.
  MockSuccessfulFetch(&test_url_loader_factory_);

  // Simulate tokens loaded.
  identity_test_env_.ReloadAccountsFromDisk();
  EXPECT_TRUE(identity_test_env_.identity_manager()->AreRefreshTokensLoaded());
  EXPECT_TRUE(identity_test_env_.identity_manager()->HasAccountWithRefreshToken(
      account_info.GetAccountId()));

  // Verify that it still did NOT fetch because the timer didn't fire and we
  // shouldn't fetch on startup token loading.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.GetGaiaId()));
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

  ASSERT_TRUE(
      service_->GetAccountPreviewData(primary_info.GetGaiaId()).has_value());

  // 2. Trigger: Clear the primary account.
  identity_test_env_.ClearPrimaryAccount();

  // 3. Assert: Its data should be removed.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(primary_info.GetGaiaId()).has_value());
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
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.GetGaiaId()));

  MockSuccessfulFetch(
      &test_url_loader_factory_,
      {.bookmark_count = 5, .password_count = 10, .history_count = 15},
      {{.cache_guid = "device_1",
        .last_updated = syncer::ProtoTimeToTime(123456789),
        .os_type = sync_pb::SyncEnums_OsType_OS_TYPE_WINDOWS,
        .form_factor =
            sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP}});

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  // 2. Go online. This should trigger the queued fetch.
  network_delay_helper_->SetNetworkCallsDelayed(false);
  run_loop.Run();

  // Assert: The queued fetch completed successfully and data was stored.
  std::optional<AccountPreviewData> data =
      service_->GetAccountPreviewData(account_info.GetGaiaId());
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

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.GetGaiaId()));

  // 2. Remove the account while network calls are still delayed.
  identity_test_env_.RemoveRefreshTokenForAccount(account_info.GetAccountId());

  // 3. Unblock network calls. The queued StartFetch() callback will run,
  // but should return early without starting a fetcher since the account was
  // removed.
  network_delay_helper_->SetNetworkCallsDelayed(false);

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_info.GetGaiaId()));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       RemovingAccountDuringActiveFetchCompletesBarrier) {
  // Make account1 and account2 available. This starts active fetches for both,
  // but we do NOT provide mock network responses yet, so they remain active.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // Set up a waiter for all fetches completed.
  base::RunLoop all_fetches_run_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_fetches_run_loop.QuitClosure());

  // Remove account1 while its fetch is still active.
  // This should run the barrier once, and erase the fetcher for account1.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));

  // Since account1 was removed, mocking a successful fetch would trigger the
  // fetch for account2.
  MockSuccessfulFetch(&test_url_loader_factory_);

  // Wait for all data available (which triggers when the barrier runs to
  // completion).
  all_fetches_run_loop.Run();

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
}

TEST_F(AccountPreviewDataServiceTest,
       RemovingPreferredAccountDuringActiveFetchKeepsBarrierSynchronized) {
  // Make account1 and account2 available.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // Manually set account1 as preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Remove account1 while both fetches are active. Removing preferred account
  // triggers EnsureAllAccountsFetched() internally.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));

  // Remove account2 while its fetch is still active. The barrier should remain
  // valid and synchronized, so CHECK(all_accounts_fetched_barrier_) succeeds.
  identity_test_env_.RemoveRefreshTokenForAccount(account2.GetAccountId());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
}

TEST_F(AccountPreviewDataServiceTest, BatchAccountRemovalWithPreferredAccount) {
  // Make account1 and account2 available.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // Manually set account1 as preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Simulate Android batch removal: remove refresh tokens from IdentityManager
  // first before processing individual OnRefreshTokenRemoved notifications.
  identity_test_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Process removal for account1 (preferred account).
  service_->OnRefreshTokenRemovedForAccount(account1.GetAccountId());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));

  // Process removal for account2.
  service_->OnRefreshTokenRemovedForAccount(account2.GetAccountId());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
}

TEST_F(AccountPreviewDataServiceTest,
       ThreeAccountsPartialBatchRemovalWithPreferredAccount) {
  // Make account1, account2, and account3 available (all 3 active).
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  AccountInfo account3 =
      identity_test_env_.MakeAccountAvailable("account3@gmail.com");

  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account3.GetGaiaId()));

  // Manually set account1 as preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Remove refresh tokens for account1 and account2 from IdentityManager,
  // leaving account3 signed in.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));

  identity_test_env_.RemoveRefreshTokenForAccount(account2.GetAccountId());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // account3 fetch is still active. Resolving its mock network response should
  // succeed cleanly and find a valid barrier.
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account3.GetGaiaId()));
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(
      service_->GetAccountPreviewData(account3.GetGaiaId()).has_value());
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

  ASSERT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // 2. Make account1 available, and fail its fetch (so it remains uncached).
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop1.Run();

  ASSERT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());

  // Manually set account1 as the preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Verify that account1 is indeed preferred.
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  // Now remove account2 (which is NOT the preferred account).
  // Because it is not the preferred account, OnRefreshTokenRemovedForAccount
  // should NOT trigger a new fetch cycle (i.e. it should NOT call
  // EnsureAllAccountsFetched()) and should NOT clear the preferred account
  // pref. We verify this by asserting that no active fetcher is started for the
  // uncached account1 and the preferred account pref remains intact.
  identity_test_env_.RemoveRefreshTokenForAccount(account2.GetAccountId());

  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  EXPECT_FALSE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       InvalidatingNonPreferredAccountDoesNotTriggerRefresh) {
  // 1. Make account2 available and cache it successfully.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop2.Run();

  ASSERT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // 2. Make account1 available, and fail its fetch (so it remains uncached).
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop1.Run();

  ASSERT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());

  // Manually set account1 as the preferred account in prefs.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Verify that account1 is indeed preferred.
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  // Now invalidate account2 (which is NOT the preferred account) with a
  // persistent error.
  // Because it is not the preferred account, OnRefreshTokenUpdatedForAccount
  // should NOT trigger a new fetch cycle (i.e. it should NOT call
  // EnsureAllAccountsFetched()) and should NOT clear the preferred account
  // pref. We verify this by asserting that no active fetcher is started for the
  // uncached account1 and the preferred account pref remains intact.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account2.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  service_->OnRefreshTokenUpdatedForAccount(account2);

  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  EXPECT_FALSE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       DoesNotComputePreferredAccountWhenFeatureDisabled) {
  // Disable preferred account computation feature flag.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kEnableAccountPreviewPreferredAccount);

  signin::WaitForRefreshTokensLoaded(identity_test_env_.identity_manager());

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
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, kFakeGaiaId)));
}

TEST_F(AccountPreviewDataServiceTest,
       RecordsSelectionHeuristicScoreMetricsOnPreferredAccountComputed) {
  base::HistogramTester histogram_tester;

  signin::WaitForRefreshTokensLoaded(identity_test_env_.identity_manager());

  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetAllDataAvailableCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", ConsentLevel::kSignin);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  identity_test_env_.SetCookieAccounts({{account.email, account.gaia}});
#endif
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kSyncDataScore, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount", 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
  EXPECT_FALSE(
      prefs_
          .GetTime(
              prefs::kAccountPreviewSelectionHeuristicScoresLastRecordedPref)
          .is_null());
}

TEST_F(AccountPreviewDataServiceTest,
       RecordsSelectionHeuristicReasonMetricsOnNonSyncReason) {
  base::HistogramTester histogram_tester;

  signin::WaitForRefreshTokensLoaded(identity_test_env_.identity_manager());

  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop run_loop;
  service_->SetAllDataAvailableCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "user@managed.com", ConsentLevel::kSignin);
  identity_test_env_.UpdateAccountInfoForAccount(
      AccountInfo::Builder(account).SetHostedDomain("managed.com").Build());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  identity_test_env_.SetCookieAccounts({{account.email, account.gaia}});
#endif
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "Signin.SelectionHeuristic.Reason",
      AccountPreviewSelectionReason::kNonRegularDefault, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PrimaryAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.PreferredAccount.SingleAccount", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.IsPrimaryDifferentFromPreferred"
      ".SingleAccount",
      0);
  histogram_tester.ExpectTotalCount(
      "Signin.SelectionHeuristicScore.OtherAccount", 0);
  EXPECT_FALSE(
      prefs_
          .GetTime(
              prefs::kAccountPreviewSelectionHeuristicScoresLastRecordedPref)
          .is_null());
}

TEST_F(AccountPreviewDataServiceTest, ReadPreviewPreferenceFromPrefsDataTypes) {
  base::DictValue dict;
  dict.Set("gaia_id", "test_gaia_id");
  base::ListValue data_types_list;

  base::DictValue bookmarks_dict;
  bookmarks_dict.Set("data_type",
                     syncer::DataTypeToStableIdentifier(syncer::BOOKMARKS));
  bookmarks_dict.Set("quartile",
                     static_cast<int>(SyncDataQuartile::kMedianToQ3));
  data_types_list.Append(std::move(bookmarks_dict));

  base::DictValue invalid_dict;
  invalid_dict.Set("data_type", -1);
  invalid_dict.Set("quartile", 1);
  data_types_list.Append(std::move(invalid_dict));

  base::DictValue passwords_dict;
  passwords_dict.Set("data_type",
                     syncer::DataTypeToStableIdentifier(syncer::PASSWORDS));
  passwords_dict.Set("quartile", static_cast<int>(SyncDataQuartile::kAboveQ3));
  data_types_list.Append(std::move(passwords_dict));

  dict.Set("data_types", std::move(data_types_list));

  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  EXPECT_THAT(
      service_->GetPreferredAccountForPromo(),
      testing::Optional(testing::AllOf(
          testing::Field(&AccountPreviewPreference::gaia_id,
                         GaiaId("test_gaia_id")),
          testing::Field(&AccountPreviewPreference::preferred_data_types,
                         testing::ElementsAre(
                             PreferredDataTypeInfo{
                                 .data_type = syncer::BOOKMARKS,
                                 .quartile = SyncDataQuartile::kMedianToQ3},
                             PreferredDataTypeInfo{
                                 .data_type = syncer::PASSWORDS,
                                 .quartile = SyncDataQuartile::kAboveQ3})))));
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
  dict.Set("gaia_id", account_info.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop2;
  service_->SetFetchCompleteCallbackForTesting(run_loop2.QuitClosure());
  identity_test_env_.RemoveRefreshTokenForAccount(account_info.GetAccountId());
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

  // 4. Trigger cause by token invalidation (persistent error on preferred
  // account). Make account3 available and fail its fetch so it remains
  // uncached.
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop_fail2;
  service_->SetFetchCompleteCallbackForTesting(run_loop_fail2.QuitClosure());
  AccountInfo account3 =
      identity_test_env_.MakeAccountAvailable("account3@gmail.com");
  run_loop_fail2.Run();

  // Manually set account2 as preferred account so invalidating it triggers a
  // refresh.
  base::DictValue dict2;
  dict2.Set("gaia_id", account2.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict2));

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop4;
  service_->SetFetchCompleteCallbackForTesting(run_loop4.QuitClosure());
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account2.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  run_loop4.Run();

  histograms.ExpectBucketCount("Signin.AccountPreview.AllFetchTriggerCause",
                               AccountPreviewDataServiceImpl::
                                   FetchTriggerCause::kRefreshTokenInvalidated,
                               1);
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.SuccessfulFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::
          kRefreshTokenInvalidated,
      1);
}

TEST_F(AccountPreviewDataServiceTest, LogsTriggerCauseWithAllCachesAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewData, {{"persist_accounts", "false"}});
  base::HistogramTester histograms;

  // Make account available and cache it.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  run_loop.Run();

  ASSERT_TRUE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());

  // Trigger a new update for the same account. Since it's already cached,
  // gaia_ids_to_fetch will be empty (all caches available).
  identity_test_env_.SetRefreshTokenForAccount(account_info.GetAccountId());

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

TEST_F(AccountPreviewDataServiceTest, AccountsNotMutatedSkipsFetch) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeature(switches::kEnableAccountPreviewData);

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");

  // Set the pref so they match
  base::ListValue fetch_accounts;
  fetch_accounts.Append(account1.GetGaiaId().ToString());
  prefs_.SetList(prefs::kAccountPreviewDataLastFetchAccounts,
                 std::move(fetch_accounts));

  // Set the last update pref to just now, so the timer does NOT fire.
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  // We explicitly tear down and recreate the service here to emulate a browser
  // cold boot. The mitigation we are testing specifically triggers when the
  // in-memory cache is empty (which natively happens at startup) but the
  // persisted valid accounts list is fully populated. Wiping the object
  // entirely ensures we do not rely on testing-only cache-clearing methods
  // and accurately replicates real startup lifecycle state.
  network_delay_helper_ = nullptr;
  service_.reset();

  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  base::RunLoop run_loop;
  service_->SetAllDataAvailableCallbackForTesting(run_loop.QuitClosure());

  // No timer fires, so we manually simulate an update.
  service_->OnRefreshTokenUpdatedForAccount(account1);
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "Signin.AccountPreview.TriggerCauseAccountsUnchangedSinceLastFetch",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenUpdated,
      1);

  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));
}

TEST_F(AccountPreviewDataServiceTest,
       M1b_AccountsMutatedAdditionTriggersFetch) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeature(switches::kEnableAccountPreviewData);

  network_delay_helper_ = nullptr;
  service_.reset();

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");

  prefs_.SetList(prefs::kAccountPreviewDataLastFetchAccounts,
                 base::ListValue());
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());

  service_->OnRefreshTokenUpdatedForAccount(account1);
  run_loop.Run();

  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest, AccountsMutatedRemovalTriggersFetch) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeature(switches::kEnableAccountPreviewData);

  network_delay_helper_ = nullptr;
  service_.reset();

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");

  base::ListValue last_fetch;
  last_fetch.Append("account1@gmail.com");
  last_fetch.Append("account2@gmail.com");
  prefs_.SetList(prefs::kAccountPreviewDataLastFetchAccounts,
                 std::move(last_fetch));
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());

  service_->OnRefreshTokenUpdatedForAccount(account1);
  run_loop.Run();

  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       ClearsPersistedLastFetchAccountsWhenNoAccounts) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeature(switches::kEnableAccountPreviewData);

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  service_->OnRefreshTokenUpdatedForAccount(account1);
  run_loop.Run();

  EXPECT_EQ(1u,
            prefs_.GetList(prefs::kAccountPreviewDataLastFetchAccounts).size());

  // Remove all refresh tokens.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());

  // Pref must be cleared when no accounts remain.
  EXPECT_TRUE(
      prefs_.GetList(prefs::kAccountPreviewDataLastFetchAccounts).empty());
}

TEST_F(AccountPreviewDataServiceTest, PeriodicRefreshTimingParam) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewData,
      {{switches::kAccountPreviewDataPeriodicRefreshTiming.name, "18h"}});

  // Destroy existing service so it can be re-created with the custom param.
  network_delay_helper_ = nullptr;
  service_.reset();

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  MockSuccessfulFetch(&test_url_loader_factory_);

  // Fast forward 17 hours: timer shouldn't fire yet.
  task_environment_.FastForwardBy(base::Hours(17));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());

  // Fast forward 1 more hour (total 18 hours): timer fires.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       PeriodicRefreshTimingParamClampedToMinimum) {
  base::test::ScopedFeatureList custom_feature_list;
  // Set parameter to 10 minutes, which is less than the 12 hour minimum.
  custom_feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewData,
      {{switches::kAccountPreviewDataPeriodicRefreshTiming.name, "10m"}});

  // Destroy existing service so it can be re-created with the custom param.
  network_delay_helper_ = nullptr;
  service_.reset();

  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  MockSuccessfulFetch(&test_url_loader_factory_);

  // Fast forward 11 hours: timer shouldn't fire (clamped to 12 hours minimum).
  task_environment_.FastForwardBy(base::Hours(11));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());

  // Fast forward 1 more hour (total 12 hours): timer fires.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       OnRefreshTokenRemovedForAccountWithNullBarrier) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  base::RunLoop all_fetches_run_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_fetches_run_loop.QuitClosure());

  // An active fetcher is created for account_info, along with a barrier that is
  // not yet hit. Removing the refresh token for this account should safely
  // erase the fetcher and hit the barrier.
  service_->OnRefreshTokenRemovedForAccount(account_info.GetAccountId());
  // Removing the account should hit the barrier which would complete the fetch.
  all_fetches_run_loop.Run();
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_info.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       OnIdentityManagerShutdownClearsCacheAndFetchers) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  // Fetcher is active.
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account.GetGaiaId()));

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(account.GetGaiaId()).has_value());

  // Set a preferred account preference to simulate stored results.
  base::DictValue dict;
  dict.Set("gaia_id", account.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));
  ASSERT_TRUE(service_->GetPreferredAccountForPromo().has_value());

  // Start fetching data for a second account, but without completing it.
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("user2@gmail.com");
  // Fetcher for account2 is active.
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // Trigger IdentityManager shutdown.
  service_->OnIdentityManagerShutdown(identity_test_env_.identity_manager());

  // Cached data and active fetchers should be cleared.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account.GetGaiaId()).has_value());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.GetGaiaId()));

  // Stored results in prefs should remain intact.
  EXPECT_TRUE(service_->GetPreferredAccountForPromo().has_value());
}

// This test verifies that there isn't a pending callback for an account
// when the refresh token is removed. And more importantly, it doesn't crash.
//
// It used to crash because the final callback used to be posted to the task
// queue outliving the fetcher (see crbug.com/533927599, crbug.com/542550030).
TEST_F(AccountPreviewDataServiceTest, NoInFlightTaskOnAccountRemoved) {
  const AccountInfo account_1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  const AccountInfo account_2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 4);

  base::RunLoop account_1_fetch_run_loop;
  service_->SetFetchCompleteCallbackForTesting(
      account_1_fetch_run_loop.QuitClosure());
  SimulateSuccessfulFetch(&test_url_loader_factory_);
  account_1_fetch_run_loop.Run();

  // `account_1`'s data is now cached, and no active fetcher exists for it.
  ASSERT_TRUE(
      service_->GetAccountPreviewData(account_1.GetGaiaId()).has_value());
  ASSERT_FALSE(service_->HasActiveFetcherForTesting(account_1.GetGaiaId()));

  // `account_2` still has an active fetcher.
  ASSERT_TRUE(service_->HasActiveFetcherForTesting(account_2.GetGaiaId()));

  // Schedule an account removal while the fetch for `account_2` is in flight.
  AccountPreviewDataFetcher* fetcher =
      service_->GetFetcherForTesting(account_2.GetGaiaId());
  ASSERT_NE(fetcher, nullptr);
  fetcher->SetOnFetchCompletedForTesting(base::BindLambdaForTesting([&]() {
    identity_test_env_.RemoveRefreshTokenForAccount(account_2.GetAccountId());
  }));

  base::RunLoop account_2_fetch_run_loop;
  service_->SetFetchCompleteCallbackForTesting(
      account_2_fetch_run_loop.QuitClosure());
  MockSuccessfulFetch(&test_url_loader_factory_);
  account_2_fetch_run_loop.Run();

  // `account_2` has been removed but more importantly the test did not crash
  // after the fetcher was destroyed.
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account_2.GetGaiaId()));
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account_2.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest, NullSyncService) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulFetch(&test_url_loader_factory_,
                      {.bookmark_count = 10, .password_count = 20},
                      {{.cache_guid = "device_1"}});

  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), /*sync_service=*/nullptr,
      &local_state_, &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(),
      std::move(helper), version_info::Channel::UNKNOWN,
      &profile_metrics_service_);

  base::RunLoop all_fetches_run_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_fetches_run_loop.QuitClosure());
  all_fetches_run_loop.Run();

  auto preview_data = service_->GetAccountPreviewData(account_info.GetGaiaId());
  ASSERT_TRUE(preview_data.has_value());
  EXPECT_EQ(10U, preview_data->counts[syncer::BOOKMARKS]);
  EXPECT_EQ(20U, preview_data->counts[syncer::PASSWORDS]);
  ASSERT_EQ(1U, preview_data->devices.size());
  EXPECT_EQ("device_1", preview_data->devices[0].cache_guid);
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreferredAccountForPromoOtherDeviceFormFactor) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  base::DictValue dict;
  dict.Set("gaia_id", account.GetGaiaId().ToString());
  dict.Set("other_device_form_factor",
           static_cast<int>(
               sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET));
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  EXPECT_THAT(
      service_->GetPreferredAccountForPromo(),
      testing::Optional(testing::AllOf(
          testing::Field(&AccountPreviewPreference::gaia_id,
                         account.GetGaiaId()),
          testing::Field(
              &AccountPreviewPreference::other_device_form_factor,
              sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET))));
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreviewPreferenceForAccountCachedData) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(account.GetGaiaId()).has_value());

  base::RunLoop fetch_run_loop;
  std::optional<AccountPreviewPreference> fetched_preference;
  service_->GetPreviewPreferenceForAccount(
      account.GetGaiaId(),
      base::BindOnce(
          [](base::OnceClosure quit,
             std::optional<AccountPreviewPreference>* result,
             std::optional<AccountPreviewPreference> pref) {
            *result = std::move(pref);
            std::move(quit).Run();
          },
          fetch_run_loop.QuitClosure(), &fetched_preference));
  fetch_run_loop.Run();

  EXPECT_THAT(fetched_preference,
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account.GetGaiaId())));
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreviewPreferenceForAccountTriggersFetch) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  ASSERT_FALSE(
      service_->GetAccountPreviewData(account.GetGaiaId()).has_value());

  base::RunLoop fetch_run_loop;
  std::optional<AccountPreviewPreference> fetched_preference;
  service_->GetPreviewPreferenceForAccount(
      account.GetGaiaId(),
      base::BindOnce(
          [](base::OnceClosure quit,
             std::optional<AccountPreviewPreference>* result,
             std::optional<AccountPreviewPreference> pref) {
            *result = std::move(pref);
            std::move(quit).Run();
          },
          fetch_run_loop.QuitClosure(), &fetched_preference));

  MockSuccessfulFetch(&test_url_loader_factory_);
  fetch_run_loop.Run();

  EXPECT_THAT(fetched_preference,
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account.GetGaiaId())));
  EXPECT_TRUE(service_->GetAccountPreviewData(account.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreviewPreferenceForAccountInvalidAccount) {
  base::RunLoop fetch_run_loop;
  std::optional<AccountPreviewPreference> fetched_preference;
  service_->GetPreviewPreferenceForAccount(
      GaiaId("non_existent_gaia_id"),
      base::BindOnce(
          [](base::OnceClosure quit,
             std::optional<AccountPreviewPreference>* result,
             std::optional<AccountPreviewPreference> pref) {
            *result = std::move(pref);
            std::move(quit).Run();
          },
          fetch_run_loop.QuitClosure(), &fetched_preference));
  fetch_run_loop.Run();

  EXPECT_EQ(fetched_preference, std::nullopt);
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreviewPreferenceForAccountFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kEnableAccountPreviewPreferredAccount);

  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  base::RunLoop fetch_run_loop;
  std::optional<AccountPreviewPreference> fetched_preference;
  service_->GetPreviewPreferenceForAccount(
      account.GetGaiaId(),
      base::BindOnce(
          [](base::OnceClosure quit,
             std::optional<AccountPreviewPreference>* result,
             std::optional<AccountPreviewPreference> pref) {
            *result = std::move(pref);
            std::move(quit).Run();
          },
          fetch_run_loop.QuitClosure(), &fetched_preference));
  fetch_run_loop.Run();

  EXPECT_EQ(fetched_preference, std::nullopt);
}

TEST_F(AccountPreviewDataServiceTest,
       GetPreviewPreferenceForAccountDoesNotInterfereWithAllAccountsBarrier) {
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");

  // An active fetcher is created for account1 during token update.
  EXPECT_TRUE(service_->HasActiveFetcherForTesting(account1.GetGaiaId()));

  // Attach a single account fetch callback to the ongoing fetch for account1.
  base::RunLoop single_fetch_run_loop;
  std::optional<AccountPreviewPreference> single_fetch_preference;
  service_->GetPreviewPreferenceForAccount(
      account1.GetGaiaId(),
      base::BindOnce(
          [](base::OnceClosure quit,
             std::optional<AccountPreviewPreference>* result,
             std::optional<AccountPreviewPreference> pref) {
            *result = std::move(pref);
            std::move(quit).Run();
          },
          single_fetch_run_loop.QuitClosure(), &single_fetch_preference));

  AllDataAvailableWaiter waiter(service_.get());

  // Complete fetch for account1.
  MockSuccessfulFetch(&test_url_loader_factory_);
  single_fetch_run_loop.Run();
  waiter.Wait();

  // Both single fetch callback and batch completion barrier should run as
  // expected.
  EXPECT_THAT(single_fetch_preference,
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));
  EXPECT_TRUE(waiter.is_all_data_available());
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
}

TEST_F(AccountPreviewDataServiceTest,
       RefreshTokenUpdatedWithPersistentErrorClearsCache) {
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop1;
  service_->SetFetchCompleteCallbackForTesting(run_loop1.QuitClosure());
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  run_loop1.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(account.GetGaiaId()).has_value());

  // Simulate token error update on the account.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Cached data and stored preferred account should be cleared.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account.GetGaiaId()).has_value());
  EXPECT_EQ(service_->GetPreferredAccountForPromo(), std::nullopt);
}

TEST_F(AccountPreviewDataServiceTest,
       RemainingAccountsAllCachedDoesNotCrashAndRecomputes) {
  base::HistogramTester histograms;

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("user1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("user2@gmail.com");
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  identity_test_env_.SetCookieAccounts(
      {{std::string(account1.GetEmail()), account1.GetGaiaId()},
       {std::string(account2.GetEmail()), account2.GetGaiaId()}});
#endif

  MockSuccessfulFetch(&test_url_loader_factory_);
  AllDataAvailableWaiter waiter(service_.get());
  waiter.Wait();

  ASSERT_TRUE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  ASSERT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // Manually set account1 as preferred so invalidating it triggers
  // EnsureAllAccountsFetched.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  // Invalidate account1 with persistent error (e.g. web sign-out for primary
  // account). account2 was already cached.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account1.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // account1 cache is cleared, account2 remains cached, and no crash occurs.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // Preferred account for promo is recomputed and selects account2.
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account2.GetGaiaId())));

  // `TriggerCauseWithAllCachesAvailable` is recorded because all remaining
  // accounts (account2) were already cached.
  histograms.ExpectUniqueSample(
      "Signin.AccountPreview.TriggerCauseWithAllCachesAvailable",
      AccountPreviewDataServiceImpl::FetchTriggerCause::
          kRefreshTokenInvalidated,
      1);

  // Verify that the persisted last fetch accounts list was updated to only
  // include account2.
  const base::ListValue& last_fetch_accounts =
      prefs_.GetList(prefs::kAccountPreviewDataLastFetchAccounts);
  ASSERT_EQ(1u, last_fetch_accounts.size());
  EXPECT_EQ(account2.GetGaiaId().ToString(),
            last_fetch_accounts[0].GetString());
}

TEST_F(AccountPreviewDataServiceTest,
       AccountRemovedOnStartupWithoutFetchTriggersRefreshIfPreferred) {
  // Simulate previous session with account1 and account2.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("user1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("user2@gmail.com");

  // Set account1 as the preferred account in prefs from previous session.
  base::DictValue dict;
  dict.Set("gaia_id", account1.GetGaiaId().ToString());
  prefs_.SetDict(prefs::kAccountPreviewPreference, std::move(dict));

  // Set last update time to Now so periodic timer does NOT fire on startup.
  prefs_.SetTime(prefs::kAccountPreviewDataLastUpdatePref, base::Time::Now());

  signin::WaitForRefreshTokensLoaded(identity_test_env_.identity_manager());

  // Re-create service (simulating Chrome restart with existing accounts).
  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  // Tokens are loaded, so RefreshAccountIdToGaiaIdMapping() ran.
  // Now remove account1 (preferred account).
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());
  run_loop.Run();

  // Preferred account pref should be cleared for account1, and account2
  // fetched.
  EXPECT_FALSE(
      service_->GetAccountPreviewData(account1.GetGaiaId()).has_value());
  EXPECT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AccountPreviewDataServiceTest, UpdateExternalAppAccountValidAccount) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  service_->UpdateExternalAppAccount("user@gmail.com");

  std::optional<GaiaId> gaia_id = service_->GetExternalAppAccountForTesting();
  ASSERT_TRUE(gaia_id.has_value());
  EXPECT_EQ(*gaia_id, account.GetGaiaId());

  const base::DictValue& dict =
      prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount);
  const std::string* gaia_id_str = dict.FindString("gaia_id");
  ASSERT_TRUE(gaia_id_str);
  EXPECT_EQ(*gaia_id_str, account.GetGaiaId().ToString());
  EXPECT_TRUE(dict.Find("timestamp"));
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountNullOrEmptyClearsPref) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  service_->UpdateExternalAppAccount(std::nullopt);
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());

  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  service_->UpdateExternalAppAccount("");
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountUnknownAccountClearsPref) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  service_->UpdateExternalAppAccount("unknown@gmail.com");
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountExpirationCleansUpPref) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  // 179 days: still valid.
  task_environment_.FastForwardBy(base::Days(179));
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  // 2 more days (181 total): expired.
  task_environment_.FastForwardBy(base::Days(2));
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());

  // Triggering periodic refresh should clear expired pref.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  task_environment_.FastForwardBy(base::Hours(24));
  run_loop.Run();

  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountAccountRemovalCleansUpPref) {
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("user1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("user2@gmail.com");

  service_->UpdateExternalAppAccount("user1@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  // Removing a different account shouldn't clear external app account pref.
  identity_test_env_.RemoveRefreshTokenForAccount(account2.GetAccountId());
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  // Removing the external app account cleans up the pref.
  identity_test_env_.RemoveRefreshTokenForAccount(account1.GetAccountId());
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountTokenLoadCleansUpNonExistentAccount) {
  // Set external app account pref for an account that does not exist in
  // identity manager.
  base::DictValue dict;
  dict.Set("gaia_id", "non_existent_gaia");
  dict.Set("timestamp", base::TimeToValue(base::Time::Now()));
  prefs_.SetDict(prefs::kAccountPreviewExternalAppAccount, std::move(dict));

  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");
  signin::WaitForRefreshTokensLoaded(identity_test_env_.identity_manager());

  // Re-create service (simulating Chrome restart).
  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  // Stored external app account is not in IdentityManager, so it should be
  // cleared.
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountSigninDisallowedClearsPref) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());

  prefs_.SetBoolean(prefs::kSigninAllowed, false);
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountFeatureDisabledClearsPref) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("user@gmail.com");

  // Store pref while feature is enabled.
  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_TRUE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_FALSE(
      prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());

  // Disable feature and call UpdateExternalAppAccount.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(
      switches::kEnableAccountPreviewUseAppAccount);

  service_->UpdateExternalAppAccount("user@gmail.com");
  EXPECT_FALSE(service_->GetExternalAppAccountForTesting().has_value());
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewExternalAppAccount).empty());
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountTriggersComputationAndUpdatesPreferredAccount) {
  EXPECT_EQ(service_->GetPreferredAccountForPromo(), std::nullopt);

  // Mock successful fetches for account1 and account2.
  MockSuccessfulFetch(&test_url_loader_factory_);
  MockSuccessfulFetch(&test_url_loader_factory_);

  base::RunLoop all_data_available_loop;
  service_->SetAllDataAvailableCallbackForTesting(
      all_data_available_loop.QuitClosure());

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  all_data_available_loop.Run();

  // account1 is default account (first in list).
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));

  // Update external app account to account2.
  // Because all accounts are already cached, preferred account is recomputed
  // immediately.
  base::HistogramTester histograms;
  service_->UpdateExternalAppAccount("account2@gmail.com");

  // account2 becomes preferred because is_external_app_primary is true for
  // account2.
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account2.GetGaiaId())));
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.AllFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::
          kExternalAppAccountUpdated,
      1);

  // Calling UpdateExternalAppAccount with the same account does not re-trigger
  // computation.
  service_->UpdateExternalAppAccount("account2@gmail.com");
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.AllFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::
          kExternalAppAccountUpdated,
      1);

  // Clearing external app account recomputes preferred account back to
  // account1.
  service_->UpdateExternalAppAccount(std::nullopt);
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account1.GetGaiaId())));
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.AllFetchTriggerCause",
      AccountPreviewDataServiceImpl::FetchTriggerCause::
          kExternalAppAccountUpdated,
      2);
}

TEST_F(AccountPreviewDataServiceTest,
       UpdateExternalAppAccountFetchesUncachedAccounts) {
  // Make account1 available and cached.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop.Run();

  // Make account2 available without caching (mock fails fetch).
  MockFailedStatsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  MockFailedPreviewsFetch(&test_url_loader_factory_, net::ERR_FAILED);
  base::RunLoop run_loop_fail;
  service_->SetFetchCompleteCallbackForTesting(run_loop_fail.QuitClosure());
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  run_loop_fail.Run();

  EXPECT_FALSE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());

  // Now update external app account to account2. This should trigger a fetch
  // for the uncached account2.
  MockSuccessfulFetch(&test_url_loader_factory_);
  base::RunLoop run_loop_fetch;
  service_->SetFetchCompleteCallbackForTesting(run_loop_fetch.QuitClosure());
  service_->UpdateExternalAppAccount("account2@gmail.com");
  run_loop_fetch.Run();

  EXPECT_TRUE(
      service_->GetAccountPreviewData(account2.GetGaiaId()).has_value());
  EXPECT_THAT(service_->GetPreferredAccountForPromo(),
              testing::Optional(testing::Field(
                  &AccountPreviewPreference::gaia_id, account2.GetGaiaId())));
}
#endif

TEST_F(AccountPreviewDataServiceTest, RateLimitOn429SingleFetch) {
  base::HistogramTester histograms;
  base::Time start_time = base::Time::Now();
  Mock429Fetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("account@gmail.com");
  run_loop.Run();

  histograms.ExpectBucketCount("Signin.AccountPreviewData.FetchHit429", true,
                               1);

  // Clear mocked 429 responses so future requests are not automatically
  // answered with 429.
  test_url_loader_factory_.ClearResponses();

  // Rate limit is now active.
  EXPECT_TRUE(service_->IsRateLimitedForTesting());
  EXPECT_EQ(prefs_.GetTime(prefs::kAccountPreviewDataLast429TimePref),
            start_time);

  // Subsequent single request immediately returns nullopt without initiating
  // network fetch.
  base::test::TestFuture<std::optional<AccountPreviewPreference>> future;
  service_->GetPreviewPreferenceForAccount(account.gaia, future.GetCallback());
  EXPECT_EQ(std::nullopt, future.Get());
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account.gaia));
  histograms.ExpectBucketCount("Signin.AccountPreview.SingleRequestRateLimited",
                               true, 1);

  // Advance clock by 23 hours - still rate limited.
  task_environment_.FastForwardBy(base::Hours(23));
  EXPECT_TRUE(service_->IsRateLimitedForTesting());

  // Advance clock past 24 hours - rate limit expires.
  MockSuccessfulFetch(&test_url_loader_factory_, {.bookmark_count = 10});
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_FALSE(service_->IsRateLimitedForTesting());

  // Now a new fetch can proceed.
  base::test::TestFuture<std::optional<AccountPreviewPreference>> future2;
  service_->GetPreviewPreferenceForAccount(account.gaia, future2.GetCallback());
  auto pref = future2.Get();
  ASSERT_TRUE(pref.has_value());
  EXPECT_EQ(account.gaia, pref->gaia_id);
  histograms.ExpectBucketCount("Signin.AccountPreviewData.FetchHit429", false,
                               1);
}

TEST_F(AccountPreviewDataServiceTest, RateLimitOn429BatchFetch) {
  base::HistogramTester histograms;
  Mock429Fetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  run_loop.Run();

  test_url_loader_factory_.ClearResponses();
  EXPECT_TRUE(service_->IsRateLimitedForTesting());

  // Adding another account while rate limited should not trigger new active
  // fetchers and records TriggerCauseRateLimited histogram.
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");
  EXPECT_FALSE(service_->HasActiveFetcherForTesting(account2.gaia));
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.TriggerCauseRateLimited",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenUpdated,
      1);

  // Fast forward by 24 hours.
  task_environment_.FastForwardBy(base::Hours(24));
  EXPECT_FALSE(service_->IsRateLimitedForTesting());
}

TEST_F(AccountPreviewDataServiceTest,
       RateLimitPersistedAcrossServiceRecreation) {
  Mock429Fetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("account@gmail.com");
  run_loop.Run();

  test_url_loader_factory_.ClearResponses();
  EXPECT_TRUE(service_->IsRateLimitedForTesting());

  // Re-create the service.
  auto helper = std::make_unique<TestWaitForNetworkCallbackHelper>();
  network_delay_helper_ = helper.get();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &sync_service_, &local_state_,
      &prefs_, test_url_loader_factory_.GetSafeWeakWrapper(), std::move(helper),
      version_info::Channel::UNKNOWN, &profile_metrics_service_);

  EXPECT_TRUE(service_->IsRateLimitedForTesting());

  // After 24 hours, rate limit expires in the recreated service as well.
  task_environment_.FastForwardBy(base::Hours(24));
  EXPECT_FALSE(service_->IsRateLimitedForTesting());
}

TEST_F(AccountPreviewDataServiceTest, RateLimitDisabledByFeatureParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kEnableAccountPreviewData,
      {{switches::kAccountPreviewData429RateLimitDuration.name, "0s"}});

  Mock429Fetch(&test_url_loader_factory_);
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("account@gmail.com");
  run_loop.Run();

  // With duration set to 0, rate limiting is disabled.
  EXPECT_FALSE(service_->IsRateLimitedForTesting());
}

TEST_F(AccountPreviewDataServiceTest, RateLimitOn429PeriodicRefresh) {
  base::HistogramTester histograms;

  // Have 1 account with successfully cached preview data.
  MockSuccessfulFetch(&test_url_loader_factory_, {.bookmark_count = 5});
  base::RunLoop run_loop;
  service_->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("account@gmail.com");
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  identity_test_env_.SetCookieAccounts({{account.email, account.gaia}});
#endif
  run_loop.Run();

  ASSERT_TRUE(service_->GetAccountPreviewData(account.gaia).has_value());
  auto preferred_account = service_->GetPreferredAccountForPromo();
  ASSERT_TRUE(preferred_account.has_value());
  EXPECT_EQ(account.gaia, preferred_account->gaia_id);

  // Fast forward by 12 hours.
  task_environment_.FastForwardBy(base::Hours(12));

  // Simulate hitting 429 at the 12-hour mark.
  prefs_.SetTime(prefs::kAccountPreviewDataLast429TimePref, base::Time::Now());
  EXPECT_TRUE(service_->IsRateLimitedForTesting());

  // Advance by another 12 hours to trigger the 24-hour periodic refresh.
  // The service is still within the 429 rate-limit window (12h elapsed < 24h).
  task_environment_.FastForwardBy(base::Hours(12));

  // The periodic refresh should have been denied due to rate limit, recording
  // the histogram.
  histograms.ExpectBucketCount(
      "Signin.AccountPreview.TriggerCauseRateLimited",
      AccountPreviewDataServiceImpl::FetchTriggerCause::kPeriodicRefresh, 1);

  // Cached data is cleared on periodic refresh.
  EXPECT_FALSE(service_->GetAccountPreviewData(account.gaia).has_value());
  EXPECT_FALSE(service_->GetPreferredAccountForPromo().has_value());
}

}  // namespace signin
