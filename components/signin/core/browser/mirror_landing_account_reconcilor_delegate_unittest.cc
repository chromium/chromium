// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mirror_landing_account_reconcilor_delegate.h"

#include <string>

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

const char* kPrimaryAccount = "primary@gmail.com";
const char* kSecondaryAccount = "secondary@gmail.com";

gaia::ListedAccount BuildListedAccount(const std::string& gaia_id) {
  CoreAccountId account_id = CoreAccountId::FromGaiaId(gaia_id);
  gaia::ListedAccount gaia_account;
  gaia_account.id = account_id;
  gaia_account.email = gaia_id + std::string("@gmail.com");
  gaia_account.gaia_id = gaia_id;
  gaia_account.raw_email = gaia_account.email;
  return gaia_account;
}

void AddPrimaryAndSecondaryAccounts(IdentityTestEnvironment* env,
                                    ConsentLevel level) {
  const CoreAccountId primary_account_id =
      env->MakePrimaryAccountAvailable(kPrimaryAccount, level).account_id;

  // Add secondary account.
  env->MakeAccountAvailable(kSecondaryAccount);

  auto* identity_manager = env->identity_manager();
  EXPECT_EQ(2U, identity_manager->GetAccountsWithRefreshTokens().size());
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
}

}  // namespace

class MirrorLandingAccountReconcilorDelegateTest : public ::testing::Test {
 public:
  MirrorLandingAccountReconcilorDelegateTest(
      const MirrorLandingAccountReconcilorDelegateTest&) = delete;
  MirrorLandingAccountReconcilorDelegateTest& operator=(
      const MirrorLandingAccountReconcilorDelegateTest&) = delete;

 protected:
  MirrorLandingAccountReconcilorDelegateTest();
  ~MirrorLandingAccountReconcilorDelegateTest() override;

  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

  IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

  std::unique_ptr<MirrorLandingAccountReconcilorDelegate>
  CreateMirrorLandingAccountReconcilorDelegate(bool is_main_profile) {
    return std::make_unique<MirrorLandingAccountReconcilorDelegate>(
        identity_manager(), is_main_profile);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

MirrorLandingAccountReconcilorDelegateTest::
    MirrorLandingAccountReconcilorDelegateTest() = default;
MirrorLandingAccountReconcilorDelegateTest::
    ~MirrorLandingAccountReconcilorDelegateTest() = default;

TEST_F(MirrorLandingAccountReconcilorDelegateTest,
       GetChromeAccountsForReconcile) {
  CoreAccountId kPrimaryAccountId = CoreAccountId::FromGaiaId("primary");
  CoreAccountId kOtherAccountId1 = CoreAccountId::FromGaiaId("1");
  CoreAccountId kOtherAccountId2 = CoreAccountId::FromGaiaId("2");
  gaia::ListedAccount gaia_account_primary = BuildListedAccount("primary");
  gaia::ListedAccount gaia_account_1 = BuildListedAccount("1");
  gaia::ListedAccount gaia_account_2 = BuildListedAccount("2");
  gaia::ListedAccount gaia_account_3 = BuildListedAccount("3");

  std::unique_ptr<MirrorLandingAccountReconcilorDelegate> delegate =
      CreateMirrorLandingAccountReconcilorDelegate(/*is_main_profile=*/false);

  // No primary account. Gaia accounts are removed.
  EXPECT_TRUE(
      delegate
          ->GetChromeAccountsForReconcile(
              /*chrome_accounts=*/{},
              /*primary_account=*/CoreAccountId(),
              /*gaia_accounts=*/{gaia_account_1, gaia_account_2},
              /*first_execution=*/true,
              /*primary_has_error=*/false,
              gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER)
          .empty());
  // With primary account. Primary is moved in front, account 1 is kept in the
  // same slot, account 2 is added, account 3 is removed.
  EXPECT_EQ(delegate->GetChromeAccountsForReconcile(
                /*chrome_accounts=*/{kOtherAccountId1, kOtherAccountId2,
                                     kPrimaryAccountId},
                /*primary_account=*/kPrimaryAccountId,
                /*gaia_accounts=*/
                {gaia_account_3, gaia_account_primary, gaia_account_1},
                /*first_execution=*/true,
                /*primary_has_error=*/false,
                gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER),
            (std::vector<CoreAccountId>{kPrimaryAccountId, kOtherAccountId2,
                                        kOtherAccountId1}));
  // Primary account error causes a logout.
  EXPECT_TRUE(
      delegate
          ->GetChromeAccountsForReconcile(
              /*chrome_accounts=*/{kPrimaryAccountId, kOtherAccountId1},
              /*primary_account=*/kPrimaryAccountId,
              /*gaia_accounts=*/{gaia_account_primary, gaia_account_1},
              /*first_execution=*/true,
              /*primary_has_error=*/true,
              gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER)
          .empty());
}

TEST_F(MirrorLandingAccountReconcilorDelegateTest,
       DeleteCookieSecondaryNonSyncingProfile) {
  AddPrimaryAndSecondaryAccounts(identity_test_env(), ConsentLevel::kSignin);

  CreateMirrorLandingAccountReconcilorDelegate(/*is_main_profile=*/false)
      ->OnAccountsCookieDeletedByUserAction(
          /*synced_data_deletion_in_progress=*/false);
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

// Tests that delete cookies in syncing profile does nothing.
TEST_F(MirrorLandingAccountReconcilorDelegateTest, DeleteCookieSyncingProfile) {
  // TODO(https://crbug.com.1464523): Migrate away from `ConsentLevel::kSync` on
  // Lacros.
  AddPrimaryAndSecondaryAccounts(identity_test_env(), ConsentLevel::kSync);

  CreateMirrorLandingAccountReconcilorDelegate(/*is_main_profile=*/false)
      ->OnAccountsCookieDeletedByUserAction(
          /*synced_data_deletion_in_progress=*/false);

  // No account has been removed.
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          CoreAccountId::FromGaiaId("primary")));
}

// Tests that delete cookies in main profile does nothing
TEST_F(MirrorLandingAccountReconcilorDelegateTest, DeleteCookieMainProfile) {
  AddPrimaryAndSecondaryAccounts(identity_test_env(), ConsentLevel::kSignin);

  CreateMirrorLandingAccountReconcilorDelegate(/*is_main_profile=*/true)
      ->OnAccountsCookieDeletedByUserAction(
          /*synced_data_deletion_in_progress=*/false);

  // No account has been removed.
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          CoreAccountId::FromGaiaId("primary")));
}

}  // namespace signin
