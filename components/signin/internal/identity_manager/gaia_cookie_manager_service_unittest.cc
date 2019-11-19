// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAccountId1[] = "account_id1";
const char kAccountId2[] = "account_id2";
const char kAccountId3[] = "account_id3";
const char kAccountId4[] = "account_id4";

using MockAddAccountToCookieCompletedCallback = base::MockCallback<
    GaiaCookieManagerService::AddAccountToCookieCompletedCallback>;

class MockObserver {
 public:
  explicit MockObserver(GaiaCookieManagerService* helper) {
    helper->SetGaiaAccountsInCookieUpdatedCallback(base::BindRepeating(
        &MockObserver::OnGaiaAccountsInCookieUpdated, base::Unretained(this)));
  }

  MOCK_METHOD3(OnGaiaAccountsInCookieUpdated,
               void(const std::vector<gaia::ListedAccount>&,
                    const std::vector<gaia::ListedAccount>&,
                    const GoogleServiceAuthError&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
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

// Custom matcher for ListedAccounts.
MATCHER_P(ListedAccountEquals, expected, "") {
  return AreAccountListsEqual(expected, arg);
}

class InstrumentedGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  InstrumentedGaiaCookieManagerService(ProfileOAuth2TokenService* token_service,
                                       SigninClient* signin_client)
      : GaiaCookieManagerService(token_service, signin_client) {
    total++;
  }

  ~InstrumentedGaiaCookieManagerService() override { total--; }

  MOCK_METHOD0(StartFetchingUbertoken, void());
  MOCK_METHOD0(StartFetchingListAccounts, void());
  MOCK_METHOD0(StartFetchingLogOut, void());
  MOCK_METHOD0(StartFetchingMergeSession, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(InstrumentedGaiaCookieManagerService);
};

class GaiaCookieManagerServiceTest : public testing::Test {
 public:
  GaiaCookieManagerServiceTest()
      : account_id1_(kAccountId1),
        account_id2_(kAccountId2),
        account_id3_(kAccountId3),
        account_id4_(kAccountId4),
        no_error_(GoogleServiceAuthError::NONE),
        error_(GoogleServiceAuthError::SERVICE_ERROR),
        canceled_(GoogleServiceAuthError::REQUEST_CANCELED) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    GaiaCookieManagerService::RegisterPrefs(pref_service_.registry());
    signin_client_ = std::make_unique<TestSigninClient>(&pref_service_);
    token_service_ =
        std::make_unique<FakeProfileOAuth2TokenService>(&pref_service_);
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
    OAuth2AccessTokenConsumer::TokenResponse token_response =
        OAuth2AccessTokenConsumer::TokenResponse("AccessToken", base::Time(),
                                                 "Idtoken");
    consumer->OnGetTokenSuccess(request, token_response);
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
    test_task_runner->FastForwardBy(advance_by +
                                    base::TimeDelta::FromMilliseconds(1));
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
  base::test::TaskEnvironment task_environment_;
  GoogleServiceAuthError no_error_;
  GoogleServiceAuthError error_;
  GoogleServiceAuthError canceled_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<FakeProfileOAuth2TokenService> token_service_;
};

}  // namespace

using ::testing::_;

TEST_F(GaiaCookieManagerServiceTest, Success) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, no_error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token");
}

TEST_F(GaiaCookieManagerServiceTest, FailedMergeSession) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);
  signin_client()->set_are_signin_cookies_allowed(false);

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, canceled()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetried) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::ScopedClosureRunner task_runner_ =
      base::ThreadTaskRunnerHandle::OverrideForTesting(test_task_runner);

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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);
  base::HistogramTester histograms;

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::ScopedClosureRunner task_runner_ =
      base::ThreadTaskRunnerHandle::OverrideForTesting(test_task_runner);

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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id1_, error()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateUbertokenFailure(&helper, error());
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterSuccess) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  SimulateLogOutSuccess(&helper);
  ASSERT_FALSE(helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsFails) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  SimulateLogOutFailure(&helper, error());
  // CookieManagerService is still running; it is retrying the failed logout.
  ASSERT_TRUE(helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsAfterOneAddInQueue) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsAfterTwoAddsInQueue) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, canceled()));

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  // The Log Out should prevent this AddAccount from being fetched.
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsTwice) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed;
  EXPECT_CALL(add_account_to_cookie_completed, Run(account_id2_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  // Only one LogOut will be fetched.
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeAdd) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed2,
      add_account_to_cookie_completed3;
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed3, Run(account_id3_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed3.Get());

  SimulateLogOutSuccess(&helper);
  // After LogOut the MergeSession should be fetched.
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, LogOutAllAccountsBeforeLogoutAndAdd) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed2,
      add_account_to_cookie_completed3;
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed3, Run(account_id3_, no_error()));

  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  SimulateMergeSessionSuccess(&helper, "token1");

  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  // Second LogOut will never be fetched.
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);
  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed3.Get());

  SimulateLogOutSuccess(&helper);
  // After LogOut the MergeSession should be fetched.
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, PendingSigninThenSignout) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  // From the first Signin.
  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));

  // From the sign out and then re-sign in.
  EXPECT_CALL(helper, StartFetchingLogOut());

  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed3;
  EXPECT_CALL(add_account_to_cookie_completed3, Run(account_id3_, no_error()));

  // Total sign in 2 times, not enforcing ordered sequences.
  EXPECT_CALL(helper, StartFetchingUbertoken()).Times(2);

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);

  helper.AddAccountToCookie(account_id3_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed3.Get());
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(GaiaCookieManagerServiceTest, CancelSignIn) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetchingUbertoken());
  MockAddAccountToCookieCompletedCallback add_account_to_cookie_completed1,
      add_account_to_cookie_completed2;
  EXPECT_CALL(add_account_to_cookie_completed1, Run(account_id1_, no_error()));
  EXPECT_CALL(add_account_to_cookie_completed2, Run(account_id2_, canceled()));
  EXPECT_CALL(helper, StartFetchingLogOut());

  helper.AddAccountToCookie(account_id1_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed1.Get());
  helper.AddAccountToCookie(account_id2_, gaia::GaiaSource::kChrome,
                            add_account_to_cookie_completed2.Get());
  helper.LogOutAllAccounts(gaia::GaiaSource::kChrome);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogOutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, ListAccountsFirstReturnsEmpty) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
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

  // Sanity-check that ListAccounts returns the cached data.
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
    InstrumentedGaiaCookieManagerService helper(token_service(),
                                                signin_client());
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
    InstrumentedGaiaCookieManagerService helper(token_service(),
                                                signin_client());
    MockObserver observer(&helper);
    auto test_task_runner =
        base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::ScopedClosureRunner task_runner_ =
        base::ThreadTaskRunnerHandle::OverrideForTesting(test_task_runner);

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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
      base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnctionInfo.
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
      base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnctionInfo.
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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  EXPECT_CALL(helper, StartFetchingMergeSession());
  result_fetcher.Start(base::BindOnce(
      &InstrumentedGaiaCookieManagerService::StartFetchingMergeSession,
      base::Unretained(&helper)));

  // Simulate a successful completion of GetCheckConnctionInfo.
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

TEST_F(GaiaCookieManagerServiceTest, UbertokenSuccessFetchesExternalCC) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());

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
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());

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
