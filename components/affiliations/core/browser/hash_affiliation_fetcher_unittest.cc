// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/hash_affiliation_fetcher.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions_win.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/affiliation_service_impl.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/mock_affiliation_fetcher_delegate.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace affiliations {

namespace {

constexpr char kExampleAndroidFacetURI[] = "android://hash@com.example";
constexpr char kExampleAndroidPlayName[] = "Example Android App";
constexpr char kExampleAndroidIconURL[] = "https://example.com/icon.png";
constexpr char kExampleWebFacet1URI[] = "https://www.example.com";
constexpr char kExampleWebFacet2URI[] = "https://www.example.org";
constexpr char kExampleWebFacet1ChangePasswordURI[] =
    "https://www.example.com/.well-known/change-password";
constexpr char kExampleWebFacet2ChangePasswordURI[] =
    "https://www.example.org/settings/passwords";

constexpr char k1ExampleURL[] = "https://1.example.com";
constexpr uint64_t k1ExampleHash16LenPrefix = 10506334980701945856ULL;
constexpr char k2ExampleURL[] = "https://2.example.com";
constexpr uint64_t k2ExampleHash16LenPrefix = 9324421553493901312ULL;

uint64_t ComputeHashPrefix(const FacetURI& uri) {
  uint8_t hash[2];
  crypto::SHA256HashString(uri.canonical_spec(), hash, 2);
  uint64_t result = ((uint64_t)hash[0] << (7 * 8));
  result |= ((uint64_t)hash[1] << (6 * 8));

  return result;
}

std::vector<uint64_t> ComputeHashes(const std::vector<FacetURI>& facet_uris) {
  std::vector<uint64_t> hashes;
  for (const FacetURI& uri : facet_uris)
    hashes.push_back(ComputeHashPrefix(uri));
  return hashes;
}

}  // namespace

class HashAffiliationFetcherTest : public testing::Test {
 public:
  HashAffiliationFetcherTest() {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          intercepted_body_ = network::GetUploadData(request);
          intercepted_headers_ = request.headers;
        }));
  }
  ~HashAffiliationFetcherTest() override = default;

 protected:
  void VerifyRequestPayload(const std::vector<uint64_t>& expected_hash_prefixes,
                            HashAffiliationFetcher::RequestInfo request_info);
  void WaitForResponse() { task_environment_.RunUntilIdle(); }

  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory() {
    return test_shared_loader_factory_;
  }

  GURL interception_url() { return HashAffiliationFetcher::BuildQueryURL(); }

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

 private:
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::string intercepted_body_;
  net::HttpRequestHeaders intercepted_headers_;
};

void HashAffiliationFetcherTest::VerifyRequestPayload(
    const std::vector<uint64_t>& expected_hash_prefixes,
    HashAffiliationFetcher::RequestInfo request_info) {
  affiliation_pb::LookupAffiliationByHashPrefixRequest request;
  ASSERT_TRUE(request.ParseFromString(intercepted_body_));

  std::vector<uint64_t> actual_hash_prefixes;
  for (const auto prefix : request.hash_prefixes())
    actual_hash_prefixes.push_back(prefix);

  EXPECT_EQ(
      "application/x-protobuf",
      intercepted_headers_.GetHeader(net::HttpRequestHeaders::kContentType)
          .value_or(std::string()));
  EXPECT_THAT(actual_hash_prefixes,
              testing::UnorderedElementsAreArray(expected_hash_prefixes));
  EXPECT_EQ(request.mask().change_password_info(),
            request_info.change_password_info);
}

TEST_F(HashAffiliationFetcherTest, BuildQueryURL) {
  MockAffiliationFetcherDelegate mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);

  GURL query_url = fetcher.BuildQueryURL();

  EXPECT_EQ("https", query_url.scheme());
  EXPECT_EQ("www.googleapis.com", query_url.host());
  EXPECT_EQ("/affiliation/v1/affiliation:lookupByHashPrefix", query_url.path());
}

TEST_F(HashAffiliationFetcherTest, GetRequestedFacetURIs) {
  MockAffiliationFetcherDelegate mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k1ExampleURL));
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k2ExampleURL));

  fetcher.StartRequest(requested_uris, kChangePasswordUrlRequestInfo);
  WaitForResponse();

  EXPECT_THAT(fetcher.GetRequestedFacetURIs(),
              testing::UnorderedElementsAreArray(requested_uris));
}

TEST_F(HashAffiliationFetcherTest,
       VerifyPayloadForMultipleHashesRequestWith16LengthPrefix) {
  MockAffiliationFetcherDelegate mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k1ExampleURL));
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k2ExampleURL));

  fetcher.StartRequest(requested_uris, kChangePasswordUrlRequestInfo);
  WaitForResponse();

  std::vector<uint64_t> hash_prefixes;
  hash_prefixes.push_back(k1ExampleHash16LenPrefix);
  hash_prefixes.push_back(k2ExampleHash16LenPrefix);

  ASSERT_NO_FATAL_FAILURE(
      VerifyRequestPayload(hash_prefixes, kChangePasswordUrlRequestInfo));
}

TEST_F(HashAffiliationFetcherTest, BasicRequestAndResponse) {
  const char kNotExampleAndroidFacetURI[] =
      "android://hash1234@com.example.not";
  const char kNotExampleWebFacetURI[] = "https://not.example.com";
  AffiliationFetcherInterface::RequestInfo request_info{.branding_info = true};

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
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);

  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(
      VerifyRequestPayload(ComputeHashes(requested_uris), request_info));
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

TEST_F(HashAffiliationFetcherTest, AndroidBrandingInfoIsReturnedIfPresent) {
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
  AffiliationFetcherInterface::RequestInfo request_info{.branding_info = true};

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(
      VerifyRequestPayload(ComputeHashes(requested_uris), request_info));
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

TEST_F(HashAffiliationFetcherTest, ChangePasswordInfoIsReturnedIfPresent) {
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

  std::vector<FacetURI> requested_uris = {
      FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)};

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, kChangePasswordUrlRequestInfo);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(ComputeHashes(requested_uris),
                                               kChangePasswordUrlRequestInfo));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->groupings.size());
  EXPECT_THAT(
      result->groupings[0].facets,
      testing::UnorderedElementsAre(
          Facet(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI),
                FacetBrandingInfo(), GURL(kExampleWebFacet1ChangePasswordURI)),
          Facet(FacetURI::FromCanonicalSpec(kExampleWebFacet2URI),
                FacetBrandingInfo(),
                GURL(kExampleWebFacet2ChangePasswordURI))));
}

// The API contract of this class is to return an equivalence class for all
// requested facets; however, the server will not return anything for facets
// that are not affiliated with any other facet. Make sure an equivalence class
// of size one is created for each of the missing facets.
TEST_F(HashAffiliationFetcherTest, MissingEquivalenceClassesAreCreated) {
  affiliation_pb::LookupAffiliationResponse empty_test_response;
  AffiliationFetcherInterface::RequestInfo request_info{.branding_info = true};

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(empty_test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(
      VerifyRequestPayload(ComputeHashes(requested_uris), request_info));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)}));
}

TEST_F(HashAffiliationFetcherTest, DuplicateEquivalenceClassesAreIgnored) {
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
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
}

TEST_F(HashAffiliationFetcherTest, NonRequestedEquivalenceClassesAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  // Equivalence class that was not requested and was added to affiliation
  // response because of some error (for example hash collision.)
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliation();
  eq_class2->add_facet()->set_id(kExampleWebFacet2URI);
  eq_class2->add_facet()->set_id(kExampleAndroidFacetURI);

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet2URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
}

TEST_F(HashAffiliationFetcherTest, EmptyEquivalenceClassesAreIgnored) {
  affiliation_pb::LookupAffiliationResponse test_response;
  affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliation();
  eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
  // Empty class.
  test_response.add_affiliation();

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)}));
}

TEST_F(HashAffiliationFetcherTest, UnrecognizedFacetURIsAreIgnored) {
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
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, {});
  WaitForResponse();

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(1u, result->affiliations.size());
  EXPECT_THAT(result->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
}

TEST_F(HashAffiliationFetcherTest, FailureBecauseResponseIsNotAProtobuf) {
  const char kMalformedResponse[] = "This is not a protocol buffer!";

  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(kMalformedResponse);
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  EXPECT_CALL(mock_delegate, OnMalformedResponse(&fetcher));
  fetcher.StartRequest(uris, {});
  WaitForResponse();
}

// Partially overlapping equivalence classes violate the invariant that
// affiliations must form an equivalence relation. Such a response is malformed.
TEST_F(HashAffiliationFetcherTest,
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
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  EXPECT_CALL(mock_delegate, OnMalformedResponse(&fetcher));
  fetcher.StartRequest(uris, {});
  WaitForResponse();
}

TEST_F(HashAffiliationFetcherTest, FailOnServerError) {
  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupServerErrorResponse();
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  EXPECT_CALL(mock_delegate, OnFetchFailed(&fetcher));
  fetcher.StartRequest(uris, {});
  WaitForResponse();
}

TEST_F(HashAffiliationFetcherTest, FailOnNetworkError) {
  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupNetworkErrorResponse();
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  EXPECT_CALL(mock_delegate, OnFetchFailed(&fetcher));
  fetcher.StartRequest(uris, {});
  WaitForResponse();
}

TEST_F(HashAffiliationFetcherTest, MetricsWhenSuccess) {
  base::HistogramTester histogram_tester;
  std::vector<FacetURI> requested_uris = {
      FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)};

  SetupSuccessfulResponse(
      affiliation_pb::LookupAffiliationResponse().SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, {});
  WaitForResponse();

  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.FetchTime.Success", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.ResponseSize.Success", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.FetchTime.Malformed", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.ResponseSize.Malformed", 0);
}

TEST_F(HashAffiliationFetcherTest, MetricsWhenFailed) {
  base::HistogramTester histogram_tester;
  const char kMalformedResponse[] = "This is not a protocol buffer!";

  std::vector<FacetURI> uris;
  uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));

  SetupSuccessfulResponse(kMalformedResponse);
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnMalformedResponse(&fetcher));
  fetcher.StartRequest(uris, {});
  WaitForResponse();

  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.FetchTime.Success", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.ResponseSize.Success", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.FetchTime.Malformed", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.AffiliationFetcher.ResponseSize.Malformed", 1);
}

TEST_F(HashAffiliationFetcherTest, GroupBrandingInfoIsReturnedIfPresent) {
  affiliation_pb::LookupAffiliationResponse test_response;

  affiliation_pb::FacetGroup* eq_class_1 = test_response.add_group();
  affiliation_pb::Facet* facet_1 = eq_class_1->add_facet();
  facet_1->set_id(kExampleAndroidFacetURI);
  auto group_branding_info =
      std::make_unique<affiliation_pb::GroupBrandingInfo>();
  group_branding_info->set_name(kExampleAndroidPlayName);
  group_branding_info->set_icon_url(kExampleAndroidIconURL);
  eq_class_1->set_allocated_group_branding_info(group_branding_info.release());

  affiliation_pb::FacetGroup* eq_class_2 = test_response.add_group();
  affiliation_pb::Facet* facet_2 = eq_class_2->add_facet();
  facet_2->set_id(kExampleWebFacet1URI);

  AffiliationFetcherInterface::RequestInfo request_info{.branding_info = true};

  std::vector<FacetURI> requested_uris = {
      FacetURI::FromCanonicalSpec(kExampleWebFacet1URI),
      FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)};

  SetupSuccessfulResponse(test_response.SerializeAsString());
  testing::StrictMock<MockAffiliationFetcherDelegate> mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  std::unique_ptr<AffiliationFetcherDelegate::Result> result;
  EXPECT_CALL(mock_delegate, OnFetchSucceeded(&fetcher, testing::_))
      .WillOnce(MoveArg<1>(&result));
  fetcher.StartRequest(requested_uris, request_info);
  WaitForResponse();

  ASSERT_NO_FATAL_FAILURE(
      VerifyRequestPayload(ComputeHashes(requested_uris), request_info));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_delegate));

  ASSERT_EQ(2u, result->groupings.size());
  EXPECT_THAT(result->groupings[0].branding_info,
              testing::Eq(FacetBrandingInfo{kExampleAndroidPlayName,
                                            GURL(kExampleAndroidIconURL)}));
  EXPECT_THAT(result->groupings[1].branding_info,
              testing::Eq(FacetBrandingInfo()));
}

}  // namespace affiliations
