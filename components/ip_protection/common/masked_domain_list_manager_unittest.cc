// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/masked_domain_list_manager.h"

#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace ip_protection {
namespace {
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;
using ::testing::Eq;

struct MatchTest {
  std::string_view name;
  std::string_view req;
  std::string_view top;
  bool matches;
  bool matches_with_bypass;
};

constexpr MatchTest kMatchTests[] = {
    MatchTest{
        "ExcludedFromResource",
        "experiment.com",
        "top.com",
        false,
        false,
    },
    MatchTest{
        "ResourceMatch",
        "example.com",
        "top.com",
        true,
        true,
    },
    // Public suffix list testcases.
    // Assumes that "googleapis.com" is added as a resource owner that owns
    // itself only.
    //
    // The `googleapis.com` domain is in the owned_resources of an MDL entry,
    // but the top level site is not owned by it so this is a 3rd party request
    // and should be proxied.
    MatchTest{
        "PslAddedAsResource_OnOwnedResources_TopToDifferentDomain",
        "googleapis.com",
        "top.com",
        true,
        true,
    },
    // The `googleapis.com` domain is in the owned_resources of an MDL entry.
    // This is a request to a subdomain of an owned_resources (known tracker)
    // The top level site is not owned by the same owner so this is a 3rd
    // party request and should be proxied.
    MatchTest{
        "PslAddedAsResource_OnOwnedResources_TopToDifferentDomainSubDomain",
        "sub.googleapis.com",
        "top.com",
        true,
        true,
    },
    // Request from one site that is added as a PSL entry to another site that
    // is added as a PSL entry.
    // `co.jp` is added as a PSL.
    // No MDL entry claims ownership of `co.jp`
    // An MDL entry claims ownership of `sub.co.jp`
    // Should be proxied because `co.jp` is listed as a resource that owns
    // itself and the top_frame_site has the same suffix but is not same-site.
    MatchTest{
        "PslAddedAsResource_MatchingOwnedResources_TopToSubOnSameDomain"
        "OwnerClaimsSubdomain",
        "sub.co.jp",
        "other.co.jp",
        true,
        true,
    },
    // Request from an owned property to an owned resource.
    // The owned resource is a subdomain of a PSL entry resource.
    // Should be proxied but not if bypass is allowed, because while
    // `co.jp` is listed as a PSL resource, the subdomain is
    // privately owned and an MDL entry claims ownership of it and this is a
    // request between an owned property to an owned resource of the same
    // owner.
    MatchTest{
        "PslAddedAsResource_MatchingOwnedResources_SubdomainNotOnPsl",
        "sub.co.jp",
        "owned_property.com",
        true,
        false,
    },
    // Request from one PSL entry site to another PSL entry site.
    // `co.jp` is listed as a resource that owns itself.
    // No MDL entry claims ownership of `co.jp`
    // No MDL entry claims ownership of `site.co.jp`
    // Bypasses the proxy for same-site check of request and top_frame_site.
    MatchTest{
        "PslAddedAsResource_SameSiteRequest"
        "OwnerClaimsSubdomain",
        "same.site.co.jp",
        "site.co.jp",
        true,
        false,
    },
    // Ensure fully-qualified domain names (FQDNs) match correctly.
    MatchTest{
        "ResourceMatch_FQDN",
        "example.com.",
        "top.com.",
        true,
        true,
    },
    MatchTest{
        "ResourceMatch_FQDN_Req",
        "example.com.",
        "top.com",
        true,
        true,
    },
    MatchTest{
        "ResourceMatch_FQDN_Top",
        "example.com",
        "top.com.",
        true,
        true,
    },
};

constexpr std::string_view kTestDomain = "example.com";

const char kFirstUpdateTimeHistogram[] =
    "NetworkService.MaskedDomainList.FirstUpdateTime";

}  // namespace

class MaskedDomainListManagerTest : public testing::Test {
 public:
  MaskedDomainListManagerTest()
      : allow_list_no_bypass_(
            network::mojom::IpProtectionProxyBypassPolicy::kNone),
        allow_list_first_party_bypass_(
            network::mojom::IpProtectionProxyBypassPolicy::
                kFirstPartyToTopLevelFrame) {}

  void SetUp() override {
    masked_domain_list::MaskedDomainList mdl;
    auto* resource_owner = mdl.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_properties("property.com");
    resource_owner->add_owned_resources()->set_domain(std::string(kTestDomain));
    allow_list_no_bypass_.UpdateMaskedDomainListForTesting(mdl);
    allow_list_first_party_bypass_.UpdateMaskedDomainListForTesting(mdl);
  }

 protected:
  MaskedDomainListManager allow_list_no_bypass_;
  MaskedDomainListManager allow_list_first_party_bypass_;
};

TEST_F(MaskedDomainListManagerTest, IsNotEnabledByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      network::features::kMaskedDomainList);

  MaskedDomainListManager allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainListManager allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  EXPECT_FALSE(allow_list_no_bypass.IsEnabled());
  EXPECT_FALSE(allow_list_first_party_bypass.IsEnabled());
}

TEST_F(MaskedDomainListManagerTest, IsEnabledWhenManuallySet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);

  EXPECT_TRUE(allow_list.IsEnabled());
}

TEST_F(MaskedDomainListManagerTest, AllowListIsNotPopulatedByDefault) {
  MaskedDomainListManager allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  EXPECT_FALSE(allow_list.IsPopulated());
}

TEST_F(MaskedDomainListManagerTest, AllowlistIsPopulated_MdlHasResourceOwners) {
  MaskedDomainListManager mdl_manager(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  masked_domain_list::MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  mdl_manager.UpdateMaskedDomainListForTesting(mdl);

  EXPECT_TRUE(mdl_manager.IsPopulated());
}

TEST_F(MaskedDomainListManagerTest, AllowlistAcceptsMultipleUpdates) {
  base::HistogramTester histogram_tester;
  MaskedDomainListManager mdl_manager(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  masked_domain_list::MaskedDomainList mdl1;
  {
    auto* resource_owner = mdl1.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_resources()->set_domain("example.com");
  }
  masked_domain_list::MaskedDomainList mdl2;
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
  mdl_manager.UpdateMaskedDomainListForTesting(mdl1);

  EXPECT_TRUE(mdl_manager.IsPopulated());
  EXPECT_TRUE(
      mdl_manager.Matches(kHttpsRequestUrl1, kEmptyNak, MdlType::kIncognito));
  EXPECT_FALSE(
      mdl_manager.Matches(kHttpsRequestUrl2, kEmptyNak, MdlType::kIncognito));
  histogram_tester.ExpectTotalCount(kFirstUpdateTimeHistogram, 1);

  // Second update. Removes old rules, adds new ones.
  mdl_manager.UpdateMaskedDomainListForTesting(mdl2);

  EXPECT_TRUE(mdl_manager.IsPopulated());
  EXPECT_TRUE(
      mdl_manager.Matches(kHttpsRequestUrl2, kEmptyNak, MdlType::kIncognito));
  EXPECT_FALSE(
      mdl_manager.Matches(kHttpsRequestUrl1, kEmptyNak, MdlType::kIncognito));
  histogram_tester.ExpectTotalCount(kFirstUpdateTimeHistogram, 1);
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchHttp) {
  const auto kHttpRequestUrl = GURL(base::StrCat({"http://", kTestDomain}));
  const auto kHttpCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("http://top.com")));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpRequestUrl, kHttpCrossSiteNak,
                                            MdlType::kIncognito));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
      kHttpRequestUrl, kHttpCrossSiteNak, MdlType::kIncognito));
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchWss) {
  const auto kWssRequestUrl = GURL(base::StrCat({"wss://", kTestDomain}));
  const auto kHttpCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("http://top.com")));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kWssRequestUrl, kHttpCrossSiteNak,
                                            MdlType::kIncognito));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
      kWssRequestUrl, kHttpCrossSiteNak, MdlType::kIncognito));
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchWs) {
  const auto kWsRequestUrl = GURL(base::StrCat({"ws://", kTestDomain}));
  const auto kHttpCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("http://top.com")));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kWsRequestUrl, kHttpCrossSiteNak,
                                            MdlType::kIncognito));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
      kWsRequestUrl, kHttpCrossSiteNak, MdlType::kIncognito));
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
  EXPECT_TRUE(allow_list_no_bypass_.Matches(
      kHttpsThirdPartyRequestUrl, kHttpsCrossSiteNak, MdlType::kIncognito));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
      kHttpsThirdPartyRequestUrl, kHttpsCrossSiteNak, MdlType::kIncognito));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(
      kHttpsThirdPartyRequestUrl, kHttpsSameSiteNak, MdlType::kIncognito));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
      kHttpsThirdPartyRequestUrl, kHttpsSameSiteNak, MdlType::kIncognito));
}

TEST_F(MaskedDomainListManagerTest,
       MatchFirstPartyToTopLevelFrameDependsOnBypass) {
  const auto kHttpsFirstPartyRequestUrl =
      GURL(base::StrCat({"https://", kTestDomain}));
  const auto kHttpsSameSiteNak = net::NetworkAnonymizationKey::CreateSameSite(
      net::SchemefulSite(kHttpsFirstPartyRequestUrl));
  const auto kHttpsCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(kHttpsFirstPartyRequestUrl));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsSameSiteNak, MdlType::kIncognito));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsSameSiteNak, MdlType::kIncognito));
  EXPECT_TRUE(allow_list_no_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsCrossSiteNak, MdlType::kIncognito));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsCrossSiteNak, MdlType::kIncognito));
}

TEST_F(MaskedDomainListManagerTest,
       MatchFirstPartyToTopLevelFrameIfEmptyNakDependsOnBypass) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kEmptyNak = net::NetworkAnonymizationKey();

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kEmptyNak,
                                            MdlType::kIncognito));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsRequestUrl, kEmptyNak, MdlType::kIncognito));
}

TEST_F(MaskedDomainListManagerTest,
       ShouldNotMatchWithFencedFrameNakIfUrlDoesNotMatch) {
  const auto kHttpsOtherRequestUrl = GURL("https://other.com");
  const auto kNakWithNonce = net::NetworkAnonymizationKey::CreateFromParts(
      net::SchemefulSite(), /*is_cross_site=*/true,
      base::UnguessableToken::Create());

  EXPECT_FALSE(allow_list_no_bypass_.Matches(
      kHttpsOtherRequestUrl, kNakWithNonce, MdlType::kIncognito));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsOtherRequestUrl, kNakWithNonce, MdlType::kIncognito));
}

TEST_F(MaskedDomainListManagerTest, ShouldMatchWithFencedFrameNakIfUrlMatches) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kNakWithNonce = net::NetworkAnonymizationKey::CreateFromParts(
      net::SchemefulSite(), /*is_cross_site=*/true,
      base::UnguessableToken::Create());

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kNakWithNonce,
                                            MdlType::kIncognito));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
      kHttpsRequestUrl, kNakWithNonce, MdlType::kIncognito));
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

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kExtensionUrlNak,
                                            MdlType::kIncognito));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "true"}});
    EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
        kHttpsRequestUrl, kExtensionUrlNak, MdlType::kIncognito));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "false"}});
    EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
        kHttpsRequestUrl, kExtensionUrlNak, MdlType::kIncognito));
  }
}

// Test whether third-party requests from pages with a data: URL top-level site
// (where the corresponding NAK is transient) should be proxied.
TEST_F(MaskedDomainListManagerTest, DataUrlTopLevelSite) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kDataUrlNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("data:text/html,<html></html>")));
  ASSERT_TRUE(kDataUrlNak.IsTransient());

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak,
                                            MdlType::kIncognito));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "true"}});
    EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
        kHttpsRequestUrl, kDataUrlNak, MdlType::kIncognito));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "false"}});
    EXPECT_TRUE(allow_list_first_party_bypass_.Matches(
        kHttpsRequestUrl, kDataUrlNak, MdlType::kIncognito));
  }
}

TEST_F(MaskedDomainListManagerTest, Matches_MdlType_MatchesCorrectly) {
  MaskedDomainListManager mdl_manager(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);

  masked_domain_list::MaskedDomainList mdl;

  ResourceOwner* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("example");
  Resource* resource = resource_owner->add_owned_resources();
  std::string default_mdl_domain = "default-mdl.com";
  resource->set_domain(default_mdl_domain);
  resource = resource_owner->add_owned_resources();
  std::string regular_mdl_domain = "regular-mdl.com";
  resource->set_domain(regular_mdl_domain);
  resource->add_experiments(
      Resource::Experiment::Resource_Experiment_EXPERIMENT_EXTERNAL_REGULAR);
  resource->set_exclude_default_group(true);

  mdl_manager.UpdateMaskedDomainListForTesting(mdl);

  // The default MDL resource should ONLY match for mdl type kIncognito.
  EXPECT_TRUE(
      mdl_manager.Matches(GURL(base::StrCat({"https://", default_mdl_domain})),
                          net::NetworkAnonymizationKey(), MdlType::kIncognito));
  EXPECT_FALSE(mdl_manager.Matches(
      GURL(base::StrCat({"https://", default_mdl_domain})),
      net::NetworkAnonymizationKey(), MdlType::kRegularBrowsing));

  // The regular MDL resource should ONLY match for mdl type kRegularBrowsing.
  EXPECT_FALSE(
      mdl_manager.Matches(GURL(base::StrCat({"https://", regular_mdl_domain})),
                          net::NetworkAnonymizationKey(), MdlType::kIncognito));
  EXPECT_TRUE(mdl_manager.Matches(
      GURL(base::StrCat({"https://", regular_mdl_domain})),
      net::NetworkAnonymizationKey(), MdlType::kRegularBrowsing));
}

class MaskedDomainListManagerMatchTest
    : public MaskedDomainListManagerTest,
      public testing::WithParamInterface<MatchTest> {};

TEST_P(MaskedDomainListManagerMatchTest, Match) {
  const MatchTest& match_test = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      network::features::kMaskedDomainList);

  MaskedDomainListManager allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainListManager allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  masked_domain_list::MaskedDomainList mdl;

  ResourceOwner* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("example");
  Resource* resource = resource_owner->add_owned_resources();
  resource->set_domain("example.com");

  // Public Suffix List domains are added to the MDL as ResourceOwners.
  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("googleapis.com");
  resource = resource_owner->add_owned_resources();
  resource->set_domain("googleapis.com");
  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("co.jp");
  resource = resource_owner->add_owned_resources();
  resource->set_domain("co.jp");

  // Additional ResourceOwner - Includes resources which are on the public
  // suffix list.
  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("public_suffix");
  resource_owner->add_owned_properties("owned_property.com");
  // Claim a subdomain on the PSL.
  resource = resource_owner->add_owned_resources();
  resource->set_domain("sub.co.jp");

  allow_list_no_bypass.UpdateMaskedDomainListForTesting(mdl);
  allow_list_first_party_bypass.UpdateMaskedDomainListForTesting(mdl);

  GURL request_url(base::StrCat({"https://", match_test.req}));
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL(base::StrCat({"https://", match_test.top}))));

  EXPECT_EQ(match_test.matches,
            allow_list_no_bypass.Matches(request_url, network_anonymization_key,
                                         MdlType::kIncognito));
  EXPECT_EQ(match_test.matches_with_bypass,
            allow_list_first_party_bypass.Matches(
                request_url, network_anonymization_key, MdlType::kIncognito));
}

INSTANTIATE_TEST_SUITE_P(All,
                         MaskedDomainListManagerMatchTest,
                         testing::ValuesIn(kMatchTests),
                         [](const testing::TestParamInfo<MatchTest>& info) {
                           return std::string(info.param.name);
                         });
}  // namespace ip_protection
