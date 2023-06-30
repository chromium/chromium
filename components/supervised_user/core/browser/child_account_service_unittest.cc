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
#include "base/test/task_environment.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/browser/permission_request_creator.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

class MockPermissionRequestCreator : public PermissionRequestCreator {
 public:
  // PermissionRequestCreator implementation.
  bool IsEnabled() const override { return true; }
  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override {}
};
class MockFilterDelegateImpl : public SupervisedUserURLFilter::Delegate {
 public:
  // SupervisedUserURLFilter::Delegate implementation.
  std::string GetCountryCode() override { return std::string(); }
};

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
            signin::AccountConsistencyMethod::kDisabled,
            test_signin_client_.get());

    kids_chrome_management_client_ =
        std::make_unique<KidsChromeManagementClient>(
            weak_wrapped_subresource_loader_factory,
            identity_test_environment_->identity_manager());

    supervised_user_service_ = std::make_unique<SupervisedUserService>(
        identity_test_environment_->identity_manager(),
        kids_chrome_management_client_.get(), syncable_pref_service_,
        settings_service_, sync_service_,
        /*check_webstore_url_callback=*/
        base::BindRepeating([](const GURL& url) { return false; }),
        std::make_unique<MockFilterDelegateImpl>(),
        /*can_show_first_time_interstitial_banner=*/false);

    list_family_members_service_ = std::make_unique<ListFamilyMembersService>(
        identity_test_environment_->identity_manager(),
        weak_wrapped_subresource_loader_factory);

    child_account_service_ = std::make_unique<ChildAccountService>(
        syncable_pref_service_, *supervised_user_service_.get(),
        identity_test_environment_->identity_manager(),
        weak_wrapped_subresource_loader_factory,
        /*check_user_child_status_callback=*/base::DoNothing(),
        std::make_unique<MockPermissionRequestCreator>(),
        *list_family_members_service_.get());
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

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  sync_preferences::TestingPrefServiceSyncable syncable_pref_service_;
  syncer::MockSyncService sync_service_;
  SupervisedUserSettingsService settings_service_;

  std::unique_ptr<TestSigninClient> test_signin_client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<KidsChromeManagementClient> kids_chrome_management_client_;
  std::unique_ptr<SupervisedUserService> supervised_user_service_;
  std::unique_ptr<ListFamilyMembersService> list_family_members_service_;
  std::unique_ptr<ChildAccountService> child_account_service_;
};

TEST_F(ChildAccountServiceTest, GetGoogleAuthStateNotAuthenticated) {
  signin::SetListAccountsResponseNoAccounts(GetTestURLLoaderFactory());
  // Initial state should be PENDING.
  ASSERT_EQ(supervised_user::ChildAccountService::AuthState::PENDING,
            child_account_service_->GetGoogleAuthState());

  // Wait until the response to the ListAccount request triggered by the call
  // above comes back.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(supervised_user::ChildAccountService::AuthState::NOT_AUTHENTICATED,
            child_account_service_->GetGoogleAuthState());
}

TEST_F(ChildAccountServiceTest, GetGoogleAuthStateAuthenticated) {
  // A valid, signed-in account means authenticated.
  signin::SetListAccountsResponseOneAccountWithParams(
      {"me@example.com",
       /*gaia_id=*/"abcdef",
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
}  // namespace supervised_user
