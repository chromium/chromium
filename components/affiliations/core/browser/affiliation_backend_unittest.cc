// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_backend.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/clock.h"
#include "base/test/run_until.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_database.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler.h"
#include "components/affiliations/core/browser/affiliation_fetch_throttler_delegate.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/facet_manager.h"
#include "components/affiliations/core/browser/fake_affiliation_api.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace affiliations {

namespace {
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

GroupedFacets GetTestGroupingAlpha() {
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

GroupedFacets GetTestGroupingBeta() {
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta2)),
  };
  return group;
}

constexpr base::TimeDelta GetCacheHardExpiryPeriod() {
  return base::Hours(FacetManager::kCacheHardExpiryInHours);
}

// Returns a smallest time difference that this test cares about.
constexpr base::TimeDelta Epsilon() {
  return base::Microseconds(1);
}

}  // namespace

class AffiliationBackendTest : public testing::Test {
 public:
  AffiliationBackendTest() = default;

  AffiliationBackendTest(const AffiliationBackendTest&) = delete;
  AffiliationBackendTest& operator=(const AffiliationBackendTest&) = delete;

 protected:
  void DestroyBackend() {
    // `backend_` owns the fetcher factory to which `fake_affiliation_api_`
    // keeps a raw pointer, so this raw pointer needs to be reset prior to
    // destroying `backend_`.
    fake_affiliation_api_.SetFetcherFactory(nullptr);
    backend_.reset();
  }

  void AdvanceTime(base::TimeDelta delta) {
    backend_task_runner_->FastForwardBy(delta);
  }

  AffiliatedFacetsWithUpdateTime GetCachedAffiliation(
      const FacetURI& facet_uri) const {
    const auto& database = backend_->GetAffiliationDatabaseForTesting();
    AffiliatedFacetsWithUpdateTime affiliation;
    database.GetAffiliationsAndBrandingForFacetURI(facet_uri, &affiliation);
    return affiliation;
  }

  bool IsFacetCached(const FacetURI& facet_uri) const {
    return !GetCachedAffiliation(facet_uri).facets.empty();
  }

  // Sends a fetch to the fake affiliation API if it is needed and returns true.
  // If no fetch is needed, returns false. |fail_request| allows controlling
  // whether the fake affiliation API should fail the request.
  // In most test cases this function is required after each call to backend
  // that would send a request to Affiliation API. If such a request is not
  // needed, e.g. because the requested facet is cached, you can skip this
  // function.
  bool SendFetchOverNetwork(bool fail_request = false) {
    // Throttler runs on the same task runner as the backend and posts a task
    // prior to sending a network request. We need to allow this task to
    // complete in order to send that request to fake affiliation API. Keep in
    // mind that the backend itself can post tasks to this runner and so this
    // call will execute them as well.
    backend_task_runner_->RunUntilIdle();
    // If throttler was not called, it will not issue an API request.
    if (!fake_affiliation_api_.HasPendingRequest()) {
      return false;
    }
    if (fail_request) {
      fake_affiliation_api_.FailNextRequest();
    } else {
      fake_affiliation_api_.ServeNextRequest();
    }
    return true;
  }

  // Returns the number of equivalence classes in the backend's database.
  size_t GetNumOfEquivalenceClassesInDatabase() {
    const auto& database = backend()->GetAffiliationDatabaseForTesting();
    std::vector<AffiliatedFacetsWithUpdateTime> all_affiliations;
    database.GetAllAffiliationsAndBranding(&all_affiliations);
    return all_affiliations.size();
  }

  size_t backend_facet_manager_count() {
    return backend()->facet_manager_count();
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

  void TurnOnInternetConnection() {
    network_connection_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
  }

  void TurnOffInternetConnection() {
    network_connection_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
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
    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    network_connection_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
    backend_->Initialize(test_shared_loader_factory->Clone(),
                         network_connection_tracker_.get(), db_path_);
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
    fake_affiliation_api_.AddTestGrouping(GetTestGroupingAlpha());
    fake_affiliation_api_.AddTestGrouping(GetTestGroupingBeta());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> backend_task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scoped_refptr<base::TestSimpleTaskRunner> consumer_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  base::FilePath db_path_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<AffiliationBackend> backend_;
  FakeAffiliationAPI fake_affiliation_api_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(AffiliationBackendTest, CachedOnlyRequestFailsDueToCacheMiss) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  base::MockOnceCallback<void(const AffiliatedFacets&, bool)>
      expected_result_callback;

  backend()->GetAffiliationsAndBranding(
      facet_uri, expected_result_callback.Get(), consumer_task_runner());
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_FALSE(fetched_over_network);
  EXPECT_CALL(expected_result_callback, Run(AffiliatedFacets(), false));
  consumer_task_runner()->RunUntilIdle();
  // Facet managers can be discarded at the end of the on demand call.
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, PrefetchTriggersInitialFetch) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  backend()->Prefetch(facet_uri, base::Time::Max());
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(fetched_over_network);
  EXPECT_TRUE(IsFacetCached(facet_uri));
  // Prefetch manager should be kept since we need to keep the facet fresh until
  // base::Time::Max()
  EXPECT_EQ(1u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, ExpiredPrefetchTriggersNoInitialFetch) {
  FacetURI facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  fake_affiliation_api()->AddTestEquivalenceClass(
      GetTestEquivalenceClassAlpha());

  backend()->Prefetch(facet, backend_task_runner()->Now() - Epsilon());
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_FALSE(fetched_over_network);
  EXPECT_FALSE(IsFacetCached(facet));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

// Two additional Prefetch() request come
// in, both for unrelated facets, shortly after an initial
// Prefetch() request.
//
// Suppose that the network request triggered by the first
// Prefetch() request has already been initiated when the
// other requests arrive. As there should be no simultaneous requests, the
// additional facets should be queried together in a second fetch after the
// first fetch completes.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests) {
  FacetURI facet_uri_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_uri_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  bool first_fetch_over_network = SendFetchOverNetwork();
  backend()->Prefetch(facet_uri_beta, base::Time::Max());
  backend()->Prefetch(facet_uri_gamma, base::Time::Max());
  bool second_fetch_over_network = SendFetchOverNetwork();
  bool third_fetch_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(first_fetch_over_network);
  EXPECT_TRUE(second_fetch_over_network);
  EXPECT_FALSE(third_fetch_over_network);
  EXPECT_TRUE(IsFacetCached(facet_uri_alpha));
  EXPECT_TRUE(IsFacetCached(facet_uri_beta));
  EXPECT_TRUE(IsFacetCached(facet_uri_gamma));
}

// Now suppose that the first fetch is somewhat delayed (e.g., because network
// requests are throttled), so the other requests arrive before it is actually
// issued. In this case, all facet URIs should be queried together in one fetch.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequestsIssuedTogether) {
  FacetURI facet_uri_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_uri_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  backend()->Prefetch(facet_uri_beta, base::Time::Max());
  backend()->Prefetch(facet_uri_gamma, base::Time::Max());
  bool first_fetch_over_network = SendFetchOverNetwork();
  bool second_fetch_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(first_fetch_over_network);
  EXPECT_FALSE(second_fetch_over_network);
  EXPECT_TRUE(IsFacetCached(facet_uri_alpha));
  EXPECT_TRUE(IsFacetCached(facet_uri_beta));
  EXPECT_TRUE(IsFacetCached(facet_uri_gamma));
}

TEST_F(AffiliationBackendTest, RetryIsMadeOnFailedFetch) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  backend()->Prefetch(facet_uri, base::Time::Max());
  // Send the request but fail it
  bool initial_fetched_over_network = SendFetchOverNetwork(true);
  // Wait for the delay issued by the throttler.
  AdvanceTime(base::Seconds(10));
  bool retry_fetched_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(initial_fetched_over_network);
  EXPECT_TRUE(retry_fetched_over_network);
  EXPECT_TRUE(IsFacetCached(facet_uri));
}

// The Prefetch() request expires before fetching corresponding affiliation
// information would be allowed. The fetch should be abandoned.
TEST_F(AffiliationBackendTest, FetchIsNoLongerNeededOnceAllowed) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  TurnOffInternetConnection();

  backend()->Prefetch(facet_uri, backend_task_runner()->Now() + Epsilon());
  bool first_fetch_over_network = SendFetchOverNetwork();
  AdvanceTime(Epsilon());
  TurnOnInternetConnection();
  bool second_fetch_over_network = SendFetchOverNetwork();

  ASSERT_FALSE(first_fetch_over_network);
  ASSERT_FALSE(second_fetch_over_network);
  EXPECT_FALSE(IsFacetCached(facet_uri));
}

TEST_F(AffiliationBackendTest, CacheServesRequestsForPrefetchedFacets) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  base::MockOnceCallback<void(const AffiliatedFacets&, bool)>
      expected_result_callback_1;

  backend()->Prefetch(facet_uri, base::Time::Max());
  bool first_fetch_over_network = SendFetchOverNetwork();
  backend()->GetAffiliationsAndBranding(
      facet_uri, expected_result_callback_1.Get(), consumer_task_runner());
  bool second_fetch_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(first_fetch_over_network);
  EXPECT_FALSE(second_fetch_over_network);
  EXPECT_CALL(expected_result_callback_1,
              Run(GetTestEquivalenceClassAlpha(), true));
  consumer_task_runner()->RunUntilIdle();
}

TEST_F(AffiliationBackendTest,
       CacheServesRequestsForFacetsAffiliatedWithPrefetchedFacets) {
  FacetURI facet_uri_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_alpha_duplicate(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  base::MockOnceCallback<void(const AffiliatedFacets&, bool)>
      expected_result_callback;

  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  bool first_fetch_over_network = SendFetchOverNetwork();
  backend()->GetAffiliationsAndBranding(facet_uri_alpha_duplicate,
                                        expected_result_callback.Get(),
                                        consumer_task_runner());
  bool second_fetch_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(first_fetch_over_network);
  EXPECT_FALSE(second_fetch_over_network);
  EXPECT_CALL(expected_result_callback,
              Run(GetTestEquivalenceClassAlpha(), true));
  consumer_task_runner()->RunUntilIdle();
}

// A second GetAffiliationsAndBranding() request for the same facet and a third
// request for an affiliated facet comes in while the network fetch triggered by
// the first request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, all
// three requests should be served without further fetches (they have the data).
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentRequestsForAffiliatedFacets) {
  FacetURI facet_uri_alpha_1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_alpha_2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  backend()->Prefetch(facet_uri_alpha_1, base::Time::Max());
  backend_task_runner()->RunUntilIdle();
  bool first_request_in_flight = fake_affiliation_api()->HasPendingRequest();
  backend()->Prefetch(facet_uri_alpha_1, base::Time::Max());
  backend()->Prefetch(facet_uri_alpha_2, base::Time::Max());
  fake_affiliation_api()->ServeNextRequest();
  backend_task_runner()->RunUntilIdle();
  bool second_fetch_needed = fake_affiliation_api()->HasPendingRequest();

  EXPECT_TRUE(first_request_in_flight);
  EXPECT_FALSE(second_fetch_needed);
  EXPECT_TRUE(IsFacetCached(facet_uri_alpha_1));
  EXPECT_TRUE(IsFacetCached(facet_uri_alpha_2));
}

// A second Prefetch() request for the same facet and a third request for an
// affiliated facet comes in while the initial fetch triggered by the first
// request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, there
// should be no further initial fetches as the data needed is already there.
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentPrefetchesForAffiliatedFacets) {
  FacetURI facet_uri_alpha_1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_alpha_2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  backend()->Prefetch(facet_uri_alpha_1, base::Time::Max());
  backend_task_runner()->RunUntilIdle();
  bool first_request_in_flight = fake_affiliation_api()->HasPendingRequest();
  backend()->Prefetch(facet_uri_alpha_1, base::Time::Max());
  backend()->Prefetch(facet_uri_alpha_1, base::Time::Max());
  fake_affiliation_api()->ServeNextRequest();
  backend_task_runner()->RunUntilIdle();
  bool second_fetch_needed = fake_affiliation_api()->HasPendingRequest();

  EXPECT_TRUE(first_request_in_flight);
  EXPECT_FALSE(second_fetch_needed);
  EXPECT_TRUE(IsFacetCached(facet_uri_alpha_1));
  EXPECT_TRUE(IsFacetCached(facet_uri_alpha_2));
}

TEST_F(AffiliationBackendTest, SimpleCacheExpiryWithPrefetches) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  backend()->Prefetch(facet_uri, backend_task_runner()->Now() + Epsilon());
  SendFetchOverNetwork();
  AdvanceTime(GetCacheHardExpiryPeriod());

  EXPECT_FALSE(IsCachedDataFreshForFacetURI(facet_uri));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CancelPrefetchRemovesFacetManager) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  backend()->Prefetch(facet_uri, base::Time::Max());
  backend()->CancelPrefetch(facet_uri, base::Time::Max());
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_FALSE(fetched_over_network);
  EXPECT_FALSE(IsCachedDataFreshForFacetURI(facet_uri));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest,
       CancelOneOfTwoEqualPrefetchesRemovesOnlyOnePrefetch) {
  FacetURI facet_uri(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  backend()->Prefetch(facet_uri, base::Time::Max());
  backend()->Prefetch(facet_uri, base::Time::Max());
  backend()->CancelPrefetch(facet_uri, base::Time::Max());
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(fetched_over_network);
  EXPECT_TRUE(IsCachedDataFreshForFacetURI(facet_uri));
  EXPECT_EQ(1u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CancelingNonExistingPrefetchIsSilentlyIgnored) {
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// Verify that TrimCacheForFacetURI() only removes the equivalence class for the
// given facet, and preserves others (even if they could be discarded).
TEST_F(AffiliationBackendTest,
       TrimCacheForFacetURIOnlyRemovesDataForTheGivenFacet) {
  FacetURI facet_uri_alpha_1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_alpha_2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  FacetURI facet_uri_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  base::MockOnceCallback<void(const AffiliatedFacets&, bool)> unused;

  backend()->Prefetch(facet_uri_beta, backend_task_runner()->Now() + Epsilon());
  SendFetchOverNetwork();
  backend()->Prefetch(facet_uri_alpha_1,
                      backend_task_runner()->Now() + Epsilon());
  SendFetchOverNetwork();
  AdvanceTime(Epsilon());
  // The facet only needs to be in the same affiliation to delete it.
  backend()->TrimCacheForFacetURI(facet_uri_alpha_2);

  EXPECT_TRUE(IsFacetCached(facet_uri_beta));
  EXPECT_FALSE(IsFacetCached(facet_uri_alpha_1));
}

TEST_F(AffiliationBackendTest, CacheIsEmptyOnStartup) {
  EXPECT_EQ(0u, GetNumOfEquivalenceClassesInDatabase());
}

TEST_F(AffiliationBackendTest, DestroyingBackendKeepsCache) {
  DestroyBackend();

  ASSERT_TRUE(base::PathExists(db_path()));
}

TEST_F(AffiliationBackendTest, DeleteCache) {
  DestroyBackend();

  AffiliationBackend::DeleteCache(db_path());

  ASSERT_FALSE(base::PathExists(db_path()));
}

TEST_F(AffiliationBackendTest, KeepPrefetchForFacetsIssuesNewPrefetch) {
  FacetURI facet_uri_alpha = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  FacetURI facet_uri_beta = FacetURI::FromCanonicalSpec(kTestFacetURIBeta1);

  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  SendFetchOverNetwork();
  backend()->KeepPrefetchForFacets({facet_uri_alpha, facet_uri_beta});
  SendFetchOverNetwork();

  EXPECT_TRUE(IsFacetCached(facet_uri_alpha));
  EXPECT_TRUE(IsFacetCached(facet_uri_beta));
  EXPECT_EQ(2u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, KeepPrefetchForFacetsRemovesOldPrefetch) {
  FacetURI facet_uri_alpha = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  FacetURI facet_uri_beta = FacetURI::FromCanonicalSpec(kTestFacetURIBeta1);

  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  SendFetchOverNetwork();
  backend()->Prefetch(facet_uri_beta, base::Time::Max());
  SendFetchOverNetwork();
  backend()->KeepPrefetchForFacets({facet_uri_alpha});

  EXPECT_TRUE(IsFacetCached(facet_uri_alpha));
  EXPECT_FALSE(IsFacetCached(facet_uri_beta));
  EXPECT_EQ(1u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, KeepPrefetchForFacetsOverridesAllPrefetches) {
  FacetURI facet_uri_alpha = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  FacetURI facet_uri_beta = FacetURI::FromCanonicalSpec(kTestFacetURIBeta1);
  FacetURI facet_uri_gamma = FacetURI::FromCanonicalSpec(kTestFacetURIGamma1);

  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  SendFetchOverNetwork();
  backend()->Prefetch(facet_uri_beta, base::Time::Max());
  SendFetchOverNetwork();
  backend()->Prefetch(facet_uri_alpha, base::Time::Max());
  backend()->KeepPrefetchForFacets({facet_uri_alpha, facet_uri_gamma});
  SendFetchOverNetwork();

  EXPECT_TRUE(IsFacetCached(facet_uri_alpha));
  EXPECT_TRUE(IsFacetCached(facet_uri_gamma));
  EXPECT_FALSE(IsFacetCached(facet_uri_beta));
  EXPECT_EQ(2u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, GetGroupingForRelatedFacets) {
  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  backend()->KeepPrefetchForFacets(fetched_uris);
  SendFetchOverNetwork();

  EXPECT_THAT(backend()->GetGroupingInfo(fetched_uris),
              testing::UnorderedElementsAre(GetTestGroupingAlpha()));
}

TEST_F(AffiliationBackendTest, GetGroupingForUnrelatedFacets) {
  std::vector<FacetURI> fetched_uris;
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  fetched_uris.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  backend()->KeepPrefetchForFacets(fetched_uris);
  SendFetchOverNetwork();

  EXPECT_THAT(backend()->GetGroupingInfo(fetched_uris),
              testing::UnorderedElementsAre(GetTestGroupingAlpha(),
                                            GetTestGroupingBeta()));
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingSuccess) {
  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)};
  base::test::TestFuture<void> completion_callback;

  backend()->UpdateAffiliationsAndBranding(facets,
                                           completion_callback.GetCallback());
  size_t facet_manager_count = backend_facet_manager_count();
  SendFetchOverNetwork();

  EXPECT_TRUE(completion_callback.IsReady());
  // UpdateAffiliationsAndBranding does not create facet managers.
  EXPECT_EQ(0u, facet_manager_count);
  EXPECT_TRUE(IsFacetCached(facets[0]));
  EXPECT_TRUE(IsFacetCached(facets[1]));
}

TEST_F(AffiliationBackendTest, MultipleUpdateAffiliationsAndBrandingSuccess) {
  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)};
  base::test::TestFuture<void> completion_callback_1;
  base::test::TestFuture<void> completion_callback_2;

  backend()->UpdateAffiliationsAndBranding(facets,
                                           completion_callback_1.GetCallback());
  backend()->UpdateAffiliationsAndBranding(facets,
                                           completion_callback_2.GetCallback());
  bool fetched_over_network_1 = SendFetchOverNetwork();
  bool needs_another_fetch = fake_affiliation_api()->HasPendingRequest();
  bool fetched_over_network_2 = SendFetchOverNetwork();

  EXPECT_TRUE(fetched_over_network_1);
  EXPECT_TRUE(needs_another_fetch);
  EXPECT_TRUE(fetched_over_network_2);
  EXPECT_TRUE(completion_callback_1.IsReady());
  EXPECT_TRUE(completion_callback_2.IsReady());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_TRUE(IsFacetCached(facets[0]));
  EXPECT_TRUE(IsFacetCached(facets[1]));
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingFailure) {
  FacetURI facet_uri = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  base::test::TestFuture<void> completion_callback;

  backend()->UpdateAffiliationsAndBranding({facet_uri},
                                           completion_callback.GetCallback());
  SendFetchOverNetwork(true);
  bool needs_fetch_after_failure = fake_affiliation_api()->HasPendingRequest();

  EXPECT_TRUE(completion_callback.IsReady());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassesInDatabase());
  // UpdateAffiliationsAndBrandingFailure will not retry failed fetches because
  // it doesn't create a facet manager for the requested urls.
  EXPECT_FALSE(needs_fetch_after_failure);
}

TEST_F(AffiliationBackendTest, UpdateAffiliationsAndBrandingFailsIfNoInternet) {
  TurnOffInternetConnection();
  FacetURI facet_uri = FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1);
  base::test::TestFuture<void> completion_callback;

  backend()->UpdateAffiliationsAndBranding({facet_uri},
                                           completion_callback.GetCallback());

  EXPECT_TRUE(completion_callback.IsReady());
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassesInDatabase());
}

TEST_F(AffiliationBackendTest,
       UpdateAffiliationsAndBrandingAndPrefetchSuccess) {
  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)};
  base::test::TestFuture<void> completion_callback_1;

  backend()->UpdateAffiliationsAndBranding(facets,
                                           completion_callback_1.GetCallback());
  backend()->Prefetch(facets[0], base::Time::Max());
  bool fetched_over_network_1 = SendFetchOverNetwork();
  bool needs_another_fetch = fake_affiliation_api()->HasPendingRequest();
  bool fetched_over_network_2 = SendFetchOverNetwork();

  EXPECT_TRUE(fetched_over_network_1);
  EXPECT_TRUE(needs_another_fetch);
  EXPECT_TRUE(fetched_over_network_2);
  EXPECT_TRUE(completion_callback_1.IsReady());
  // Prefetch will create a manager
  EXPECT_EQ(1u, backend_facet_manager_count());
  EXPECT_TRUE(IsFacetCached(facets[0]));
  EXPECT_TRUE(IsFacetCached(facets[1]));
}

TEST_F(AffiliationBackendTest,
       ConcurrentPrefetchAndUpdateAffiliationsAndBranding) {
  std::vector<FacetURI> facets = {
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)};
  base::test::TestFuture<void> completion_callback_1;

  backend()->Prefetch(facets[0], base::Time::Max());
  backend_task_runner()->RunUntilIdle();
  bool prefetch_in_flight = fake_affiliation_api()->HasPendingRequest();
  backend()->UpdateAffiliationsAndBranding(facets,
                                           completion_callback_1.GetCallback());
  // This will serve the prefetch
  fake_affiliation_api()->ServeNextRequest();
  bool needs_another_fetch = fake_affiliation_api()->HasPendingRequest();
  bool fetched_over_network_2 = SendFetchOverNetwork();

  EXPECT_TRUE(prefetch_in_flight);
  EXPECT_TRUE(needs_another_fetch);
  EXPECT_TRUE(fetched_over_network_2);
  EXPECT_TRUE(completion_callback_1.IsReady());
  // Prefetch will create a manager
  EXPECT_EQ(1u, backend_facet_manager_count());
  EXPECT_TRUE(IsFacetCached(facets[0]));
  EXPECT_TRUE(IsFacetCached(facets[1]));
}

TEST_F(AffiliationBackendTest, GetGroupingInfoInjectsGroupsForMissingFacets) {
  std::vector<FacetURI> facets;
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  GroupedFacets group1;
  group1.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1))};
  GroupedFacets group2;
  group2.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2))};

  std::vector<GroupedFacets> grouping_info = backend()->GetGroupingInfo(facets);

  EXPECT_THAT(grouping_info, testing::UnorderedElementsAre(group1, group2));
}

TEST_F(AffiliationBackendTest, GetGroupingInfoWithDuplicates) {
  std::vector<FacetURI> facets;
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1))};

  std::vector<GroupedFacets> grouping_info = backend()->GetGroupingInfo(facets);

  EXPECT_THAT(grouping_info, testing::UnorderedElementsAre(group));
}

TEST_F(AffiliationBackendTest, GetGroupingInfoForInvalidFacet) {
  // Http schema is not supported by the affiliation service.
  FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec("http://test.com");
  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("http://test.com"))};

  std::vector<GroupedFacets> grouping_info =
      backend()->GetGroupingInfo({facet_uri});

  EXPECT_THAT(grouping_info, testing::UnorderedElementsAre(group));
}

TEST_F(AffiliationBackendTest,
       UpdateAffiliationsAndBrandingSkipsInvalidFacets) {
  // Http schema is not supported by the affiliation service.
  FacetURI facet_uri =
      FacetURI::FromPotentiallyInvalidSpec("http://example.com");
  base::test::TestFuture<void> completion_callback;

  backend()->UpdateAffiliationsAndBranding({facet_uri},
                                           completion_callback.GetCallback());
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(completion_callback.IsReady());
  EXPECT_FALSE(fetched_over_network);
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_EQ(0u, GetNumOfEquivalenceClassesInDatabase());
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
  bool fetched_over_network = SendFetchOverNetwork();

  EXPECT_TRUE(fetched_over_network);
  GroupedFacets expected_group = GetTestGroupingAlpha();
  expected_group.facets.emplace_back(fetched_uris.back());
  EXPECT_THAT(backend()->GetGroupingInfo(fetched_uris),
              testing::ElementsAre(expected_group));
}

TEST_F(AffiliationBackendTest, ChangePasswordUrlsArerequested) {
  FacetURI facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri_alpha_2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  backend()->Prefetch(facet, base::Time::Max());
  backend_task_runner()->RunUntilIdle();

  EXPECT_EQ(base::FeatureList::IsEnabled(kFetchChangePasswordUrl),
            fake_affiliation_api()
                ->GetNextAffiliationFetcher()
                ->GetRequestInfo()
                .change_password_info);
  fake_affiliation_api()->ServeNextRequest();
}

}  // namespace affiliations
