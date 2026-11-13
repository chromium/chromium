// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_service_impl.h"

#include <memory>
#include <variant>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "components/affiliations/core/browser/affiliation_backend.h"
#include "components/affiliations/core/browser/affiliation_database.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/fake_affiliation_api.h"
#include "components/affiliations/core/browser/mock_affiliation_consumer.h"
#include "components/affiliations/core/browser/mock_affiliation_fetcher.h"
#include "components/affiliations/core/browser/mock_affiliation_fetcher_factory.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
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

std::vector<FacetURI> ToFacetsURIs(const GURL& url) {
  return {FacetURI::FromPotentiallyInvalidSpec(url.possibly_invalid_spec()),
          FacetURI::FromPotentiallyInvalidSpec("https://example.com")};
}

AffiliationFetcherInterface::FetchResult GetSuccessfulFetchResult(
    const AffiliationFetcherInterface::ParsedFetchResponse& parsed_data) {
  FakeAffiliationFetcher::FetchResult fetch_result;
  fetch_result.data = parsed_data;
  fetch_result.http_status_code = net::HTTP_OK;
  return fetch_result;
}

}  // namespace

class AffiliationServiceImplTest : public testing::Test {
 public:
  AffiliationServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AffiliationServiceImplTest() override = default;

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
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET);
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

  raw_ptr<MockAffiliationFetcherFactory> fetcher_factory_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  MockAffiliationConsumer mock_consumer_;
};

TEST_F(AffiliationServiceImplTest, FetchRequestIsStarted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(GURL(k1ExampleURL)),
                           kChangePasswordUrlRequestInfo, testing::_));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(GURL(k1ExampleURL), base::DoNothing());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceededReturnsChangePasswordURLOfRequestedSiteIfFound) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
            FacetBrandingInfo(), GURL(kOneExampleChangePasswordURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);
  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));

  EXPECT_EQ(GURL(k1ExampleChangePasswordURL), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceededIgnoresChangePasswordURLOfAnotherSiteFromAGroup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(kM1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
                        FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);
  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));

  EXPECT_EQ(GURL(), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceededReturnsEmptyWhenNoChangePasswordURLsAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL)),
                  Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);
  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));

  EXPECT_EQ(GURL(), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest, OnFetchFailedResetsFetcher) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  FakeAffiliationFetcher::FetchResult fetch_result;
  fetch_result.http_status_code = net::HTTP_BAD_REQUEST;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(GURL(k1ExampleURL)),
                           kChangePasswordUrlRequestInfo, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::test::TestFuture<GURL> callback;
  service()->FetchChangePasswordURL(GURL(k1ExampleURL), callback.GetCallback());

  std::move(fetch_result_callback).Run(fetch_result);
  base::test::RunUntil([&]() { return callback.IsReady(); });
  EXPECT_FALSE(mock_fetcher);
}

TEST_F(AffiliationServiceImplTest, OnMalformedResponseResetsFetcher) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  FakeAffiliationFetcher::FetchResult fetch_result;
  fetch_result.http_status_code = net::HTTP_OK;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(GURL(k1ExampleURL)),
                           kChangePasswordUrlRequestInfo, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::test::TestFuture<GURL> callback;
  service()->FetchChangePasswordURL(GURL(k1ExampleURL), callback.GetCallback());
  std::move(fetch_result_callback).Run(fetch_result);
  base::test::RunUntil([&]() { return callback.IsReady(); });
  EXPECT_FALSE(mock_fetcher);
}

TEST_F(AffiliationServiceImplTest,
       FetchChangePasswordURLWhenFetcherNotCreated) {
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(mock_fetcher_factory(), CreateInstance).WillOnce(Return(nullptr));

  service()->FetchChangePasswordURL(GURL(k1ExampleURL),
                                    completion_callback.GetCallback());
  base::test::RunUntil([&]() { return completion_callback.IsReady(); });
}

TEST_F(AffiliationServiceImplTest,
       EachPrefetchCallCreatesNewAffiliationFetcherInstance) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin1(k1ExampleURL);
  const GURL origin2(k2ExampleURL);

  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto new_mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin1), kChangePasswordUrlRequestInfo,
                           testing::_));
  EXPECT_CALL(*new_mock_fetcher,
              StartRequest(ToFacetsURIs(origin2), kChangePasswordUrlRequestInfo,
                           testing::_));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))))
      .WillOnce(Return(ByMove(std::move(new_mock_fetcher))));

  service()->FetchChangePasswordURL(origin1, base::DoNothing());
  service()->FetchChangePasswordURL(origin2, base::DoNothing());
}

TEST_F(AffiliationServiceImplTest, OnFetchSucceededFallsBackToMainDomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL))};
  group.facets.back().is_facet_synthesized = true;
  GroupedFacets main_domain_group;
  main_domain_group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://example.com"),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);
  test_result.groupings.push_back(main_domain_group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest, OnFetchSuccedeedRunsCallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::test::TestFuture<GURL> callback;
  service()->FetchChangePasswordURL(origin, callback.GetCallback());

  std::move(fetch_result_callback)
      .Run(AffiliationFetcherInterface::FetchResult());
  base::test::RunUntil([&]() { return callback.IsReady(); });
}

TEST_F(AffiliationServiceImplTest, SupportForMultipleRequests) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin1(k1ExampleURL);
  const GURL origin2(k2ExampleURL);

  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  auto new_mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      new_fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback_1;
  base::test::TestFuture<GURL> completion_callback_2;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origin1), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(*new_mock_fetcher,
              StartRequest(ToFacetsURIs(origin2), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&new_fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))))
      .WillOnce(Return(ByMove(std::move(new_mock_fetcher))));

  service()->FetchChangePasswordURL(origin1,
                                    completion_callback_1.GetCallback());
  service()->FetchChangePasswordURL(origin2,
                                    completion_callback_2.GetCallback());

  GroupedFacets group1;
  group1.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
                         FacetBrandingInfo(),
                         GURL(k1ExampleChangePasswordURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result1;
  test_result1.groupings.push_back(group1);
  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result1));
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL), completion_callback_1.Get());

  GroupedFacets group2;
  group2.facets = {Facet(FacetURI::FromPotentiallyInvalidSpec(k2ExampleURL),
                         FacetBrandingInfo(),
                         GURL(k2ExampleChangePasswordURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result2;
  test_result2.groupings.push_back(group2);
  std::move(new_fetch_result_callback)
      .Run(GetSuccessfulFetchResult(test_result2));
  EXPECT_EQ(GURL(k2ExampleChangePasswordURL), completion_callback_2.Get());
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
       GetAffiliationsAndBranding) {
  // This request is restricted to the cache, but cannot be served
  // from cache, thus it should fail.
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectFailure();
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // Now update cache to verify requests succeeds.
  service()->UpdateAffiliationsAndBranding(
      {FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)}, base::DoNothing());
  background_task_runner()->RunUntilIdle();
  fake_affiliation_api()->ServeNextRequest();
  background_task_runner()->RunUntilIdle();

  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  const auto equivalence_class_alpha(GetTestEquivalenceClassAlpha());
  mock_consumer()->ExpectSuccessWithResult(equivalence_class_alpha);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationServiceImplTestWithFetcherFactory,
       ShutdownWhileTasksArePosted) {
  service()->GetAffiliationsAndBranding(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
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

TEST_F(AffiliationServiceImplTest, FetchChangePasswordURLForAndroidApp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(kTestAndroidFacetURIBeta1);
  FacetURI android_facet =
      FacetURI::FromPotentiallyInvalidSpec(kTestAndroidFacetURIBeta1);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ElementsAre(android_facet),
                           kChangePasswordUrlRequestInfo, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(kTestAndroidFacetURIBeta1),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
            FacetBrandingInfo(), GURL(kOneExampleChangePasswordURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest, FetchChangePasswordURLForUrlWithPath) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  const GURL origin(kOneExampleChangePasswordURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  EXPECT_CALL(*mock_fetcher, StartRequest)
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec(kTestAndroidFacetURIBeta1),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL)),
      Facet(FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
            FacetBrandingInfo(), GURL(k1ExampleChangePasswordURL))};
  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest, FetchChangePasswordURLForDomainInEPSL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kFetchChangePasswordPatterns);

  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](AffiliationBackend* backend) {
            backend->GetAffiliationDatabaseForTesting().UpdatePslExtensions(
                {"example.com"});
          },
          service()->GetBackendForTesting()));

  base::test::TestFuture<std::vector<std::string>> epsl_callback;
  service()->GetPSLExtensions(epsl_callback.GetCallback());
  background_task_runner()->RunUntilIdle();
  EXPECT_THAT(epsl_callback.Take(), ElementsAre("example.com"));

  const GURL origin(kOneExampleURL);
  FacetURI facet = FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  // Verify that fetch is made only for the one.example.com.
  EXPECT_CALL(*mock_fetcher,
              StartRequest(ElementsAre(facet), kChangePasswordUrlRequestInfo,
                           testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());
}

TEST_F(AffiliationServiceImplTest, OnFetchSucceededMatchesPattern) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kFetchChangePasswordPatterns);

  const GURL origin("https://1.example.com/path1/login");
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  AffiliationFetcherInterface::RequestInfo expected_request_info =
      kChangePasswordUrlRequestInfo;
  expected_request_info.fetch_patterns = true;

  EXPECT_CALL(
      *mock_fetcher,
      StartRequest(
          ElementsAre(FacetURI::FromCanonicalSpec(k1ExampleURL),
                      FacetURI::FromCanonicalSpec("https://example.com")),
          expected_request_info, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://example.com"))};

  ChangePasswordPattern pattern;
  pattern.url_pattern_re2 = "^https://1\\.example\\.com/path1.*";
  pattern.change_password_url = GURL("https://matched.example.com");

  group.facets.back().change_password_patterns.push_back(pattern);

  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  base::test::RunUntil([&]() { return completion_callback.IsReady(); });

  EXPECT_EQ(GURL("https://matched.example.com"), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest,
       FetchChangePasswordURLRequestsOriginalAndMainDomainWhenPatternsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kFetchChangePasswordPatterns);

  const GURL origin("https://1.example.com/path1/login");
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  AffiliationFetcherInterface::RequestInfo expected_request_info =
      kChangePasswordUrlRequestInfo;
  expected_request_info.fetch_patterns = true;

  EXPECT_CALL(
      *mock_fetcher,
      StartRequest(
          ElementsAre(FacetURI::FromCanonicalSpec(k1ExampleURL),
                      FacetURI::FromCanonicalSpec("https://example.com")),
          expected_request_info, testing::_));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, base::DoNothing());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceededMatchesPatternWithSubdomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kFetchChangePasswordPatterns);

  const GURL origin("https://auth.example.com/path1/login");
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  AffiliationFetcherInterface::RequestInfo expected_request_info =
      kChangePasswordUrlRequestInfo;
  expected_request_info.fetch_patterns = true;

  EXPECT_CALL(
      *mock_fetcher,
      StartRequest(
          ElementsAre(FacetURI::FromCanonicalSpec("https://auth.example.com"),
                      FacetURI::FromCanonicalSpec("https://example.com")),
          expected_request_info, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://auth.example.com")),
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://example.com"))};

  ChangePasswordPattern pattern;
  pattern.url_pattern_re2 = "^https://([^/]+\\.)example\\.com/path1.*";
  pattern.change_password_url = GURL("https://matched.example.com");

  group.facets.back().change_password_patterns.push_back(pattern);

  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  base::test::RunUntil([&]() { return completion_callback.IsReady(); });

  EXPECT_EQ(GURL("https://matched.example.com"), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceededWhenRequestedFacetIsMainDomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kFetchChangePasswordPatterns);

  const GURL origin("https://example.com/path1/login");
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  AffiliationFetcherInterface::RequestInfo expected_request_info =
      kChangePasswordUrlRequestInfo;
  expected_request_info.fetch_patterns = true;

  FacetURI expected_facet = FacetURI::FromCanonicalSpec("https://example.com");

  EXPECT_CALL(*mock_fetcher, StartRequest(ElementsAre(expected_facet),
                                          expected_request_info, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  GroupedFacets group;
  group.facets = {
      Facet(FacetURI::FromPotentiallyInvalidSpec("https://example.com"))};

  ChangePasswordPattern pattern;
  pattern.url_pattern_re2 = "^https://example\\.com/path1.*";
  pattern.change_password_url = GURL("https://matched.example.com");

  group.facets.back().change_password_patterns.push_back(pattern);

  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  base::test::RunUntil([&]() { return completion_callback.IsReady(); });

  EXPECT_EQ(GURL("https://matched.example.com"), completion_callback.Get());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceededPrefersRequestedFacetPatternsOverMainDomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kFetchChangePasswordPatterns);

  // Simulates an ePSL domain (e.g. `1.example.com` where `example.com` is in
  // the server's ePSL) when the client's `psl_extension_list_` is empty or
  // stale: the server attaches patterns to `https://1.example.com`.
  const GURL origin("https://1.example.com/path1/login");
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  AffiliationFetcherInterface::RequestInfo expected_request_info =
      kChangePasswordUrlRequestInfo;
  expected_request_info.fetch_patterns = true;

  EXPECT_CALL(
      *mock_fetcher,
      StartRequest(
          ElementsAre(FacetURI::FromCanonicalSpec(k1ExampleURL),
                      FacetURI::FromCanonicalSpec("https://example.com")),
          expected_request_info, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  Facet requested_facet(FacetURI::FromCanonicalSpec(k1ExampleURL));
  ChangePasswordPattern requested_pattern;
  requested_pattern.url_pattern_re2 = "^https://1\\.example\\.com/path1.*";
  requested_pattern.change_password_url =
      GURL("https://1.example.com/change-password");
  requested_facet.change_password_patterns.push_back(requested_pattern);

  Facet main_domain_facet(FacetURI::FromCanonicalSpec("https://example.com"));
  ChangePasswordPattern main_domain_pattern;
  main_domain_pattern.url_pattern_re2 =
      "^https://([^/]+\\.)*example\\.com/path1.*";
  main_domain_pattern.change_password_url =
      GURL("https://example.com/change-password");
  main_domain_facet.change_password_patterns.push_back(main_domain_pattern);

  GroupedFacets group;
  group.facets = {std::move(requested_facet), std::move(main_domain_facet)};

  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  base::test::RunUntil([&]() { return completion_callback.IsReady(); });

  EXPECT_EQ(GURL("https://1.example.com/change-password"),
            completion_callback.Get());
}

TEST_F(
    AffiliationServiceImplTest,
    OnFetchSucceededDoesNotFallBackToMainDomainWhenRequestedFacetPatternMismatches) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kFetchChangePasswordPatterns);

  const GURL origin("https://1.example.com/path1/login");
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
      fetch_result_callback;
  base::test::TestFuture<GURL> completion_callback;

  AffiliationFetcherInterface::RequestInfo expected_request_info =
      kChangePasswordUrlRequestInfo;
  expected_request_info.fetch_patterns = true;

  EXPECT_CALL(
      *mock_fetcher,
      StartRequest(
          ElementsAre(FacetURI::FromCanonicalSpec(k1ExampleURL),
                      FacetURI::FromCanonicalSpec("https://example.com")),
          expected_request_info, testing::_))
      .WillOnce(testing::SaveArgByMove<2>(&fetch_result_callback));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->FetchChangePasswordURL(origin, completion_callback.GetCallback());

  Facet requested_facet(FacetURI::FromCanonicalSpec(k1ExampleURL));
  ChangePasswordPattern non_matching_pattern;
  non_matching_pattern.url_pattern_re2 =
      "^https://1\\.example\\.com/other_path.*";
  non_matching_pattern.change_password_url =
      GURL("https://1.example.com/change-password");
  requested_facet.change_password_patterns.push_back(non_matching_pattern);

  Facet main_domain_facet(FacetURI::FromCanonicalSpec("https://example.com"));
  ChangePasswordPattern main_domain_pattern;
  main_domain_pattern.url_pattern_re2 =
      "^https://([^/]+\\.)*example\\.com/path1.*";
  main_domain_pattern.change_password_url =
      GURL("https://example.com/change-password");
  main_domain_facet.change_password_patterns.push_back(main_domain_pattern);

  GroupedFacets group;
  group.facets = {std::move(requested_facet), std::move(main_domain_facet)};

  AffiliationFetcherInterface::ParsedFetchResponse test_result;
  test_result.groupings.push_back(group);

  std::move(fetch_result_callback).Run(GetSuccessfulFetchResult(test_result));
  base::test::RunUntil([&]() { return completion_callback.IsReady(); });

  EXPECT_EQ(GURL(), completion_callback.Get());
}

}  // namespace affiliations
