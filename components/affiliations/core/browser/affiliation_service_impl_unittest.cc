// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/affiliations/core/browser/affiliation_backend.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_service_impl.h"
#include "components/affiliations/core/browser/fake_affiliation_api.h"
#include "components/affiliations/core/browser/mock_affiliation_consumer.h"
#include "components/affiliations/core/browser/mock_affiliation_fetcher.h"
#include "components/affiliations/core/browser/mock_affiliation_fetcher_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

namespace affiliations {

namespace {

constexpr char k1ExampleURL[] = "https://1.example.com";
constexpr char k1ExampleChangePasswordURL[] =
    "https://1.example.com/.well-known/change-password";
constexpr char kM1ExampleURL[] = "https://m.1.example.com";
constexpr char kOneExampleURL[] = "https://one.example.com";
constexpr char kOneExampleChangePasswordURL[] =
    "https://one.example.com/settings/passwords";
constexpr char k2ExampleURL[] = "https://2.example.com";
constexpr char k2ExampleChangePasswordURL[] = "https://2.example.com/pwd";
constexpr char k3ExampleURL[] = "https://3.example.com";
constexpr char k4ExampleURL[] = "https://4.example.com";
constexpr char k5ExampleURL[] = "https://5.example.com";

using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

constexpr char kTestFacetURIAlpha1[] = "https://one.alpha.example.com";
constexpr char kTestFacetURIAlpha2[] = "https://two.alpha.example.com";
constexpr char kTestFacetURIAlpha3[] = "https://three.alpha.example.com";
constexpr char kTestFacetURIBeta1[] = "https://one.beta.example.com";

const char kTestAndroidFacetURIAlpha[] =
    "android://hash@com.example.alpha.android";
const char kTestAndroidFacetNameAlpha1[] = "Facet Name Alpha 1";
const char kTestAndroidFacetIconURLAlpha1[] = "https://example.com/alpha_1.png";

const char kTestAndroidFacetURIBeta1[] =
    "android://hash@com.example.beta.android";
const char kTestAndroidFacetNameBeta1[] = "Facet Name Beta 1";
const char kTestAndroidFacetIconURLBeta1[] = "https://example.com/beta_1.png";

const char kTestAndroidFacetURIBeta2[] =
    "android://hash@com.yetanother.beta.android";
const char kTestAndroidFacetNameBeta2[] = "Facet Name Beta 2";
const char kTestAndroidFacetIconURLBeta2[] = "https://example.com/beta_2.png";

const char kTestAndroidFacetURIGamma[] =
    "android://hash@com.example.gamma.android";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)),
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3)),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIAlpha),
            FacetBrandingInfo{kTestAndroidFacetNameAlpha1,
                              GURL(kTestAndroidFacetIconURLAlpha1)}),
  };
}

AffiliatedFacets GetTestEquivalenceClassBeta() {
  return {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1)),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta1),
            FacetBrandingInfo{kTestAndroidFacetNameBeta1,
                              GURL(kTestAndroidFacetIconURLBeta1)}),
      Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIBeta2),
            FacetBrandingInfo{kTestAndroidFacetNameBeta2,
                              GURL(kTestAndroidFacetIconURLBeta2)}),
  };
}

AffiliatedFacets GetTestEquivalenceClassGamma() {
  return {Facet(FacetURI::FromCanonicalSpec(kTestAndroidFacetURIGamma))};
}

std::vector<FacetURI> ToFacetsURIs(const std::vector<GURL>& urls) {
  std::vector<FacetURI> facet_URIs;
  for (const auto& url : urls) {
    facet_URIs.push_back(
        FacetURI::FromCanonicalSpec(url::SchemeHostPort(url).Serialize()));
  }
  return facet_URIs;
}

}  // namespace

class AffiliationServiceImplTest : public testing::Test {
 public:
  AffiliationServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AffiliationServiceImplTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockAffiliationFetcherFactory& mock_fetcher_factory() {
    return *fetcher_factory_;
  }

  void CreateService() {
    service_ = std::make_unique<AffiliationServiceImpl>(
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
        background_task_runner());

    network::TestNetworkConnectionTracker* network_connection_tracker =
        network::TestNetworkConnectionTracker::GetInstance();
    network_connection_tracker->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
    base::FilePath database_path;
    ASSERT_TRUE(CreateTemporaryFile(&database_path));
    service_->Init(network_connection_tracker, database_path);
  }

  void DestroyService() {
    fake_affiliation_api_.SetFetcherFactory(nullptr);
    service_->Shutdown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  AffiliationServiceImpl* service() { return service_.get(); }
  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  base::TestSimpleTaskRunner* background_task_runner() {
    return background_task_runner_.get();
  }

  FakeAffiliationAPI* fake_affiliation_api() { return &fake_affiliation_api_; }

 protected:
  std::unique_ptr<AffiliationServiceImpl> service_;
  FakeAffiliationAPI fake_affiliation_api_;
  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();

 private:
  void SetUp() override {
    CreateService();
    auto mock_fetcher_factory =
        std::make_unique<MockAffiliationFetcherFactory>();
    fetcher_factory_ = mock_fetcher_factory.get();
    service_->SetFetcherFactoryForTesting(std::move(mock_fetcher_factory));
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
  }

  void TearDown() override {
    // The service uses DeleteSoon to asynchronously destroy its backend. Pump
    // the background thread to make sure destruction actually takes place.
    DestroyService();
    background_task_runner_->RunUntilIdle();
  }

  base::HistogramTester histogram_tester_;
  raw_ptr<MockAffiliationFetcherFactory> fetcher_factory_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  MockAffiliationConsumer mock_consumer_;
};

TEST_F(AffiliationServiceImplTest, GetChangePasswordURLReturnsEmpty) {
  EXPECT_EQ(GURL(), service()->GetChangePasswordURL(GURL(k1ExampleURL)));
}

TEST_F(AffiliationServiceImplTest, FetchRequestIsStarted) {
  const std::vector<GURL> origins = {GURL(k1ExampleURL), GURL(k2ExampleURL)};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs(origins),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs(origins, base::DoNothing());
}

TEST_F(AffiliationServiceImplTest, ClearStopsOngoingRequest) {
  const std::vector<GURL> origins = {GURL(k1ExampleURL), GURL(k2ExampleURL)};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs(origins),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::MockOnceClosure callback;
  service()->PrefetchChangePasswordURLs(origins, callback.Get());

  EXPECT_CALL(callback, Run());
  service()->Clear();
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSuccededInsertsChangePasswordURLOfRequestedSiteIfFound) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
            FacetBrandingInfo(), GURL(kOneExampleChangePasswordURL))};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));

  // Expect Change Password URL of requested site.
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL),
            service()->GetChangePasswordURL(origin));
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSuccededInsertsChangePasswordURLOfAnotherSiteFromAGroup) {
  const GURL origin(kM1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
                        FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL))};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));

  // Expect Change Password URL of another site from a grouping.
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL),
            service()->GetChangePasswordURL(origin));
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceedTakesNoActionWhenNoChangePasswordURLsAvailable) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL))};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));

  EXPECT_EQ(GURL(), service()->GetChangePasswordURL(origin));
}

TEST_F(AffiliationServiceImplTest, OnFetchFailedResetsFetcher) {
  std::vector<GURL> origins = {GURL(k1ExampleURL), GURL(k2ExampleURL)};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs(origins),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::MockOnceClosure callback;
  service()->PrefetchChangePasswordURLs(origins, callback.Get());

  EXPECT_CALL(callback, Run());
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchFailed(
      raw_mock_fetcher);
}

TEST_F(AffiliationServiceImplTest, OnMalformedResponseResetsFetcher) {
  std::vector<GURL> origins = {GURL(k1ExampleURL), GURL(k2ExampleURL)};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs(origins),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::MockOnceClosure callback;
  service()->PrefetchChangePasswordURLs(origins, callback.Get());

  EXPECT_CALL(callback, Run());
  static_cast<AffiliationFetcherDelegate*>(service())->OnMalformedResponse(
      raw_mock_fetcher);
}

TEST_F(AffiliationServiceImplTest,
       PrefetchChangePasswordURLsWhenFetcherNotCreated) {
  base::MockOnceClosure completion_callback;

  EXPECT_CALL(mock_fetcher_factory(), CreateInstance).WillOnce(Return(nullptr));

  service()->PrefetchChangePasswordURLs({GURL(k1ExampleURL)},
                                        completion_callback.Get());

  EXPECT_CALL(completion_callback, Run);
  RunUntilIdle();
}

TEST_F(AffiliationServiceImplTest,
       EachPrefetchCallCreatesNewAffiliationFetcherInstance) {
  const GURL origin1(k1ExampleURL);
  const GURL origin2(k2ExampleURL);
  const GURL origin3(k3ExampleURL);
  const GURL origin4(k4ExampleURL);
  const GURL origin5(k5ExampleURL);

  const std::vector<GURL> origins_1 = {origin1, origin2, origin3};
  const std::vector<GURL> origins_2 = {origin3, origin4, origin5};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto new_mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs(origins_1),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(*new_mock_fetcher, StartRequest(ToFacetsURIs(origins_2),
                                              kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))))
      .WillOnce(Return(ByMove(std::move(new_mock_fetcher))));

  service()->PrefetchChangePasswordURLs(origins_1, base::DoNothing());
  service()->PrefetchChangePasswordURLs(origins_2, base::DoNothing());
}

// Below are the tests veryfing recorded metrics for
// PasswordManager.AffiliationService.GetChangePasswordUsage.

TEST_F(AffiliationServiceImplTest, NotFetchedYetMetricIfWaitingForResponse) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());
  service()->GetChangePasswordURL(origin);

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      GetChangePasswordUrlMetric::kNotFetchedYet, 1);
}

TEST_F(AffiliationServiceImplTest, NoUrlOverrideAvailableMetric) {
  service()->GetChangePasswordURL(GURL(k1ExampleURL));

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      GetChangePasswordUrlMetric::kNoUrlOverrideAvailable, 1);
}

TEST_F(AffiliationServiceImplTest, FoundForRequestedFacetMetric) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
            FacetBrandingInfo(), GURL(kOneExampleChangePasswordURL))};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);

  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));
  service()->GetChangePasswordURL(origin);

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      GetChangePasswordUrlMetric::kUrlOverrideUsed, 1);
}

TEST_F(AffiliationServiceImplTest, FoundForGroupedFacetMetric) {
  const GURL origin(kM1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
                        FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL))};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);

  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));
  service()->GetChangePasswordURL(origin);

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      GetChangePasswordUrlMetric::kGroupUrlOverrideUsed, 1);
}

TEST_F(AffiliationServiceImplTest, OnFetchSuccedeedRunsCallback) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs({origin}),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::MockOnceClosure callback;
  service()->PrefetchChangePasswordURLs({origin}, callback.Get());

  EXPECT_CALL(callback, Run());
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::make_unique<AffiliationFetcherDelegate::Result>());
}

TEST_F(AffiliationServiceImplTest, SupportForMultipleRequests) {
  const GURL origin1(k1ExampleURL);
  const GURL origin2(k2ExampleURL);
  const std::vector<GURL> origins_1 = {origin1};
  const std::vector<GURL> origins_2 = {origin2};

  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();
  auto new_mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* new_raw_mock_fetcher = new_mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher, StartRequest(ToFacetsURIs(origins_1),
                                          kChangePasswordUrlRequestInfo));
  EXPECT_CALL(*new_mock_fetcher, StartRequest(ToFacetsURIs(origins_2),
                                              kChangePasswordUrlRequestInfo));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))))
      .WillOnce(Return(ByMove(std::move(new_mock_fetcher))));

  service()->PrefetchChangePasswordURLs(origins_1, base::DoNothing());
  service()->PrefetchChangePasswordURLs(origins_2, base::DoNothing());

  GroupedFacets group1;
  group1.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
                         FacetBrandingInfo(),
                         GURL(k1ExampleChangePasswordURL))};
  auto test_result1 = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result1->groupings.push_back(group1);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result1));
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL),
            service()->GetChangePasswordURL(origin1));

  GroupedFacets group2;
  group2.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k2ExampleURL),
                         FacetBrandingInfo(),
                         GURL(k2ExampleChangePasswordURL))};
  auto test_result2 = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result2->groupings.push_back(group2);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      new_raw_mock_fetcher, std::move(test_result2));
  EXPECT_EQ(GURL(k2ExampleChangePasswordURL),
            service()->GetChangePasswordURL(origin2));
}

// Test fixture to imitate a fetch factory for testing and not just mock it.
class AffiliationServiceImplTestWithFetcherFactory
    : public AffiliationServiceImplTest {
 public:
  void SetUp() override {
    CreateService();

    auto fake_fetcher_factory =
        std::make_unique<FakeAffiliationFetcherFactory>();
    fake_affiliation_api_.SetFetcherFactory(fake_fetcher_factory.get());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassBeta());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassGamma());
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AffiliationBackend::SetFetcherFactoryForTesting,
                       base::Unretained(service_->GetBackendForTesting()),
                       std::move(fake_fetcher_factory)));
  }
};

TEST_F(AffiliationServiceImplTestWithFetcherFactory,
       GetAffiliationsAndBrandingSucceeds) {
  // The first request allows on-demand fetching, and should trigger a fetch.
  // Then, it should succeed after the fetch is complete.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK,
      mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  fake_affiliation_api()->ServeNextRequest();

  const auto equivalence_class_alpha(GetTestEquivalenceClassAlpha());
  mock_consumer()->ExpectSuccessWithResult(equivalence_class_alpha);
  EXPECT_THAT(
      equivalence_class_alpha,
      testing::Contains(testing::Field(
          &Facet::uri, FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1))));

  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // The second request should be (and can be) served from cache.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FAIL, mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectSuccessWithResult(equivalence_class_alpha);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationServiceImplTestWithFetcherFactory,
       GetAffiliationsAndBrandingFails) {
  // The third request is also restricted to the cache, but cannot be served
  // from cache, thus it should fail.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      StrategyOnCacheMiss::FAIL, mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectFailure();
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationServiceImplTestWithFetcherFactory,
       ShutdownWhileTasksArePosted) {
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      StrategyOnCacheMiss::FETCH_OVER_NETWORK,
      mock_consumer()->GetResultCallback());
  EXPECT_TRUE(background_task_runner()->HasPendingTask());

  DestroyService();
  background_task_runner()->RunUntilIdle();

  mock_consumer()->ExpectFailure();
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationServiceImplTestWithFetcherFactory, GetGroupingInfoUsesCache) {
  base::MockCallback<AffiliationService::GroupsCallback> completion_callback;

  service()->GetGroupingInfo({FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)},
                             completion_callback.Get());
  background_task_runner()->RunUntilIdle();

  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)),
  };

  EXPECT_CALL(completion_callback, Run(testing::UnorderedElementsAre(group)));
  RunUntilIdle();
}

TEST_F(AffiliationServiceImplTestWithFetcherFactory,
       UpdateAffiliationsAndBranding) {
  base::MockOnceClosure completion_callback;

  service()->UpdateAffiliationsAndBranding(
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)},
      completion_callback.Get());
  background_task_runner()->RunUntilIdle();

  EXPECT_CALL(completion_callback, Run);
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  fake_affiliation_api()->ServeNextRequest();
  background_task_runner()->RunUntilIdle();

  RunUntilIdle();
}

}  // namespace affiliations
