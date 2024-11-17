// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/buildflag.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_options.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

const char kAccountId1[] = "account_id1";
const char kAccountId2[] = "account_id2";
const char kAccountId3[] = "account_id3";
const char kAccountId4[] = "account_id4";

using MockSetAccountsInCookieCompletedCallback = base::MockCallback<
    GaiaCookieManagerService::SetAccountsInCookieCompletedCallback>;
using MockLogOutFromCookieCompletedCallback = base::MockCallback<
    GaiaCookieManagerService::LogOutFromCookieCompletedCallback>;

class MockObserver {
 public:
  explicit MockObserver(GaiaCookieManagerService* helper) {
    helper->SetGaiaAccountsInCookieUpdatedCallback(base::BindRepeating(
        &MockObserver::OnGaiaAccountsInCookieUpdated, base::Unretained(this)));
  }

  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;

  MOCK_METHOD2(OnGaiaAccountsInCookieUpdated,
               void(const signin::AccountsInCookieJarInfo&,
                    const GoogleServiceAuthError&));
};

// Counts number of InstrumentedGaiaCookieManagerService created.
// We can EXPECT_* to be zero at the end of our unit tests
// to make sure everything is properly deleted.

int total = 0;

net::CanonicalCookie GetTestCookie(const GURL& url, const std::string& name) {
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url, name, /*value=*/"cookie_value", /*domain=*/"." + url.host(),
          /*path=*/"/", /*creation_time=*/base::Time(),
          /*expiration_time=*/base::Time(), /*last_access_time=*/base::Time(),
          /*secure=*/true, /*http_only=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
          /*partition_key=*/std::nullopt, /*status=*/nullptr);
  return *cookie;
}

class InstrumentedGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  InstrumentedGaiaCookieManagerService(
      AccountTrackerService* account_tracker_service,
      ProfileOAuth2TokenService* token_service,
      SigninClient* signin_client)
      : GaiaCookieManagerService(account_tracker_service,
                                 token_service,
                                 signin_client) {
    total++;
  }

  InstrumentedGaiaCookieManagerService(
      const InstrumentedGaiaCookieManagerService&) = delete;
  InstrumentedGaiaCookieManagerService& operator=(
      const InstrumentedGaiaCookieManagerService&) = delete;

  ~InstrumentedGaiaCookieManagerService() override { total--; }

  MOCK_METHOD0(StartFetchingListAccounts, void());
  MOCK_METHOD0(StartGaiaLogOut, void());
  MOCK_METHOD0(StartSetAccounts, void());
};

class GaiaCookieManagerServiceTest : public testing::Test {
 public:
  GaiaCookieManagerServiceTest()
      : account_id1_(CoreAccountId::FromGaiaId(kAccountId1)),
        account_id2_(CoreAccountId::FromGaiaId(kAccountId2)),
        account_id3_(CoreAccountId::FromGaiaId(kAccountId3)),
        account_id4_(CoreAccountId::FromGaiaId(kAccountId4)),
        no_error_(GoogleServiceAuthError::NONE),
        error_(GoogleServiceAuthError::SERVICE_ERROR),
        canceled_(GoogleServiceAuthError::REQUEST_CANCELED),
        account_tracker_service_(CreateAccountTrackerService()) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    GaiaCookieManagerService::RegisterPrefs(pref_service_.registry());
    signin_client_ = std::make_unique<TestSigninClient>(&pref_service_);
    account_tracker_service_ = std::make_unique<AccountTrackerService>();
    account_tracker_service_->Initialize(&pref_service_, base::FilePath());
    token_service_ =
        std::make_unique<FakeProfileOAuth2TokenService>(&pref_service_);
  }

  AccountTrackerService* account_tracker_service() {
    return account_tracker_service_.get();
  }
  ProfileOAuth2TokenService* token_service() { return token_service_.get(); }
  TestSigninClient* signin_client() { return signin_client_.get(); }

  void SimulateAccessTokenFailure(OAuth2AccessTokenManager::Consumer* consumer,
                                  OAuth2AccessTokenManager::Request* request,
                                  const GoogleServiceAuthError& error) {
    consumer->OnGetTokenFailure(request, error);
  }

  void SimulateAccessTokenSuccess(OAuth2AccessTokenManager::Consumer* consumer,
                                  OAuth2AccessTokenManager::Request* request) {
    consumer->OnGetTokenSuccess(request, TokenResponseBuilder()
                                             .WithAccessToken("AccessToken")
                                             .WithIdToken("Idtoken")
                                             .build());
  }

  void SimulateMultiloginFinished(GaiaCookieManagerService* service,
                                  signin::SetAccountsInCookieResult result) {
    service->OnSetAccountsFinished(result);
  }

  void SimulateListAccountsSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnListAccountsSuccess(data);
  }

  void SimulateListAccountsFailure(GaiaAuthConsumer* consumer,
                                   const GoogleServiceAuthError& error) {
    consumer->OnListAccountsFailure(error);
  }

  void SimulateLogOutSuccess(GaiaAuthConsumer* consumer) {
    consumer->OnLogOutSuccess();
  }

  void SimulateLogOutFailure(GaiaAuthConsumer* consumer,
                             const GoogleServiceAuthError& error) {
    consumer->OnLogOutFailure(error);
  }

  void SimulateGetCheckConnectionInfoSuccess(const std::string& data) {
    signin_client_->GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()
            ->GetCheckConnectionInfoURLWithSource(GaiaConstants::kChromeSource)
            .spec(),
        data);
    base::RunLoop().RunUntilIdle();
  }

  void SimulateGetCheckConnectionInfoResult(const std::string& url,
                                            const std::string& result) {
    signin_client_->GetTestURLLoaderFactory()->AddResponse(url, result);
    base::RunLoop().RunUntilIdle();
  }

  void Advance(scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner,
               base::TimeDelta advance_by) {
    test_task_runner->FastForwardBy(advance_by + base::Milliseconds(1));
    test_task_runner->RunUntilIdle();
  }

  bool IsLoadPending(const std::string& url) {
    return signin_client_->GetTestURLLoaderFactory()->IsPending(
        GURL(url).spec());
  }

  bool IsLoadPending() {
    return signin_client_->GetTestURLLoaderFactory()->NumPending() > 0;
  }

  const GoogleServiceAuthError& no_error() { return no_error_; }
  const GoogleServiceAuthError& error() { return error_; }
  const GoogleServiceAuthError& canceled() { return canceled_; }

  scoped_refptr<network::SharedURLLoaderFactory> factory() const {
    return signin_client_->GetURLLoaderFactory();
  }

  const CoreAccountId account_id1_;
  const CoreAccountId account_id2_;
  const CoreAccountId account_id3_;
  const CoreAccountId account_id4_;

 private:
  std::unique_ptr<AccountTrackerService> CreateAccountTrackerService() {
#if BUILDFLAG(IS_ANDROID)
    signin::SetUpMockAccountManagerFacade();
#endif
    return std::make_unique<AccountTrackerService>();
  }

  base::test::TaskEnvironment task_environment_;
  GoogleServiceAuthError no_error_;
  GoogleServiceAuthError error_;
  GoogleServiceAuthError canceled_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<AccountTrackerService> account_tracker_service_;
  std::unique_ptr<FakeProfileOAuth2TokenService> token_service_;
};

const signin::AccountsInCookieJarInfo kCookiesEmptyStale(
    /*accounts_are_fresh=*/false,
    /*accounts=*/{});
const signin::AccountsInCookieJarInfo kCookiesEmptyFresh(
    /*accounts_are_fresh=*/true,
    /*accounts=*/{});

}  // namespace

using ::testing::_;
using ::testing::ElementsAre;

TEST_F(GaiaCookieManagerServiceTest, MultiloginCookiesDisabled) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  signin_client()->set_are_signin_cookies_allowed(false);

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed;
  EXPECT_CALL(set_accounts_in_cookie_completed,
              Run(signin::SetAccountsInCookieResult::kPersistentError));

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());
}

TEST_F(GaiaCookieManagerServiceTest, LogoutRetried) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      sttrcdh_override(test_task_runner);

  EXPECT_CALL(helper, StartGaiaLogOut()).Times(2);

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());

  SimulateLogOutFailure(&helper, canceled());
  DCHECK(helper.is_running());
  Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
  SimulateLogOutSuccess(&helper);
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogoutRetriedTwice) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  base::HistogramTester histograms;

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      sttrcdh_override(test_task_runner);

  EXPECT_CALL(helper, StartGaiaLogOut()).Times(3);

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  // Logout requests are retried even if the error is persistent.
  SimulateLogOutFailure(&helper, error());
  DCHECK(helper.is_running());
  Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
  SimulateLogOutFailure(&helper, canceled());
  DCHECK(helper.is_running());
  Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
  SimulateLogOutSuccess(&helper);
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterSuccess) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.TriggerListAccounts();
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  std::string data =
      "[\"f\", [[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"]]]";
  SimulateListAccountsSuccess(&helper, data);
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure1) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.TriggerListAccounts();
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  SimulateListAccountsFailure(&helper, error());
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, AllRequestsInMultipleGoes) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartSetAccounts()).Times(4);

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed;
  EXPECT_CALL(set_accounts_in_cookie_completed,
              Run(signin::SetAccountsInCookieResult::kSuccess))
      .Times(4);

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());
  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());

  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id3_, kAccountId3}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());

  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id4_, kAccountId4}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());

  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsNoQueue) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartSetAccounts());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed;
  EXPECT_CALL(set_accounts_in_cookie_completed,
              Run(signin::SetAccountsInCookieResult::kSuccess));

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  SimulateLogOutSuccess(&helper);
  ASSERT_FALSE(helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsFails) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartSetAccounts());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed;
  EXPECT_CALL(set_accounts_in_cookie_completed,
              Run(signin::SetAccountsInCookieResult::kSuccess));

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  // A completion callback shouldn't be called.
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  SimulateLogOutFailure(&helper, error());
  // CookieManagerService is still running; it is retrying the failed logout.
  ASSERT_TRUE(helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsAfterOneAddInQueue) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartSetAccounts());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed;
  EXPECT_CALL(set_accounts_in_cookie_completed,
              Run(signin::SetAccountsInCookieResult::kSuccess));
  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());

  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsTwice) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartGaiaLogOut());
  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed1,
      log_out_from_cookie_completed2;
  EXPECT_CALL(log_out_from_cookie_completed1, Run(no_error()));
  EXPECT_CALL(log_out_from_cookie_completed2, Run(canceled()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed1.Get());
  // Only one LogOut will be fetched.
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed2.Get());
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeAdd) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartSetAccounts()).Times(2);
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed1;
  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed2;
  EXPECT_CALL(set_accounts_in_cookie_completed1,
              Run(signin::SetAccountsInCookieResult::kSuccess));
  EXPECT_CALL(set_accounts_in_cookie_completed2,
              Run(signin::SetAccountsInCookieResult::kSuccess));

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed1.Get());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed2.Get());

  SimulateLogOutSuccess(&helper);
  // After LogOut the MultiLogin should be fetched.
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeLogoutAndAdd) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartSetAccounts()).Times(2);
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed1;
  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed2;
  EXPECT_CALL(set_accounts_in_cookie_completed1,
              Run(signin::SetAccountsInCookieResult::kSuccess));
  EXPECT_CALL(set_accounts_in_cookie_completed2,
              Run(signin::SetAccountsInCookieResult::kSuccess));

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed1.Get());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed1,
      log_out_from_cookie_completed2;
  EXPECT_CALL(log_out_from_cookie_completed1, Run(no_error()));
  EXPECT_CALL(log_out_from_cookie_completed2, Run(canceled()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed1.Get());
  // Second LogOut will never be fetched.
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed2.Get());
  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed2.Get());

  SimulateLogOutSuccess(&helper);
  // After LogOut the MultiLogin should be fetched.
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
}

TEST_F(GaiaCookieManagerServiceTest, PendingSigninThenSignout) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  // From the first Signin.
  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed1;
  EXPECT_CALL(set_accounts_in_cookie_completed1,
              Run(signin::SetAccountsInCookieResult::kSuccess));

  // From the sign out and then re-sign in.
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockSetAccountsInCookieCompletedCallback set_accounts_in_cookie_completed2;
  EXPECT_CALL(set_accounts_in_cookie_completed2,
              Run(signin::SetAccountsInCookieResult::kSuccess));
  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  // Total sign in 2 times, not enforcing ordered sequences.
  EXPECT_CALL(helper, StartSetAccounts()).Times(2);

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed1.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
  SimulateLogOutSuccess(&helper);

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id2_, kAccountId2}}, gaia::GaiaSource::kChrome,
      set_accounts_in_cookie_completed2.Get());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFirstReturnsEmpty) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);
  ASSERT_TRUE(signin_client()
                  ->GetPrefs()
                  ->GetString(prefs::kGaiaCookieLastListAccountsData)
                  .empty());
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFindsOneAccount) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  gaia::ListedAccount account;
  account.id = CoreAccountId::FromGaiaId("8");
  account.email = "a@b.com";
  account.gaia_id = "8";
  account.raw_email = "a@b.com";
  signin::AccountsInCookieJarInfo cookies_expected_fresh(true, {account});
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(cookies_expected_fresh,
                                                      no_error()));

  std::string data =
      "[\"f\", [[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"]]]";
  SimulateListAccountsSuccess(&helper, data);
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFindsSignedOutAccounts) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  gaia::ListedAccount signed_in_account;
  signed_in_account.id = CoreAccountId::FromGaiaId("8");
  signed_in_account.email = "a@b.com";
  signed_in_account.gaia_id = "8";
  signed_in_account.raw_email = "a@b.com";
  gaia::ListedAccount signed_out_account;
  signed_out_account.id = CoreAccountId::FromGaiaId("9");
  signed_out_account.email = "c@d.com";
  signed_out_account.gaia_id = "9";
  signed_out_account.raw_email = "c@d.com";
  signed_out_account.signed_out = true;
  signin::AccountsInCookieJarInfo cookies_expected_fresh(
      true, {signed_in_account, signed_out_account});
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(cookies_expected_fresh,
                                                      no_error()));

  std::string data =
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
      "null,null,null,1]]]";
  SimulateListAccountsSuccess(&helper, data);
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsAfterOnCookieChange) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // Add a single account.
  gaia::ListedAccount account;
  account.id = CoreAccountId::FromGaiaId("8");
  account.email = "a@b.com";
  account.gaia_id = "8";
  account.raw_email = "a@b.com";
  signin::AccountsInCookieJarInfo cookies_expected_fresh(true, {account});
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(cookies_expected_fresh,
                                                      no_error()));

  std::string data =
      R"(["f", [["b", 0, "n", "a@b.com", "p", 0, 0, 0, 0, 1, "8"]]])";
  SimulateListAccountsSuccess(&helper, data);

  // Confidence check that ListAccounts returns the cached data.
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  helper.ForceOnCookieChangeProcessing();

  // OnCookieChange should invalidate the cached data and trigger a/ListAccounts
  // request.
  signin::AccountsInCookieJarInfo cookies_expected_stale(false, {account});
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_stale);

  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(kCookiesEmptyFresh, no_error()));

  data = R"(["f",[]])";
  SimulateListAccountsSuccess(&helper, data);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest,
       OnCookieChangeWhileInFlightListAccountsRequest) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  // Add a single account.
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(2);
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // Cookies have changed while in-flight /ListAccounts requests. A new request
  // should still be added to the queue of requests.
  helper.ForceOnCookieChangeProcessing();
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // First request.
  gaia::ListedAccount account;
  account.id = CoreAccountId::FromGaiaId("8");
  account.email = "a@b.com";
  account.gaia_id = "8";
  account.raw_email = "a@b.com";
  signin::AccountsInCookieJarInfo cookies_expected_fresh(true, {account});
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(cookies_expected_fresh,
                                                      no_error()));
  std::string data =
      R"(["f", [["b", 0, "n", "a@b.com", "p", 0, 0, 0, 0, 1, "8"]]])";
  SimulateListAccountsSuccess(&helper, data);
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);

  // Second request.
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(kCookiesEmptyFresh, no_error()));
  data = R"(["f",[]])";
  SimulateListAccountsSuccess(&helper, data);

  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyFresh);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, TriggerListAccountsNoInProgressRequest) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);
  std::string data = R"(["f",[]])";
  SimulateListAccountsSuccess(&helper, data);
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyFresh);

  MockObserver observer(&helper);

  // `TriggerListAccounts()` should start a fetch even if accounts in the cookie
  // jar are fresh. It doesn't invalidate the current state.
  EXPECT_CALL(helper, StartFetchingListAccounts());

  helper.TriggerListAccounts();
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyFresh);

  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(kCookiesEmptyFresh, no_error()));
  SimulateListAccountsSuccess(&helper, data);
}

TEST_F(GaiaCookieManagerServiceTest, TriggerListAccountsInFlightRequest) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartFetchingListAccounts());
  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // `TriggerListAccounts()` should start a fetch even there is an in-flight
  // request.
  helper.TriggerListAccounts();
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(kCookiesEmptyFresh, no_error()))
      .Times(2);
  // Next request should be started as soon as the first completes.
  EXPECT_CALL(helper, StartFetchingListAccounts());

  std::string data = R"(["f",[]])";
  SimulateListAccountsSuccess(&helper, data);
  SimulateListAccountsSuccess(&helper, data);
}

TEST_F(GaiaCookieManagerServiceTest, MultipleTriggerListAccounts) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  // No more than 2 list accounts should be in the request queue, one in-flight
  // and one waiting.
  EXPECT_CALL(helper, StartFetchingListAccounts());
  for (size_t i = 0; i < 5; i++) {
    helper.TriggerListAccounts();
  }

  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(kCookiesEmptyFresh, no_error()))
      .Times(2);

  // Next request should be started as soon as the first completes.
  EXPECT_CALL(helper, StartFetchingListAccounts());
  std::string data = R"(["f",[]])";
  SimulateListAccountsSuccess(&helper, data);
  SimulateListAccountsSuccess(&helper, data);
}

TEST_F(GaiaCookieManagerServiceTest, GaiaCookieLastListAccountsDataSaved) {
  std::string data =
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
      "null,null,null,1]]]";
  gaia::ListedAccount signed_in_account;
  signed_in_account.id = CoreAccountId::FromGaiaId("8");
  signed_in_account.email = "a@b.com";
  signed_in_account.gaia_id = "8";
  signed_in_account.raw_email = "a@b.com";
  gaia::ListedAccount signed_out_account;
  signed_out_account.id = CoreAccountId::FromGaiaId("9");
  signed_out_account.email = "c@d.com";
  signed_out_account.gaia_id = "9";
  signed_out_account.raw_email = "c@d.com";
  signed_out_account.signed_out = true;
  signin::AccountsInCookieJarInfo cookies_expected_fresh(
      true, {signed_in_account, signed_out_account});
  {
    InstrumentedGaiaCookieManagerService helper(
        account_tracker_service(), token_service(), signin_client());
    MockObserver observer(&helper);

    EXPECT_CALL(helper, StartFetchingListAccounts());
    // |kGaiaCookieLastListAccountsData| is empty.
    ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

    EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(cookies_expected_fresh,
                                                        no_error()));

    SimulateListAccountsSuccess(&helper, data);
    // |kGaiaCookieLastListAccountsData| is set.
    ASSERT_EQ(signin_client()->GetPrefs()->GetString(
                  prefs::kGaiaCookieLastListAccountsData),
              data);
    // List accounts is not stale.
    ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);
  }

  // Now that the list accounts data is saved to the pref service, test that
  // starting a new Gaia Service Manager gives synchronous answers to list
  // accounts.
  {
    InstrumentedGaiaCookieManagerService helper(
        account_tracker_service(), token_service(), signin_client());
    MockObserver observer(&helper);
    auto test_task_runner =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
        sttrcdh_override(test_task_runner);

    EXPECT_CALL(helper, StartFetchingListAccounts()).Times(3);

    // Though |SimulateListAccountsSuccess| is not yet called, we are able to
    // retrieve last |list_accounts| and  |expected_accounts| from the pref,
    // but mark them as stale. A |StartFetchingListAccounts| is triggered.
    signin::AccountsInCookieJarInfo cookies_expected_stale(
        false, {signed_in_account, signed_out_account});
    EXPECT_EQ(helper.ListAccounts(), cookies_expected_stale);

    // |SimulateListAccountsSuccess| and assert list accounts is not stale
    // anymore.
    EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(cookies_expected_fresh,
                                                        no_error()));
    SimulateListAccountsSuccess(&helper, data);
    ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);

    // Change list account state to be stale, which will trigger list accounts
    // request.
    helper.ForceOnCookieChangeProcessing();

    // Receive an unexpected response from the server. Listed accounts as well
    // as the pref should be cleared.
    GoogleServiceAuthError error =
        GoogleServiceAuthError::FromUnexpectedServiceResponse(
            "Error parsing ListAccounts response");
    EXPECT_CALL(observer,
                OnGaiaAccountsInCookieUpdated(kCookiesEmptyStale, error));
    SimulateListAccountsSuccess(&helper, "[]");
    EXPECT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

    // List accounts retries once on |UNEXPECTED_SERVICE_RESPONSE| errors with
    // backoff protection.
    Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
    SimulateListAccountsSuccess(&helper, "[]");

    // |kGaiaCookieLastListAccountsData| is cleared.
    EXPECT_TRUE(signin_client()
                    ->GetPrefs()
                    ->GetString(prefs::kGaiaCookieLastListAccountsData)
                    .empty());
  }

  {
    // On next startup, |kGaiaCookieLastListAccountsData| contains last list
    // accounts data.
    EXPECT_TRUE(signin_client()
                    ->GetPrefs()
                    ->GetString(prefs::kGaiaCookieLastListAccountsData)
                    .empty());
  }
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcher) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartSetAccounts());
  result_fetcher.Start(
      base::BindOnce(&InstrumentedGaiaCookieManagerService::StartSetAccounts,
                     base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnectionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"},"
      " {\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");

  // Simulate responses for the two connection URLs.
  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(2u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));
  ASSERT_TRUE(IsLoadPending("http://www.bl.com"));

  ASSERT_EQ("bl:null,yt:null", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnectionInfoResult("http://www.yt.com", "yt_result");
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnectionInfoResult("http://www.bl.com", "bl_result");
  ASSERT_EQ("bl:bl_result,yt:yt_result", result_fetcher.GetExternalCcResult());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherTimeout) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartSetAccounts());
  result_fetcher.Start(
      base::BindOnce(&InstrumentedGaiaCookieManagerService::StartSetAccounts,
                     base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnectionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"},"
      " {\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(2u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));
  ASSERT_TRUE(IsLoadPending("http://www.bl.com"));

  // Simulate response only for "yt".
  ASSERT_EQ("bl:null,yt:null", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnectionInfoResult("http://www.yt.com", "yt_result");
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());

  // Now timeout.
  result_fetcher.TimeoutForTests();
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());
  loaders = result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(0u, loaders.size());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherTruncate) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartSetAccounts());
  result_fetcher.Start(
      base::BindOnce(&InstrumentedGaiaCookieManagerService::StartSetAccounts,
                     base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnectionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(1u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));

  // Simulate response for "yt" with a string that is too long.
  SimulateGetCheckConnectionInfoResult("http://www.yt.com",
                                       "1234567890123456trunc");
  ASSERT_EQ("yt:1234567890123456", result_fetcher.GetExternalCcResult());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherWithCommas) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartSetAccounts());
  result_fetcher.Start(
      base::BindOnce(&InstrumentedGaiaCookieManagerService::StartSetAccounts,
                     base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnectionInfo.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher.get_loader_map_for_testing();
  ASSERT_EQ(1u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.yt.com"));

  // Simulate response for "yt" with a string that contains a comma-separated
  // string that could trick the server that a connection to "bl" is ok.
  SimulateGetCheckConnectionInfoResult("http://www.yt.com", "ok,bl:ok");
  ASSERT_EQ("yt:ok%2Cbl%3Aok", result_fetcher.GetExternalCcResult());
}

TEST_F(GaiaCookieManagerServiceTest, RemoveLoggedOutAccountByGaiaId) {
  const std::string kTestGaiaId1 = "8";
  const std::string kTestGaiaId2 = "9";

  ::testing::NiceMock<InstrumentedGaiaCookieManagerService> helper(
      account_tracker_service(), token_service(), signin_client());
  ::testing::NiceMock<MockObserver> observer(&helper);

  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // Simulate two signed out accounts being listed.
  SimulateListAccountsSuccess(
      &helper,
      base::StringPrintf(
          "[\"f\","
          "[[\"a\", 0, \"n\", \"a@d.com\", \"p\", 0, 0, 0, 0, 1, \"%s\","
          "null,null,null,1],"
          "[\"b\", 0, \"n\", \"b@d.com\", \"p\", 0, 0, 0, 0, 1, \"%s\","
          "null,null,null,1]]]",
          kTestGaiaId1.c_str(), kTestGaiaId2.c_str()));

  gaia::ListedAccount account1;
  account1.id = CoreAccountId::FromGaiaId(kTestGaiaId1);
  account1.email = "a@d.com";
  account1.gaia_id = kTestGaiaId1;
  account1.raw_email = "a@d.com";
  account1.signed_out = true;
  gaia::ListedAccount account2;
  account2.id = CoreAccountId::FromGaiaId(kTestGaiaId2);
  account2.email = "b@d.com";
  account2.gaia_id = kTestGaiaId2;
  account2.raw_email = "b@d.com";
  account2.signed_out = true;
  signin::AccountsInCookieJarInfo cookies_expected_two_accounts_fresh(
      true, {account1, account2});
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_two_accounts_fresh);

  // The removal should notify observers, with one account removed.
  signin::AccountsInCookieJarInfo cookies_expected_one_account_fresh(
      true, {account2});
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                            cookies_expected_one_account_fresh, _));
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(0);
  helper.RemoveLoggedOutAccountByGaiaId(kTestGaiaId1);

  // Verify that ListAccounts wasn't triggered.
  EXPECT_FALSE(helper.is_running());
  testing::Mock::VerifyAndClearExpectations(&helper);

  ASSERT_EQ(helper.ListAccounts(), cookies_expected_one_account_fresh);
}

TEST_F(GaiaCookieManagerServiceTest,
       RemoveLoggedOutAccountByGaiaIdWhileAccountsStale) {
  const std::string kTestGaiaId1 = "8";

  ::testing::NiceMock<InstrumentedGaiaCookieManagerService> helper(
      account_tracker_service(), token_service(), signin_client());
  ::testing::NiceMock<MockObserver> observer(&helper);

  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // Simulate one signed out account being listed.
  SimulateListAccountsSuccess(
      &helper,
      base::StringPrintf(
          "[\"f\","
          "[[\"a\", 0, \"n\", \"a@d.com\", \"p\", 0, 0, 0, 0, 1, \"%s\","
          "null,null,null,1]]]",
          kTestGaiaId1.c_str()));

  // Change list account state to be stale, which will trigger list accounts
  // request.
  helper.ForceOnCookieChangeProcessing();

  gaia::ListedAccount account;
  account.id = CoreAccountId::FromGaiaId(kTestGaiaId1);
  account.email = "a@d.com";
  account.gaia_id = kTestGaiaId1;
  account.raw_email = "a@d.com";
  account.signed_out = true;
  signin::AccountsInCookieJarInfo cookies_expected_stale(false, {account});
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_stale);

  // The removal should be ignored because the account list is stale.
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(_, _)).Times(0);
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(0);
  helper.RemoveLoggedOutAccountByGaiaId(kTestGaiaId1);

  // Verify that ListAccounts wasn't triggered again.
  testing::Mock::VerifyAndClearExpectations(&helper);

  ASSERT_EQ(helper.ListAccounts(), cookies_expected_stale);
}

TEST_F(GaiaCookieManagerServiceTest,
       RemoveLoggedOutAccountByGaiaIdForMissingAccount) {
  const std::string kTestGaiaId1 = "8";
  const std::string kNonListedAccount = "9";

  ::testing::NiceMock<InstrumentedGaiaCookieManagerService> helper(
      account_tracker_service(), token_service(), signin_client());
  ::testing::NiceMock<MockObserver> observer(&helper);

  ASSERT_EQ(helper.ListAccounts(), kCookiesEmptyStale);

  // Simulate one signed out account being listed.
  SimulateListAccountsSuccess(
      &helper,
      base::StringPrintf(
          "[\"f\","
          "[[\"a\", 0, \"n\", \"a@d.com\", \"p\", 0, 0, 0, 0, 1, \"%s\","
          "null,null,null,1]]]",
          kTestGaiaId1.c_str()));

  gaia::ListedAccount account;
  account.id = CoreAccountId::FromGaiaId(kTestGaiaId1);
  account.email = "a@d.com";
  account.gaia_id = kTestGaiaId1;
  account.raw_email = "a@d.com";
  account.signed_out = true;
  signin::AccountsInCookieJarInfo cookies_expected_fresh(true, {account});
  ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);

  // The removal should be ignored because the Gaia ID is not listed/known.
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(_, _)).Times(0);
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(0);
  helper.RemoveLoggedOutAccountByGaiaId(kNonListedAccount);

  // Verify that ListAccounts wasn't triggered.
  EXPECT_FALSE(helper.is_running());
  testing::Mock::VerifyAndClearExpectations(&helper);

  ASSERT_EQ(helper.ListAccounts(), cookies_expected_fresh);
}

TEST_F(GaiaCookieManagerServiceTest, OptimizeListAccounts) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  EXPECT_CALL(helper, StartFetchingListAccounts());
  helper.TriggerListAccounts();
  // Should be delayed.
  helper.TriggerListAccounts();
  // Should be deduplicated.
  helper.TriggerListAccounts();

  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      base::DoNothing());

  // Should be deduplicated.
  helper.TriggerListAccounts();
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome, base::DoNothing());
  // Should be deduplicated.
  helper.TriggerListAccounts();

  // // Expect: ListAccounts, SetAccounts, Logout, ListAccounts
  EXPECT_CALL(helper, StartSetAccounts());
  std::string data = R"(["f",[]])";
  SimulateListAccountsSuccess(&helper, data);

  EXPECT_CALL(helper, StartGaiaLogOut());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
  EXPECT_CALL(helper, StartFetchingListAccounts());
  SimulateLogOutSuccess(&helper);
  SimulateListAccountsSuccess(&helper, data);

  // List accounts not the first request.
  EXPECT_CALL(helper, StartGaiaLogOut());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome, base::DoNothing());
  // Should be delayed.
  helper.TriggerListAccounts();
  // Should be deduplicated.
  helper.TriggerListAccounts();
  helper.SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
      {{account_id1_, kAccountId1}}, gaia::GaiaSource::kChrome,
      base::DoNothing());
  // Expect:  Logout, SetAccounts, ListAccounts.
  EXPECT_CALL(helper, StartSetAccounts());
  SimulateLogOutSuccess(&helper);
  EXPECT_CALL(helper, StartFetchingListAccounts());
  SimulateMultiloginFinished(&helper,
                             signin::SetAccountsInCookieResult::kSuccess);
  SimulateListAccountsSuccess(&helper, data);
  EXPECT_FALSE(helper.is_running());
}

class GaiaCookieManagerServiceCookieTest
    : public GaiaCookieManagerServiceTest,
      public testing::WithParamInterface<
          std::tuple<bool /*is_gaia_signin_cookie*/, net::CookieChangeCause>> {
};

TEST_P(GaiaCookieManagerServiceCookieTest, CookieChange) {
  GURL kGoogleUrl = GaiaUrls::GetInstance()->secure_google_url();
  network::TestCookieManager* test_cookie_manager = nullptr;
  {
    auto cookie_manager = std::make_unique<network::TestCookieManager>();
    test_cookie_manager = cookie_manager.get();
    signin_client()->set_cookie_manager(std::move(cookie_manager));
  }
  ASSERT_EQ(test_cookie_manager, signin_client()->GetCookieManager());
  base::MockCallback<
      GaiaCookieManagerService::GaiaCookieDeletedByUserActionCallback>
      cookie_deleted_callback;
  InstrumentedGaiaCookieManagerService service(
      account_tracker_service(), token_service(), signin_client());
  service.SetGaiaCookieDeletedByUserActionCallback(
      cookie_deleted_callback.Get());
  service.InitCookieListener();

  auto [/*bool*/ is_gaia_signin_cookie, cause] = GetParam();
  std::string cookie_name =
      is_gaia_signin_cookie ? GaiaConstants::kGaiaSigninCookieName : "Foo";
  bool list_account_expected =
      is_gaia_signin_cookie || cause == net::CookieChangeCause::EXPLICIT;
  bool callback_expected =
      is_gaia_signin_cookie && cause == net::CookieChangeCause::EXPLICIT;

  if (list_account_expected) {
    EXPECT_CALL(service, StartFetchingListAccounts()).Times(1);
  }
  if (callback_expected) {
    EXPECT_CALL(cookie_deleted_callback, Run).Times(1);
  }
  test_cookie_manager->DispatchCookieChange(
      net::CookieChangeInfo(GetTestCookie(kGoogleUrl, cookie_name),
                            net::CookieAccessResult(), cause));
  service.cookie_listener_receiver_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&cookie_deleted_callback);
  testing::Mock::VerifyAndClearExpectations(&service);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GaiaCookieManagerServiceCookieTest,
    testing::Combine(/*is_gaia_signin_cookie=*/testing::Bool(),
                     testing::Values(net::CookieChangeCause::UNKNOWN_DELETION,
                                     net::CookieChangeCause::EXPLICIT)));
