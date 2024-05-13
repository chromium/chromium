// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {
using ::affiliations::FacetURI;
using ::affiliations::MockAffiliationService;
using ::base::test::RunOnceCallback;
using ::testing::UnorderedElementsAreArray;

constexpr const char kAffiliatedAndroidApp[] =
    "android://"
    "5Z0D_o6B8BqileZyWhXmqO_wkO8uO0etCEXvMn5tUzEqkWUgfTSjMcTM7eMMTY_"
    "FGJC9RlpRNt_8Qp5tgDocXw==@com.bambuna.podcastaddict/";
}  // namespace

class PlusAddressAffiliationMatchHelperTest : public testing::Test {
 public:
  PlusAddressAffiliationMatchHelperTest() {
    plus_address_service_ = std::make_unique<PlusAddressService>(
        /*identity_manager=*/nullptr,
        /*pref_service=*/nullptr,
        std::make_unique<testing::NiceMock<MockPlusAddressHttpClient>>(),
        /*webdata_service=*/nullptr);

    match_helper_ = std::make_unique<PlusAddressAffiliationMatchHelper>(
        plus_address_service(), mock_affiliation_service());
  }

  testing::AssertionResult ExpectMatchHelperToReturnProfiles(
      const FacetURI& requested_facet,
      const std::vector<PlusProfile>& expected_profiles) {
    base::MockCallback<
        PlusAddressAffiliationMatchHelper::AffiliatedPlusProfilesCallback>
        callback;
    int calls = 0;
    ON_CALL(callback, Run(UnorderedElementsAreArray(expected_profiles)))
        .WillByDefault([&] { ++calls; });
    match_helper()->GetAffiliatedPlusProfiles(requested_facet, callback.Get());
    return calls == 1
               ? testing::AssertionSuccess()
               : (testing::AssertionFailure() << "Error fetching profiles.");
  }

  void SaveProfiles(const std::vector<PlusProfile>& profiles) {
    std::vector<PlusAddressDataChange> changes;
    for (const PlusProfile& profile : profiles) {
      changes.push_back(
          PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile));
    }
    plus_address_service()->OnWebDataChangedBySync(changes);
  }

  MockAffiliationService* mock_affiliation_service() {
    return &mock_affiliation_service_;
  }

  PlusAddressService* plus_address_service() {
    return plus_address_service_.get();
  }

  PlusAddressAffiliationMatchHelper* match_helper() {
    return match_helper_.get();
  }

 private:
  testing::StrictMock<MockAffiliationService> mock_affiliation_service_;
  std::unique_ptr<PlusAddressService> plus_address_service_;
  std::unique_ptr<PlusAddressAffiliationMatchHelper> match_helper_;
  base::test::ScopedFeatureList features_{features::kPlusAddressAffiliations};
};

// Verifies that PSL extensions are cached within the match helper and a single
// affiliation service call is issued.
TEST_F(PlusAddressAffiliationMatchHelperTest, GetPSLExtensionsCachesResult) {
  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(pls_extensions));

  base::MockCallback<PlusAddressAffiliationMatchHelper::PSLExtensionCallback>
      callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAreArray(pls_extensions)));

  match_helper()->GetPSLExtensions(callback.Get());

  // Now affiliation service isn't called.
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions).Times(0);
  EXPECT_CALL(callback, Run(UnorderedElementsAreArray(pls_extensions)));
  match_helper()->GetPSLExtensions(callback.Get());
}

// Verifies that the match helper correctly handles simultaneous fetch requests
// for PSL extensions.
TEST_F(PlusAddressAffiliationMatchHelperTest,
       GetPSLExtensionsHandlesSimultaneousCalls) {
  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  base::OnceCallback<void(std::vector<std::string>)> first_callback;

  base::MockCallback<PlusAddressAffiliationMatchHelper::PSLExtensionCallback>
      callback1;
  base::MockCallback<PlusAddressAffiliationMatchHelper::PSLExtensionCallback>
      callback2;
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
        // Store the callback and wait until all the GetPSLExtensions calls are
        // made.
        .WillOnce(MoveArg<0>(&first_callback));

    EXPECT_CALL(callback1, Run(UnorderedElementsAreArray(pls_extensions)));
    EXPECT_CALL(callback2, Run(UnorderedElementsAreArray(pls_extensions)));
  }

  match_helper()->GetPSLExtensions(callback1.Get());
  match_helper()->GetPSLExtensions(callback2.Get());

  // After all `GetPSLExtensions` are made, resolve the affiliation service
  // callback and make sure all the calls are resolved.
  std::move(first_callback).Run(pls_extensions);
}

// Verifies that the list of returned values is empty if no profiles are stored.
TEST_F(PlusAddressAffiliationMatchHelperTest, EmptyResult) {
  FacetURI facet = FacetURI::FromCanonicalSpec("https://example.com");

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(facet);
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(facet, {}));
}

// Verifies that exact and PSL matches (respecting the PSL extensions list) are
// returned.
TEST_F(PlusAddressAffiliationMatchHelperTest, ExactAndPslMatchesTest) {
  PlusProfile profile1 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://one.foo.example.com"));
  PlusProfile profile2 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://two.foo.example.com"));
  PlusProfile profile3 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec(kAffiliatedAndroidApp));
  PlusProfile profile4 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://bar.example.com"));

  SaveProfiles({profile1, profile2, profile3, profile4});
  ASSERT_THAT(
      plus_address_service()->GetPlusProfiles(),
      UnorderedElementsAreArray({profile1, profile2, profile3, profile4}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{"example.com"}));
  // Simulate no grouping affiliations, only the requested facet is returned.
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(profile1.facet));
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // `profile3` is not a PSL match because it is an Android facet.
  // `profile4` is not a match due to the PSL extension list exception.
  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(
      absl::get<FacetURI>(profile1.facet), {profile1, profile2}));
}

// Verifies that group affiliation matches are returned.
TEST_F(PlusAddressAffiliationMatchHelperTest, GroupedMatchesTest) {
  PlusProfile profile1 = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://example.com"));
  PlusProfile profile2 = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://example2.com"));
  PlusProfile android_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec(kAffiliatedAndroidApp));
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));

  SaveProfiles({profile1, profile2, android_profile, group_profile});
  ASSERT_THAT(plus_address_service()->GetPlusProfiles(),
              UnorderedElementsAreArray(
                  {profile1, profile2, android_profile, group_profile}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(profile1.facet));
  group.facets.emplace_back(absl::get<FacetURI>(android_profile.facet));
  group.facets.emplace_back(absl::get<FacetURI>(group_profile.facet));
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // `profile2` was not a PSL match nor group affiliated.
  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(
      absl::get<FacetURI>(profile1.facet),
      {profile1, android_profile, group_profile}));
}

// Verifies that elements in both group and PSL matches matches are returned
// only once.
TEST_F(PlusAddressAffiliationMatchHelperTest,
       GroupedAndPSLMatchesIntersectTest) {
  PlusProfile profile1 = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://example.com"));
  PlusProfile psl_match = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://foo.example.com"));
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));

  SaveProfiles({profile1, psl_match, group_profile});
  ASSERT_THAT(
      plus_address_service()->GetPlusProfiles(),
      testing::UnorderedElementsAreArray({profile1, psl_match, group_profile}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(profile1.facet));
  group.facets.emplace_back(absl::get<FacetURI>(psl_match.facet));
  group.facets.emplace_back(absl::get<FacetURI>(group_profile.facet));
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // `psl_match` is part of both group and PSL matches but must be returned only
  // once.
  EXPECT_TRUE(
      ExpectMatchHelperToReturnProfiles(absl::get<FacetURI>(profile1.facet),
                                        {profile1, psl_match, group_profile}));
}

}  // namespace plus_addresses
