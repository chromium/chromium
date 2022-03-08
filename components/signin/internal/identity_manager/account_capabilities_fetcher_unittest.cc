// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_gaia.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;
using ::testing::_;
using ::testing::Eq;

const char kAccountCapabilitiesResponseFormat[] =
    R"({
        "accountCapabilities": [
          {"name": "%s", "booleanValue": %s}
        ]
       })";

std::string GenerateValidAccountCapabilitiesResponse(bool capability_value) {
  return base::StringPrintf(kAccountCapabilitiesResponseFormat,
                            kCanOfferExtendedChromeSyncPromosCapabilityName,
                            capability_value ? "true" : "false");
}

}  // namespace

class AccountCapabilitiesFetcherGaiaTest : public testing::Test {
 public:
  AccountCapabilitiesFetcherGaiaTest()
      : fake_oauth2_token_service_(&pref_service_) {
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  }

  void SetUp() override {
    fake_oauth2_token_service_.UpdateCredentials(
        account_id_, base::StringPrintf("fake-refresh-token-%s",
                                        account_id_.ToString().c_str()));
  }

  std::unique_ptr<AccountCapabilitiesFetcher> CreateFetcher(
      AccountCapabilitiesFetcher::OnCompleteCallback callback) {
    return std::make_unique<AccountCapabilitiesFetcherGaia>(
        &fake_oauth2_token_service_,
        test_url_loader_factory_.GetSafeWeakWrapper(), account_id_,
        std::move(callback));
  }

  void ReturnAccountCapabilitiesFetchSuccess(bool capability_value) {
    IssueAccessToken();
    ReturnFetchResults(
        GaiaUrls::GetInstance()->account_capabilities_url(), net::HTTP_OK,
        GenerateValidAccountCapabilitiesResponse(capability_value));
  }

  void ReturnAccountCapabilitiesFetchFailure() {
    IssueAccessToken();
    ReturnFetchResults(GaiaUrls::GetInstance()->account_capabilities_url(),
                       net::HTTP_BAD_REQUEST, std::string());
  }

  void SimulateIssueAccessTokenPersistentError() {
    fake_oauth2_token_service_.IssueErrorForAllPendingRequestsForAccount(
        account_id_, GoogleServiceAuthError(
                         GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

  const CoreAccountId& account_id() { return account_id_; }

 private:
  void ReturnFetchResults(const GURL& url,
                          net::HttpStatusCode response_code,
                          const std::string& response_string) {
    EXPECT_TRUE(test_url_loader_factory_.IsPending(url.spec()));

    // It's possible for multiple requests to be pending. Respond to all of
    // them.
    while (test_url_loader_factory_.IsPending(url.spec())) {
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(response_code), response_string,
          network::TestURLLoaderFactory::kMostRecentMatch);
    }
  }

  void IssueAccessToken() {
    fake_oauth2_token_service_.IssueAllTokensForAccount(
        account_id_, TokenResponseBuilder()
                         .WithAccessToken(base::StringPrintf(
                             "access_token-%s", account_id_.ToString().c_str()))
                         .WithExpirationTime(base::Time::Max())
                         .build());
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  FakeProfileOAuth2TokenService fake_oauth2_token_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  CoreAccountId account_id_ = CoreAccountId::FromEmail("test@gmail.com");
};

TEST_F(AccountCapabilitiesFetcherGaiaTest, Success_True) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  AccountCapabilities expected_capabilities;
  AccountCapabilitiesTestMutator mutator(&expected_capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback,
              Run(account_id(), ::testing::Optional(expected_capabilities)));
  ReturnAccountCapabilitiesFetchSuccess(true);

  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Success",
                          1);
  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Failure",
                          0);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kSuccess, 1);
}

TEST_F(AccountCapabilitiesFetcherGaiaTest, Success_False) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  AccountCapabilities expected_capabilities;
  AccountCapabilitiesTestMutator mutator(&expected_capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(false);

  fetcher->Start();
  EXPECT_CALL(callback,
              Run(account_id(), ::testing::Optional(expected_capabilities)));
  ReturnAccountCapabilitiesFetchSuccess(false);
}

TEST_F(AccountCapabilitiesFetcherGaiaTest, FetchFailure) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback, Run(account_id(), Eq(absl::nullopt)));
  ReturnAccountCapabilitiesFetchFailure();

  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Success",
                          0);
  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Failure",
                          1);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kOAuthError, 1);
}

TEST_F(AccountCapabilitiesFetcherGaiaTest, TokenFailure) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback, Run(account_id(), Eq(absl::nullopt)));
  SimulateIssueAccessTokenPersistentError();

  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Success",
                          0);
  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Failure",
                          1);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kGetTokenFailure, 1);
}

TEST_F(AccountCapabilitiesFetcherGaiaTest, Cancelled) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  fetcher.reset();

  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Success",
                          0);
  tester.ExpectTotalCount("Signin.AccountCapabilities.FetchDuration.Failure",
                          1);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kCancelled, 1);
}
