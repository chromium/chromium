// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/child_account_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kEmail[] = "me@example.com";
}  // namespace

namespace supervised_user {

class ChildAccountServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_signin_client_ =
        std::make_unique<TestSigninClient>(&syncable_pref_service_);

    scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
        weak_wrapped_subresource_loader_factory =
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                GetTestURLLoaderFactory());

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>(
            // By passing nullptr we use the default Url loader factory.
            /*test_url_loader_factory=*/nullptr, &syncable_pref_service_,
            test_signin_client_.get());

    settings_service_.Init(syncable_pref_service_.user_prefs_store());
    PrefRegistrySimple* registry = syncable_pref_service_.registry();
    supervised_user::RegisterProfilePrefs(registry);
    registry->RegisterBooleanPref(policy::policy_prefs::kForceGoogleSafeSearch,
                                  false);

    list_family_members_service_ = std::make_unique<ListFamilyMembersService>(
        identity_test_environment_->identity_manager(),
        weak_wrapped_subresource_loader_factory, syncable_pref_service_);

    child_account_service_ = std::make_unique<ChildAccountService>(
        syncable_pref_service_, identity_test_environment_->identity_manager(),
        weak_wrapped_subresource_loader_factory,
        /*check_user_child_status_callback=*/base::DoNothing(),
        *list_family_members_service_.get());

    child_account_service_->Init();
  }

  void TearDown() override {
    settings_service_.Shutdown();
    child_account_service_->Shutdown();
  }

 protected:
  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    return static_cast<TestSigninClient*>(test_signin_client_.get())
        ->GetTestURLLoaderFactory();
  }

  signin::AccountsCookieMutator* GetAccountsCookieMutator() {
    return identity_test_environment_->identity_manager()
        ->GetAccountsCookieMutator();
  }

  PrefService& GetUserPerferences() { return syncable_pref_service_; }

  void SetListAccountsResponseAndTriggerCookieJarUpdate(
      const signin::CookieParams& params) {
    signin::SetListAccountsResponseOneAccountWithParams(
        params, GetTestURLLoaderFactory());
    GetAccountsCookieMutator()->TriggerCookieJarUpdate();
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  sync_preferences::TestingPrefServiceSyncable syncable_pref_service_;
  syncer::MockSyncService sync_service_;
  SupervisedUserSettingsService settings_service_;

  std::unique_ptr<TestSigninClient> test_signin_client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<ListFamilyMembersService> list_family_members_service_;
  std::unique_ptr<ChildAccountService> child_account_service_;
};

TEST_F(ChildAccountServiceTest, GetGoogleAuthStateNoPrimaryAccount) {
  // Initial state should be NOT_AUTHENTICATED, as there is no primary account.
  ASSERT_EQ(supervised_user::ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

TEST_F(ChildAccountServiceTest,
       GetGoogleAuthStatePrimaryAccountNoCookieNoToken) {
  // Sign in.
  AccountInfo account_info =
      identity_test_environment_->MakePrimaryAccountAvailable(
          kEmail, signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account_info, identity_test_environment_->identity_manager(),
      /*is_subject_to_parental_controls=*/true);

  // Wait until the response to the ListAccount request triggered by the call
  // above comes back.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(supervised_user::ChildAccountService::AuthState::
                TRANSIENT_MOVING_TO_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

TEST_F(ChildAccountServiceTest, GetGoogleAuthStateAuthenticated) {
  // Sign in.
  AccountInfo account_info =
      identity_test_environment_->MakePrimaryAccountAvailable(
          kEmail, signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account_info, identity_test_environment_->identity_manager(),
      /*is_subject_to_parental_controls=*/true);

  // A valid, signed-in account means authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {kEmail, account_info.gaia,
       /*valid= */ true,
       /*signed_out=*/false,
       /*verified=*/true},
      GetTestURLLoaderFactory());

  GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(supervised_user::ChildAccountService::AuthState::AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

TEST_F(ChildAccountServiceTest,
       GetGoogleAuthStateNotAuthenticatedWithInvalidAccount) {
  // An invalid (but signed-in) account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", /*gaia_id=*/"abcdef",
       /*valid=*/false,
       /*signed_out=*/false,
       /*verified=*/true},
      GetTestURLLoaderFactory());

  GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(supervised_user::ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

TEST_F(ChildAccountServiceTest, GetGoogleAuthStateNotAuthenticatedNotSignedIn) {
  // A valid but not signed-in account means not authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com", /*gaia_id=*/"abcdef",
       /*valid=*/true,
       /*signed_out=*/true,
       /*verified=*/true},
      GetTestURLLoaderFactory());

  GetAccountsCookieMutator()->TriggerCookieJarUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(supervised_user::ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

// Tests that SafeSearch is correctly enforced for a supervised profile.
TEST_F(ChildAccountServiceTest, UpdateForceGoogleSafeSearch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      supervised_user::kForceSafeSearchForUnauthenticatedSupervisedUsers);

  // SafeSearch should not be forced for signed-out users.
  ASSERT_FALSE(GetUserPerferences().GetBoolean(
      policy::policy_prefs::kForceGoogleSafeSearch));

  // Add supervised account to the identity manager.
  identity_test_environment_->WaitForRefreshTokensLoaded();
  AccountInfo account = identity_test_environment_->MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account, identity_test_environment_->identity_manager(),
      /*is_subject_to_parental_controls=*/true);

  // At this point the user is in a transient state (since their cookies are not
  // up to date) so SafeSearch should be forced).
  ASSERT_TRUE(GetUserPerferences().GetBoolean(
      policy::policy_prefs::kForceGoogleSafeSearch));

  // SafeSearch should not be forced for a fully signed in supervised user.
  SetListAccountsResponseAndTriggerCookieJarUpdate({kEmail, account.gaia,
                                                    /*valid= */ true,
                                                    /*signed_out=*/false,
                                                    /*verified=*/true});
  ASSERT_FALSE(GetUserPerferences().GetBoolean(
      policy::policy_prefs::kForceGoogleSafeSearch));
}

}  // namespace supervised_user
