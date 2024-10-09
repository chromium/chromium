// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
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
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestAccountEmail[] = "test_user@test.com";
const char kTestOtherAccountEmail[] = "test_other_user@test.com";
const char kTestAccountGaiaId[] = "gaia_id_for_test_user_test.com";
const char kTestAccessToken[] = "access_token";
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
  kSetAccountsInCookie,
  kTriggerCookieJarUpdateNoAccounts,
  kTriggerCookieJarUpdateOneAccount,
  kTriggerOnCookieChangeNoAccounts,
  kLogOutFromCookie,
};

}  // namespace

namespace signin {
class AccountsCookieMutatorTest
    : public testing::Test,
      public AccountsCookieMutator::PartitionDelegate {
 public:
  const CoreAccountId kTestUnavailableAccountId;
  const CoreAccountId kTestOtherUnavailableAccountId;

  AccountsCookieMutatorTest()
      : kTestUnavailableAccountId(
            CoreAccountId::FromGaiaId("unavailable_account_id")),
        kTestOtherUnavailableAccountId(
            CoreAccountId::FromGaiaId("other_unavailable_account_id")),
        test_signin_client_(&prefs_),
        identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &prefs_,
                           &test_signin_client_) {}

  AccountsCookieMutatorTest(const AccountsCookieMutatorTest&) = delete;
  AccountsCookieMutatorTest& operator=(const AccountsCookieMutatorTest&) =
      delete;

  ~AccountsCookieMutatorTest() override = default;

  // Make an account available and returns the account ID.
  CoreAccountId AddAcountWithRefreshToken(const std::string& email) {
    return identity_test_env_.MakeAccountAvailable(email).account_id;
  }

  // Feed the TestURLLoaderFactory with the responses for the requests that will
  // be created by GaiaCookieManagerService for mutating the accounts in the
  // cookie jar.
  void PrepareURLLoaderResponsesForAction(AccountsCookiesMutatorAction action) {
    switch (action) {
      case AccountsCookiesMutatorAction::kSetAccountsInCookie:
        GetTestURLLoaderFactory()->AddResponse(
            GaiaUrls::GetInstance()
                ->oauth_multilogin_url()
                .Resolve(base::StringPrintf("?source=%s&reuseCookies=0",
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
      case AccountsCookiesMutatorAction::kLogOutFromCookie:
        GetTestURLLoaderFactory()->AddResponse(
            GaiaUrls::GetInstance()
                ->LogOutURLWithSource(GaiaConstants::kChromeSource)
                .spec(),
            std::string(), net::HTTP_OK);
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
  // AccountsCookieMutator::PartitionDelegate
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override {
    return test_signin_client_.CreateGaiaAuthFetcher(consumer, source);
  }

  network::mojom::CookieManager* GetCookieManagerForPartition() override {
    return &cookie_manager_for_partition_;
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient test_signin_client_;
  IdentityTestEnvironment identity_test_env_;
  network::TestCookieManager cookie_manager_for_partition_;
};

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
      account_id, kTestAccessToken, base::Time::Now() + base::Hours(1));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      other_account_id, kTestAccessToken, base::Time::Now() + base::Hours(1));

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
  EXPECT_EQ(accounts_in_jar_info.GetPotentiallyInvalidSignedInAccounts().size(),
            0U);
  EXPECT_EQ(accounts_in_jar_info.GetSignedOutAccounts().size(), 0U);
  EXPECT_TRUE(accounts_in_jar_info.AreAccountsFresh());

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
  EXPECT_EQ(accounts_in_jar_info.GetPotentiallyInvalidSignedInAccounts().size(),
            1U);
  EXPECT_EQ(
      accounts_in_jar_info.GetPotentiallyInvalidSignedInAccounts()[0].gaia_id,
      kTestAccountGaiaId);
  EXPECT_EQ(
      accounts_in_jar_info.GetPotentiallyInvalidSignedInAccounts()[0].email,
      kTestAccountEmail);

  EXPECT_EQ(accounts_in_jar_info.GetSignedOutAccounts().size(), 0U);
  EXPECT_TRUE(accounts_in_jar_info.AreAccountsFresh());

  EXPECT_EQ(identity_manager_observer()
                ->ErrorFromAccountsInCookieUpdatedCallback()
                .state(),
            GoogleServiceAuthError::NONE);
}

#if BUILDFLAG(IS_IOS)
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
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kLogOutFromCookie);

  base::RunLoop run_loop;
  accounts_cookie_mutator()->LogOutAllAccounts(
      gaia::GaiaSource::kChrome, base::BindOnce(
                                     [](base::OnceClosure quit_closure,
                                        const GoogleServiceAuthError& error) {
                                       EXPECT_EQ(error.state(),
                                                 GoogleServiceAuthError::NONE);
                                       std::move(quit_closure).Run();
                                     },
                                     run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace signin
