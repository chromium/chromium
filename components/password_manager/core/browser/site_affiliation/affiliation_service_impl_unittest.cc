// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/test_affiliation_fetcher_factory.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"
#include "components/sync/driver/test_sync_service.h"
#include "services/network/test/test_shared_url_loader_factory.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace password_manager {

namespace {

constexpr char kEmptyURL[] = "";
constexpr char k1ExampleURL[] = "https://1.example.com";
constexpr char k1ExampleChangePasswordURL[] =
    "https://1.example.com/.well-known/change-password";
constexpr char kM1ExampleURL[] = "https://m.1.example.com";
constexpr char kOneExampleURL[] = "https://one.example.com";
constexpr char kOneExampleChangePasswordURL[] =
    "https://one.example.com/settings/passwords";
constexpr char k2ExampleURL[] = "https://2.example.com";
constexpr char k3ExampleURL[] = "https://3.example.com";
constexpr char k4ExampleURL[] = "https://4.example.com";
constexpr char k5ExampleURL[] = "https://5.example.com";

std::vector<FacetURI> SchemeHostPortsToFacetsURIs(
    const std::vector<url::SchemeHostPort>& scheme_host_ports) {
  std::vector<FacetURI> facet_URIs;
  for (const auto& scheme_host_port : scheme_host_ports) {
    facet_URIs.push_back(
        FacetURI::FromCanonicalSpec(scheme_host_port.Serialize()));
  }
  return facet_URIs;
}

}  // namespace

class MockAffiliationFetcherFactory : public TestAffiliationFetcherFactory {
 public:
  MockAffiliationFetcherFactory() = default;
  ~MockAffiliationFetcherFactory() override = default;

  MOCK_METHOD(
      AffiliationFetcherInterface*,
      CreateInstance,
      (scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
       AffiliationFetcherDelegate* delegate),
      (override));
};

class AffiliationServiceImplTest : public testing::Test {
 public:
  AffiliationServiceImplTest()
      : sync_service_(std::make_unique<syncer::TestSyncService>()),
        service_(sync_service_.get(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    AffiliationFetcher::SetFactoryForTesting(mock_fetcher_factory());
  }

  ~AffiliationServiceImplTest() override {
    AffiliationFetcher::SetFactoryForTesting(nullptr);
  }

  syncer::TestSyncService* sync_service() { return sync_service_.get(); }
  AffiliationServiceImpl* service() { return &service_; }
  MockAffiliationFetcherFactory* mock_fetcher_factory() {
    return &mock_fetcher_factory_;
  }

  // Sets TestSyncService flags.
  void SetSyncServiceStates(bool is_setup_completed, bool is_passphrase_set) {
    sync_service()->SetFirstSetupComplete(is_setup_completed);
    sync_service()->SetIsUsingSecondaryPassphrase(is_passphrase_set);
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<syncer::TestSyncService> sync_service_;
  AffiliationServiceImpl service_;
  MockAffiliationFetcherFactory mock_fetcher_factory_;
};

TEST_F(AffiliationServiceImplTest, GetChangePasswordURLReturnsEmpty) {
  EXPECT_EQ(GURL(), service()->GetChangePasswordURL(
                        url::SchemeHostPort(GURL(k1ExampleURL))));
}

TEST_F(AffiliationServiceImplTest, ClearStopsOngoingAffiliationFetcherRequest) {
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();
  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));

  const std::vector<url::SchemeHostPort> tuple_origins = {
      url::SchemeHostPort(GURL(k1ExampleURL)),
      url::SchemeHostPort(GURL(k2ExampleURL))};
  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));

  service()->PrefetchChangePasswordURLs(tuple_origins);
  EXPECT_NE(nullptr, service()->GetFetcherForTesting());

  service()->Clear();
  EXPECT_EQ(nullptr, service()->GetFetcherForTesting());
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSuccededInsertsChangePasswordURLOfRequestedSiteIfFound) {
  const url::SchemeHostPort scheme_host_port{GURL(k1ExampleURL)};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs({scheme_host_port}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));

  service()->PrefetchChangePasswordURLs({scheme_host_port});

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL(kEmptyURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
       .change_password_url = GURL(kOneExampleChangePasswordURL)}};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);
  AffiliationFetcherDelegate* service_delegate = service();
  service_delegate->OnFetchSucceeded(std::move(test_result));

  // Expect Change Password URL of requested site.
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL),
            service()->GetChangePasswordURL(scheme_host_port));
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSuccededInsertsChangePasswordURLOfAnotherSiteFromAGroup) {
  const url::SchemeHostPort scheme_host_port{GURL(kM1ExampleURL)};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs({scheme_host_port}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));

  service()->PrefetchChangePasswordURLs({scheme_host_port});

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL(k1ExampleChangePasswordURL)},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL(kEmptyURL)}};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);
  AffiliationFetcherDelegate* service_delegate = service();
  service_delegate->OnFetchSucceeded(std::move(test_result));

  // Expect Change Password URL of another site from a grouping.
  EXPECT_EQ(GURL(k1ExampleChangePasswordURL),
            service()->GetChangePasswordURL(scheme_host_port));
}

TEST_F(AffiliationServiceImplTest,
       OnFetchSucceedTakesNoActionWhenNoChangePasswordURLsAvailable) {
  const url::SchemeHostPort scheme_host_port{GURL(k1ExampleURL)};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();

  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs({scheme_host_port}),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));
  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));

  service()->PrefetchChangePasswordURLs({scheme_host_port});

  const GroupedFacets group = {
      {.uri = FacetURI::FromPotentiallyInvalidSpec(k1ExampleURL),
       .change_password_url = GURL()},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kM1ExampleURL),
       .change_password_url = GURL()},
      {.uri = FacetURI::FromPotentiallyInvalidSpec(kOneExampleURL),
       .change_password_url = GURL()}};
  auto test_result = std::make_unique<AffiliationFetcherDelegate::Result>();
  test_result->groupings.push_back(group);
  AffiliationFetcherDelegate* service_delegate = service();
  service_delegate->OnFetchSucceeded(std::move(test_result));

  EXPECT_EQ(GURL(), service()->GetChangePasswordURL(scheme_host_port));
}

TEST_F(AffiliationServiceImplTest, OnFetchFailedResetsFetcher) {
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();
  AffiliationFetcherDelegate* service_delegate = service();

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));

  std::vector<url::SchemeHostPort> tuple_origins = {
      url::SchemeHostPort(GURL(k1ExampleURL)),
      url::SchemeHostPort(GURL(k2ExampleURL))};
  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));

  service()->PrefetchChangePasswordURLs(tuple_origins);
  EXPECT_NE(nullptr, service()->GetFetcherForTesting());

  service_delegate->OnFetchFailed();
  EXPECT_EQ(nullptr, service()->GetFetcherForTesting());
}

TEST_F(AffiliationServiceImplTest, OnMalformedResponseResetsFetcher) {
  std::vector<url::SchemeHostPort> tuple_origins = {
      url::SchemeHostPort(GURL(k1ExampleURL)),
      url::SchemeHostPort(GURL(k2ExampleURL))};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();
  AffiliationFetcherDelegate* service_delegate = service();

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));
  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));

  service()->PrefetchChangePasswordURLs(tuple_origins);
  EXPECT_NE(nullptr, service()->GetFetcherForTesting());

  service_delegate->OnMalformedResponse();
  EXPECT_EQ(nullptr, service()->GetFetcherForTesting());
}

TEST_F(AffiliationServiceImplTest,
       EachPrefetchCallCreatesNewAffiliationFetcherInstance) {
  const url::SchemeHostPort scheme_host_port1{GURL(k1ExampleURL)};
  const url::SchemeHostPort scheme_host_port2{GURL(k2ExampleURL)};
  const url::SchemeHostPort scheme_host_port3{GURL(k3ExampleURL)};
  const url::SchemeHostPort scheme_host_port4{GURL(k4ExampleURL)};
  const url::SchemeHostPort scheme_host_port5{GURL(k5ExampleURL)};

  const std::vector<url::SchemeHostPort> tuple_origins_1 = {
      scheme_host_port1, scheme_host_port2, scheme_host_port3};
  const std::vector<url::SchemeHostPort> tuple_origins_2 = {
      scheme_host_port3, scheme_host_port4, scheme_host_port5};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();
  MockAffiliationFetcher* new_mock_fetcher = new MockAffiliationFetcher();
  AffiliationFetcher::RequestInfo request_info{.change_password_info = true};

  EXPECT_CALL(
      *mock_fetcher,
      StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins_1), request_info));
  EXPECT_CALL(
      *new_mock_fetcher,
      StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins_2), request_info));
  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher))
      .WillOnce(Return(new_mock_fetcher));

  service()->PrefetchChangePasswordURLs(tuple_origins_1);
  service()->PrefetchChangePasswordURLs(tuple_origins_2);
}

TEST_F(AffiliationServiceImplTest,
       FetchRequiresCompleteSetupAndPassphraseDisabled) {
  // The only scenario when the StartRequest() should be called.
  // Setup is completed and secondary passphrase feature is disabled.
  SetSyncServiceStates(/*is_setup_completed=*/true,
                       /*is_passphrase_set=*/false);

  const std::vector<url::SchemeHostPort> tuple_origins = {
      url::SchemeHostPort(GURL(k1ExampleURL)),
      url::SchemeHostPort(GURL(k2ExampleURL))};
  MockAffiliationFetcher* mock_fetcher = new MockAffiliationFetcher();

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance)
      .WillOnce(Return(mock_fetcher));
  EXPECT_CALL(*mock_fetcher,
              StartRequest(SchemeHostPortsToFacetsURIs(tuple_origins),
                           AffiliationFetcherInterface::RequestInfo{
                               .change_password_info = true}));

  service()->PrefetchChangePasswordURLs(tuple_origins);
}

TEST_F(AffiliationServiceImplTest, SecondaryPassphraseSetPreventsFetch) {
  SetSyncServiceStates(/*is_setup_completed=*/true, /*is_passphrase_set=*/true);

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance).Times(0);

  service()->PrefetchChangePasswordURLs(
      {url::SchemeHostPort(GURL(k1ExampleURL)),
       url::SchemeHostPort(GURL(k2ExampleURL))});
}

TEST_F(AffiliationServiceImplTest, SetupNotCompletedPreventsFetch) {
  SetSyncServiceStates(/*is_setup_completed=*/false,
                       /*is_passphrase_set=*/false);

  EXPECT_CALL(*mock_fetcher_factory(), CreateInstance).Times(0);

  service()->PrefetchChangePasswordURLs(
      {url::SchemeHostPort(GURL(k1ExampleURL)),
       url::SchemeHostPort(GURL(k2ExampleURL))});
}

}  // namespace password_manager
