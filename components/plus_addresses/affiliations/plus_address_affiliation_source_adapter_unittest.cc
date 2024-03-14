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
  PlusAddressAffiliationSourceAdapterTest()
      : service_(
            /*identity_manager=*/nullptr,
            /*pref_service=*/nullptr,
            std::make_unique<testing::NiceMock<MockPlusAddressHttpClient>>(),
            /*webdata_service=*/nullptr),
        adapter_(std::make_unique<PlusAddressAffiliationSourceAdapter>(
            &service_,
            &mock_source_observer_)) {}

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

  PlusAddressService& service() { return service_; }

  affiliations::MockAffiliationSourceObserver& mock_source_observer() {
    return mock_source_observer_;
  }

  PlusAddressAffiliationSourceAdapter* adapter() { return adapter_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  PlusAddressService service_;
  testing::StrictMock<MockAffiliationSourceObserver> mock_source_observer_;
  std::unique_ptr<PlusAddressAffiliationSourceAdapter> adapter_;
};

// Verifies that no facets are returned when no plus addresses are registered.
TEST_F(PlusAddressAffiliationSourceAdapterTest, TestGetFacetsEmpty) {
  EXPECT_TRUE(ExpectAdapterToReturnFacets({}));
}

// Verifies that facets for plus addresses are available via GetFacets.
TEST_F(PlusAddressAffiliationSourceAdapterTest, TestGetFacets) {
  service().SavePlusAddress(url::Origin::Create(GURL("https://foo.com")),
                            "plus+foo@plus.plus");
  service().SavePlusAddress(url::Origin::Create(GURL("https://bar.com")),
                            "plus+bar@plus.plus");

  EXPECT_TRUE(ExpectAdapterToReturnFacets(
      {FacetURI::FromCanonicalSpec("https://foo.com"),
       FacetURI::FromCanonicalSpec("https://bar.com")}));
}

}  // namespace plus_addresses
