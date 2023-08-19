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
#include "components/signin/public/identity_manager/identity_test_utils.h"
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

using MockAddAccountToCookieCompletedCallback = base::MockCallback<
    GaiaCookieManagerService::AddAccountToCookieCompletedCallback>;
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

  MOCK_METHOD3(OnGaiaAccountsInCookieUpdated,
               void(const std::vector<gaia::ListedAccount>&,
                    const std::vector<gaia::ListedAccount>&,
                    const GoogleServiceAuthError&));
};

// Counts number of InstrumentedGaiaCookieManagerService created.
// We can EXPECT_* to be zero at the end of our unit tests
// to make sure everything is properly deleted.

int total = 0;

bool AreAccountListsEqual(const std::vector<gaia::ListedAccount>& left,
                          const std::vector<gaia::ListedAccount>& right) {
  if (left.size() != right.size())
    return false;

  for (size_t i = 0u; i < left.size(); ++i) {
    const gaia::ListedAccount& left_account = left[i];
    const gaia::ListedAccount& actual_account = right[i];
    // If both accounts have an ID, use it for the comparison.
    if (!left_account.id.empty() && !actual_account.id.empty()) {
      if (left_account.id != actual_account.id)
        return false;
    } else if (left_account.email != actual_account.email ||
               left_account.gaia_id != actual_account.gaia_id ||
               left_account.raw_email != actual_account.raw_email ||
               left_account.valid != actual_account.valid ||
               left_account.signed_out != actual_account.signed_out ||
               left_account.verified != actual_account.verified) {
      return false;
    }
  }
  return true;
}

net::CanonicalCookie GetTestCookie(const GURL& url, const std::string& name) {
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          url, name, /*value=*/"cookie_value", /*domain=*/"." + url.host(),
          /*path=*/"/", /*creation_time=*/base::Time(),
          /*expiration_time=*/base::Time(), /*last_access_time=*/base::Time(),
          /*secure=*/true, /*http_only=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
          /*same_party=*/false, /*partition_key=*/absl::nullopt);
  return *cookie;
}

// Custom matcher for ListedAccounts.
MATCHER_P(ListedAccountEquals, expected, "") {
  return AreAccountListsEqual(expected, arg);
}

// Custom matcher for ListedAccount.
MATCHER_P(ListedAccountMatchesGaiaId, gaia_id, "") {
  return arg.gaia_id == std::string(gaia_id);
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

  MOCK_METHOD0(StartFetchingUbertoken, void());
  MOCK_METHOD0(StartFetchingListAccounts, void());
  MOCK_METHOD0(StartGaiaLogOut, void());
  MOCK_METHOD0(StartFetchingMergeSession, void());
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

  void SimulateUbertokenSuccess(GaiaCookieManagerService* gcms,
                                const std::string& uber_token) {
    gcms->OnUbertokenFetchComplete(
        GoogleServiceAuthError(GoogleServiceAuthError::NONE), uber_token);
  }

  void SimulateUbertokenFailure(GaiaCookieManagerService* gcms,
                                const GoogleServiceAuthError& error) {
    gcms->OnUbertokenFetchComplete(error, /*uber_token=*/std::string());
  }

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

  void SimulateMergeSessionSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnMergeSessionSuccess(data);
  }

  void SimulateMergeSessionFailure(GaiaAuthConsumer* consumer,
                                   const GoogleServiceAuthError& error) {
    consumer->OnMergeSessionFailure(error);
  }

  void SimulateMultiloginFinished(GaiaAuthConsumer* consumer,
                                  const OAuthMultiloginResult& result) {
    consumer->OnOAuthMultiloginFinished(result);
  }

  void SimulateListAccountsSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnListAccountsSuccess(data);
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

}  // namespace

using ::testing::_;
using ::testing::ElementsAre;

TEST_F(GaiaCookieManagerServiceTest, Success) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token");
}

TEST_F(GaiaCookieManagerServiceTest, FailedMergeSession) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  base::HistogramTester histograms;

  EXPECT_CALL(helper, StartFetchingUbertoken());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionFailure(&helper, error());
  // Persistent error incurs no further retries.
  DCHECK(!helper.is_running());
  histograms.ExpectUniqueSample("OAuth2Login.MergeSessionFailure",
                                GoogleServiceAuthError::SERVICE_ERROR, 1);
}

TEST_F(GaiaCookieManagerServiceTest, AddAccountCookiesDisabled) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  signin_client()->set_are_signin_cookies_allowed(false);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, canceled()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetried) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      sttrcdh_override(test_task_runner);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingMergeSession());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
  SimulateMergeSessionSuccess(&helper, "token");
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetriedTwice) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);
  base::HistogramTester histograms;

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting
      sttrcdh_override(test_task_runner);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingMergeSession()).Times(2);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  Advance(test_task_runner, helper.GetBackoffEntry()->GetTimeUntilRelease());
  SimulateMergeSessionSuccess(&helper, "token");
  DCHECK(!helper.is_running());
  histograms.ExpectUniqueSample("OAuth2Login.MergeSessionRetry",
                                GoogleServiceAuthError::REQUEST_CANCELED, 2);
}

TEST_F(GaiaCookieManagerServiceTest, FailedUbertoken) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateUbertokenFailure(&helper, error());
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterSuccess) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure1) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateMergeSessionFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure2) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateUbertokenFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, AllRequestsInMultipleGoes) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(4);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(_, no_error())).Times(4);

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token1");

  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token2");
  SimulateMergeSessionSuccess(&helper, "token3");

  helper.AddAccountToCookie(account_id4_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token4");
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsNoQueue) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

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

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

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

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));
  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsAfterTwoAddsInQueue) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, canceled()));
  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  // The Log Out should prevent this AddAccount from being fetched.
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsTwice) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

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

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed2,
      add_account_to_cookie_completed3;
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed3, Run(account_id3_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());
  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed3.Get());

  SimulateLogOutSuccess(&helper);
  // After LogOut the MergeSession should be fetched.
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeLogoutAndAdd) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed2,
      add_account_to_cookie_completed3;
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed3, Run(account_id3_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed1,
      log_out_from_cookie_completed2;
  EXPECT_CALL(log_out_from_cookie_completed1, Run(no_error()));
  EXPECT_CALL(log_out_from_cookie_completed2, Run(canceled()));

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed1.Get());
  // Second LogOut will never be fetched.
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed2.Get());
  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed3.Get());

  SimulateLogOutSuccess(&helper);
  // After LogOut the MergeSession should be fetched.
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, PendingSigninThenSignout) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  // From the first Signin.
  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));

  // From the sign out and then re-sign in.
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed3;
  EXPECT_CALL(add_account_to_cookie_completed3, Run(account_id3_, no_error()));
  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  // Total sign in 2 times, not enforcing ordered sequences.
  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);

  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed3.Get());
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(GaiaCookieManagerServiceTest, CancelSignIn) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, canceled()));
  EXPECT_CALL(helper, StartGaiaLogOut());

  MockLogOutFromCookieCompletedCallback log_out_from_cookie_completed;
  EXPECT_CALL(log_out_from_cookie_completed, Run(no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome,
                           log_out_from_cookie_completed.Get());

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFirstReturnsEmpty) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;

  EXPECT_CALL(helper, StartFetchingListAccounts());

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
  ASSERT_TRUE(list_accounts.empty());
  ASSERT_TRUE(signed_out_accounts.empty());
  ASSERT_TRUE(signin_client()
                  ->GetPrefs()
                  ->GetString(prefs::kGaiaCookieLastListAccountsData)
                  .empty());
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFindsOneAccount) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> expected_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  expected_accounts.push_back(listed_account);

  std::vector<gaia::ListedAccount> signed_out_accounts;
  std::vector<gaia::ListedAccount> expected_signed_out_accounts;

  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                            ListedAccountEquals(expected_accounts),
                            ListedAccountEquals(expected_signed_out_accounts),
                            no_error()));

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));

  std::string data =
      "[\"f\", [[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"]]]";
  SimulateListAccountsSuccess(&helper, data);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFindsSignedOutAccounts) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> expected_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  expected_accounts.push_back(listed_account);

  std::vector<gaia::ListedAccount> signed_out_accounts;
  std::vector<gaia::ListedAccount> expected_signed_out_accounts;
  gaia::ListedAccount signed_out_account;
  signed_out_account.email = "c@d.com";
  signed_out_account.raw_email = "c@d.com";
  signed_out_account.gaia_id = "9";
  signed_out_account.signed_out = true;
  expected_signed_out_accounts.push_back(signed_out_account);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                            ListedAccountEquals(expected_accounts),
                            ListedAccountEquals(expected_signed_out_accounts),
                            no_error()));

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));

  std::string data =
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
      "null,null,null,1]]]";
  SimulateListAccountsSuccess(&helper, data);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsAcceptsNull) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  ASSERT_FALSE(helper.ListAccounts(nullptr, nullptr));

  std::string data =
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
      "null,null,null,1]]]";
  SimulateListAccountsSuccess(&helper, data);

  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_TRUE(helper.ListAccounts(nullptr, &signed_out_accounts));
  ASSERT_EQ(1u, signed_out_accounts.size());

  std::vector<gaia::ListedAccount> accounts;
  ASSERT_TRUE(helper.ListAccounts(&accounts, nullptr));
  ASSERT_EQ(1u, accounts.size());

  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsAfterOnCookieChange) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> empty_list_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  std::vector<gaia::ListedAccount> empty_signed_out_accounts;

  std::vector<gaia::ListedAccount> nonempty_list_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  nonempty_list_accounts.push_back(listed_account);

  // Add a single account.
  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(
                  ListedAccountEquals(nonempty_list_accounts),
                  ListedAccountEquals(empty_signed_out_accounts), no_error()));
  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
  ASSERT_TRUE(list_accounts.empty());
  ASSERT_TRUE(signed_out_accounts.empty());

  std::string data =
      "[\"f\", [[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"]]]";
  SimulateListAccountsSuccess(&helper, data);

  // Confidence check that ListAccounts returns the cached data.
  ASSERT_TRUE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
  ASSERT_TRUE(AreAccountListsEqual(nonempty_list_accounts, list_accounts));
  ASSERT_TRUE(signed_out_accounts.empty());
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);

  EXPECT_CALL(helper, StartFetchingListAccounts());
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(
                  ListedAccountEquals(empty_list_accounts),
                  ListedAccountEquals(empty_signed_out_accounts), no_error()));
  helper.ForceOnCookieChangeProcessing();

  // OnCookieChange should invalidate the cached data.

  // Clear the list before calling |ListAccounts()| to make sure that
  // GaiaCookieManagerService repopulates it with the stale cached information.
  list_accounts.clear();

  ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
  ASSERT_TRUE(AreAccountListsEqual(nonempty_list_accounts, list_accounts));
  ASSERT_TRUE(signed_out_accounts.empty());
  data = "[\"f\",[]]";
  SimulateListAccountsSuccess(&helper, data);
  EXPECT_EQ(signin_client()->GetPrefs()->GetString(
                prefs::kGaiaCookieLastListAccountsData),
            data);
}

TEST_F(GaiaCookieManagerServiceTest, GaiaCookieLastListAccountsDataSaved) {
  std::string data =
      "[\"f\","
      "[[\"b\", 0, \"n\", \"a@b.com\", \"p\", 0, 0, 0, 0, 1, \"8\"],"
      " [\"b\", 0, \"n\", \"c@d.com\", \"p\", 0, 0, 0, 0, 1, \"9\","
      "null,null,null,1]]]";
  std::vector<gaia::ListedAccount> expected_accounts;
  gaia::ListedAccount listed_account;
  listed_account.email = "a@b.com";
  listed_account.raw_email = "a@b.com";
  listed_account.gaia_id = "8";
  expected_accounts.push_back(listed_account);

  std::vector<gaia::ListedAccount> expected_signed_out_accounts;
  gaia::ListedAccount signed_out_account;
  signed_out_account.email = "c@d.com";
  signed_out_account.raw_email = "c@d.com";
  signed_out_account.gaia_id = "9";
  signed_out_account.signed_out = true;
  expected_signed_out_accounts.push_back(signed_out_account);
  std::vector<gaia::ListedAccount> list_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;

  {
    InstrumentedGaiaCookieManagerService helper(
        account_tracker_service(), token_service(), signin_client());
    MockObserver observer(&helper);

    EXPECT_CALL(helper, StartFetchingListAccounts());
    EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                              ListedAccountEquals(expected_accounts),
                              ListedAccountEquals(expected_signed_out_accounts),
                              no_error()));

    ASSERT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
    // |kGaiaCookieLastListAccountsData| is empty.
    EXPECT_TRUE(list_accounts.empty());
    EXPECT_TRUE(signed_out_accounts.empty());
    SimulateListAccountsSuccess(&helper, data);
    // |kGaiaCookieLastListAccountsData| is set.
    ASSERT_EQ(signin_client()->GetPrefs()->GetString(
                  prefs::kGaiaCookieLastListAccountsData),
              data);
    // List accounts is not stale.
    ASSERT_TRUE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
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
    EXPECT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));
    EXPECT_TRUE(AreAccountListsEqual(list_accounts, expected_accounts));
    EXPECT_TRUE(AreAccountListsEqual(signed_out_accounts,
                                     expected_signed_out_accounts));

    // |SimulateListAccountsSuccess| and assert list accounts is not stale
    // anymore.
    EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(
                              ListedAccountEquals(expected_accounts),
                              ListedAccountEquals(expected_signed_out_accounts),
                              no_error()));
    SimulateListAccountsSuccess(&helper, data);
    ASSERT_TRUE(helper.ListAccounts(&list_accounts, &signed_out_accounts));

    // Change list account state to be stale, which will trigger list accounts
    // request.
    helper.ForceOnCookieChangeProcessing();

    // Receive an unexpected response from the server. Listed accounts as well
    // as the pref should be cleared.
    expected_accounts.clear();
    expected_signed_out_accounts.clear();
    GoogleServiceAuthError error(
        GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    EXPECT_CALL(observer,
                OnGaiaAccountsInCookieUpdated(
                    ListedAccountEquals(expected_accounts),
                    ListedAccountEquals(expected_signed_out_accounts), error));
    SimulateListAccountsSuccess(&helper, "[]");
    EXPECT_FALSE(helper.ListAccounts(&list_accounts, &signed_out_accounts));

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
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
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
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
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
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
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
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
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

TEST_F(GaiaCookieManagerServiceTest, UbertokenSuccessFetchesExternalCC) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());

  EXPECT_CALL(helper, StartFetchingUbertoken());
  helper.AddAccountToCookie(
      account_id1_, gaia::GaiaSource::kChrome,
      GaiaCookieManagerService::AddAccountToCookieCompletedCallback());

  ASSERT_FALSE(IsLoadPending());
  SimulateUbertokenSuccess(&helper, "token");

  // Check there is now a fetcher that belongs to the ExternalCCResultFetcher.
  SimulateGetCheckConnectionInfoSuccess(
      "[{\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");
  GaiaCookieManagerService::ExternalCcResultFetcher* result_fetcher =
      helper.external_cc_result_fetcher_for_testing();
  GaiaCookieManagerService::ExternalCcResultFetcher::LoaderToToken loaders =
      result_fetcher->get_loader_map_for_testing();
  ASSERT_EQ(1u, loaders.size());
  ASSERT_TRUE(IsLoadPending("http://www.bl.com"));
}

TEST_F(GaiaCookieManagerServiceTest, UbertokenSuccessFetchesExternalCCOnce) {
  InstrumentedGaiaCookieManagerService helper(account_tracker_service(),
                                              token_service(), signin_client());

  helper.external_cc_result_fetcher_for_testing()->Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
      base::Unretained(&helper)));

  EXPECT_CALL(helper, StartFetchingUbertoken());
  helper.AddAccountToCookie(
      account_id2_, gaia::GaiaSource::kChrome,
      GaiaCookieManagerService::AddAccountToCookieCompletedCallback());
  // There is already a ExternalCCResultFetch underway. This will trigger
  // StartFetchingMergeSession.
  EXPECT_CALL(helper, StartFetchingMergeSession());
  SimulateUbertokenSuccess(&helper, "token3");
}

TEST_F(GaiaCookieManagerServiceTest, RemoveLoggedOutAccountByGaiaId) {
  const std::string kTestGaiaId1 = "8";
  const std::string kTestGaiaId2 = "9";

  ::testing::NiceMock<InstrumentedGaiaCookieManagerService> helper(
      account_tracker_service(), token_service(), signin_client());
  ::testing::NiceMock<MockObserver> observer(&helper);

  std::vector<gaia::ListedAccount> signed_in_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_FALSE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));

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

  ASSERT_TRUE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));
  ASSERT_THAT(signed_out_accounts,
              ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId1),
                          ListedAccountMatchesGaiaId(kTestGaiaId2)));

  // The removal should notify observers, with one account removed.
  EXPECT_CALL(observer,
              OnGaiaAccountsInCookieUpdated(
                  _, /*signed_out_accounts=*/
                  ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId2)), _));
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(0);
  helper.RemoveLoggedOutAccountByGaiaId(kTestGaiaId1);

  // Verify that ListAccounts wasn't triggered.
  EXPECT_FALSE(helper.is_running());
  testing::Mock::VerifyAndClearExpectations(&helper);

  ASSERT_TRUE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));
  EXPECT_THAT(signed_out_accounts,
              ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId2)));
}

TEST_F(GaiaCookieManagerServiceTest,
       RemoveLoggedOutAccountByGaiaIdWhileAccountsStale) {
  const std::string kTestGaiaId1 = "8";

  ::testing::NiceMock<InstrumentedGaiaCookieManagerService> helper(
      account_tracker_service(), token_service(), signin_client());
  ::testing::NiceMock<MockObserver> observer(&helper);

  std::vector<gaia::ListedAccount> signed_in_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_FALSE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));

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

  ASSERT_FALSE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));
  ASSERT_THAT(signed_out_accounts,
              ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId1)));

  // The removal should be ignored because the account list is stale.
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(_, _, _)).Times(0);
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(0);
  helper.RemoveLoggedOutAccountByGaiaId(kTestGaiaId1);

  // Verify that ListAccounts wasn't triggered again.
  testing::Mock::VerifyAndClearExpectations(&helper);

  ASSERT_FALSE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));
  EXPECT_THAT(signed_out_accounts,
              ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId1)));
}

TEST_F(GaiaCookieManagerServiceTest,
       RemoveLoggedOutAccountByGaiaIdForMissingAccount) {
  const std::string kTestGaiaId1 = "8";
  const std::string kNonListedAccount = "9";

  ::testing::NiceMock<InstrumentedGaiaCookieManagerService> helper(
      account_tracker_service(), token_service(), signin_client());
  ::testing::NiceMock<MockObserver> observer(&helper);

  std::vector<gaia::ListedAccount> signed_in_accounts;
  std::vector<gaia::ListedAccount> signed_out_accounts;
  ASSERT_FALSE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));

  // Simulate one signed out account being listed.
  SimulateListAccountsSuccess(
      &helper,
      base::StringPrintf(
          "[\"f\","
          "[[\"a\", 0, \"n\", \"a@d.com\", \"p\", 0, 0, 0, 0, 1, \"%s\","
          "null,null,null,1]]]",
          kTestGaiaId1.c_str()));

  ASSERT_TRUE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));
  ASSERT_THAT(signed_out_accounts,
              ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId1)));

  // The removal should be ignored because the Gaia ID is not listed/known.
  EXPECT_CALL(observer, OnGaiaAccountsInCookieUpdated(_, _, _)).Times(0);
  EXPECT_CALL(helper, StartFetchingListAccounts()).Times(0);
  helper.RemoveLoggedOutAccountByGaiaId(kNonListedAccount);

  // Verify that ListAccounts wasn't triggered.
  EXPECT_FALSE(helper.is_running());
  testing::Mock::VerifyAndClearExpectations(&helper);

  ASSERT_TRUE(helper.ListAccounts(&signed_in_accounts, &signed_out_accounts));
  EXPECT_THAT(signed_out_accounts,
              ElementsAre(ListedAccountMatchesGaiaId(kTestGaiaId1)));
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
