// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_source_adapter.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/mock_affiliation_source.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace plus_addresses {
namespace {
using ::affiliations::FacetURI;
using ::affiliations::MockAffiliationSourceObserver;
using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAreArray;
}  // namespace

class PlusAddressAffiliationSourceAdapterTest : public testing::Test {
 protected:
  PlusAddressAffiliationSourceAdapterTest() {
    service_ = std::make_unique<PlusAddressService>(
        /*identity_manager=*/nullptr,
        /*pref_service=*/nullptr,
        std::make_unique<testing::NiceMock<MockPlusAddressHttpClient>>(),
        /*webdata_service=*/nullptr);
    adapter_ =
        std::make_unique<PlusAddressAffiliationSourceAdapter>(service_.get());
  }

  testing::AssertionResult ExpectAdapterToReturnFacets(
      const std::vector<FacetURI>& expected_facets) {
    base::MockCallback<affiliations::AffiliationSource::ResultCallback>
        callback;
    int calls = 0;
    ON_CALL(callback, Run(UnorderedElementsAreArray(expected_facets)))
        .WillByDefault([&] { ++calls; });
    adapter_->GetFacets(callback.Get());
    RunUntilIdle();
    return calls == 1 ? AssertionSuccess()
                      : (AssertionFailure() << "Error fetching facets.");
  }

  affiliations::MockAffiliationSourceObserver* mock_source_observer() {
    return &mock_source_observer_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::StrictMock<MockAffiliationSourceObserver> mock_source_observer_;
  std::unique_ptr<PlusAddressAffiliationSourceAdapter> adapter_;
  std::unique_ptr<PlusAddressService> service_;
};

// Verifies that no facets are returned when no plus addresses are registered.
TEST_F(PlusAddressAffiliationSourceAdapterTest, TestGetFacetsEmpty) {
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
}

// Verifies that facets for plus addresses are available via GetFacets.
TEST_F(PlusAddressAffiliationSourceAdapterTest, TestGetFacets) {
  const PlusProfile profile1 =
      test::CreatePlusProfile(/*use_full_domain=*/true);
  const PlusProfile profile2 =
      test::CreatePlusProfile2(/*use_full_domain=*/true);

  service_->OnWebDataChangedBySync(
      {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile1),
       PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile2)});

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec("https://foo.com"),
       FacetURI::FromCanonicalSpec("https://bar.com")}));
}

// Verifies that updates (e.g. add or remove) of valid facets are communicated
// to the affiliation source observer.
TEST_F(PlusAddressAffiliationSourceAdapterTest, OnPlusAddressesChanged) {
  PlusProfile profile1 = test::CreatePlusProfile(/*use_full_domain=*/true);
  PlusProfile profile2 = test::CreatePlusProfile2(/*use_full_domain=*/true);
  service_->OnWebDataChangedBySync(
      {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile1)});

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec("https://foo.com")}));
  adapter_->StartObserving(mock_source_observer());

  // Assert that `add` operations are reported before `remove` operation for
  // similar facets.
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*mock_source_observer(),
                OnFacetsAdded(ElementsAre(
                    FacetURI::FromCanonicalSpec("https://foo.com"),
                    FacetURI::FromCanonicalSpec("https://bar.com"))));
    EXPECT_CALL(*mock_source_observer(),
                OnFacetsRemoved(ElementsAre(
                    FacetURI::FromCanonicalSpec("https://foo.com"))));
  }

  // Simulate update of `profile1`.
  PlusProfile updated_profile1 = profile1;
  updated_profile1.plus_address = "new-" + updated_profile1.plus_address;

  service_->OnWebDataChangedBySync(
      {PlusAddressDataChange(PlusAddressDataChange::Type::kRemove, profile1),
       PlusAddressDataChange(PlusAddressDataChange::Type::kAdd,
                             updated_profile1),
       PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile2)});
}

// Verifies that the adapter keeps functioning if the service is destroyed.
TEST_F(PlusAddressAffiliationSourceAdapterTest,
       TestPlusAddressServiceDestroyed) {
  const PlusProfile profile1 =
      test::CreatePlusProfile(/*use_full_domain=*/true);
  const PlusProfile profile2 =
      test::CreatePlusProfile2(/*use_full_domain=*/true);

  service_->OnWebDataChangedBySync(
      {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile1),
       PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile2)});

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec("https://foo.com"),
       FacetURI::FromCanonicalSpec("https://bar.com")}));

  service_.reset();
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
}

}  // namespace plus_addresses
