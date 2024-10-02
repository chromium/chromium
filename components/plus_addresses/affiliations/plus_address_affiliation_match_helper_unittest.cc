// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_service_impl.h"
#include "components/plus_addresses/plus_address_test_environment.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
namespace {

using ::affiliations::FacetURI;
using ::affiliations::MockAffiliationService;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAreArray;

constexpr const char kAffiliatedAndroidApp[] =
    "android://"
    "5Z0D_o6B8BqileZyWhXmqO_wkO8uO0etCEXvMn5tUzEqkWUgfTSjMcTM7eMMTY_"
    "FGJC9RlpRNt_8Qp5tgDocXw==@com.bambuna.podcastaddict/";
constexpr char kUmaKeyResponseTime[] =
    "PlusAddresses.AffiliationRequest.ResponseTime";

class PlusAddressAffiliationMatchHelperTest : public testing::Test {
 public:
  PlusAddressAffiliationMatchHelperTest() {
    plus_address_service_ = std::make_unique<PlusAddressServiceImpl>(
        &plus_environment_.pref_service(),
        plus_environment_.identity_env().identity_manager(),
        &plus_environment_.setting_service(),
        std::make_unique<NiceMock<MockPlusAddressHttpClient>>(),
        /*webdata_service=*/nullptr,
        /*affiliation_service=*/mock_affiliation_service(),
        /*feature_enabled_for_profile_check=*/
        base::BindRepeating(&base::FeatureList::IsEnabled));

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
      changes.emplace_back(PlusAddressDataChange::Type::kAdd, profile);
    }
    plus_address_service()->OnWebDataChangedBySync(changes);
  }

  MockAffiliationService* mock_affiliation_service() {
    return &plus_environment_.affiliation_service();
  }

  PlusAddressServiceImpl* plus_address_service() {
    return plus_address_service_.get();
  }

  PlusAddressAffiliationMatchHelper* match_helper() {
    return match_helper_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::PlusAddressTestEnvironment plus_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<PlusAddressServiceImpl> plus_address_service_;
  std::unique_ptr<PlusAddressAffiliationMatchHelper> match_helper_;
};

// Verifies that PSL extensions are cached within the match helper and a single
// affiliation service call is issued.
TEST_F(PlusAddressAffiliationMatchHelperTest, GetPSLExtensionsCachesResult) {
  FacetURI facet = FacetURI::FromCanonicalSpec("https://example.com");
  std::vector<std::string> pls_extensions = {"a.com", "b.com"};

  affiliations::GroupedFacets group;
  group.facets.emplace_back(facet);
  ON_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillByDefault(RunOnceCallbackRepeatedly<1>(
          std::vector<affiliations::GroupedFacets>{group}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(pls_extensions));
  base::MockCallback<
      PlusAddressAffiliationMatchHelper::AffiliatedPlusProfilesCallback>
      callback;
  match_helper()->GetAffiliatedPlusProfiles(facet, callback.Get());

  // Now affiliation service isn't called.
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions).Times(0);
  match_helper()->GetAffiliatedPlusProfiles(facet, callback.Get());
}

// Verifies that the match helper correctly handles simultaneous fetch requests
// for PSL extensions.
TEST_F(PlusAddressAffiliationMatchHelperTest,
       GetPSLExtensionsHandlesSimultaneousCalls) {
  FacetURI facet = FacetURI::FromCanonicalSpec("https://example.com");
  affiliations::GroupedFacets group;
  group.facets.emplace_back(facet);
  ON_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillByDefault(RunOnceCallbackRepeatedly<1>(
          std::vector<affiliations::GroupedFacets>{group}));

  std::vector<std::string> pls_extensions = {"a.com", "b.com"};
  base::OnceCallback<void(std::vector<std::string>)> first_callback;

  base::MockCallback<
      PlusAddressAffiliationMatchHelper::AffiliatedPlusProfilesCallback>
      callback1;
  base::MockCallback<
      PlusAddressAffiliationMatchHelper::AffiliatedPlusProfilesCallback>
      callback2;
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
        // Store the callback and wait until all the GetPSLExtensions calls are
        // made.
        .WillOnce(MoveArg<0>(&first_callback));

    EXPECT_CALL(callback1, Run);
    EXPECT_CALL(callback2, Run);
  }

  match_helper()->GetAffiliatedPlusProfiles(facet, callback1.Get());
  match_helper()->GetAffiliatedPlusProfiles(facet, callback2.Get());

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
  histogram_tester().ExpectTotalCount(kUmaKeyResponseTime, 1u);
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
  group.facets.emplace_back(profile1.facet);
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // `profile3` is not a PSL match because it is an Android facet.
  // `profile4` is not a match due to the PSL extension list exception.
  EXPECT_TRUE(
      ExpectMatchHelperToReturnProfiles(profile1.facet, {profile1, profile2}));
  histogram_tester().ExpectTotalCount(kUmaKeyResponseTime, 1u);
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
  group.facets.emplace_back(profile1.facet);
  group.facets.emplace_back(android_profile.facet);
  group.facets.emplace_back(group_profile.facet);
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // `profile2` was not a PSL match nor group affiliated.
  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(
      profile1.facet, {profile1, android_profile, group_profile}));
}

// Verifies that elements in both group and PSL matches are returned only once.
TEST_F(PlusAddressAffiliationMatchHelperTest,
       GroupedAndPSLMatchesIntersectTest) {
  PlusProfile profile1 = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://example.com"));
  PlusProfile psl_match = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://foo.example.com"));
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));

  SaveProfiles({profile1, psl_match, group_profile});
  ASSERT_THAT(plus_address_service()->GetPlusProfiles(),
              UnorderedElementsAreArray({profile1, psl_match, group_profile}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(profile1.facet);
  group.facets.emplace_back(psl_match.facet);
  group.facets.emplace_back(group_profile.facet);
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // `psl_match` is part of both group and PSL matches but must be returned only
  // once.
  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(
      profile1.facet, {profile1, psl_match, group_profile}));
}

// Verifies that facets that PSL-match entries in the affiliation group are
// returned.
TEST_F(PlusAddressAffiliationMatchHelperTest, GroupePSLMatchesTest) {
  PlusProfile requested_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://example.com"));
  PlusProfile psl_match = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://foo.example.com"));
  PlusProfile group_psl_match = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.foo.com"));

  SaveProfiles({psl_match, group_psl_match});
  ASSERT_THAT(plus_address_service()->GetPlusProfiles(),
              UnorderedElementsAreArray({psl_match, group_psl_match}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(requested_profile.facet);
  // The `group_psl_match` matches the following entry in the affiliation group.
  group.facets.emplace_back(FacetURI::FromCanonicalSpec("https://foo.com"));
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // The expected results:
  // * `psl_match` is a PSL match with the requested facet. It is *not* part of
  // group results.
  // * `group_psl_match` is a PSL match with an entry in the affiliation group.
  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(requested_profile.facet,
                                                {psl_match, group_psl_match}));
}

// Verifies a case where affiliations are requested on an http domain, and
// client has existing plus addresses for PSL matched (https) domains.
TEST_F(PlusAddressAffiliationMatchHelperTest, SupportHttpPSLMatchedDomains) {
  PlusProfile http_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("http://example.com"));
  PlusProfile https_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://one.example.com"));

  SaveProfiles({https_profile});
  ASSERT_THAT(plus_address_service()->GetPlusProfiles(),
              UnorderedElementsAreArray({https_profile}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));

  affiliations::GroupedFacets group;
  group.facets.emplace_back(http_profile.facet);
  group.facets.emplace_back(https_profile.facet);
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  EXPECT_TRUE(
      ExpectMatchHelperToReturnProfiles(http_profile.facet, {https_profile}));
}

// Verifies that http domains that are part of the affiliation group of
// the requested domain are returned.
TEST_F(PlusAddressAffiliationMatchHelperTest, SupportGroupHttpDomainMatches) {
  PlusProfile http_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("http://example.com"));
  PlusProfile stored_https_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromPotentiallyInvalidSpec("https://example.com"));

  // The user has the `profile` stored.
  SaveProfiles({stored_https_profile});
  ASSERT_THAT(plus_address_service()->GetPlusProfiles(),
              testing::UnorderedElementsAreArray({stored_https_profile}));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));

  FacetURI requested_facet =
      FacetURI::FromPotentiallyInvalidSpec("http://foo.com");

  affiliations::GroupedFacets group;
  group.facets.emplace_back(requested_facet);
  // Include http domain as part of the group results.
  group.facets.emplace_back(http_profile.facet);
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  EXPECT_TRUE(ExpectMatchHelperToReturnProfiles(requested_facet,
                                                {stored_https_profile}));
  histogram_tester().ExpectTotalCount(kUmaKeyResponseTime, 1u);
}

// Verifies that affiliation info is requested on top level domains while
// honoring the PSL extension list.
TEST_F(PlusAddressAffiliationMatchHelperTest,
       RequestGroupInfoOnExtendedTopLevelDomain) {
  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillRepeatedly(
          RunOnceCallbackRepeatedly<0>(std::vector<std::string>{"psl.com"}));

  const std::map<std::string, std::string> kTopLevelMap = {
      {"https://example.com", "https://example.com"},
      {"https://www.example.com", "https://example.com"},
      {"https://group.affiliated.com", "https://affiliated.com"},
      {"https://www.group.affiliated.com", "https://affiliated.com"},
      {"https://foo.bar.com", "https://bar.com"},
      // PSL extended.
      {"https://foo.psl.com", "https://foo.psl.com"},
      {"https://foo.bar.psl.com", "https://bar.psl.com"},
  };

  base::MockCallback<
      PlusAddressAffiliationMatchHelper::AffiliatedPlusProfilesCallback>
      callback;
  {
    testing::InSequence in_sequence;

    for (const auto& [_, expected_top_level] : kTopLevelMap) {
      EXPECT_CALL(
          *mock_affiliation_service(),
          GetGroupingInfo(
              std::vector<FacetURI>{
                  FacetURI::FromPotentiallyInvalidSpec(expected_top_level)},
              testing::_));
    }
  }
  for (const auto& [requested_domain, _] : kTopLevelMap) {
    match_helper()->GetAffiliatedPlusProfiles(
        FacetURI::FromPotentiallyInvalidSpec(requested_domain), callback.Get());
  }
}

}  // namespace
}  // namespace plus_addresses
