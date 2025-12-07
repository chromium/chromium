// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

using kidsmanagement::ClassifyUrlResponse;
using testing::_;

static constexpr std::string_view kKidsApiEndpoint{
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
    "me:classifyUrl?alt=proto"};

// By default, bootstraps client in "FamilyLink" mode, which implies at least
// "best effort" credentials mode.
class KidsChromeManagementURLCheckerClientTest : public ::testing::Test {
 protected:
  KidsChromeManagementURLCheckerClientTest() {
    RegisterProfilePrefs(pref_service_.registry());
    url_classifier_ = std::make_unique<KidsChromeManagementURLCheckerClient>(
        identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), pref_service_, "us",
        version_info::Channel::UNKNOWN);
  }
  void SetUp() override { EnableParentalControls(pref_service_); }

  void MakePrimaryAccountAvailable() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "homer@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  void StopAutomaticIssueOfAccessTokens() {
    identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/false);
  }

  void SimulateAccessTokenError() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

  void SimulateKidsApiResponse(
      kidsmanagement::ClassifyUrlResponse::DisplayClassification
          display_classification) {
    kidsmanagement::ClassifyUrlResponse response;
    response.set_display_classification(display_classification);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kKidsApiEndpoint), response.SerializeAsString());
  }

  void SimulateMalformedResponse() {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kKidsApiEndpoint),
        /*content=*/"garbage");
  }

  void SimulateNetworkError(int net_error) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kKidsApiEndpoint), network::URLLoaderCompletionStatus(net_error),
        network::CreateURLResponseHead(net::HTTP_OK), /*content=*/"");
  }

  void SimulateHttpError(net::HttpStatusCode http_status) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kKidsApiEndpoint),
        /*content=*/"", http_status);
  }

  // Asynchronously checks the URL and waits until finished.
  void CheckUrl(std::string_view url) {
    StartCheckUrl(url);
    task_environment_.RunUntilIdle();
  }

  // Starts a URL check, but doesn't wait for ClassifyURL() to finish.
  void CheckUrlWithoutResponse(std::string_view url) { StartCheckUrl(url); }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url,
                    safe_search_api::ClientClassification classification));

  void DestroyUrlClassifier() { url_classifier_.reset(); }


 private:
  void StartCheckUrl(std::string_view url) {
    url_classifier_->CheckURL(
        GURL(url),
        base::BindOnce(&KidsChromeManagementURLCheckerClientTest::OnCheckDone,
                       base::Unretained(this)));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<KidsChromeManagementURLCheckerClient> url_classifier_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(KidsChromeManagementURLCheckerClientTest, UrlAllowed) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, HistogramsAreEmitted) {
  base::HistogramTester histogram_tester;
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);

  // Non-proto test is mocking the whole client thus bypassing the http stack.
  histogram_tester.ExpectTotalCount("FamilyLinkUser.ClassifyUrlRequest.Latency",
                                    /*expected_count(grew by)*/ 1);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, UrlRestricted) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kRestricted));
  CheckUrl("http://example.com");

  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::RESTRICTED);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(KidsChromeManagementURLCheckerClientTest, NoPrimaryAccount) {
  // On desktop platforms, uncredentialed access is allowed.
  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");
  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
}
#elif BUILDFLAG(IS_ANDROID)
TEST_F(KidsChromeManagementURLCheckerClientTest, NoPrimaryAccount) {
  // On Android, uncredentialed access will hang on access token wait.
  EXPECT_CALL(*this, OnCheckDone(GURL("http://example.com"), _)).Times(0);
  CheckUrl("http://example.com");
}
#else
TEST_F(KidsChromeManagementURLCheckerClientTest, NoPrimaryAccount) {
  ASSERT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  // On other platforms platforms, uncredentialed classification is not
  // available.
  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");
}
#endif

TEST_F(KidsChromeManagementURLCheckerClientTest, AccessTokenError) {
  MakePrimaryAccountAvailable();
  StopAutomaticIssueOfAccessTokens();

  // This outcome depends on the feature flag values.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // We fallback to making an uncredentialed request to ClassifyUrl, which
  // succeeds.
  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
#else
  // We fail the request when we fail the access token fetch (returning
  // unknown) to the client.
  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
#endif

  CheckUrl("http://example.com");
  SimulateAccessTokenError();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
#endif
}

TEST_F(KidsChromeManagementURLCheckerClientTest, NetworkError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  SimulateNetworkError(net::ERR_UNEXPECTED);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, HttpError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  SimulateHttpError(net::HTTP_BAD_GATEWAY);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, ServiceError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  SimulateMalformedResponse();
}

TEST_F(KidsChromeManagementURLCheckerClientTest,
       PendingRequestsAreCanceledWhenClientIsDestroyed) {
  EXPECT_CALL(*this, OnCheckDone(_, _)).Times(0);

  CheckUrlWithoutResponse("http://example.com");
  DestroyUrlClassifier();

  // Now run the callback.
  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(IS_ANDROID)
class KidsChromeManagementURLCheckerClientForRegularUserTest
    : public KidsChromeManagementURLCheckerClientTest {
 protected:
  void SetUp() override { DisableParentalControls(pref_service_); }

 private:
  base::test::ScopedFeatureList feature_list{kAllowNonFamilyLinkUrlFilterMode};
};

TEST_F(KidsChromeManagementURLCheckerClientForRegularUserTest,
       MakesRequestWithoutPrimaryAccount) {
  ASSERT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");
  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
}
#endif

}  // namespace
}  // namespace supervised_user
