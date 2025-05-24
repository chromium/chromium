// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_manager.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/affiliations/core/browser/affiliation_api.pb.h"
#include "components/affiliations/core/browser/affiliation_fetcher_factory_impl.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/fake_affiliation_api.h"
#include "components/affiliations/core/browser/hash_affiliation_fetcher.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {

namespace {
constexpr char kNotExampleAndroidFacetURI[] =
    "android://hash1234@com.example.not";
constexpr char kNotExampleWebFacetURI[] = "https://not.example.com";
constexpr char kExampleWebFacet1URI[] = "https://www.example.com";
constexpr char kExampleWebFacet2URI[] = "https://www.example.org";
constexpr char kExampleAndroidFacetURI[] = "android://hash@com.example";
constexpr AffiliationFetcherInterface::RequestInfo kRequestInfo{
    .branding_info = true,
    .change_password_info = true};

// Fetcher factory that will create |HashAffiliationFetcher| but avoid checking
// API keys since they aren't always available in unit tests. Instead, to test
// failed fetcher creation, this class allows setting a bool that will disallow
// fetcher creation.
class KeylessFetcherFactory : public AffiliationFetcherFactory {
 public:
  KeylessFetcherFactory();
  ~KeylessFetcherFactory() override;

  std::unique_ptr<AffiliationFetcherInterface> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  void SetCanCreateFetcher(bool can_create_fetcher) {
    can_create_fetcher_ = can_create_fetcher;
  }

  bool CanCreateFetcher() const override { return can_create_fetcher_; }

 private:
  bool can_create_fetcher_ = true;
};

KeylessFetcherFactory::KeylessFetcherFactory() = default;
KeylessFetcherFactory::~KeylessFetcherFactory() = default;

std::unique_ptr<AffiliationFetcherInterface>
KeylessFetcherFactory::CreateInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (can_create_fetcher_) {
    return std::make_unique<HashAffiliationFetcher>(
        std::move(url_loader_factory));
  }
  return nullptr;
}
}  // namespace

class AffiliationFetcherManagerTest : public testing::Test {
 public:
  AffiliationFetcherManagerTest() = default;

 protected:
  GURL interception_url() const {
    return HashAffiliationFetcher::BuildQueryURL();
  }

  // Setup a complex response from Affiliation Server to make sure it is passed
  // down to |AffiliationFetcherManager|.
  std::string GetSuccessfulAffiliationResponse() const {
    affiliation_pb::LookupAffiliationByHashPrefixResponse test_response;
    affiliation_pb::Affiliation* eq_class1 = test_response.add_affiliations();
    eq_class1->add_facet()->set_id(kExampleWebFacet1URI);
    eq_class1->add_facet()->set_id(kExampleWebFacet2URI);
    eq_class1->add_facet()->set_id(kExampleAndroidFacetURI);
    affiliation_pb::Affiliation* eq_class2 = test_response.add_affiliations();
    eq_class2->add_facet()->set_id(kNotExampleWebFacetURI);
    eq_class2->add_facet()->set_id(kNotExampleAndroidFacetURI);

    return test_response.SerializeAsString();
  }

  void SetupSuccessfulResponse(const std::string& response) {
    test_url_loader_factory_.ClearResponses();
    test_url_loader_factory_.AddResponse(interception_url().spec(), response,
                                         net::HTTP_OK);
  }

  void SetupServerErrorResponse() {
    test_url_loader_factory_.ClearResponses();
    test_url_loader_factory_.AddResponse(interception_url().spec(), "",
                                         net::HTTP_INTERNAL_SERVER_ERROR);
  }

  int GetNumPendingRequests() { return test_url_loader_factory_.NumPending(); }

  AffiliationFetcherManager* manager() { return manager_.get(); }

  void DisallowFetcherCreation() {
    keyless_fetcher_factory_->SetCanCreateFetcher(false);
  }

  void DestroyManager() {
    keyless_fetcher_factory_ = nullptr;
    manager_.reset();
  }

 private:
  // testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<AffiliationFetcherManager>(
        test_shared_loader_factory_);
    auto fetcher_factory = std::make_unique<KeylessFetcherFactory>();
    keyless_fetcher_factory_ = fetcher_factory.get();

    manager_->SetFetcherFactoryForTesting(std::move(fetcher_factory));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  std::unique_ptr<AffiliationFetcherManager> manager_;
  // Owned by |manager_|.
  raw_ptr<KeylessFetcherFactory> keyless_fetcher_factory_ = nullptr;
};

TEST_F(AffiliationFetcherManagerTest, OneFetchCallCreatesOneFetcher) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));

  manager()->Fetch(requested_uris, kRequestInfo, base::DoNothing());

  EXPECT_EQ(1u, manager()->GetFetchersForTesting()->size());
  EXPECT_EQ(1, GetNumPendingRequests());
}

TEST_F(AffiliationFetcherManagerTest,
       MultipleFetchCallsCreateMultipleFetchers) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));

  manager()->Fetch(requested_uris, kRequestInfo, base::DoNothing());
  manager()->Fetch(requested_uris, kRequestInfo, base::DoNothing());
  manager()->Fetch(requested_uris, kRequestInfo, base::DoNothing());

  EXPECT_EQ(3u, manager()->GetFetchersForTesting()->size());
  EXPECT_EQ(3, GetNumPendingRequests());
}

TEST_F(AffiliationFetcherManagerTest,
       ImmediatelyInvokeCallbackIfFetcherCreationFailed) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback;
  DisallowFetcherCreation();

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback.GetCallback());

  EXPECT_FALSE(manager()->IsFetchPossible());
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(completion_callback.IsReady());
}

TEST_F(AffiliationFetcherManagerTest,
       CompletionClosureInvokedOnFetchCompletion) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback;

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback.GetCallback());
  SetupSuccessfulResponse(GetSuccessfulAffiliationResponse());

  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(completion_callback.Take().data);
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
}

TEST_F(AffiliationFetcherManagerTest,
       CompletionClosureInvokedOnMultipleFetchCompletions) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_1;
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_2;
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_3;

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_1.GetCallback());
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_2.GetCallback());
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_3.GetCallback());
  // All requests have the same URL, so this will serve all of them
  SetupSuccessfulResponse(GetSuccessfulAffiliationResponse());

  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(completion_callback_1.Take().data);
  EXPECT_TRUE(completion_callback_2.Take().data);
  EXPECT_TRUE(completion_callback_3.Take().data);
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
}

// This test mimics |HashAffiliationFetcherTest::BasicRequestAndResponse| to
// makes sure that the response from |HashAffiliationFetcher| is passed down to
// |AffiliationFetcherManger|.
TEST_F(AffiliationFetcherManagerTest, DelegateInvokedOnFetchSuccess) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback;
  SetupSuccessfulResponse(GetSuccessfulAffiliationResponse());

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback.GetCallback());

  auto affiliation_response = completion_callback.Take().data;
  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(affiliation_response);
  ASSERT_EQ(2u, affiliation_response->affiliations.size());
  EXPECT_THAT(affiliation_response->affiliations[0],
              testing::UnorderedElementsAre(
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet1URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleWebFacet2URI)},
                  Facet{FacetURI::FromCanonicalSpec(kExampleAndroidFacetURI)}));
  EXPECT_THAT(
      affiliation_response->affiliations[1],
      testing::UnorderedElementsAre(
          Facet{FacetURI::FromCanonicalSpec(kNotExampleWebFacetURI)},
          Facet{FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI)}));
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
}

TEST_F(AffiliationFetcherManagerTest, CallbackInvokedOnFetchFailed) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback;
  SetupServerErrorResponse();

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback.GetCallback());

  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(completion_callback.IsReady());
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
}

TEST_F(AffiliationFetcherManagerTest, CallbackInvokedOnMalformedResponse) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback;
  SetupSuccessfulResponse("gibberish");

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback.GetCallback());

  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_TRUE(completion_callback.IsReady());
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
}

// The structure of this test is awkward because |TestURLLoaderFactory| seeds
// responses per URL, not per ResourceRequest. Since all URLs that we request
// are the same, we can't seed different responses for different requests. In
// this test we seed and serve the responses one-by-one to work around this,
// which is not perfect.
// However the test case is still valuable to check that consecutive calls can
// produce different results.
TEST_F(AffiliationFetcherManagerTest, DelegateInvokedOnAllPossibleResponses) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_1;
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_2;
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_3;

  // Successful response
  SetupSuccessfulResponse(GetSuccessfulAffiliationResponse());
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_1.GetCallback());
  // Failing response
  SetupServerErrorResponse();
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_2.GetCallback());
  // Malformed response
  SetupSuccessfulResponse("gibberish");
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_3.GetCallback());

  EXPECT_EQ(0, GetNumPendingRequests());
  EXPECT_EQ(0u, manager()->GetFetchersForTesting()->size());
  // First response
  auto result_1 = completion_callback_1.Take().data;
  ASSERT_TRUE(result_1);
  EXPECT_EQ(2u, result_1->affiliations.size());
  // Second response
  auto result_2 = completion_callback_2.Take();
  EXPECT_FALSE(result_2.data);
  EXPECT_EQ(net::HTTP_INTERNAL_SERVER_ERROR, result_2.http_status_code);
  // Third response
  auto result_3 = completion_callback_3.Take();
  EXPECT_FALSE(result_3.data);
  EXPECT_EQ(net::HTTP_OK, result_3.http_status_code);
  EXPECT_EQ(0, result_3.network_status);
}

TEST_F(AffiliationFetcherManagerTest, CallbackIsCalledOnManagerDestruction) {
  std::vector<FacetURI> requested_uris;
  requested_uris.push_back(FacetURI::FromCanonicalSpec(kExampleWebFacet1URI));
  requested_uris.push_back(
      FacetURI::FromCanonicalSpec(kNotExampleAndroidFacetURI));
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_1;
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_2;
  base::test::TestFuture<HashAffiliationFetcher::FetchResult>
      completion_callback_3;

  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_1.GetCallback());
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_2.GetCallback());
  manager()->Fetch(requested_uris, kRequestInfo,
                   completion_callback_3.GetCallback());
  DestroyManager();

  EXPECT_TRUE(completion_callback_1.IsReady());
  EXPECT_TRUE(completion_callback_2.IsReady());
  EXPECT_TRUE(completion_callback_3.IsReady());
  EXPECT_EQ(0, GetNumPendingRequests());
}

}  // namespace affiliations
