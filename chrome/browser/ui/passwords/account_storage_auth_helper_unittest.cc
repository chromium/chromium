// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

const signin_metrics::ReauthAccessPoint kReauthAccessPoint =
    signin_metrics::ReauthAccessPoint::kAutofillDropdown;

class MockSigninViewController : public SigninViewController {
 public:
  MockSigninViewController() : SigninViewController(/*browser=*/nullptr) {}
  ~MockSigninViewController() override = default;

  MOCK_METHOD(std::unique_ptr<ReauthAbortHandle>,
              ShowReauthPrompt,
              (const CoreAccountId&,
               signin_metrics::ReauthAccessPoint,
               base::OnceCallback<void(signin::ReauthResult)>),
              (override));
};

class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile*,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile*,
               const std::string&,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
};

}  // namespace

class AccountStorageAuthHelperTest : public ::testing::Test {
 public:
  AccountStorageAuthHelperTest()
      : profile_(IdentityTestEnvironmentProfileAdaptor::
                     CreateProfileForIdentityTestEnvironment()),
        identity_env_adaptor_(profile_.get()),
        auth_helper_(
            profile_.get(),
            identity_test_env()->identity_manager(),
            &mock_password_feature_manager_,
            base::BindLambdaForTesting([this]() -> SigninViewController* {
              return &mock_signin_view_controller_;
            })) {}
  ~AccountStorageAuthHelperTest() override = default;

  CoreAccountId MakeUnconsentedAccountAvailable() {
    return identity_test_env()
        ->MakePrimaryAccountAvailable("alice@gmail.com",
                                      signin::ConsentLevel::kSignin)
        .account_id;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_env_adaptor_.identity_test_env();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  IdentityTestEnvironmentProfileAdaptor identity_env_adaptor_;
  password_manager::MockPasswordFeatureManager mock_password_feature_manager_;
  MockSigninViewController mock_signin_view_controller_;
  AccountStorageAuthHelper auth_helper_;
};

TEST_F(AccountStorageAuthHelperTest, ShouldTriggerReauthForPrimaryAccount) {
  // Opt-in reauth is disabled with explicit signin.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);

  CoreAccountId account_id = MakeUnconsentedAccountAvailable();
  EXPECT_CALL(mock_signin_view_controller_,
              ShowReauthPrompt(account_id, kReauthAccessPoint, _));

  auth_helper_.TriggerOptInReauth(kReauthAccessPoint, base::DoNothing());
}

TEST_F(AccountStorageAuthHelperTest, ShouldSetOptInOnSucessfulReauth) {
  // Opt-in reauth is disabled with explicit signin.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);

  CoreAccountId account_id = MakeUnconsentedAccountAvailable();
  EXPECT_CALL(mock_signin_view_controller_,
              ShowReauthPrompt(account_id, kReauthAccessPoint, _))
      .WillOnce([](auto, auto,
                   base::OnceCallback<void(signin::ReauthResult)> callback) {
        std::move(callback).Run(signin::ReauthResult::kSuccess);
        return nullptr;
      });
  EXPECT_CALL(mock_password_feature_manager_, OptInToAccountStorage);

  auth_helper_.TriggerOptInReauth(kReauthAccessPoint, base::DoNothing());
}

TEST_F(AccountStorageAuthHelperTest, ShouldNotSetOptInOnFailedReauth) {
  // Opt-in reauth is disabled with explicit signin.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);

  CoreAccountId account_id = MakeUnconsentedAccountAvailable();
  EXPECT_CALL(mock_signin_view_controller_,
              ShowReauthPrompt(account_id, kReauthAccessPoint, _))
      .WillOnce([](auto, auto,
                   base::OnceCallback<void(signin::ReauthResult)> callback) {
        std::move(callback).Run(signin::ReauthResult::kCancelled);
        return nullptr;
      });
  EXPECT_CALL(mock_password_feature_manager_, OptInToAccountStorage).Times(0);

  auth_helper_.TriggerOptInReauth(kReauthAccessPoint, base::DoNothing());
}

TEST_F(AccountStorageAuthHelperTest, ShouldTriggerSignin) {
  testing::StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset =
      signin_ui_util::SetSigninUiDelegateForTesting(&mock_signin_ui_delegate);
  const signin_metrics::AccessPoint kAccessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN;
  EXPECT_CALL(mock_signin_ui_delegate,
              ShowSigninUI(profile_.get(), /*enable_sync=*/false, kAccessPoint,
                           signin_metrics::PromoAction::
                               PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT));

  auth_helper_.TriggerSignIn(kAccessPoint);
}
