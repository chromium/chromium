// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_backend.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/affiliations/core/browser/affiliation_database.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler_delegate.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/facet_manager.h"
#include "components/affiliations/core/browser/fake_affiliation_api.h"
#include "components/affiliations/core/browser/mock_affiliation_consumer.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {

namespace {

using StrategyOnCacheMiss = AffiliationBackend::StrategyOnCacheMiss;

// Mock fetch throttler that has some extra logic to accurately portray the real
// AffiliationFetchThrottler in how it ignores SignalNetworkRequestNeeded()
// requests when a request is already known to be needed or one is already in
// flight, and in how it goes back to the idle state afterwards.
class MockAffiliationFetchThrottler : public AffiliationFetchThrottler {
 public:
  explicit MockAffiliationFetchThrottler(
      AffiliationFetchThrottlerDelegate* delegate)
      : AffiliationFetchThrottler(
            delegate,
            nullptr,
            network::TestNetworkConnectionTracker::GetInstance(),
            nullptr) {
    EXPECT_CALL(*this, OnInformOfNetworkRequestComplete(testing::_)).Times(0);
  }

  MockAffiliationFetchThrottler(const MockAffiliationFetchThrottler&) = delete;
  MockAffiliationFetchThrottler& operator=(
      const MockAffiliationFetchThrottler&) = delete;

  ~MockAffiliationFetchThrottler() override {
    EXPECT_FALSE(signaled_network_request_needed_);
  }

  // Expects that InformOfNetworkRequestComplete() will be called to indicate
  // either success or failure, depending on |expected_success|.
  void ExpectInformOfNetworkRequestComplete(bool expected_success) {
    EXPECT_CALL(*this, OnInformOfNetworkRequestComplete(expected_success));
  }

  // Informs the |delegate_| that it can send the needed network request.
  // Returns true if the |delegate_| reported that it actually ended up issuing
  // a request.
  bool LetNetworkRequestBeSent() {
    EXPECT_TRUE(has_signaled_network_request_needed());
    if (!delegate_->OnCanSendNetworkRequest()) {
      reset_signaled_network_request_needed();
      return false;
    }
    return true;
  }

  // Whether or not the throttler is 'signaled', meaning that the real throttler
  // would eventually call OnCanSendNetworkRequest() on the |delegate_|.
  bool has_signaled_network_request_needed() const {
    return signaled_network_request_needed_;
  }

  // Forces the mock throttler back to 'non-signaled' state. Normally, this does
  // not need to be manually called, as this is done by the mock automatically.
  void reset_signaled_network_request_needed() {
    signaled_network_request_needed_ = false;
  }

  void SetInternetConnectivity(bool has_connection) {
    has_network_connectivity_ = has_connection;
  }

 private:
  MOCK_METHOD1(OnInformOfNetworkRequestComplete, void(bool));

  // AffiliationFetchThrottler:
  void SignalNetworkRequestNeeded() override {
    signaled_network_request_needed_ = true;
  }

  bool HasInternetConnection() const override {
    return has_network_connectivity_;
  }

  void InformOfNetworkRequestComplete(bool success) override {
    OnInformOfNetworkRequestComplete(success);
    reset_signaled_network_request_needed();
  }

  bool signaled_network_request_needed_ = false;
  bool has_network_connectivity_ = true;
};

const char kTestFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestFacetURIAlpha3[] = "https://three.alpha.example.com";
const char kTestFacetURIAlpha4[] = "android://hash@com.example.alpha.android";
const char kTestFacetNameAlpha4[] = "Facet Name Alpha";
const char kTestFacetIconURLAlpha4[] = "https://example.com/alpha.png";
const char kTestFacetURIBeta1[] = "https://one.beta.example.org";
const char kTestFacetURIBeta2[] = "https://two.beta.example.org";
const char kTestFacetURIGamma1[] = "https://gamma.example.de";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha4),
            FacetBrandingInfo{kTestFacetNameAlpha4,
                              GURL(kTestFacetIconURLAlpha4)}),
  };
}

AffiliatedFacets GetTestEquivalenceClassBeta() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta2)),
  };
}

AffiliatedFacets GetTestEquivalenceClassGamma() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1)),
  };
}

GroupedFacets GetTestGropingAlpha() {
  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)),
                  Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)),
                  Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3)),
                  Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha4)),
                  Facet(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1))};
  group.branding_info =
      FacetBrandingInfo{kTestFacetNameAlpha4, GURL(kTestFacetIconURLAlpha4)};
  return group;
}

GroupedFacets GetTestGropingBeta() {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta2)),
  };
  return group;
}

base::TimeDelta GetCacheHardExpiryPeriod() {
  return base::Hours(FacetManager::kCacheHardExpiryInHours);
}

base::TimeDelta GetCacheSoftExpiryPeriod() {
  return base::Hours(FacetManager::kCacheSoftExpiryInHours);
}

base::TimeDelta GetShortTestPeriod() {
  return base::Hours(1);
}

// Returns a smallest time difference that this test cares about.
base::TimeDelta Epsilon() {
  return base::Microseconds(1);
}

}  // namespace

class AffiliationBackendTest : public testing::Test {
 public:
  AffiliationBackendTest() = default;

  AffiliationBackendTest(const AffiliationBackendTest&) = delete;
  AffiliationBackendTest& operator=(const AffiliationBackendTest&) = delete;

 protected:
  void GetAffiliationsAndBranding(MockAffiliationConsumer* consumer,
                                  const FacetURI& facet_uri,
                                  StrategyOnCacheMiss cache_miss_strategy) {
    backend_->GetAffiliationsAndBranding(facet_uri, cache_miss_strategy,
                                         consumer->GetResultCallback(),
                                         consumer_task_runner());
  }

  void Prefetch(const FacetURI& facet_uri, base::Time keep_fresh_until) {
    backend_->Prefetch(facet_uri, keep_fresh_until);
  }

  void CancelPrefetch(const FacetURI& facet_uri, base::Time keep_fresh_until) {
    backend_->CancelPrefetch(facet_uri, keep_fresh_until);
  }

  void ExpectNeedForFetchAndLetItBeSent() {
    ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
    ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
    ASSERT_TRUE(mock_fetch_throttler()->LetNetworkRequestBeSent());
  }

  void ExpectAndCompleteFetch(
      const std::vector<FacetURI>& expected_requested_facet_uris) {
    ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
    EXPECT_THAT(
        fake_affiliation_api()->GetNextRequestedFacets(),
        testing::UnorderedElementsAreArray(expected_requested_facet_uris));
    mock_fetch_throttler()->ExpectInformOfNetworkRequestComplete(true);
    fake_affiliation_api()->ServeNextRequest();
    testing::Mock::VerifyAndClearExpectations(mock_fetch_throttler());
  }

  void ExpectAndCompleteFetch(const FacetURI& expected_requested_facet_uri) {
    std::vector<FacetURI> expected_facet_uris;
    expected_facet_uris.push_back(expected_requested_facet_uri);
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(expected_facet_uris));
  }

  void ExpectAndFailFetch(const FacetURI& expected_requested_facet_uri) {
    ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
    EXPECT_THAT(fake_affiliation_api()->GetNextRequestedFacets(),
                testing::UnorderedElementsAre(expected_requested_facet_uri));
    mock_fetch_throttler()->ExpectInformOfNetworkRequestComplete(false);
    fake_affiliation_api()->FailNextRequest();
    testing::Mock::VerifyAndClearExpectations(mock_fetch_throttler());
  }

  void ExpectNoFetchNeeded() {
    ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
    ASSERT_FALSE(mock_fetch_throttler()->has_signaled_network_request_needed());
  }

  void ExpectFailureWithoutFetch(MockAffiliationConsumer* consumer) {
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    consumer->ExpectFailure();
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(consumer);
  }

  void GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      const FacetURI& facet_uri,
      const AffiliatedFacets& expected_result) {
    GetAffiliationsAndBranding(mock_consumer(), facet_uri,
                               StrategyOnCacheMiss::FETCH_OVER_NETWORK);
    ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));
    mock_consumer()->ExpectSuccessWithResult(expected_result);
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  void GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
      const FacetURI& facet_uri,
      StrategyOnCacheMiss cache_miss_strategy,
      const AffiliatedFacets& expected_result) {
    GetAffiliationsAndBranding(mock_consumer(), facet_uri, cache_miss_strategy);
    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    mock_consumer()->ExpectSuccessWithResult(expected_result);
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  // TODO(engedy): Within this test fixture, the word "failure" refers to GTest
  // failures, simulated network failures (above), and also AffiliationService
  // failure callbacks. Make this less ambiguous.
  void GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
      const FacetURI& facet_uri) {
    GetAffiliationsAndBranding(mock_consumer(), facet_uri,
                               StrategyOnCacheMiss::FAIL);
    ASSERT_NO_FATAL_FAILURE(ExpectFailureWithoutFetch(mock_consumer()));
  }

  void PrefetchAndExpectFetch(const FacetURI& facet_uri,
                              base::Time keep_fresh_until) {
    Prefetch(facet_uri, keep_fresh_until);
    ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));
  }

  // Verifies that both on-demand and cached-only GetAffiliationsAndBranding()
  // requests for each facet in |affiliated_facets| are served from cache with
  // no fetches.
  void ExpectThatEquivalenceClassIsServedFromCache(
      const AffiliatedFacets& affiliated_facets) {
    for (const Facet& facet : affiliated_facets) {
      SCOPED_TRACE(facet.uri);
      ASSERT_NO_FATAL_FAILURE(
          GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
              facet.uri, StrategyOnCacheMiss::FAIL, affiliated_facets));
      ASSERT_NO_FATAL_FAILURE(
          GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
              facet.uri, StrategyOnCacheMiss::FAIL, affiliated_facets));
    }
  }

  void DestroyBackend() {
    mock_fetch_throttler_ = nullptr;
    // `backend_` owns the fetcher factory to which `fake_affiliation_api_`
    // keeps a raw pointer, so this raw pointer needs to be reset prior to
    // destroying `backend_`.
    fake_affiliation_api_.SetFetcherFactory(nullptr);
    backend_.reset();
  }

  void AdvanceTime(base::TimeDelta delta) {
    backend_task_runner_->FastForwardBy(delta);
  }

  // Returns the number of equivalence classes in the backend's database.
  size_t GetNumOfEquivalenceClassInDatabase() {
    const auto& database = backend()->GetAffiliationDatabaseForTesting();
    std::vector<AffiliatedFacetsWithUpdateTime> all_affiliations;
    database.GetAllAffiliationsAndBranding(&all_affiliations);
    return all_affiliations.size();
  }

  size_t backend_facet_manager_count() {
    return backend()->facet_manager_count_for_testing();
  }

  bool IsCachedDataFreshForFacetURI(const FacetURI& facet_uri) {
    return FacetManager(facet_uri, backend(),
                        backend_task_runner_->GetMockClock())
        .IsCachedDataFresh();
  }

  bool IsCachedDataNearStaleForFacetURI(const FacetURI& facet_uri) {
    return FacetManager(facet_uri, backend(),
                        backend_task_runner_->GetMockClock())
        .IsCachedDataNearStale();
  }

  AffiliationBackend* backend() { return backend_.get(); }

  const base::FilePath& db_path() const { return db_path_; }

  base::TestMockTimeTaskRunner* backend_task_runner() {
    return backend_task_runner_.get();
  }

  base::TestSimpleTaskRunner* consumer_task_runner() {
    return consumer_task_runner_.get();
  }

  FakeAffiliationAPI* fake_affiliation_api() { return &fake_affiliation_api_; }

  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  MockAffiliationFetchThrottler* mock_fetch_throttler() {
    return mock_fetch_throttler_;
  }

 private:
  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(CreateTemporaryFile(&db_path_));
    backend_ = std::make_unique<AffiliationBackend>(
        backend_task_runner_, backend_task_runner_->GetMockClock(),
        backend_task_runner_->GetMockTickClock());
    auto test_shared_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    backend_->Initialize(test_shared_loader_factory->Clone(),
                         network::TestNetworkConnectionTracker::GetInstance(),
                         db_path());
    auto mock_fetch_throttler =
        std::make_unique<MockAffiliationFetchThrottler>(backend_.get());
    mock_fetch_throttler_ = mock_fetch_throttler.get();
    backend_->SetThrottlerForTesting(std::move(mock_fetch_throttler));
    auto fake_fetcher_factory =
        std::make_unique<FakeAffiliationFetcherFactory>();
    fake_affiliation_api_.SetFetcherFactory(fake_fetcher_factory.get());
    backend_->SetFetcherFactoryForTesting(std::move(fake_fetcher_factory));

    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassBeta());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassGamma());
    fake_affiliation_api_.AddTestGrouping(GetTestGropingAlpha());
    fake_affiliation_api_.AddTestGrouping(GetTestGropingBeta());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> backend_task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scoped_refptr<base::TestSimpleTaskRunner> consumer_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  base::FilePath db_path_;
  std::unique_ptr<AffiliationBackend> backend_;
  FakeAffiliationAPI fake_affiliation_api_;
  MockAffiliationConsumer mock_consumer_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  // Owned by |backend_|.
  raw_ptr<MockAffiliationFetchThrottler> mock_fetch_throttler_ = nullptr;
};

TEST_F(AffiliationBackendTest, OnDemandRequestSucceedsWithFetch) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
  EXPECT_EQ(0u, backend_facet_manager_count());

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      GetTestEquivalenceClassBeta()));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

// This test also verifies that the FacetManager is immediately discarded.
TEST_F(AffiliationBackendTest, CachedOnlyRequestFailsDueToCacheMiss) {
  GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, PrefetchTriggersInitialFetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
}

// This test also verifies that the FacetManager is immediately discarded.
TEST_F(AffiliationBackendTest, ExpiredPrefetchTriggersNoInitialFetch) {
  // Prefetch intervals are open from the right, thus intervals ending Now() are
  // already expired.
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
           backend_task_runner()->Now());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
}

// One additional GetAffiliationsAndBranding() and one Prefetch() request come
// in, both for unrelated facets, shortly after an initial
// GetAffiliationsAndBranding() request.
//
// Suppose that the network request triggered by the first
// GetAffiliationsAndBranding() request has already been initiated when the
// other requests arrive. As there should be no simultaneous requests, the
// additional facets should be queried together in a second fetch after the
// first fetch completes.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests) {
  FacetURI facet_uri_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_uri_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  // Pretend the fetch is already away when the two other requests come in.
  MockAffiliationConsumer second_consumer;
  GetAffiliationsAndBranding(mock_consumer(), facet_uri_alpha,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  GetAffiliationsAndBranding(&second_consumer, facet_uri_beta,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  Prefetch(facet_uri_gamma, base::Time::Max());

  std::vector<FacetURI> second_fetch_uris;
  second_fetch_uris.push_back(facet_uri_beta);
  second_fetch_uris.push_back(facet_uri_gamma);
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri_alpha));
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(second_fetch_uris));

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassBeta());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // Now that the two GetAffiliation() requests have been completed, the first
  // two FacetManagers should be discarded. The third FacetManager corresponding
  // to the prefetched facet should be kept.
  EXPECT_GE(1u, backend_facet_manager_count());
}

// Now suppose that the first fetch is somewhat delayed (e.g., because network
// requests are throttled), so the other requests arrive before it is actually
// issued. In this case, all facet URIs should be queried together in one fetch.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests2) {
  FacetURI facet_uri_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_uri_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  MockAffiliationConsumer second_consumer;
  GetAffiliationsAndBranding(mock_consumer(), facet_uri_alpha,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  GetAffiliationsAndBranding(&second_consumer, facet_uri_beta,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  Prefetch(facet_uri_gamma, base::Time::Max());

  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(facet_uri_alpha);
  fetched_uris.push_back(facet_uri_beta);
  fetched_uris.push_back(facet_uri_gamma);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(fetched_uris));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassBeta());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // Now that the two GetAffiliation() requests have been completed, the first
  // two FacetManagers should be discarded. The third FacetManager corresponding
  // to the prefetched facet should be kept.
  EXPECT_GE(1u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, RetryIsMadeOnFailedFetch) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  GetAffiliationsAndBranding(mock_consumer(), facet_uri,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndFailFetch(facet_uri));
  EXPECT_EQ(1u, backend_facet_manager_count());

  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// The Prefetch() request expires before fetching corresponding affiliation
// information would be allowed. The fetch should be abandoned.
TEST_F(AffiliationBackendTest, FetchIsNoLongerNeededOnceAllowed) {
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
           backend_task_runner()->Now() + GetShortTestPeriod());
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  AdvanceTime(GetShortTestPeriod() + Epsilon());

  bool did_send_request = mock_fetch_throttler()->LetNetworkRequestBeSent();
  EXPECT_FALSE(did_send_request);
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForSameFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK, GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForAffiliatedFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesRequestsForPrefetchedFacets) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK, GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest,
       CacheServesRequestsForFacetsAffiliatedWithPrefetchedFacets) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

// A second GetAffiliationsAndBranding() request for the same facet and a third
// request for an affiliated facet comes in while the network fetch triggered by
// the first request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, all
// three requests should be served without further fetches (they have the data).
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentRequestsForAffiliatedFacets) {
  FacetURI facet_uri1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  MockAffiliationConsumer second_consumer;
  MockAffiliationConsumer third_consumer;
  GetAffiliationsAndBranding(mock_consumer(), facet_uri1,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  GetAffiliationsAndBranding(&second_consumer, facet_uri1,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  GetAffiliationsAndBranding(&third_consumer, facet_uri2,
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri1));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  third_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// A second Prefetch() request for the same facet and a third request for an
// affiliated facet comes in while the initial fetch triggered by the first
// request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, there
// should be no further initial fetches as the data needed is already there.
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentPrefetchesForAffiliatedFacets) {
  FacetURI facet_uri1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  Prefetch(facet_uri1, base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  Prefetch(facet_uri1, base::Time::Max());
  Prefetch(facet_uri2, base::Time::Max());

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri1));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, SimpleCacheExpiryWithoutPrefetches) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  EXPECT_TRUE(IsCachedDataFreshForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(Epsilon());

  // After the data becomes stale, the cached-only request should fail, but the
  // subsequent on-demand request should fetch the data again and succeed.
  EXPECT_FALSE(IsCachedDataFreshForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// A Prefetch() request for a finite period. It should trigger an initial fetch
// and exactly one refetch, as the Prefetch() request expires exactly when the
// cached data obtained with the refetch expires.
TEST_F(AffiliationBackendTest,
       PrefetchTriggersOneInitialFetchAndOneRefetchBeforeExpiring) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      backend_task_runner()->Now() + GetCacheHardExpiryPeriod() +
          GetCacheSoftExpiryPeriod()));

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_FALSE(IsCachedDataNearStaleForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(Epsilon());

  EXPECT_TRUE(IsCachedDataNearStaleForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  EXPECT_TRUE(IsCachedDataFreshForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  EXPECT_TRUE(IsCachedDataNearStaleForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(Epsilon());

  // The data should be allowed to expire and the FacetManager discarded.
  EXPECT_FALSE(IsCachedDataFreshForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());

  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
          FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
          FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)));

  // However, a subsequent on-demand request should be able to trigger a fetch.
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
}

// Affiliation data for prefetched facets should be automatically refetched once
// every 23 hours, and GetAffiliationsAndBranding() requests regarding
// affiliated facets should be continuously served from cache.
TEST_F(AffiliationBackendTest, PrefetchTriggersPeriodicRefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  for (int cycle = 0; cycle < 3; ++cycle) {
    SCOPED_TRACE(cycle);

    AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

    ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
    EXPECT_TRUE(IsCachedDataFreshForFacetURI(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    EXPECT_FALSE(IsCachedDataNearStaleForFacetURI(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
        GetTestEquivalenceClassAlpha()));

    AdvanceTime(Epsilon());

    EXPECT_TRUE(IsCachedDataNearStaleForFacetURI(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
        GetTestEquivalenceClassAlpha()));
  }
}

TEST_F(AffiliationBackendTest,
       PrefetchTriggersNoInitialFetchIfDataIsAlreadyFresh) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  EXPECT_FALSE(IsCachedDataNearStaleForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, CancelPrefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  // Cancel the prefetch the last microsecond before a refetch would take place.
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_TRUE(backend_task_runner()->HasPendingTask());

  AdvanceTime(GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod() +
              Epsilon());

  // The data should be allowed to expire.
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
  EXPECT_TRUE(IsCachedDataNearStaleForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
          FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
          FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)));
}

TEST_F(AffiliationBackendTest, CancelDuplicatePrefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  // Cancel the prefetch the last microsecond before a refetch would take place.
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());

  AdvanceTime(Epsilon());

  // However, there is a second Prefetch() request which should keep the data
  // fresh.
  EXPECT_EQ(1u, backend_facet_manager_count());
  EXPECT_TRUE(IsCachedDataNearStaleForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod());

  EXPECT_TRUE(IsCachedDataFreshForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

// Canceling a non-existing prefetch request for a non-prefetched facet.
TEST_F(AffiliationBackendTest, CancelingNonExistingPrefetchIsSilentlyIgnored) {
  CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                 backend_task_runner()->Now() + base::Hours(24));
  ASSERT_NO_FATAL_FAILURE(ExpectNoFetchNeeded());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
}

// Verify that TrimCacheForFacetURI() only removes the equivalence class for the
// given facet, and preserves others (even if they could be discarded).
TEST_F(AffiliationBackendTest,
       TrimCacheForFacetURIOnlyRemovesDataForTheGivenFacet) {
  FacetURI preserved_facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndBrandingAndExpectFetchAndThenResult(
      preserved_facet_uri, GetTestEquivalenceClassBeta()));
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());

  backend()->TrimCacheForFacetURI(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  EXPECT_EQ(1u, GetNumOfEquivalenceClassInDatabase());

  // Also verify that the last update time of the affiliation data is preserved,
  // i.e., that it expires when it would normally have expired.
  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());
  EXPECT_TRUE(IsCachedDataFreshForFacetURI(preserved_facet_uri));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassBeta()));
  AdvanceTime(Epsilon());
  EXPECT_FALSE(IsCachedDataFreshForFacetURI(preserved_facet_uri));
  ASSERT_NO_FATAL_FAILURE(
      GetAffiliationsAndBrandingAndExpectFailureWithoutFetch(
          preserved_facet_uri));
}

TEST_F(AffiliationBackendTest, NothingExplodesWhenShutDownDuringFetch) {
  GetAffiliationsAndBranding(mock_consumer(),
                             FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  mock_fetch_throttler()->reset_signaled_network_request_needed();
  DestroyBackend();
}

TEST_F(AffiliationBackendTest,
       FailureCallbacksAreCalledIfBackendIsDestroyedWithPendingRequest) {
  GetAffiliationsAndBranding(mock_consumer(),
                             FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK);
  // Currently, a GetAffiliationsAndBranding() request can only be blocked due
  // to fetch in flight -- so emulate this condition when destroying the
  // backend.
  ASSERT_TRUE(mock_fetch_throttler()->has_signaled_network_request_needed());
  mock_fetch_throttler()->reset_signaled_network_request_needed();
  DestroyBackend();
  mock_consumer()->ExpectFailure();
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationBackendTest, DeleteCache) {
  DestroyBackend();
  ASSERT_TRUE(base::PathExists(db_path()));
  AffiliationBackend::DeleteCache(db_path());
  ASSERT_FALSE(base::PathExists(db_path()));
}

TEST_F(AffiliationBackendTest, KeepPrefetchForFacets) {
  // Have {kTestFacetURIAlpha1, kTestFacetURIAlpha1, kTestFacetURIBeta1} as a
  // list of actively fetching facets.
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1), base::Time::Max()));
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());
  EXPECT_EQ(2u, backend_facet_manager_count());
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  backend()->KeepPrefetchForFacets(
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
       FacetURI::FromCanonicalSpec(kTestFacetURIGamma1)});
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1)));
  EXPECT_EQ(2u, backend_facet_manager_count());
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());

  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationBackendTest, GetGroupingWith) {
  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  backend()->KeepPrefetchForFacets(fetched_uris);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(fetched_uris));

  EXPECT_THAT(backend()->GetGroupingInfo(fetched_uris),
              testing::UnorderedElementsAre(GetTestGropingAlpha(),
                                            GetTestGropingBeta()));
}

TEST_F(AffiliationBackendTest, SingleGroupForAffiliatedFacets) {
  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  backend()->KeepPrefetchForFacets(fetched_uris);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(fetched_uris));

  EXPECT_THAT(backend()->GetGroupingInfo(fetched_uris),
              testing::UnorderedElementsAre(GetTestGropingAlpha()));
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingClearsOldCache) {
  mock_fetch_throttler()->SetInternetConnectivity(/*has_connection=*/true);
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1), base::Time::Max()));
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());

  backend()->UpdateAffiliationsAndBranding(
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
       FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)},
      base::DoNothing());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingSuccess) {
  mock_fetch_throttler()->SetInternetConnectivity(/*has_connection=*/true);
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());

  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)};

  base::MockOnceClosure completion_callback;

  backend()->UpdateAffiliationsAndBranding(facets, completion_callback.Get());
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facets));

  // Expect completion callback.
  EXPECT_CALL(completion_callback, Run);
  backend_task_runner()->RunUntilIdle();

  EXPECT_GE(2u, backend_facet_manager_count());
  EXPECT_EQ(2u, GetNumOfEquivalenceClassInDatabase());
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingFailure) {
  mock_fetch_throttler()->SetInternetConnectivity(/*has_connection=*/true);
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());

  FacetURI facet = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  base::MockOnceClosure completion_callback;

  backend()->UpdateAffiliationsAndBranding({facet}, completion_callback.Get());
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndFailFetch(facet));

  // Still expect completion callback.
  EXPECT_CALL(completion_callback, Run);
  backend_task_runner()->RunUntilIdle();

  EXPECT_GE(1u, backend_facet_manager_count());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingFailsIfNoInternet) {
  mock_fetch_throttler()->SetInternetConnectivity(/*has_connection=*/false);
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());

  FacetURI facet = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  base::MockOnceClosure completion_callback;

  // Expect call to completion callback right away.
  EXPECT_CALL(completion_callback, Run);
  backend()->UpdateAffiliationsAndBranding({facet}, completion_callback.Get());
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
  ASSERT_FALSE(mock_fetch_throttler()->has_signaled_network_request_needed());

  EXPECT_GE(0u, backend_facet_manager_count());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());
}

TEST_F(AffiliationBackendTest, GetGroupingInfoInjectsGroupsForMissingFacets) {
  std::vector<FacetURI> facets;
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  GroupedFacets group1;
  group1.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1))};

  GroupedFacets group2;
  group2.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2))};

  EXPECT_THAT(backend()->GetGroupingInfo(facets),
              testing::UnorderedElementsAre(group1, group2));
}

TEST_F(AffiliationBackendTest, GetGroupingInfoWithDuplicates) {
  std::vector<FacetURI> facets;
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1))};

  EXPECT_THAT(backend()->GetGroupingInfo(facets),
              testing::UnorderedElementsAre(group));
}

TEST_F(AffiliationBackendTest, GetGroupingInfoForInvalidFacet) {
  // Http schema is not supported by the affiliation service.
  FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec("http://test.com");
  ASSERT_FALSE(facet_uri.is_valid());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com"))};

  EXPECT_THAT(backend()->GetGroupingInfo({facet_uri}),
              testing::UnorderedElementsAre(group));
}

TEST_F(AffiliationBackendTest,
       UpdateAffiliationsAndBrandingSkipsInvalidFacets) {
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());

  // Http schema is not supported by the affiliation service.
  FacetURI facet = FacetURI::FromPotentiallyInvalidSpec("http://example.com");
  base::MockOnceClosure completion_callback;

  // Expect call to completion callback right away.
  EXPECT_CALL(completion_callback, Run);
  backend()->UpdateAffiliationsAndBranding({facet}, completion_callback.Get());
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
  ASSERT_FALSE(mock_fetch_throttler()->has_signaled_network_request_needed());

  EXPECT_GE(0u, backend_facet_manager_count());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassInDatabase());
}

TEST_F(AffiliationBackendTest, GroupsUpdatedByMainDomain) {
  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  // Add a different facet to its own group with the same eTLD+1.
  fetched_uris.push_back(
      FacetURI::FromCanonicalSpec("https://alpha.example.com"));

  GroupedFacets group;
  group.facets = {Facet(fetched_uris.back())};
  fake_affiliation_api()->AddTestEquivalenceClass(group.facets);
  fake_affiliation_api()->AddTestGrouping(group);

  backend()->KeepPrefetchForFacets(fetched_uris);
  ASSERT_NO_FATAL_FAILURE(ExpectNeedForFetchAndLetItBeSent());
  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(fetched_uris));

  auto expected_group = GetTestGropingAlpha();
  expected_group.facets.emplace_back(fetched_uris.back());
  EXPECT_THAT(backend()->GetGroupingInfo(fetched_uris),
              testing::ElementsAre(expected_group));
}

}  // namespace affiliations
