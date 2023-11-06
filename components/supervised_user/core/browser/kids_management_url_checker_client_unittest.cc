// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_management_url_checker_client.h"

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
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using UseProtoFetcher = bool;
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

class KidsManagementURLCheckerClientTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<UseProtoFetcher> {
 public:
  KidsManagementURLCheckerClientTest() {
    if (UseProtoFetcher()) {
      proto_api_feature_list_.InitWithFeatures(
          /*enabled_features=*/{supervised_user::kEnableProtoApiForClassifyUrl},
          /*disabled_features=*/{});
    }
    shadow_call_feature_list_.InitWithFeatures(
        /*enabled_features=*/{supervised_user::kShadowKidsApiWithSafeSites},
        /*disabled_features=*/{});
  }

  ~KidsManagementURLCheckerClientTest() override {
    if (UseProtoFetcher()) {
      // Since scoped_feature_list_::Init* / scoped_feature_list_.Reset are
      // stack-based clean-up in the same life-cycle moment.
      shadow_call_feature_list_.Reset();
      proto_api_feature_list_.Reset();
    }
  }

  void SetUp() override {
    test_kids_chrome_management_client_ =
        std::make_unique<kids_management::KidsChromeManagementClientForTesting>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            identity_test_env_.identity_manager());
    url_classifier_ = std::make_unique<KidsManagementURLCheckerClient>(
        test_kids_chrome_management_client_.get(), "us");
  }

 protected:
  // TODO(b/276898959): Remove after migration.
  void SetUpLegacyClientResponse(
      safe_search_api::ClientClassification client_classification,
      KidsChromeManagementClient::ErrorCode error_code) {
    test_kids_chrome_management_client_->SetResponseWithError(
        kids_management::BuildResponseProto(client_classification), error_code);
  }

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
        )json",
                           ConvertToString(classification).c_str()));
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

  bool UseProtoFetcher() const { return GetParam(); }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url,
                    safe_search_api::ClientClassification classification));

  void DestroyUrlClassifier() { url_classifier_.reset(); }

  base::test::TaskEnvironment task_environment_;

 private:
  void StartCheckUrl(base::StringPiece url) {
    url_classifier_->CheckURL(
        GURL(url),
        base::BindOnce(&KidsManagementURLCheckerClientTest::OnCheckDone,
                       base::Unretained(this)));
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<kids_management::KidsChromeManagementClientForTesting>
      test_kids_chrome_management_client_;
  std::unique_ptr<KidsManagementURLCheckerClient> url_classifier_;

  base::test::ScopedFeatureList proto_api_feature_list_;
  base::test::ScopedFeatureList shadow_call_feature_list_;
};

TEST_P(KidsManagementURLCheckerClientTest, UrlAllowed) {
  if (!UseProtoFetcher()) {
    // TODO(b/276898959): Remove branch after migration.
    SetUpLegacyClientResponse(safe_search_api::ClientClassification::kAllowed,
                              KidsChromeManagementClient::ErrorCode::kSuccess);
  }

  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kRestricted);

  if (UseProtoFetcher()) {
    SimulateKidsApiResponse(
        kids_chrome_management::ClassifyUrlResponse::ALLOWED);
  }
}

TEST_P(KidsManagementURLCheckerClientTest, HistogramsAreEmitted) {
  base::HistogramTester histogram_tester;

  if (!UseProtoFetcher()) {
    // TODO(b/276898959): Remove branch after migration.
    SetUpLegacyClientResponse(safe_search_api::ClientClassification::kAllowed,
                              KidsChromeManagementClient::ErrorCode::kSuccess);
  }

  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kAllowed));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kRestricted);

  if (UseProtoFetcher()) {
    SimulateKidsApiResponse(
        kids_chrome_management::ClassifyUrlResponse::ALLOWED);
  }

  // The actual latency of mocked fetch is variable, so only expect that some
  // value was recorded.
  if (UseProtoFetcher()) {
    // Non-proto test is mocking the whole client thus bypassing the http stack.
    histogram_tester.ExpectTotalCount(
        "FamilyLinkUser.ClassifyUrlRequest.Latency",
        /*expected_count(grew by)*/ 1);
  }
  histogram_tester.ExpectTotalCount("Enterprise.SafeSites.Latency",
                                    /*expected_count(grew by)*/ 1);
}

TEST_P(KidsManagementURLCheckerClientTest, UrlRestricted) {
  if (!UseProtoFetcher()) {
    // TODO(b/276898959): Remove branch after migration.
    SetUpLegacyClientResponse(
        safe_search_api::ClientClassification::kRestricted,
        KidsChromeManagementClient::ErrorCode::kSuccess);
  }

  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kRestricted));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);

  if (UseProtoFetcher()) {
    SimulateKidsApiResponse(
        kids_chrome_management::ClassifyUrlResponse::RESTRICTED);
  }
}

TEST_P(KidsManagementURLCheckerClientTest, AccessTokenError) {
  if (!UseProtoFetcher()) {
    SetUpLegacyClientResponse(
        safe_search_api::ClientClassification::kUnknown,
        KidsChromeManagementClient::ErrorCode::kTokenError);
  }

  // UseProtoFetcher() == true requires no access token at all to prove this
  // test, since the token's fetch mode is kImmediate, so url check will fail
  // for that reason when the token is not available.

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);
}

TEST_P(KidsManagementURLCheckerClientTest, NetworkError) {
  if (!UseProtoFetcher()) {
    SetUpLegacyClientResponse(
        safe_search_api::ClientClassification::kUnknown,
        KidsChromeManagementClient::ErrorCode::kNetworkError);
  }

  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);

  if (UseProtoFetcher()) {
    SimulateNetworkError(net::ERR_UNEXPECTED);
  }
}

TEST_P(KidsManagementURLCheckerClientTest, HttpError) {
  if (!UseProtoFetcher()) {
    SetUpLegacyClientResponse(
        safe_search_api::ClientClassification::kUnknown,
        KidsChromeManagementClient::ErrorCode::kNetworkError);
  }

  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);

  if (UseProtoFetcher()) {
    SimulateHttpError(net::HTTP_BAD_GATEWAY);
  }
}

TEST_P(KidsManagementURLCheckerClientTest, ServiceError) {
  if (!UseProtoFetcher()) {
    SetUpLegacyClientResponse(
        safe_search_api::ClientClassification::kUnknown,
        KidsChromeManagementClient::ErrorCode::kNetworkError);
  }

  MakePrimaryAccountAvailable();

  EXPECT_CALL(*this,
              OnCheckDone(GURL("http://example.com"),
                          safe_search_api::ClientClassification::kUnknown));
  CheckUrl("http://example.com");

  // Simulate opposite response from shadow call to prove that it is
  // ineffective.
  SimulateSafeSitesResponse(safe_search_api::ClientClassification::kAllowed);

  if (UseProtoFetcher()) {
    SimulateMalformedResponse();
  }
}

TEST_P(KidsManagementURLCheckerClientTest,
       PendingRequestsAreCanceledWhenClientIsDestroyed) {
  EXPECT_CALL(*this, OnCheckDone(_, _)).Times(0);

  CheckUrlWithoutResponse("http://example.com");
  DestroyUrlClassifier();

  // Now run the callback.
  task_environment_.RunUntilIdle();
}

// Instead of /0, /1... print human-readable description of the test.
std::string PrettyPrintTestCaseName(
    const ::testing::TestParamInfo<UseProtoFetcher>& info) {
  if (info.param) {
    return "ProtoFetcher";
  } else {
    return "JsonFetcher";
  }
}

// TODO(b/276898959): Remove ::testing::Bool() == false once migrated.
INSTANTIATE_TEST_SUITE_P(All,
                         KidsManagementURLCheckerClientTest,
                         ::testing::Bool(),
                         &PrettyPrintTestCaseName);

}  // namespace
