// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/browser/web_signin_tracker.h"

#include <memory>
#include <optional>
#include <set>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/mirror_account_reconcilor_delegate.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/mock_account_manager_facade.h"
#endif

using ::testing::_;

namespace signin {

class WebSigninTrackerTest : public ::testing::Test {
 public:
  WebSigninTrackerTest()
      : signin_client_(&prefs_),
        identity_test_env_(nullptr, &prefs_, &signin_client_) {
    account_reconcilor_ = std::make_unique<AccountReconcilor>(
        identity_test_env_.identity_manager(), &signin_client_,
#if BUILDFLAG(IS_CHROMEOS)
        &mock_facade_,
#endif
        std::make_unique<MirrorAccountReconcilorDelegate>(
            identity_test_env_.identity_manager()));
    account_reconcilor_->RegisterProfilePrefs(prefs_.registry());
    account_reconcilor_->Initialize(
        /*start_reconcile_if_tokens_available=*/false);
  }

  WebSigninTrackerTest(const WebSigninTrackerTest&) = delete;
  WebSigninTrackerTest& operator=(const WebSigninTrackerTest&) = delete;

  ~WebSigninTrackerTest() override { account_reconcilor_->Shutdown(); }

  std::unique_ptr<WebSigninTracker> CreateWebSigninTracker(
      CoreAccountId account,
      base::OnceCallback<void(WebSigninTracker::Result)> callback,
      std::optional<base::TimeDelta> timeout = std::nullopt) {
    return std::make_unique<WebSigninTracker>(
        identity_test_env_.identity_manager(), account_reconcilor_.get(),
        account, std::move(callback), timeout);
  }

  ConsentLevel GetConsentLevel() const {
#if BUILDFLAG(IS_CHROMEOS)
    return ConsentLevel::kSync;
#else
    return ConsentLevel::kSignin;
#endif
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_;
  IdentityTestEnvironment identity_test_env_;
#if BUILDFLAG(IS_CHROMEOS)
  account_manager::MockAccountManagerFacade mock_facade_;
#endif
  std::unique_ptr<AccountReconcilor> account_reconcilor_;
};

TEST_F(WebSigninTrackerTest,
       CookiesWithSigninAccountShouldTriggerSuccessResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get());
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kSuccess));

  identity_test_env_.SetPrimaryAccount(account.email, GetConsentLevel());
  CookieParamsForTest cookie_params{account.email, account.gaia};
  identity_test_env_.SetCookieAccounts({cookie_params});
}

TEST_F(WebSigninTrackerTest,
       DeferredCreationAndCookiesWithSigninAccountShouldTriggerSuccessResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  identity_test_env_.SetPrimaryAccount(account.email, GetConsentLevel());
  CookieParamsForTest cookie_params{account.email, account.gaia};
  identity_test_env_.SetCookieAccounts({cookie_params});

  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kSuccess));
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get());
}

TEST_F(WebSigninTrackerTest,
       CookiesWithoutSigninAccountShouldNotTriggerResult) {
  AccountInfo signin_account =
      identity_test_env_.MakeAccountAvailable("test1@gmail.com");
  AccountInfo non_signin_account =
      identity_test_env_.MakeAccountAvailable("test2@gmail.com");
  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(signin_account.account_id, callback.Get());
  EXPECT_CALL(callback, Run(_)).Times(0);

  identity_test_env_.SetPrimaryAccount(non_signin_account.email,
                                       GetConsentLevel());
  CookieParamsForTest cookie_params{non_signin_account.email,
                                    non_signin_account.gaia};
  identity_test_env_.SetCookieAccounts({cookie_params});
}

TEST_F(WebSigninTrackerTest, ReconcilorAuthErrorShouldTriggerAuthErrorResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get());

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kAuthError))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  identity_test_env_.SetInvalidRefreshTokenForAccount(account.account_id);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  identity_test_env_.SetCookieAccounts({});
  identity_test_env_.SetPrimaryAccount(account.email, GetConsentLevel());
  run_loop.Run();
}

TEST_F(WebSigninTrackerTest,
       DeferredCreationAndReconcilorAuthErrorShouldTriggerAuthErrorResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  identity_test_env_.SetInvalidRefreshTokenForAccount(account.account_id);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  identity_test_env_.SetCookieAccounts({});
  identity_test_env_.SetPrimaryAccount(account.email, GetConsentLevel());

  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kAuthError));
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get());
}

TEST_F(WebSigninTrackerTest,
       ReconcilorNonAuthErrorShouldTriggerOtherErrorResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get());
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kOtherError))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  identity_test_env_.SetPrimaryAccount(account.email, GetConsentLevel());
  identity_test_env_.SetCookieAccounts({});

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR));

  run_loop.Run();
}

TEST_F(WebSigninTrackerTest,
       DeferredCreationAndReconcilorNonAuthErrorShouldTriggerOtherErrorResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  identity_test_env_.SetPrimaryAccount(account.email, GetConsentLevel());
  identity_test_env_.SetCookieAccounts({});

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR));

  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kOtherError));
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get());
}

TEST_F(WebSigninTrackerTest, TimeoutResult) {
  AccountInfo account =
      identity_test_env_.MakeAccountAvailable("test@gmail.com");
  base::MockOnceCallback<void(WebSigninTracker::Result)> callback;
  base::TimeDelta timeout = base::Seconds(30);
  std::unique_ptr<WebSigninTracker> web_signin_bridge =
      CreateWebSigninTracker(account.account_id, callback.Get(), timeout);

  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(WebSigninTracker::Result::kTimeout))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  task_environment_.FastForwardBy(timeout);

  run_loop.Run();
}

}  // namespace signin
