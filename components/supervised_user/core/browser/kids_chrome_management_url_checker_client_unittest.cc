// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/features.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

using kids_chrome_management::ClassifyUrlResponse;
using testing::_;

static constexpr std::string_view kKidsApiEndpoint{
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
    "me:classifyUrl?alt=proto"};
static constexpr std::string_view kSafeSitesEndpoint{
    "https://safesearch.googleapis.com/v1:classify"};

std::string ConvertToString(
    safe_search_api::ClientClassification classification) {
  switch (classification) {
    case safe_search_api::ClientClassification::kAllowed:
      return "allowed";
    case safe_search_api::ClientClassification::kRestricted:
      return "restricted";
    default:
      NOTREACHED_NORETURN();
  }
}

class KidsChromeManagementURLCheckerClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    url_classifier_ = std::make_unique<KidsChromeManagementURLCheckerClient>(
        identity_test_env_.identity_manager(),
        test_url_loader_factory_.GetSafeWeakWrapper(), "us");
  }

 protected:
  void MakePrimaryAccountAvailable() {
    this->identity_test_env_.MakePrimaryAccountAvailable(
        "homer@gmail.com", signin::ConsentLevel::kSignin);
    this->identity_test_env_.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  }

  void SimulateKidsApiResponse(
      kids_chrome_management::ClassifyUrlResponse::DisplayClassification
          display_classification) {
    kids_chrome_management::ClassifyUrlResponse response;
    response.set_display_classification(display_classification);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kKidsApiEndpoint), response.SerializeAsString());
  }
  void SimulateSafeSitesResponse(
      safe_search_api::ClientClassification classification) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kSafeSitesEndpoint),
        base::StringPrintf(R"json(
          {"displayClassification": "%s"}
        )json",ConvertToString(classification).c_str()));
  }

  void SimulateMalformedResponse() {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kKidsApiEndpoint),
        /*content=*/"garbage");
  }
  void SimulateNetworkError(int net_error) {
    this->test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kKidsApiEndpoint), network::URLLoaderCompletionStatus(net_error),
        network::CreateURLResponseHead(net::HTTP_OK), /*content=*/"");
  }
  void SimulateHttpError(net::HttpStatusCode http_status) {
    this->test_url_loader_factory_.SimulateResponseForPendingRequest(
        std::string(kKidsApiEndpoint),
        /*content=*/"", http_status);
  }

  // Asynchronously checks the URL and waits until finished.
  void CheckUrl(base::StringPiece url) {
    StartCheckUrl(url);
    task_environment_.RunUntilIdle();
  }

  // Starts a URL check, but doesn't wait for ClassifyURL() to finish.
  void CheckUrlWithoutResponse(base::StringPiece url) { StartCheckUrl(url); }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url,
                    safe_search_api::ClientClassification classification));

  void DestroyUrlClassifier() { url_classifier_.reset(); }

  base::test::TaskEnvironment task_environment_;

 private:
  void StartCheckUrl(base::StringPiece url) {
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
  base::test::ScopedFeatureList shadow_call_feature_list_{kShadowKidsApiWithSafeSites};
};

TEST_F(KidsChromeManagementURLCheckerClientTest, UrlAllowed) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kRestricted);
  SimulateKidsApiResponse(kids_chrome_management::ClassifyUrlResponse::ALLOWED);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, HistogramsAreEmitted) {
  base::HistogramTester histogram_tester;
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kRestricted);
  SimulateKidsApiResponse(kids_chrome_management::ClassifyUrlResponse::ALLOWED);

  // Non-proto test is mocking the whole client thus bypassing the http stack.
  histogram_tester.ExpectTotalCount("FamilyLinkUser.ClassifyUrlRequest.Latency",
                                    /*expected_count(grew by)*/ 1);
  histogram_tester.ExpectTotalCount("Enterprise.SafeSites.Latency",
                                    /*expected_count(grew by)*/ 1);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, UrlRestricted) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kRestricted));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);
  SimulateKidsApiResponse(
      kids_chrome_management::ClassifyUrlResponse::RESTRICTED);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, AccessTokenError) {
  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, NetworkError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);
  SimulateNetworkError(net::ERR_UNEXPECTED);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, HttpError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);
  SimulateHttpError(net::HTTP_BAD_GATEWAY);
}

TEST_F(KidsChromeManagementURLCheckerClientTest, ServiceError) {
  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);
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
}  // namespace
}  // namespace supervised_user
