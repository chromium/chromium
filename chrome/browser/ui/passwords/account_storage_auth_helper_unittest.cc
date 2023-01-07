// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"

#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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

  MOCK_METHOD(void,
              ShowDiceAddAccountTab,
              (signin_metrics::AccessPoint, const std::string&),
              (override));
};

}  // namespace

class AccountStorageAuthHelperTest : public ::testing::Test {
 public:
  AccountStorageAuthHelperTest()
      : auth_helper_(
            identity_test_env_.identity_manager(),
            &mock_password_feature_manager_,
            base::BindLambdaForTesting([this]() -> SigninViewController* {
              return &mock_signin_view_controller_;
            })) {}
  ~AccountStorageAuthHelperTest() override = default;

  CoreAccountId MakeUnconsentedAccountAvailable() {
    return identity_test_env_
        .MakePrimaryAccountAvailable("alice@gmail.com",
                                     signin::ConsentLevel::kSignin)
        .account_id;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  password_manager::MockPasswordFeatureManager mock_password_feature_manager_;
  MockSigninViewController mock_signin_view_controller_;
  AccountStorageAuthHelper auth_helper_;
};

TEST_F(AccountStorageAuthHelperTest, ShouldTriggerReauthForPrimaryAccount) {
  CoreAccountId account_id = MakeUnconsentedAccountAvailable();
  EXPECT_CALL(mock_signin_view_controller_,
              ShowReauthPrompt(account_id, kReauthAccessPoint, _));

  auth_helper_.TriggerOptInReauth(kReauthAccessPoint, base::DoNothing());
}

TEST_F(AccountStorageAuthHelperTest, ShouldSetOptInOnSucessfulReauth) {
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

TEST_F(AccountStorageAuthHelperTest, ShouldTriggerSigninIfDiceEnabled) {
  const signin_metrics::AccessPoint kAccessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN;
  EXPECT_CALL(mock_signin_view_controller_,
              ShowDiceAddAccountTab(kAccessPoint, _));

  auth_helper_.TriggerSignIn(kAccessPoint);
}
