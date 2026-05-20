// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include "base/test/task_environment.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

class AccountPreviewDataServiceTest : public testing::Test {
 public:
  void SetUp() override {
    AccountPreviewDataService::RegisterProfilePrefs(prefs_.registry());
    service_ = std::make_unique<AccountPreviewDataServiceImpl>(
        identity_test_env_.identity_manager(), &prefs_);
  }

  void TearDown() override { service_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  IdentityTestEnvironment identity_test_env_;
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

  // Simulating OnRefreshTokenUpdatedForAccount for primary account
  service_->OnRefreshTokenUpdatedForAccount(primary_info);

  // It should trigger fetcher and save to prefs (even if empty)
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(primary_info.gaia.ToString()));
}

TEST_F(AccountPreviewDataServiceTest, RemovesCachedData) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  service_->OnRefreshTokenUpdatedForAccount(account_info);
  ASSERT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(account_info.gaia.ToString()));

  service_->OnRefreshTokenRemovedForAccount(account_info.account_id);

  EXPECT_FALSE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                   .contains(account_info.gaia.ToString()));
}

TEST_F(AccountPreviewDataServiceTest, LoadsCachedDataFromPrefs) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  // Pre-populate the dictionary pref.
  {
    ScopedDictPrefUpdate update(&prefs_, prefs::kAccountPreviewDataDict);
    AccountPreviewData cached_data;
    cached_data.password_count = 42;
    cached_data.bookmark_count = 7;
    cached_data.history_count = 3;
    update->Set(account_info.gaia.ToString(),
                AccountPreviewData::Serialize(cached_data));
  }

  // Re-create the service to simulate startup.
  service_.reset();
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &prefs_);

  std::optional<AccountPreviewData> data =
      service_->GetAccountPreviewData(account_info.gaia);
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(42, data->password_count);
  EXPECT_EQ(7, data->bookmark_count);
  EXPECT_EQ(3, data->history_count);
}

TEST_F(AccountPreviewDataServiceTest, PeriodicRefreshDefersUntilTokensLoaded) {
  // Destroy the service created in SetUp to prevent it from fetching when we
  // make the account available.
  service_.reset();

  // Make an account available.
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");

  // Simulate tokens not loaded yet.
  identity_test_env_.ResetToAccountsNotYetLoadedFromDiskState();

  // Clear the timer last update pref so that the recreated service's timer
  // fires immediately on startup.
  prefs_.ClearPref(prefs::kAccountPreviewDataLastUpdatePref);

  // Re-create the service. It will try to refresh on startup because pref is
  // empty, but it should defer because tokens are not loaded.
  service_ = std::make_unique<AccountPreviewDataServiceImpl>(
      identity_test_env_.identity_manager(), &prefs_);

  // Verify that it did NOT fetch yet (pref is empty).
  EXPECT_FALSE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                   .contains(account_info.gaia.ToString()));

  // Simulate tokens loaded. This should trigger the deferred refresh.
  identity_test_env_.ReloadAccountsFromDisk();

  // Verify that it HAS fetched now (pref contains the account).
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(account_info.gaia.ToString()));
}

TEST_F(AccountPreviewDataServiceTest, ClearsInvalidDataOnCookieUpdate) {
  // 1. Setup: Make two accounts available.
  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("account1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("account2@gmail.com");

  // Populate cached data and prefs for both (using the minimal fetcher).
  service_->OnRefreshTokenUpdatedForAccount(account1);
  service_->OnRefreshTokenUpdatedForAccount(account2);

  // Verify both are present in prefs and cache.
  ASSERT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(account1.gaia.ToString()));
  ASSERT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(account2.gaia.ToString()));
  ASSERT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  ASSERT_TRUE(service_->GetAccountPreviewData(account2.gaia).has_value());

  // 2. Trigger: Set cookies to only contain account1 (removing account2).
  identity_test_env_.SetCookieAccounts({{account1.email, account1.gaia}});

  // 3. Assert: account2's data should be removed, account1's should remain.
  EXPECT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(account1.gaia.ToString()));
  EXPECT_FALSE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                   .contains(account2.gaia.ToString()));

  EXPECT_TRUE(service_->GetAccountPreviewData(account1.gaia).has_value());
  EXPECT_FALSE(service_->GetAccountPreviewData(account2.gaia).has_value());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AccountPreviewDataServiceTest,
       ClearsInvalidDataOnPrimaryAccountCleared) {
  // 1. Setup: Make a primary account available.
  AccountInfo primary_info = identity_test_env_.MakePrimaryAccountAvailable(
      "primary@gmail.com", ConsentLevel::kSignin);

  // Populate data (minimal fetcher).
  service_->OnRefreshTokenUpdatedForAccount(primary_info);

  ASSERT_TRUE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                  .contains(primary_info.gaia.ToString()));
  ASSERT_TRUE(service_->GetAccountPreviewData(primary_info.gaia).has_value());

  // 2. Trigger: Clear the primary account.
  identity_test_env_.ClearPrimaryAccount();

  // 3. Assert: Its data should be removed.
  EXPECT_FALSE(prefs_.GetDict(prefs::kAccountPreviewDataDict)
                   .contains(primary_info.gaia.ToString()));
  EXPECT_FALSE(service_->GetAccountPreviewData(primary_info.gaia).has_value());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace signin
