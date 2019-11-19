// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/multilogin_parameters.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestAccountEmail[] = "test_user@test.com";
const char kTestOtherAccountEmail[] = "test_other_user@test.com";
const char kTestAccountGaiaId[] = "gaia_id_for_test_user_test.com";
const char kTestAccessToken[] = "access_token";
const char kTestUberToken[] = "test_uber_token";
const char kTestOAuthMultiLoginResponse[] = R"(
    { "status": "OK",
      "cookies":[
        {
          "name":"CookieName",
          "value":"CookieValue",
          "domain":".google.com",
          "path":"/"
        }
      ]
    })";

enum class AccountsCookiesMutatorAction {
  kAddAccountToCookie,
  kSetAccountsInCookie,
  kTriggerCookieJarUpdateNoAccounts,
  kTriggerCookieJarUpdateOneAccount,
  kTriggerOnCookieChangeNoAccounts,
};

}  // namespace

namespace signin {
class AccountsCookieMutatorTest : public testing::Test {
 public:
  const CoreAccountId kTestUnavailableAccountId;
  const CoreAccountId kTestOtherUnavailableAccountId;

  AccountsCookieMutatorTest()
      : kTestUnavailableAccountId("unavailable_account_id"),
        kTestOtherUnavailableAccountId("other_unavailable_account_id"),
        test_signin_client_(&prefs_),
        identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &prefs_,
                           AccountConsistencyMethod::kDisabled,
                           &test_signin_client_) {}

  ~AccountsCookieMutatorTest() override {}

  // Make an account available and returns the account ID.
  CoreAccountId AddAcountWithRefreshToken(const std::string& email) {
    return identity_test_env_.MakeAccountAvailable(email).account_id;
  }

  // Feed the TestURLLoaderFactory with the responses for the requests that will
  // be created by UberTokenFetcher when mergin accounts into the cookie jar.
  void PrepareURLLoaderResponsesForAction(AccountsCookiesMutatorAction action) {
    switch (action) {
      case AccountsCookiesMutatorAction::kAddAccountToCookie:
        GetTestURLLoaderFactory()->AddResponse(
            GaiaUrls::GetInstance()
                ->oauth1_login_url()
                .Resolve(base::StringPrintf("?source=%s&issueuberauth=1",
                                            GaiaConstants::kChromeSource))
                .spec(),
            kTestUberToken, net::HTTP_OK);

        GetTestURLLoaderFactory()->AddResponse(
            GaiaUrls::GetInstance()
                ->GetCheckConnectionInfoURLWithSource(
                    GaiaConstants::kChromeSource)
                .spec(),
            std::string(), net::HTTP_OK);

        GetTestURLLoaderFactory()->AddResponse(
            GaiaUrls::GetInstance()
                ->merge_session_url()
                .Resolve(base::StringPrintf(
                    "?uberauth=%s&continue=http://www.google.com&source=%s",
                    kTestUberToken, GaiaConstants::kChromeSource))
                .spec(),
            std::string(), net::HTTP_OK);
        break;
      case AccountsCookiesMutatorAction::kSetAccountsInCookie:
        GetTestURLLoaderFactory()->AddResponse(
            GaiaUrls::GetInstance()
                ->oauth_multilogin_url()
                .Resolve(base::StringPrintf("?source=%s&mlreuse=0",
                                            GaiaConstants::kChromeSource))
                .spec(),
            std::string(kTestOAuthMultiLoginResponse), net::HTTP_OK);
        break;
      case AccountsCookiesMutatorAction::kTriggerCookieJarUpdateNoAccounts:
        SetListAccountsResponseNoAccounts(GetTestURLLoaderFactory());
        break;
      case AccountsCookiesMutatorAction::kTriggerCookieJarUpdateOneAccount:
        SetListAccountsResponseOneAccount(kTestAccountEmail, kTestAccountGaiaId,
                                          GetTestURLLoaderFactory());
        break;
      case AccountsCookiesMutatorAction::kTriggerOnCookieChangeNoAccounts:
        SetListAccountsResponseNoAccounts(GetTestURLLoaderFactory());
        break;
    }
  }

  IdentityTestEnvironment* identity_test_env() { return &identity_test_env_; }

  TestIdentityManagerObserver* identity_manager_observer() {
    return identity_test_env_.identity_manager_observer();
  }

  AccountsCookieMutator* accounts_cookie_mutator() {
    return identity_test_env_.identity_manager()->GetAccountsCookieMutator();
  }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    return test_signin_client_.GetTestURLLoaderFactory();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient test_signin_client_;
  IdentityTestEnvironment identity_test_env_;

  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutatorTest);
};

// Test that adding a non existing account without providing an access token
// results in an error due to such account not being available.
TEST_F(AccountsCookieMutatorTest, AddAccountToCookie_NonExistingAccount) {
  base::RunLoop run_loop;
  CoreAccountId account_id_from_add_account_to_cookie_completed_callback;
  GoogleServiceAuthError error_from_add_account_to_cookie_completed_callback;
  auto completion_callback =
      base::BindLambdaForTesting([&](const CoreAccountId& account_id,
                                     const GoogleServiceAuthError& error) {
        account_id_from_add_account_to_cookie_completed_callback = account_id;
        error_from_add_account_to_cookie_completed_callback = error;
        run_loop.Quit();
      });

  accounts_cookie_mutator()->AddAccountToCookie(kTestUnavailableAccountId,
                                                gaia::GaiaSource::kChrome,
                                                std::move(completion_callback));
  run_loop.Run();

  EXPECT_EQ(account_id_from_add_account_to_cookie_completed_callback,
            kTestUnavailableAccountId);
  EXPECT_EQ(error_from_add_account_to_cookie_completed_callback.state(),
            GoogleServiceAuthError::USER_NOT_SIGNED_UP);
}

// Test that adding an already available account without providing an access
// token results in such account being successfully merged into the cookie jar.
TEST_F(AccountsCookieMutatorTest, AddAccountToCookie_ExistingAccount) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kAddAccountToCookie);
  // Adding an account with refresh token will trigger a cookie jar update.
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerCookieJarUpdateNoAccounts);

  CoreAccountId account_id = AddAcountWithRefreshToken(kTestAccountEmail);
  base::RunLoop run_loop;
  CoreAccountId account_id_from_add_account_to_cookie_completed_callback;
  GoogleServiceAuthError error_from_add_account_to_cookie_completed_callback;
  auto completion_callback =
      base::BindLambdaForTesting([&](const CoreAccountId& account_id,
                                     const GoogleServiceAuthError& error) {
        account_id_from_add_account_to_cookie_completed_callback = account_id;
        error_from_add_account_to_cookie_completed_callback = error;
        run_loop.Quit();
      });

  accounts_cookie_mutator()->AddAccountToCookie(
      account_id, gaia::GaiaSource::kChrome, std::move(completion_callback));

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, kTestAccessToken,
      base::Time::Now() + base::TimeDelta::FromHours(1));
  run_loop.Run();

  EXPECT_EQ(account_id_from_add_account_to_cookie_completed_callback,
            account_id);
  EXPECT_EQ(error_from_add_account_to_cookie_completed_callback.state(),
            GoogleServiceAuthError::NONE);
}

// Test that adding a non existing account along with an access token, results
// on such account being successfully merged into the cookie jar.
TEST_F(AccountsCookieMutatorTest,
       AddAccountToCookieWithAccessToken_NonExistingAccount) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kAddAccountToCookie);

  base::RunLoop run_loop;
  CoreAccountId account_id_from_add_account_to_cookie_completed_callback;
  GoogleServiceAuthError error_from_add_account_to_cookie_completed_callback;
  auto completion_callback =
      base::BindLambdaForTesting([&](const CoreAccountId& account_id,
                                     const GoogleServiceAuthError& error) {
        account_id_from_add_account_to_cookie_completed_callback = account_id;
        error_from_add_account_to_cookie_completed_callback = error;
        run_loop.Quit();
      });

  accounts_cookie_mutator()->AddAccountToCookieWithToken(
      kTestUnavailableAccountId, kTestAccessToken, gaia::GaiaSource::kChrome,
      std::move(completion_callback));
  run_loop.Run();

  EXPECT_EQ(account_id_from_add_account_to_cookie_completed_callback,
            kTestUnavailableAccountId);
  EXPECT_EQ(error_from_add_account_to_cookie_completed_callback.state(),
            GoogleServiceAuthError::NONE);
}

// Test that adding an already available account along with an access token,
// results in such account being successfully merged into the cookie jar.
TEST_F(AccountsCookieMutatorTest,
       AddAccountToCookieWithAccessToken_ExistingAccount) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kAddAccountToCookie);
  // Adding an account with refresh token will trigger a cookie jar update.
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerCookieJarUpdateNoAccounts);

  CoreAccountId account_id = AddAcountWithRefreshToken(kTestAccountEmail);
  base::RunLoop run_loop;
  CoreAccountId account_id_from_add_account_to_cookie_completed_callback;
  GoogleServiceAuthError error_from_add_account_to_cookie_completed_callback;
  auto completion_callback =
      base::BindLambdaForTesting([&](const CoreAccountId& account_id,
                                     const GoogleServiceAuthError& error) {
        account_id_from_add_account_to_cookie_completed_callback = account_id;
        error_from_add_account_to_cookie_completed_callback = error;
        run_loop.Quit();
      });

  accounts_cookie_mutator()->AddAccountToCookieWithToken(
      account_id, kTestAccessToken, gaia::GaiaSource::kChrome,
      std::move(completion_callback));

  run_loop.Run();

  EXPECT_EQ(account_id_from_add_account_to_cookie_completed_callback,
            account_id);
  EXPECT_EQ(error_from_add_account_to_cookie_completed_callback.state(),
            GoogleServiceAuthError::NONE);
}

// Test that trying to set a list of accounts in the cookie jar where none of
// those accounts have refresh tokens in IdentityManager results in an error.
TEST_F(AccountsCookieMutatorTest, SetAccountsInCookie_AllNonExistingAccounts) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kSetAccountsInCookie);

  base::RunLoop run_loop;
  MultiloginParameters parameters = {
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {kTestUnavailableAccountId, kTestOtherUnavailableAccountId}};
  accounts_cookie_mutator()->SetAccountsInCookie(
      parameters, gaia::GaiaSource::kChrome,
      base::BindOnce(
          [](base::OnceClosure quit_closure, SetAccountsInCookieResult result) {
            EXPECT_EQ(result, SetAccountsInCookieResult::kPersistentError);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  run_loop.Run();
}

// Test that trying to set a list of accounts in the cookie jar where some of
// those accounts have no refresh tokens in IdentityManager results in an error.
TEST_F(AccountsCookieMutatorTest, SetAccountsInCookie_SomeNonExistingAccounts) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kSetAccountsInCookie);
  // Adding an account with refresh token will trigger a cookie jar update.
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerCookieJarUpdateNoAccounts);

  CoreAccountId account_id = AddAcountWithRefreshToken(kTestAccountEmail);
  base::RunLoop run_loop;
  MultiloginParameters parameters = {
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {account_id, kTestUnavailableAccountId}};
  accounts_cookie_mutator()->SetAccountsInCookie(
      parameters, gaia::GaiaSource::kChrome,
      base::BindOnce(
          [](base::OnceClosure quit_closure, SetAccountsInCookieResult result) {
            EXPECT_EQ(result, SetAccountsInCookieResult::kPersistentError);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

// Test that trying to set a list of accounts in the cookie jar where all of
// those accounts have refresh tokens in IdentityManager results in them being
// successfully set.
TEST_F(AccountsCookieMutatorTest, SetAccountsInCookie_AllExistingAccounts) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kSetAccountsInCookie);
  // Adding an account with refresh token will trigger a cookie jar update.
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerCookieJarUpdateNoAccounts);

  CoreAccountId account_id = AddAcountWithRefreshToken(kTestAccountEmail);
  CoreAccountId other_account_id =
      AddAcountWithRefreshToken(kTestOtherAccountEmail);
  base::RunLoop run_loop;
  MultiloginParameters parameters = {
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {account_id, other_account_id}};
  accounts_cookie_mutator()->SetAccountsInCookie(
      parameters, gaia::GaiaSource::kChrome,
      base::BindOnce(
          [](base::OnceClosure quit_closure, SetAccountsInCookieResult result) {
            EXPECT_EQ(result, SetAccountsInCookieResult::kSuccess);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, kTestAccessToken,
      base::Time::Now() + base::TimeDelta::FromHours(1));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      other_account_id, kTestAccessToken,
      base::Time::Now() + base::TimeDelta::FromHours(1));

  run_loop.Run();
}

// Test triggering the update of a cookie jar with no accounts works.
TEST_F(AccountsCookieMutatorTest, TriggerCookieJarUpdate_NoListedAccounts) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerCookieJarUpdateNoAccounts);

  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());
  accounts_cookie_mutator()->TriggerCookieJarUpdate();
  run_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_jar_info =
      identity_manager_observer()
          ->AccountsInfoFromAccountsInCookieUpdatedCallback();
  EXPECT_EQ(accounts_in_jar_info.signed_in_accounts.size(), 0U);
  EXPECT_EQ(accounts_in_jar_info.signed_out_accounts.size(), 0U);
  EXPECT_TRUE(accounts_in_jar_info.accounts_are_fresh);

  EXPECT_EQ(identity_manager_observer()
                ->ErrorFromAccountsInCookieUpdatedCallback()
                .state(),
            GoogleServiceAuthError::NONE);
}

// Test triggering the update of a cookie jar with one accounts works and that
// the received accounts match the data injected via the TestURLLoaderFactory.
TEST_F(AccountsCookieMutatorTest, TriggerCookieJarUpdate_OneListedAccounts) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerCookieJarUpdateOneAccount);

  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());
  accounts_cookie_mutator()->TriggerCookieJarUpdate();
  run_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_jar_info =
      identity_manager_observer()
          ->AccountsInfoFromAccountsInCookieUpdatedCallback();
  EXPECT_EQ(accounts_in_jar_info.signed_in_accounts.size(), 1U);
  EXPECT_EQ(accounts_in_jar_info.signed_in_accounts[0].gaia_id,
            kTestAccountGaiaId);
  EXPECT_EQ(accounts_in_jar_info.signed_in_accounts[0].email,
            kTestAccountEmail);

  EXPECT_EQ(accounts_in_jar_info.signed_out_accounts.size(), 0U);
  EXPECT_TRUE(accounts_in_jar_info.accounts_are_fresh);

  EXPECT_EQ(identity_manager_observer()
                ->ErrorFromAccountsInCookieUpdatedCallback()
                .state(),
            GoogleServiceAuthError::NONE);
}

#if defined(OS_IOS)
TEST_F(AccountsCookieMutatorTest, ForceTriggerOnCookieChange) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kTriggerOnCookieChangeNoAccounts);

  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  // Forces the processing of OnCookieChange and it calls
  // OnGaiaAccountsInCookieUpdated.
  accounts_cookie_mutator()->ForceTriggerOnCookieChange();
  run_loop.Run();
}
#endif

// Test that trying to log out all sessions generates the right network request.
TEST_F(AccountsCookieMutatorTest, LogOutAllAccounts) {
  base::RunLoop run_loop;
  GetTestURLLoaderFactory()->SetInterceptor(base::BindRepeating(
      [](base::OnceClosure quit_closure,
         const network::ResourceRequest& request) {
        EXPECT_EQ(request.url.spec(),
                  GaiaUrls::GetInstance()
                      ->LogOutURLWithSource(GaiaConstants::kChromeSource)
                      .spec());
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));

  accounts_cookie_mutator()->LogOutAllAccounts(gaia::GaiaSource::kChrome);
  run_loop.Run();
}

}  // namespace signin
