// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/masked_domain_list_manager.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace ip_protection {
namespace {
using ::masked_domain_list::MaskedDomainList;
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;
using ::testing::Eq;

struct ExperimentGroupMatchTest {
  std::string name;
  std::string req;
  std::string top;
  // The proto has an int type but feature init needs a string representation.
  std::string experiment_group;
  bool matches;
  bool matches_with_bypass;
};

const std::vector<ExperimentGroupMatchTest> kMatchTests = {
    ExperimentGroupMatchTest{
        "NoExperimentGroup_ExcludedFromResource",
        "experiment.com",
        "top.com",
        "0",
        false,
        false,
    },
    ExperimentGroupMatchTest{
        "NoExperimentGroup_DefaultResourceMatch",
        "example.com",
        "top.com",
        "0",
        true,
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup1_ExperimentResourceMatch",
        "experiment.com",
        "top.com",
        "1",
        true,
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup2_ExperimentResourceMatch",
        "experiment.com",
        "top.com",
        "2",
        true,
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup1_DefaultResourceMatch",
        "example.com",
        "top.com",
        "1",
        true,
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup2_ExcludedFromDefaultResource",
        "example.com",
        "top.com",
        "2",
        false,
        false,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup3_ExcludedFromDefaultResource",
        "experiment.com",
        "top.com",
        "3",
        false,
        false,
    },
    // Public suffix list testcases.
    // Assumes that "googleapis.com" is on the public suffix list.

    // The `googleapis.com` domain is in the owned_resources of an MDL entry,
    // but the top level site is not owned by it so this is a 3rd party request
    // and should be proxied.
    ExperimentGroupMatchTest{
        "OnPsl_OnOwnedResources_TopToDifferentDomain",
        "googleapis.com",
        "top.com",
        "1",
        true,
        true,
    },

    // The `googleapis.com` domain is in the owned_resources of an MDL entry.
    // This is a request to a subdomain of an owned_resources (known tracker)
    // The top level site is not owned by the same owner so this is a 3rd
    // party request and should be proxied.
    ExperimentGroupMatchTest{
        "OnPsl_OnOwnedResources_TopToDifferentDomainSubDomain",
        "sub.googleapis.com",
        "top.com",
        "1",
        true,
        true,
    },
    // Request from a domain to its subdomain.
    // `googleapis.com` is listed on the PSL.
    // An MDL entry claims ownership of `googleapis.com`
    // No MDL entry claims ownership of `sub.googleapis.com`
    // Should be proxied because `googleapis.com` is on the MDL and the two
    // don't belong to the same owner.
    ExperimentGroupMatchTest{
        "OnPsl_MatchingOwnedResources_TopToSubOnSameDomain"
        "OwnerDoesntClaimSubdomain",
        "sub.googleapis.com",
        "googleapis.com",
        "1",
        true,
        true,
    },
    // Request from one site in the PSL rules to another in the PSL rules.
    // `co.jp` is listed on the PSL.
    // No MDL entry claims ownership of `co.jp`
    // An MDL entry claims ownership of `sub.co.jp`
    // Should be proxied because `co.jp` is listed on the PSL and the
    // top_frame_site has the same suffix but is not same-site.
    ExperimentGroupMatchTest{
        "OnPsl_MatchingOwnedResources_TopToSubOnSameDomain"
        "OwnerClaimsSubdomain",
        "sub.co.jp",
        "other.co.jp",
        "1",
        true,
        true,
    },
    // Request from an owned property to an owned resource.
    // The owned resource is a subdomain of a PSL entry.
    // Should be proxied but not if bypass is allowed, because while
    // `co.jp` is listed in the PSL, the subdomain is
    // privately owned and an MDL entry claims ownership of it and this is a
    // request between an owned property to an owned resource of the same owner.
    ExperimentGroupMatchTest{
        "Psl_MatchingOwnedResources_SubdomainNotOnPsl",
        "sub.co.jp",
        "owned_property.com",
        "1",
        true,
        false,
    },
    // Request from one site in the PSL rules to another in the PSL rules.
    // `co.jp` is listed on the PSL.
    // No MDL entry claims ownership of `co.jp`
    // No MDL entry claims ownership of `site.co.jp`
    // Bypasses the proxy for same-site check of request and top_frame_site.
    ExperimentGroupMatchTest{
        "OnPsl_SameSiteRequest"
        "OwnerClaimsSubdomain",
        "same.site.co.jp",
        "site.co.jp",
        "1",
        true,
        false,
    },
};

constexpr std::string_view kTestDomain = "example.com";

const char kFirstUpdateTimeHistogram[] =
    "NetworkService.MaskedDomainList.FirstUpdateTime";

}  // namespace

class MaskedDomainListManagerBaseTest : public testing::Test {};

class MaskedDomainListManagerTest : public MaskedDomainListManagerBaseTest {
 public:
  MaskedDomainListManagerTest()
      : allow_list_no_bypass_(
            network::mojom::IpProtectionProxyBypassPolicy::kNone),
        allow_list_first_party_bypass_(
            network::mojom::IpProtectionProxyBypassPolicy::
                kFirstPartyToTopLevelFrame) {}

  void SetUp() override {
    MaskedDomainList mdl;
    auto* resource_owner = mdl.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_properties("property.com");
    resource_owner->add_owned_resources()->set_domain(std::string(kTestDomain));
    allow_list_no_bypass_.UpdateMaskedDomainList(
        mdl, /*exclusion_list=*/std::vector<std::string>());
    allow_list_first_party_bypass_.UpdateMaskedDomainList(
        mdl, /*exclusion_list=*/std::vector<std::string>());
  }

 protected:
  MaskedDomainListManager allow_list_no_bypass_;
  MaskedDomainListManager allow_list_first_party_bypass_;
};

TEST_F(MaskedDomainListManagerBaseTest, IsNotEnabledByDefault) {
  MaskedDomainListManager allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainListManager allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  EXPECT_FALSE(allow_list_no_bypass.IsEnabled());
  EXPECT_FALSE(allow_list_first_party_bypass.IsEnabled());
}

TEST_F(MaskedDomainListManagerBaseTest, IsEnabledWhenManuallySet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);

  EXPECT_TRUE(allow_list.IsEnabled());
}

TEST_F(MaskedDomainListManagerBaseTest, AllowListIsNotPopulatedByDefault) {
  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  EXPECT_FALSE(allow_list.IsPopulated());
}

TEST_F(MaskedDomainListManagerBaseTest,
       AllowlistIsPopulated_MdlHasResourceOwners) {
  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UpdateMaskedDomainList(
      mdl,
      /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(allow_list.IsPopulated());
}

TEST_F(MaskedDomainListManagerBaseTest, AllowlistIsPopulated_MdlHasPslRules) {
  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl;
  mdl.add_public_suffix_list_rules()->set_private_domain("example.com");
  allow_list.UpdateMaskedDomainList(
      mdl,
      /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(allow_list.IsPopulated());
}

TEST_F(MaskedDomainListManagerBaseTest,
       AllowlistIsPopulated_MdlHasResourceOwnersAndPslRules) {
  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  mdl.add_public_suffix_list_rules()->set_private_domain("example.com");
  allow_list.UpdateMaskedDomainList(
      mdl,
      /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(allow_list.IsPopulated());
}

TEST_F(MaskedDomainListManagerBaseTest, AllowlistAcceptsMultipleUpdates) {
  base::HistogramTester histogram_tester;
  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl1;
  {
    auto* resource_owner = mdl1.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_resources()->set_domain("example.com");
  }
  MaskedDomainList mdl2;
  {
    auto* resource_owner = mdl2.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_resources()->set_domain("example.net");
  }
  const auto kHttpsRequestUrl1 = GURL("https://example.com");
  const auto kHttpsRequestUrl2 = GURL("https://example.net");
  const auto kEmptyNak = net::NetworkAnonymizationKey();

  // No updates yet.
  histogram_tester.ExpectTotalCount(kFirstUpdateTimeHistogram, 0);

  // First update.
  allow_list.UpdateMaskedDomainList(
      mdl1, /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(allow_list.IsPopulated());
  EXPECT_TRUE(allow_list.Matches(kHttpsRequestUrl1, kEmptyNak));
  EXPECT_FALSE(allow_list.Matches(kHttpsRequestUrl2, kEmptyNak));
  histogram_tester.ExpectTotalCount(kFirstUpdateTimeHistogram, 1);

  // Second update. Removes old rules, adds new ones.
  allow_list.UpdateMaskedDomainList(
      mdl2, /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(allow_list.IsPopulated());
  EXPECT_FALSE(allow_list.Matches(kHttpsRequestUrl1, kEmptyNak));
  EXPECT_TRUE(allow_list.Matches(kHttpsRequestUrl2, kEmptyNak));
  histogram_tester.ExpectTotalCount(kFirstUpdateTimeHistogram, 1);
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchHttp) {
  const auto kHttpRequestUrl = GURL(base::StrCat({"http://", kTestDomain}));
  const auto kHttpCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("http://top.com")));

  EXPECT_TRUE(
      allow_list_no_bypass_.Matches(kHttpRequestUrl, kHttpCrossSiteNak));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpRequestUrl,
                                                     kHttpCrossSiteNak));
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchThirdPartyToTopLevelFrame) {
  const auto kHttpsCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("https://top.com")));
  const auto kHttpsSameSiteNak = net::NetworkAnonymizationKey::CreateSameSite(
      net::SchemefulSite(GURL("https://top.com")));
  const auto kHttpsThirdPartyRequestUrl =
      GURL(base::StrCat({"https://", kTestDomain}));

  // Regardless of whether the NAK is cross-site, the request URL should be
  // considered third-party because the request URL doesn't match the top-level
  // site.
  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                            kHttpsCrossSiteNak));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                                     kHttpsCrossSiteNak));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                            kHttpsSameSiteNak));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                                     kHttpsSameSiteNak));
}

TEST_F(MaskedDomainListManagerTest,
       MatchFirstPartyToTopLevelFrameDependsOnBypass) {
  const auto kHttpsFirstPartyRequestUrl =
      GURL(base::StrCat({"https://", kTestDomain}));
  const auto kHttpsSameSiteNak = net::NetworkAnonymizationKey::CreateSameSite(
      net::SchemefulSite(kHttpsFirstPartyRequestUrl));
  const auto kHttpsCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(kHttpsFirstPartyRequestUrl));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsFirstPartyRequestUrl,
                                            kHttpsSameSiteNak));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsSameSiteNak));
  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsFirstPartyRequestUrl,
                                            kHttpsCrossSiteNak));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsCrossSiteNak));
}

TEST_F(MaskedDomainListManagerTest,
       MatchFirstPartyToTopLevelFrameIfEmptyNakDependsOnBypass) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kEmptyNak = net::NetworkAnonymizationKey();

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kEmptyNak));
  EXPECT_FALSE(
      allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kEmptyNak));
}

TEST_F(MaskedDomainListManagerTest,
       ShouldNotMatchWithFencedFrameNakIfUrlDoesNotMatch) {
  const auto kHttpsOtherRequestUrl = GURL("https://other.com");
  const auto kNakWithNonce = net::NetworkAnonymizationKey::CreateFromParts(
      net::SchemefulSite(), /*is_cross_site=*/true,
      base::UnguessableToken::Create());

  EXPECT_FALSE(
      allow_list_no_bypass_.Matches(kHttpsOtherRequestUrl, kNakWithNonce));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(kHttpsOtherRequestUrl,
                                                      kNakWithNonce));
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchWithFencedFrameNakIfUrlMatches) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kNakWithNonce = net::NetworkAnonymizationKey::CreateFromParts(
      net::SchemefulSite(), /*is_cross_site=*/true,
      base::UnguessableToken::Create());

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kNakWithNonce));
  EXPECT_TRUE(
      allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kNakWithNonce));
}

TEST_F(MaskedDomainListManagerTest, CustomSchemeTopLevelSite) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::ClearSchemesForTests();
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kExtensionUrlNak =
      net::NetworkAnonymizationKey::CreateCrossSite(net::SchemefulSite(
          GURL("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/")));
  ASSERT_FALSE(kExtensionUrlNak.IsTransient());

  EXPECT_TRUE(
      allow_list_no_bypass_.Matches(kHttpsRequestUrl, kExtensionUrlNak));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "true"}});
    EXPECT_FALSE(allow_list_first_party_bypass_.Matches(kHttpsRequestUrl,
                                                        kExtensionUrlNak));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "false"}});
    EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpsRequestUrl,
                                                       kExtensionUrlNak));
  }
}

// Test whether third-party requests from pages with a data: URL top-level site
// (where the corresponding NAK is transient) should be proxied.
TEST_F(MaskedDomainListManagerTest, DataUrlTopLevelSite) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kDataUrlNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("data:text/html,<html></html>")));
  ASSERT_TRUE(kDataUrlNak.IsTransient());

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "true"}});
    EXPECT_FALSE(
        allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "false"}});
    EXPECT_TRUE(
        allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak));
  }
}

TEST_F(MaskedDomainListManagerTest, AllowListWithoutBypassUsesLessMemory) {
  EXPECT_GT(allow_list_first_party_bypass_.EstimateMemoryUsage(),
            allow_list_no_bypass_.EstimateMemoryUsage());
}

class MaskedDomainListManagerExperimentGroupMatchTest
    : public MaskedDomainListManagerBaseTest,
      public testing::WithParamInterface<ExperimentGroupMatchTest> {};

TEST_P(MaskedDomainListManagerExperimentGroupMatchTest, Match) {
  const ExperimentGroupMatchTest& p = GetParam();

  std::map<std::string, std::string> parameters;
  parameters[network::features::kMaskedDomainListExperimentGroup.name] =
      p.experiment_group;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      network::features::kMaskedDomainList, std::move(parameters));

  MaskedDomainListManager allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainListManager allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  MaskedDomainList mdl;

  // ResourceOwner 1 - Includes the default group.
  ResourceOwner* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("example");
  Resource* resource = resource_owner->add_owned_resources();
  resource->set_domain("example.com");
  resource->add_experiment_group_ids(1);

  // ResourceOwner 2 - Excludes the default group.
  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("experiment");
  resource = resource_owner->add_owned_resources();
  resource->set_domain("experiment.com");
  resource->set_exclude_default_group(true);
  resource->add_experiment_group_ids(1);
  resource->add_experiment_group_ids(2);

  // Public Suffix List (includes private section)
  mdl.add_public_suffix_list_rules()->set_private_domain("googleapis.com");
  mdl.add_public_suffix_list_rules()->set_private_domain("co.jp");

  // ResourceOwner 3 - Includes resources which are on the public suffix list.
  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("public_suffix");
  resource_owner->add_owned_properties("owned_property.com");
  // Claim top level domain on the PSL.
  resource = resource_owner->add_owned_resources();
  resource->set_domain("googleapis.com");
  resource->add_experiment_group_ids(1);

  // Claim a subdomain on the PSL.
  resource = resource_owner->add_owned_resources();
  resource->set_domain("sub.co.jp");
  resource->add_experiment_group_ids(1);

  allow_list_no_bypass.UpdateMaskedDomainList(
      mdl, /*exclusion_list=*/std::vector<std::string>());
  allow_list_first_party_bypass.UpdateMaskedDomainList(
      mdl, /*exclusion_list=*/std::vector<std::string>());

  GURL request_url(base::StrCat({"https://", p.req}));
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL(base::StrCat({"https://", p.top}))));

  EXPECT_EQ(p.matches, allow_list_no_bypass.Matches(request_url,
                                                    network_anonymization_key));
  EXPECT_EQ(p.matches_with_bypass, allow_list_first_party_bypass.Matches(
                                       request_url, network_anonymization_key));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MaskedDomainListManagerExperimentGroupMatchTest,
    testing::ValuesIn(kMatchTests),
    [](const testing::TestParamInfo<ExperimentGroupMatchTest>& info) {
      return info.param.name;
    });

TEST_F(MaskedDomainListManagerBaseTest, ExclusionSetDomainsRemovedFromMDL) {
  MaskedDomainListManager allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kExclusionList);
  const std::set<std::string> mdl_domains(
      {"com", "example.com", "subdomain.example.com",
       "sub.subdomain.example.com", "unrelated-example.com", "example.net",
       "subdomain.example.net", "example.com.example.net", "excluded-tld",
       "included-tld", "subdomain.excluded-tld", "subdomain.included-tld"});
  const std::set<std::string> exclusion_set(
      {"example.com", "excluded-tld", "irrelevant-tld"});
  const std::set<std::string> mdl_domains_after_exclusions(
      {"com", "unrelated-example.com", "example.net", "subdomain.example.net",
       "example.com.example.net", "included-tld", "subdomain.included-tld"});
  const std::set<std::string> empty_exclusion_set({});

  EXPECT_THAT(
      allow_list_no_bypass.ExcludeDomainsFromMDL(mdl_domains, exclusion_set),
      Eq(mdl_domains_after_exclusions));
  EXPECT_THAT(allow_list_no_bypass.ExcludeDomainsFromMDL(mdl_domains,
                                                         empty_exclusion_set),
              Eq(mdl_domains));
}

}  // namespace ip_protection
