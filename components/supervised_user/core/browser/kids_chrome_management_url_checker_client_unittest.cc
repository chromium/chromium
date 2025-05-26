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
#include "components/safe_search_api/url_checker_client.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
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

class KidsChromeManagementURLCheckerClientTest
    : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatureState(
        supervised_user::kUncredentialedFilteringFallbackForSupervisedUsers,
        UncredentialedFilteringFallbackEnabled());
    url_classifier_ = std::make_unique<KidsChromeManagementURLCheckerClient>(
        identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), "us",
        version_info::Channel::UNKNOWN);
  }

 protected:
  bool UncredentialedFilteringFallbackEnabled() { return GetParam(); }

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

  base::test::TaskEnvironment task_environment_;

 private:
  void StartCheckUrl(std::string_view url) {
    url_classifier_->CheckURL(
        GURL(url),
        base::BindOnce(&KidsChromeManagementURLCheckerClientTest::OnCheckDone,
                       base::Unretained(this)));
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<KidsChromeManagementURLCheckerClient> url_classifier_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(KidsChromeManagementURLCheckerClientTest, UrlAllowed) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
}

TEST_P(KidsChromeManagementURLCheckerClientTest, HistogramsAreEmitted) {
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

TEST_P(KidsChromeManagementURLCheckerClientTest, UrlRestricted) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kRestricted));
  CheckUrl("http://example.com");

  SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::RESTRICTED);
}

TEST_P(KidsChromeManagementURLCheckerClientTest, NoPrimaryAccount) {
  // This test does not add a primary account, and therefore no access token is
  // available. On platforms with kWaitUntilAccessTokenAvailableForClassifyUrl
  // enabled this means that the ClassifyUrl call will not be made.
  if (!base::FeatureList::IsEnabled(
          kWaitUntilAccessTokenAvailableForClassifyUrl)) {
    if (UncredentialedFilteringFallbackEnabled()) {
      // We fallback to making an uncredentialed request to ClassifyUrl, which
      // succeeds.
      EXPECT_CALL(*this,
                  OnCheckDone(GURL("http://example.com"),
                              safe_search_api::ClientClassification::kAllowed));
    } else {
      EXPECT_CALL(*this,
                  OnCheckDone(GURL("http://example.com"),
                              safe_search_api::ClientClassification::kUnknown));
    }
  }
  CheckUrl("http://example.com");

  if (UncredentialedFilteringFallbackEnabled()) {
    SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
  }
}

TEST_P(KidsChromeManagementURLCheckerClientTest, AccessTokenError) {
  MakePrimaryAccountAvailable();
  StopAutomaticIssueOfAccessTokens();

  // This outcome depents on the feature flag values.
  if (UncredentialedFilteringFallbackEnabled()) {
    // We fallback to making an uncredentialed request to ClassifyUrl, which
    // succeeds.
    EXPECT_CALL(*this,
                OnCheckDone(GURL("http://example.com"),
                            safe_search_api::ClientClassification::kAllowed));
  } else {
    // We fail the request when we fail the access token fetch (returning
    // unknown) to the client.
    EXPECT_CALL(*this,
                OnCheckDone(GURL("http://example.com"),
                            safe_search_api::ClientClassification::kUnknown));
  }

  CheckUrl("http://example.com");

  SimulateAccessTokenError();
  if (UncredentialedFilteringFallbackEnabled()) {
    SimulateKidsApiResponse(kidsmanagement::ClassifyUrlResponse::ALLOWED);
  }
}

TEST_P(KidsChromeManagementURLCheckerClientTest, NetworkError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  SimulateNetworkError(net::ERR_UNEXPECTED);
}

TEST_P(KidsChromeManagementURLCheckerClientTest, HttpError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  SimulateHttpError(net::HTTP_BAD_GATEWAY);
}

TEST_P(KidsChromeManagementURLCheckerClientTest, ServiceError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  SimulateMalformedResponse();
}

TEST_P(KidsChromeManagementURLCheckerClientTest,
       PendingRequestsAreCanceledWhenClientIsDestroyed) {
  EXPECT_CALL(*this, OnCheckDone(_, _)).Times(0);

  CheckUrlWithoutResponse("http://example.com");
  DestroyUrlClassifier();

  // Now run the callback.
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    KidsChromeManagementURLCheckerClientTest,
    KidsChromeManagementURLCheckerClientTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "UncredentialedFilteringFallbackEnabled"
                        : "UncredentialedFilteringFallbackDisabled";
    });
}  // namespace
}  // namespace supervised_user
