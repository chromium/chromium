// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "components/password_manager/core/browser/site_affiliation/mock_affiliation_fetcher_factory.h"
#include "components/sync/driver/test_sync_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

namespace password_manager {

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
      : sync_service_(std::make_unique<syncer::TestSyncService>()),
        service_(sync_service_.get(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    auto fetcher_factory = std::make_unique<MockAffiliationFetcherFactory>();
    mock_fetcher_factory_ = fetcher_factory.get();
    service_.SetFetcherFactoryForTesting(std::move(fetcher_factory));
  }

  ~AffiliationServiceImplTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  syncer::TestSyncService* sync_service() { return sync_service_.get(); }
  AffiliationServiceImpl* service() { return &service_; }
  MockAffiliationFetcherFactory& mock_fetcher_factory() {
    return *mock_fetcher_factory_;
  }

  // Sets TestSyncService flags.
  void SetSyncServiceStates(bool is_setup_completed, bool is_passphrase_set) {
    sync_service()->SetFirstSetupComplete(is_setup_completed);
    sync_service()->SetIsUsingSecondaryPassphrase(is_passphrase_set);
  }

 private:
  base::test::TaskEnvironment task_env_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  AffiliationServiceImpl service_;
  MockAffiliationFetcherFactory* mock_fetcher_factory_;
};

TEST_F(AffiliationServiceImplTest, GetChangePasswordURLReturnsEmpty) {
  EXPECT_EQ(GURL(), service()->GetChangePasswordURL(GURL(k1ExampleURL)));
}

TEST_F(AffiliationServiceImplTest, ClearStopsOngoingRequest) {
  const std::vector<GURL> origins = {GURL(k1ExampleURL), GURL(k2ExampleURL)};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
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

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL()},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
       .change_password_url = GURL(kOneExampleChangePasswordURL)}};
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

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL()}};
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

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL()},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL()},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
       .change_password_url = GURL()}};
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

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
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

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  base::MockOnceClosure callback;
  service()->PrefetchChangePasswordURLs(origins, callback.Get());

  EXPECT_CALL(callback, Run());
  static_cast<AffiliationFetcherDelegate*>(service())->OnMalformedResponse(
      raw_mock_fetcher);
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
  AffiliationFetcher::RequestInfo request_info{.change_password_info = true};

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origins_1), request_info));
  EXPECT_CALL(*new_mock_fetcher,
              StartRequest(ToFacetsURIs(origins_2), request_info));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))))
      .WillOnce(Return(ByMove(std::move(new_mock_fetcher))));

  service()->PrefetchChangePasswordURLs(origins_1, base::DoNothing());
  service()->PrefetchChangePasswordURLs(origins_2, base::DoNothing());
}

TEST_F(AffiliationServiceImplTest,
       FetchRequiresCompleteSetupAndPassphraseDisabled) {
  // The only scenario when the StartRequest() should be called.
  // Setup is completed and secondary passphrase feature is disabled.
  SetSyncServiceStates(/*is_setup_completed=*/true,
                       /*is_passphrase_set=*/false);

  const std::vector<GURL> origins = {GURL(k1ExampleURL), GURL(k2ExampleURL)};
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs(origins, base::DoNothing());
}

TEST_F(AffiliationServiceImplTest, SecondaryPassphraseSetPreventsFetch) {
  SetSyncServiceStates(/*is_setup_completed=*/true, /*is_passphrase_set=*/true);

  EXPECT_CALL(mock_fetcher_factory(), CreateInstance).Times(0);

  service()->PrefetchChangePasswordURLs(
      {GURL(k1ExampleURL), GURL(k2ExampleURL)}, base::DoNothing());
}

TEST_F(AffiliationServiceImplTest, SetupNotCompletedPreventsFetch) {
  SetSyncServiceStates(/*is_setup_completed=*/false,
                       /*is_passphrase_set=*/false);

  EXPECT_CALL(mock_fetcher_factory(), CreateInstance).Times(0);

  service()->PrefetchChangePasswordURLs(
      {GURL(k1ExampleURL), GURL(k2ExampleURL)}, base::DoNothing());
}

// Below are the tests veryfing recorded metrics for
// PasswordManager.AffiliationService.GetChangePasswordUsage.

TEST_F(AffiliationServiceImplTest, NotFetchedYetMetricIfWaitingForResponse) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());
  service()->GetChangePasswordURL(origin);

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      metrics_util::GetChangePasswordUrlMetric::kNotFetchedYet, 1);
}

TEST_F(AffiliationServiceImplTest, NoUrlOverrideAvailableMetric) {
  service()->GetChangePasswordURL(GURL(k1ExampleURL));

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      metrics_util::GetChangePasswordUrlMetric::kNoUrlOverrideAvailable, 1);
}

TEST_F(AffiliationServiceImplTest, FoundForRequestedFacetMetric) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
       .change_password_url = GURL(kOneExampleChangePasswordURL)}};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);

  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));
  service()->GetChangePasswordURL(origin);

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      metrics_util::GetChangePasswordUrlMetric::kUrlOverrideUsed, 1);
}

TEST_F(AffiliationServiceImplTest, FoundForGroupedFacetMetric) {
  const GURL origin(kM1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))));

  service()->PrefetchChangePasswordURLs({origin}, base::DoNothing());

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL()}};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);

  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result));
  service()->GetChangePasswordURL(origin);

  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordURLMetricName,
      metrics_util::GetChangePasswordUrlMetric::kGroupUrlOverrideUsed, 1);
}

TEST_F(AffiliationServiceImplTest, OnFetchSuccedeedRunsCallback) {
  const GURL origin(k1ExampleURL);
  auto mock_fetcher = std::make_unique<MockAffiliationFetcher>();
  auto* raw_mock_fetcher = mock_fetcher.get();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs({origin}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
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

  AffiliationFetcher::RequestInfo request_info{.change_password_info = true};

  EXPECT_CALL(*mock_fetcher,
              StartRequest(ToFacetsURIs(origins_1), request_info));
  EXPECT_CALL(*new_mock_fetcher,
              StartRequest(ToFacetsURIs(origins_2), request_info));
  EXPECT_CALL(mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(ByMove(std::move(mock_fetcher))))
      .WillOnce(Return(ByMove(std::move(new_mock_fetcher))));

  service()->PrefetchChangePasswordURLs(origins_1, base::DoNothing());
  service()->PrefetchChangePasswordURLs(origins_2, base::DoNothing());

  const GroupedFacets group1 = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)}};
  auto test_result1 = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result1->groupings.push_back(group1);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      raw_mock_fetcher, std::move(test_result1));
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL),
            service()->GetChangePasswordURL(origin1));

  const GroupedFacets group2 = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k2ExampleURL),
       .change_password_url = GURL(k2ExampleChangePasswordURL)}};
  auto test_result2 = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result2->groupings.push_back(group2);
  static_cast<AffiliationFetcherDelegate*>(service())->OnFetchSucceeded(
      new_raw_mock_fetcher, std::move(test_result2));
  EXPECT_EQ(GURL(k2ExampleChangePasswordURL),
            service()->GetChangePasswordURL(origin2));
}

}  // namespace password_manager
