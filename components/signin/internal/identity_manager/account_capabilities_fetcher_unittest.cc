// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

#include <optional>
#include <string_view>

#include "account_capabilities_fetcher.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/jni_android.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_android.h"
#include "components/signin/public/android/test_support_jni_headers/AccountCapabilitiesFetcherTestUtil_jni.h"
#else

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher_gaia.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using ::testing::_;
using ::testing::Eq;

CoreAccountInfo GetTestAccountInfoByEmail(const std::string& email) {
  CoreAccountInfo result;
  result.email = email;
  result.gaia = signin::GetTestGaiaIdForEmail(email);
  result.account_id = CoreAccountId::FromGaiaId(result.gaia);
  return result;
}

#if BUILDFLAG(IS_ANDROID)
class TestSupportAndroid {
 public:
  TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_ref =
        signin::Java_AccountCapabilitiesFetcherTestUtil_Constructor(env);
    java_test_util_ref_.Reset(env, java_ref.obj());
  }

  ~TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    signin::Java_AccountCapabilitiesFetcherTestUtil_destroy(
        env, java_test_util_ref_);
  }

  void AddAccount(const CoreAccountInfo& account_info) {
    JNIEnv* env = base::android::AttachCurrentThread();
    signin::Java_AccountCapabilitiesFetcherTestUtil_expectAccount(
        env, java_test_util_ref_,
        ConvertToJavaCoreAccountInfo(env, account_info));
  }

  std::unique_ptr<AccountCapabilitiesFetcher> CreateFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback callback) {
    return std::make_unique<AccountCapabilitiesFetcherAndroid>(
        account_info, fetch_priority, std::move(callback));
  }

  void ReturnAccountCapabilitiesFetchSuccess(
      const CoreAccountInfo& account_info,
      bool capability_value) {
    AccountCapabilities capabilities;
    AccountCapabilitiesTestMutator mutator(&capabilities);
    mutator.SetAllSupportedCapabilities(capability_value);
    ReturnFetchResults(account_info, capabilities);
  }

  void ReturnAccountCapabilitiesFetchFailure(
      const CoreAccountInfo& account_info) {
    // Return an empty `AccountCapabilities` object.
    ReturnFetchResults(account_info, AccountCapabilities());
  }

  void SimulateIssueAccessTokenPersistentError(
      const CoreAccountInfo& account_info) {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  void ReturnFetchResults(const CoreAccountInfo& account_info,
                          const AccountCapabilities& capabilities) {
    JNIEnv* env = base::android::AttachCurrentThread();
    signin::Java_AccountCapabilitiesFetcherTestUtil_returnCapabilities(
        env, java_test_util_ref_,
        ConvertToJavaCoreAccountInfo(env, account_info),
        capabilities.ConvertToJavaAccountCapabilities(env));
  }

  base::android::ScopedJavaGlobalRef<jobject> java_test_util_ref_;
};

using TestSupport = TestSupportAndroid;
#else
using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

const char kAccountCapabilitiesResponseFormat[] =
    R"({"accountCapabilities": [%s]})";

const char kSingleCapabilitiyResponseFormat[] =
    R"({"name": "%s", "booleanValue": %s})";

const char kCapabilityParamName[] = "names=";

std::string GenerateValidAccountCapabilitiesResponse(bool capability_value) {
  std::vector<std::string> dict_array;
  for (const std::string& name :
       AccountCapabilitiesTestMutator::GetSupportedAccountCapabilityNames()) {
    dict_array.push_back(
        base::StringPrintf(kSingleCapabilitiyResponseFormat, name.c_str(),
                           capability_value ? "true" : "false"));
  }
  return base::StringPrintf(kAccountCapabilitiesResponseFormat,
                            base::JoinString(dict_array, ",").c_str());
}

void VerifyAccountCapabilitiesRequest(const network::ResourceRequest& request) {
  EXPECT_EQ(request.method, "POST");
  std::string_view request_string = request.request_body->elements()
                                        ->at(0)
                                        .As<network::DataElementBytes>()
                                        .AsStringPiece();
  // The request body should look like:
  // "names=Name1&names=Name2&names=Name3"
  std::vector<std::string> requested_capabilities = base::SplitString(
      request_string, "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (auto& name : requested_capabilities) {
    EXPECT_TRUE(base::StartsWith(name, kCapabilityParamName));
    name = name.substr(std::strlen(kCapabilityParamName));
  }
  EXPECT_THAT(requested_capabilities,
              ::testing::ContainerEq(AccountCapabilitiesTestMutator::
                                         GetSupportedAccountCapabilityNames()));
}
class TestSupportGaia {
 public:
  TestSupportGaia() : fake_oauth2_token_service_(&pref_service_) {
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  }

  void AddAccount(const CoreAccountInfo& account_info) {
    const CoreAccountId& account_id = account_info.account_id;
    fake_oauth2_token_service_.UpdateCredentials(
        account_id, base::StringPrintf("fake-refresh-token-%s",
                                       account_id.ToString().c_str()));
  }

  std::unique_ptr<AccountCapabilitiesFetcher> CreateFetcher(
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback callback) {
    return std::make_unique<AccountCapabilitiesFetcherGaia>(
        &fake_oauth2_token_service_,
        test_url_loader_factory_.GetSafeWeakWrapper(), account_info,
        fetch_priority, std::move(callback));
  }

  void ReturnAccountCapabilitiesFetchSuccess(
      const CoreAccountInfo& account_info,
      bool capability_value) {
    IssueAccessToken(account_info.account_id);
    ReturnFetchResults(
        GaiaUrls::GetInstance()->account_capabilities_url(), net::HTTP_OK,
        GenerateValidAccountCapabilitiesResponse(capability_value));
  }

  void ReturnAccountCapabilitiesFetchFailure(
      const CoreAccountInfo& account_info) {
    IssueAccessToken(account_info.account_id);
    ReturnFetchResults(GaiaUrls::GetInstance()->account_capabilities_url(),
                       net::HTTP_BAD_REQUEST, std::string());
  }

  void SimulateIssueAccessTokenPersistentError(
      const CoreAccountInfo& account_info) {
    fake_oauth2_token_service_.IssueErrorForAllPendingRequestsForAccount(
        account_info.account_id,
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

 private:
  void ReturnFetchResults(const GURL& url,
                          net::HttpStatusCode response_code,
                          const std::string& response_string) {
    EXPECT_TRUE(test_url_loader_factory_.IsPending(url.spec()));

    // It's possible for multiple requests to be pending. Respond to all of
    // them.
    while (test_url_loader_factory_.IsPending(url.spec())) {
      VerifyAccountCapabilitiesRequest(
          test_url_loader_factory_.GetPendingRequest(/*index=*/0)->request);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(response_code), response_string,
          network::TestURLLoaderFactory::kMostRecentMatch);
    }
  }

  void IssueAccessToken(const CoreAccountId& account_id) {
    fake_oauth2_token_service_.IssueAllTokensForAccount(
        account_id, TokenResponseBuilder()
                        .WithAccessToken(base::StringPrintf(
                            "access_token-%s", account_id.ToString().c_str()))
                        .WithExpirationTime(base::Time::Max())
                        .build());
  }

  TestingPrefServiceSimple pref_service_;
  FakeProfileOAuth2TokenService fake_oauth2_token_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

using TestSupport = TestSupportGaia;
#endif

}  // namespace

class AccountCapabilitiesFetcherTest : public ::testing::Test {
 public:
  void SetUp() override { test_support_.AddAccount(account_info()); }

  std::unique_ptr<AccountCapabilitiesFetcher> CreateFetcher(
      AccountCapabilitiesFetcher::OnCompleteCallback callback,
      AccountCapabilitiesFetcher::FetchPriority priority =
          AccountCapabilitiesFetcher::FetchPriority::kForeground) {
    return test_support_.CreateFetcher(account_info(), priority,
                                       std::move(callback));
  }

  void ReturnAccountCapabilitiesFetchSuccess(bool capability_value) {
    test_support_.ReturnAccountCapabilitiesFetchSuccess(account_info(),
                                                        capability_value);
  }

  void ReturnAccountCapabilitiesFetchFailure() {
    test_support_.ReturnAccountCapabilitiesFetchFailure(account_info());
  }

  void SimulateIssueAccessTokenPersistentError() {
    test_support_.SimulateIssueAccessTokenPersistentError(account_info());
  }

  const CoreAccountInfo& account_info() { return account_info_; }
  const CoreAccountId& account_id() { return account_info_.account_id; }

 private:
  base::test::TaskEnvironment task_environment_;
  CoreAccountInfo account_info_ = GetTestAccountInfoByEmail("test@gmail.com");
  TestSupport test_support_;
};

TEST_F(AccountCapabilitiesFetcherTest, Success_True) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  AccountCapabilities expected_capabilities;
  AccountCapabilitiesTestMutator mutator(&expected_capabilities);
  mutator.SetAllSupportedCapabilities(true);
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback,
              Run(account_id(), ::testing::Optional(expected_capabilities)));
  ReturnAccountCapabilitiesFetchSuccess(true);

#if !BUILDFLAG(IS_ANDROID)
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Success", 1);
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Failure", 0);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.Foreground.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kSuccess, 1);
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(AccountCapabilitiesFetcherTest, Success_True_Background) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher = CreateFetcher(
      callback.Get(), AccountCapabilitiesFetcher::FetchPriority::kBackground);
  AccountCapabilities expected_capabilities;
  AccountCapabilitiesTestMutator mutator(&expected_capabilities);
  mutator.SetAllSupportedCapabilities(true);
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback,
              Run(account_id(), ::testing::Optional(expected_capabilities)));
  ReturnAccountCapabilitiesFetchSuccess(true);

#if !BUILDFLAG(IS_ANDROID)
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Background.FetchDuration.Success", 1);
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Background.FetchDuration.Failure", 0);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.Background.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kSuccess, 1);
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(AccountCapabilitiesFetcherTest, Success_False) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  AccountCapabilities expected_capabilities;
  AccountCapabilitiesTestMutator mutator(&expected_capabilities);
  mutator.SetAllSupportedCapabilities(false);

  fetcher->Start();
  EXPECT_CALL(callback,
              Run(account_id(), ::testing::Optional(expected_capabilities)));
  ReturnAccountCapabilitiesFetchSuccess(false);
}

TEST_F(AccountCapabilitiesFetcherTest, FetchFailure) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  base::HistogramTester tester;

  fetcher->Start();
  std::optional<AccountCapabilities> expected_capabilities;
#if BUILDFLAG(IS_ANDROID)
  // Android never returns std::nullopt even if the fetcher has failed to get
  // all capabilities.
  expected_capabilities = AccountCapabilities();
#endif
  EXPECT_CALL(callback, Run(account_id(), Eq(expected_capabilities)));
  ReturnAccountCapabilitiesFetchFailure();

#if !BUILDFLAG(IS_ANDROID)
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Success", 0);
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Failure", 1);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.Foreground.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kOAuthError, 1);
#endif  // !BUILDFLAG(IS_ANDROID)
}

// Exclude Android because `AccountCapabilitiesFetcherAndroid` doesn't request
// an access token.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(AccountCapabilitiesFetcherTest, TokenFailure) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback, Run(account_id(), Eq(std::nullopt)));
  SimulateIssueAccessTokenPersistentError();

  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Success", 0);
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Failure", 1);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.Foreground.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kGetTokenFailure, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(AccountCapabilitiesFetcherTest, Cancelled) {
  base::MockCallback<AccountCapabilitiesFetcher::OnCompleteCallback> callback;
  std::unique_ptr<AccountCapabilitiesFetcher> fetcher =
      CreateFetcher(callback.Get());
  base::HistogramTester tester;

  fetcher->Start();
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  fetcher.reset();

#if !BUILDFLAG(IS_ANDROID)
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Success", 0);
  tester.ExpectTotalCount(
      "Signin.AccountCapabilities.Foreground.FetchDuration.Failure", 1);
  tester.ExpectUniqueSample(
      "Signin.AccountCapabilities.Foreground.FetchResult",
      AccountCapabilitiesFetcherGaia::FetchResult::kCancelled, 1);
#endif  // !BUILDFLAG(IS_ANDROID)
}
