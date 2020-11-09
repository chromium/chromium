// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/hash_affiliation_fetcher.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions_win.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_api.pb.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher_delegate.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

constexpr char k1ExampleURL[] = "https://1.example.com";
constexpr uint64_t k1ExampleHash16LenPrefix = 10506334980701945856ULL;
constexpr char k2ExampleURL[] = "https://2.example.com";
constexpr uint64_t k2ExampleHash16LenPrefix = 9324421553493901312ULL;

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

 private:
  base::test::TaskEnvironment task_environment_;
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

  std::string content_type;
  intercepted_headers_.GetHeader(net::HttpRequestHeaders::kContentType,
                                 &content_type);
  EXPECT_EQ("application/x-protobuf", content_type);
  EXPECT_THAT(actual_hash_prefixes,
              testing::UnorderedElementsAreArray(expected_hash_prefixes));

  // Change password info requires grouping info enabled.
  EXPECT_EQ(request.mask().grouping_info(), request_info.change_password_info);
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
  HashAffiliationFetcher::RequestInfo request_info{.change_password_info =
                                                       true};

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k1ExampleURL));
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k2ExampleURL));

  fetcher.StartRequest(requested_uris, request_info);
  WaitForResponse();

  EXPECT_THAT(fetcher.GetRequestedFacetURIs(),
              testing::UnorderedElementsAreArray(requested_uris));
}

TEST_F(HashAffiliationFetcherTest,
       VerifyPayloadForMultipleHashesRequestWith16LengthPrefix) {
  MockAffiliationFetcherDelegate mock_delegate;
  HashAffiliationFetcher fetcher(test_shared_loader_factory(), &mock_delegate);
  HashAffiliationFetcher::RequestInfo request_info{.change_password_info =
                                                       true};

  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k1ExampleURL));
  requested_uris.push_back(FacetURI::FromCanonicalSpec(k2ExampleURL));

  fetcher.StartRequest(requested_uris, request_info);
  WaitForResponse();

  std::vector<uint64_t> hash_prefixes;
  hash_prefixes.push_back(k1ExampleHash16LenPrefix);
  hash_prefixes.push_back(k2ExampleHash16LenPrefix);

  ASSERT_NO_FATAL_FAILURE(VerifyRequestPayload(hash_prefixes, request_info));
}

}  // namespace password_manager
