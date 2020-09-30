// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_interface.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_fetcher_factory_impl.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

const char kExampleAndroidFacetURI[] = "android://hash@com.example";
const char kExampleAndroidPlayName[] = "Example Android App";
const char kExampleAndroidIconURL[] = "https://example.com/icon.png";
const char kExampleWebFacet1URI[] = "https://www.example.com";
const char kExampleWebFacet2URI[] = "https://www.example.org";
const char kExampleWebFacet1ChangePasswordURI[] =
    "https://www.example.com/.well-known/change-password";
const char kExampleWebFacet2ChangePasswordURI[] =
    "https://www.example.org/settings/passwords";

}  // namespace

class AffiliationFetcherTest : public testing::Test {
 public:
  AffiliationFetcherTest() {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          intercepted_body_ = network::GetUploadData(request);
          intercepted_headers_ = request.headers;
        }));
  }

 protected:
  void VerifyRequestPayload(const std::vector<FacetURI>& expected_facet_uris,
                            AffiliationFetcher::RequestInfo request_info) {
    affiliation_pb::LookupAffiliationRequest request;
    ASSERT_TRUE(request.ParseFromString(intercepted_body_));

    std::vector<FacetURI> actual_uris;
    for (int i = 0; i < request.facet_size(); ++i)
      actual_uris.push_back(FacetURI::FromCanonicalSpec(request.facet(i)));

    std::string content_type;
    intercepted_headers_.GetHeader(net::HttpRequestHeaders::kContentType,
                                   &content_type);
    EXPECT_EQ("application/x-protobuf", content_type);
    EXPECT_THAT(actual_uris,
                testing::UnorderedElementsAreArray(expected_facet_uris));

    EXPECT_EQ(request.mask().branding_info(), request_info.branding_info);
    // Change password info requires grouping info enabled.
    EXPECT_EQ(request.mask().grouping_info(),
              request_info.change_password_info);
    EXPECT_EQ(request.mask().change_password_info(),
              request_info.change_password_info);
  }

  GURL interception_url() { return AffiliationFetcher::BuildQueryURL(); }

  void SetupSuccessfulResponse(const std::string& response) {
    test_url_loader_factory_.AddResponse(interception_url().spec(), response);
  }

  void SetupServerErrorResponse() {
    test_url_loader_factory_.AddResponse(interception_url().spec(), "",
                                         net::HTTP_INTERNAL_SERVER_ERROR);
  }

  void SetupNetworkErrorResponse() {
    auto head = network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR);
    head->mime_type = "application/protobuf";
    network::URLLoaderCompletionStatus status(net::ERR_NETWORK_CHANGED);
    test_url_loader_factory_.AddResponse(interception_url(), std::move(head),
                                         "", status);
  }

  void WaitForResponse() { task_environment_.RunUntilIdle(); }

  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory() {
    return test_shared_loader_factory_;
  }

  AffiliationFetcherFactoryImpl* fetcher_factory() { return &fetcher_factory_; }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::string intercepted_body_;
  net::HttpRequestHeaders intercepted_headers_;
  AffiliationFetcherFactoryImpl fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationFetcherTest);
};

TEST_F(AffiliationFetcherTest, BasicReqestAndResponse) {
  const char kNotExampleAndroidFacetURI[] =
      "android://hash1234@com.example.not";
  const char kNotExampleWebFacetURI[] = "https://not.example.com";
  AffiliationFetcher::RequestInfo request_info{.branding_info = true};

  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kNotExampleWebFacetURI);
  eq_class2->add_facet()->set_id(kNotExampleAndroidFacetURI);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(requested_uris, request_info));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(2u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
  EXPECT_THAT(
      result->affiliations[1],
      testing::UnorderedElementsAre(
          Facet{FacetURI::FromCanonicalSpec(kNotExampleWebFacetURI)},
          Facet{FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI)}));
}

TEST_F(AffiliationFetcherTest, AndroidBrandingInfoIsReturnedIfPresent) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class = test_response.add_affiliation();
  eq_class->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class->add_facet()->set_id(kExampleWebFacet2URI);
  auto android_branding_info = std::make_unique<affiliation_pb::BrandingInfo>();
  android_branding_info->set_name(kExampleAndroidPlayName);
  android_branding_info->set_icon_url(kExampleAndroidIconURL);
  affiliation_pb::Facet* android_facet = eq_class->add_facet();
  android_facet->set_id(kExampleAndroidFacetURI);
  android_facet->set_allocated_branding_info(android_branding_info.release());
  AffiliationFetcher::RequestInfo request_info{.branding_info = true};

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(requested_uris, request_info));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI),
                        FacetBrandingInfo{kExampleAndroidPlayName,
                                          GURL(kExampleAndroidIconURL)}}));
}

TEST_F(AffiliationFetcherTest, ChangePasswordInfoIsReturnedIfPresent) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::FacetGroup* eq_class = test_response.add_group();

  // kExampleWebFacet1URI, kExampleWebFacet1ChangePasswordURI
  affiliation_pb::Facet* web_facet_1 = eq_class->add_facet();
  web_facet_1->set_id(kExampleWebFacet1URI);
  web_facet_1->mutable_change_password_info()->set_change_password_url(
      kExampleWebFacet1ChangePasswordURI);

  // kExampleWebFacet2URI, kExampleWebFacet2ChangePasswordURI
  affiliation_pb::Facet* web_facet_2 = eq_class->add_facet();
  web_facet_2->set_id(kExampleWebFacet2URI);
  web_facet_2->mutable_change_password_info()->set_change_password_url(
      kExampleWebFacet2ChangePasswordURI);

  AffiliationFetcher::RequestInfo request_info{.change_password_info = true};

  std::vector<FacetURI> requested_uris = {
      FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)};

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(requested_uris, request_info));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->groupings.size());
  EXPECT_THAT(
      result->groupings[0],
      testing::UnorderedElementsAre(
          Facet{
              .uri = FacetURI::FromCanonicalSpec(kExampleWebFacet1URI),
              .change_password_url = GURL(kExampleWebFacet1ChangePasswordURI)},
          Facet{.uri = FacetURI::FromCanonicalSpec(kExampleWebFacet2URI),
                .change_password_url =
                    GURL(kExampleWebFacet2ChangePasswordURI)}));
}

// The API contract of this class is to return an equivalence class for all
// requested facets; however, the server will not return anything for facets
// that are not affiliated with any other facet. Make sure an equivalence class
// of size one is created for each of the missing facets.
TEST_F(AffiliationFetcherTest, MissingEquivalenceClassesAreCreated) {
  affiliation_pb::LookupAffiliationResponse empty_test_response;
  AffiliationFetcher::RequestInfo request_info{.branding_info = true};

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(empty_test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(requested_uris, request_info));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)}));
}

TEST_F(AffiliationFetcherTest, DuplicateEquivalenceClassesAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class2->add_facet()->set_id(kExampleAndroidFacetURI);
  eq_class2->add_facet()->set_id(kExampleWebFacet1URI);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
}

TEST_F(AffiliationFetcherTest, EmptyEquivalenceClassesAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  // Empty class.
  test_response.add_affiliation();

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)}));
}

TEST_F(AffiliationFetcherTest, UnrecognizedFacetURIsAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  // Equivalence class having, alongside known facet URIs, a facet URI that
  // corresponds to new platform unknown to this version.
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
  eq_class1->add_facet()->set_id("new-platform://app-id-on-new-platform");
  // Equivalence class consisting solely of an unknown facet URI.
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id("new-platform2://app2-id-on-new-platform2");

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
}

TEST_F(AffiliationFetcherTest, FailureBecauseResponseIsNotAProtobuf) {
  const char kMalformedResponse[] = "This is not a protocol buffer!";

  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(kMalformedResponse);
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  EXPECT_CALL(mock_delegate, OnMalformedResponse(fetcher.get()));
  fetcher->StartRequest(uris, {});
  WaitForResponse();
}

// Partially overlapping equivalence classes violate the invariant that
// affiliations must form an equivalence relation. Such a response is malformed.
TEST_F(AffiliationFetcherTest,
       FailureBecausePartiallyOverlappingEquivalenceClasses) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kExampleWebFacet1URI);
  eq_class2->add_facet()->set_id(kExampleAndroidFacetURI);

  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  EXPECT_CALL(mock_delegate, OnMalformedResponse(fetcher.get()));
  fetcher->StartRequest(uris, {});
  WaitForResponse();
}

TEST_F(AffiliationFetcherTest, FailOnServerError) {
  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupServerErrorResponse();
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  EXPECT_CALL(mock_delegate, OnFetchFailed(fetcher.get()));
  fetcher->StartRequest(uris, {});
  WaitForResponse();
}

TEST_F(AffiliationFetcherTest, FailOnNetworkError) {
  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupNetworkErrorResponse();
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  EXPECT_CALL(mock_delegate, OnFetchFailed(fetcher.get()));
  fetcher->StartRequest(uris, {});
  WaitForResponse();
}

TEST_F(AffiliationFetcherTest, FetchTimeMetric) {
  base::HistogramTester histogram_tester;
  std::vector<FacetURI> requested_uris = {
      FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)};

  SetupSuccessfulResponse(
      affiliation_pb::LookupAffiliationResponse().SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  auto fetcher = fetcher_factory()->CreateInstance(test_shared_loader_factory(),
                                                   &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(fetcher.get(), testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher->StartRequest(requested_uris, {});
  WaitForResponse();

  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.FetchTime", 1);
}

}  // namespace password_manager
