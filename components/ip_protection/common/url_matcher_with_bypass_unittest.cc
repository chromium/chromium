// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/url_matcher_with_bypass.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {
using ::masked_domain_list::MaskedDomainList;
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;

struct MatchTest {
  std::string name;
  std::string req;
  std::string top;
  bool skip_bypass_check;
  UrlMatcherWithBypassResult result;
};

}  // namespace

class UrlMatcherWithBypassTest : public ::testing::Test {};

TEST_F(UrlMatcherWithBypassTest, MatchesDefaultGroupOnly) {
  UrlMatcherWithBypass matcher;
  ResourceOwner resource_owner;

  resource_owner.set_owner_name("example");
  resource_owner.add_owned_resources()->set_domain("example.com");

  matcher.AddRules(resource_owner, /*excluded_domains=*/{},
                   /*create_bypass_matcher=*/false);

  // The resource is not in the regular browsing group, so it should not match
  // when matching for that group.
  EXPECT_EQ(matcher.Matches(GURL("http://example.com"),
                            /*top_frame_site=*/std::nullopt,
                            MdlType::kRegularBrowsing,
                            /*skip_bypass_check=*/true),
            UrlMatcherWithBypassResult::kNoMatch);

  // The resource is in the default group, so it should match when matching for
  // that group.
  EXPECT_EQ(
      matcher.Matches(GURL("http://example.com"),
                      /*top_frame_site=*/std::nullopt, MdlType::kIncognito,
                      /*skip_bypass_check=*/true),
      UrlMatcherWithBypassResult::kMatchAndNoBypass);
}

TEST_F(UrlMatcherWithBypassTest, MatchesDefaultGroupExcluded) {
  UrlMatcherWithBypass matcher;
  ResourceOwner resource_owner;

  resource_owner.set_owner_name("example");
  Resource* resource = resource_owner.add_owned_resources();
  resource->set_domain("example.com");
  resource->set_exclude_default_group(true);
  resource->add_experiments(
      Resource::Experiment::Resource_Experiment_EXPERIMENT_EXTERNAL_REGULAR);

  matcher.AddRules(resource_owner, /*excluded_domains=*/{},
                   /*create_bypass_matcher=*/false);

  // The resource is not in the default group, so it should not match when
  // matching for the default group.
  EXPECT_EQ(
      matcher.Matches(GURL("http://example.com"),
                      /*top_frame_site=*/std::nullopt, MdlType::kIncognito,
                      /*skip_bypass_check=*/true),
      UrlMatcherWithBypassResult::kNoMatch);

  // The resource is in the regular browsing group, so it should match when
  // matching for that group.
  EXPECT_EQ(matcher.Matches(GURL("http://example.com"),
                            /*top_frame_site=*/std::nullopt,
                            MdlType::kRegularBrowsing,
                            /*skip_bypass_check=*/true),
            UrlMatcherWithBypassResult::kMatchAndNoBypass);
}

TEST_F(UrlMatcherWithBypassTest, MatchesMultipleMdlTypes) {
  UrlMatcherWithBypass matcher;
  ResourceOwner resource_owner;

  resource_owner.set_owner_name("example");
  Resource* resource = resource_owner.add_owned_resources();
  resource->set_domain("example.com");
  resource->add_experiments(
      Resource::Experiment::Resource_Experiment_EXPERIMENT_EXTERNAL_REGULAR);

  matcher.AddRules(resource_owner, /*excluded_domains=*/{},
                   /*create_bypass_matcher=*/false);

  // The resource is in the default group, so it should match when matching for
  // that group.
  EXPECT_EQ(
      matcher.Matches(GURL("http://example.com"),
                      /*top_frame_site=*/std::nullopt, MdlType::kIncognito,
                      /*skip_bypass_check=*/true),
      UrlMatcherWithBypassResult::kMatchAndNoBypass);

  // The resource is in the regular browsing group, so it should match when
  // matching for that group.
  EXPECT_EQ(matcher.Matches(GURL("http://example.com"),
                            /*top_frame_site=*/std::nullopt,
                            MdlType::kRegularBrowsing,
                            /*skip_bypass_check=*/true),
            UrlMatcherWithBypassResult::kMatchAndNoBypass);
}

TEST_F(UrlMatcherWithBypassTest, Matches_ResourceWithNoMdlTypeAdded_NoMatch) {
  UrlMatcherWithBypass matcher;
  ResourceOwner resource_owner;

  resource_owner.set_owner_name("example");
  Resource* resource = resource_owner.add_owned_resources();
  resource->set_domain("example.com");
  resource->set_exclude_default_group(true);

  matcher.AddRules(resource_owner, /*excluded_domains=*/{},
                   /*create_bypass_matcher=*/false);

  // The resource is not in any MDL type, so it should not match when matching
  // for any MDL type.
  GURL resource_url("http://example.com");
  EXPECT_EQ(
      matcher.Matches(resource_url,
                      /*top_frame_site=*/std::nullopt, MdlType::kIncognito,
                      /*skip_bypass_check=*/true),
      UrlMatcherWithBypassResult::kNoMatch);

  EXPECT_EQ(matcher.Matches(resource_url,
                            /*top_frame_site=*/std::nullopt,
                            MdlType::kRegularBrowsing,
                            /*skip_bypass_check=*/true),
            UrlMatcherWithBypassResult::kNoMatch);
}

TEST_F(UrlMatcherWithBypassTest, PartitionMapKey) {
  auto PartitionMapKey = &UrlMatcherWithBypass::PartitionMapKey;
  EXPECT_EQ(PartitionMapKey("com"), "com");
  EXPECT_EQ(PartitionMapKey("foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("www.tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("foo.co.uk"), "co.uk");
}

TEST_F(UrlMatcherWithBypassTest,
       GetEligibleDomains_DoesNotRemoveDomainsIfExclusionSetIsEmpty) {
  ResourceOwner resource_owner = masked_domain_list::ResourceOwner();
  const std::string expected_domain = "example.com";
  const std::string another_domain = "example2.com";
  resource_owner.set_owner_name("example");

  Resource* default_resource = resource_owner.add_owned_resources();
  default_resource->set_domain(expected_domain);
  Resource* another_resource = resource_owner.add_owned_resources();
  another_resource->set_domain(another_domain);

  std::set<std::string> eligible_domains =
      UrlMatcherWithBypass::GetEligibleDomains(
          resource_owner, /*excluded_domains=*/{});  // Empty exclusion set.

  EXPECT_EQ(eligible_domains,
            std::set<std::string>({expected_domain, another_domain}));
}

TEST_F(UrlMatcherWithBypassTest,
       GetEligibleDomains_RemovesDomainsIfPresentInExclusionSet) {
  ResourceOwner resource_owner = masked_domain_list::ResourceOwner();
  const std::string expected_domain = "example.com";
  const std::string excluded_domain = "excluded.com";
  resource_owner.set_owner_name("example");

  Resource* default_resource = resource_owner.add_owned_resources();
  default_resource->set_domain(expected_domain);
  Resource* excluded_resource = resource_owner.add_owned_resources();
  excluded_resource->set_domain(excluded_domain);

  std::set<std::string> eligible_domains =
      UrlMatcherWithBypass::GetEligibleDomains(
          resource_owner, /*excluded_domains=*/{excluded_domain});

  EXPECT_EQ(eligible_domains, std::set<std::string>({expected_domain}));
}

TEST_F(UrlMatcherWithBypassTest, AddRulesWithoutBypass_BypassCheckIsSkipped) {
  UrlMatcherWithBypass matcher;
  ResourceOwner resource_owner;

  resource_owner.set_owner_name("example");
  resource_owner.add_owned_resources()->set_domain("example.com");
  resource_owner.add_owned_properties("example.com");

  matcher.AddRules(resource_owner, /*excluded_domains=*/{},
                   /*create_bypass_matcher=*/false);

  EXPECT_EQ(
      matcher.Matches(GURL("http://example.com"),
                      /*top_frame_site=*/std::nullopt, MdlType::kIncognito,
                      /*skip_bypass_check=*/true),
      UrlMatcherWithBypassResult::kMatchAndNoBypass);
}

TEST_F(UrlMatcherWithBypassTest,
       AddRulesWithoutBypass_BypassCheckIsNotSkipped) {
  UrlMatcherWithBypass matcher;
  ResourceOwner resource_owner;

  resource_owner.set_owner_name("example");
  resource_owner.add_owned_resources()->set_domain("example.com");
  resource_owner.add_owned_properties("example.com");

  matcher.AddRules(resource_owner, /*excluded_domains=*/{},
                   /*create_bypass_matcher=*/false);

  EXPECT_EQ(matcher.Matches(GURL("http://example.com"),
                            net::SchemefulSite(GURL("http://top.frame.com")),
                            MdlType::kIncognito,
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndNoBypass);
}

TEST_F(UrlMatcherWithBypassTest,
       AddRulesWithoutBypass_RulesAreEvaluatedInCorrectOrder) {
  UrlMatcherWithBypass matcher;
  MaskedDomainList mdl;

  auto* resource_owner = mdl.add_resource_owners();

  resource_owner->set_owner_name("a.com");
  resource_owner->add_owned_properties("a.com");
  resource_owner->add_owned_resources()->set_domain("test.com");

  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("b.com");
  resource_owner->add_owned_properties("b.com");
  resource_owner->add_owned_resources()->set_domain("sub.test.com");

  for (auto owner : mdl.resource_owners()) {
    matcher.AddRules(owner, /*excluded_domains=*/{},
                     /*create_bypass_matcher=*/true);
  }

  // test.com should match with a.com when a.com is the top frame site.
  // test.com does not match with the sub.test.com rule, but it does match the
  // test.com rule.
  EXPECT_EQ(matcher.Matches(GURL("https://test.com"),
                            net::SchemefulSite(GURL("https://a.com")),
                            MdlType::kIncognito,
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndBypass);

  // sub.test.com should match with b.com when b.com is the top frame site.
  // Even though sub.test.com is a subdomain of test.com, it should match with
  // b.com since it's a more specific domain that that should have a rule.
  EXPECT_EQ(matcher.Matches(GURL("https://sub.test.com"),
                            net::SchemefulSite(GURL("https://b.com")),
                            MdlType::kIncognito,
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndBypass);

  // ssub.test.com should match with a.com. Since ssub.test.com does not have
  // its own matcher, it matches to the a.com *.test.com rule.
  EXPECT_EQ(matcher.Matches(GURL("https://ssub.test.com"),
                            net::SchemefulSite(GURL("https://a.com")),
                            MdlType::kIncognito,
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndBypass);
}

TEST_F(
    UrlMatcherWithBypassTest,
    AddRulesWithoutBypass_LongDomainDoesntDisruptMatcherOrderingAndMatching) {
  UrlMatcherWithBypass matcher;
  MaskedDomainList mdl;

  auto* resource_owner = mdl.add_resource_owners();

  resource_owner->set_owner_name("a.com");
  resource_owner->add_owned_properties("a.com");
  resource_owner->add_owned_resources()->set_domain("test.com");
  resource_owner->add_owned_resources()->set_domain("abtesting.com");

  resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("b.com");
  resource_owner->add_owned_properties("b.com");
  resource_owner->add_owned_resources()->set_domain("sub.test.com");

  for (auto owner : mdl.resource_owners()) {
    matcher.AddRules(owner, /*excluded_domains=*/{},
                     /*create_bypass_matcher=*/true);
  }

  // "abtesting.com" should match with a.com when "a.com" is the top frame site.
  // "abtesting.com" does not match with the "sub.test.com" rule, but it does
  // match the test.com rule.
  EXPECT_EQ(matcher.Matches(GURL("https://abtesting.com"),
                            net::SchemefulSite(GURL("https://a.com")),
                            MdlType::kIncognito,
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndBypass);

  // "sub.test.com" should match with "b.com" when "b.com" is the top frame
  // site.
  // This test ensures that the matchers aren't sorted with the matcher
  // containing "test.com" being first (due to "abtesting.com").
  EXPECT_EQ(matcher.Matches(GURL("https://sub.test.com"),
                            net::SchemefulSite(GURL("https://b.com")),
                            MdlType::kIncognito,
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndBypass);
}

class UrlMatcherWithBypassMatchTest : public testing::TestWithParam<MatchTest> {
};

TEST_P(UrlMatcherWithBypassMatchTest, Match) {
  UrlMatcherWithBypass matcher;
  MaskedDomainList mdl;

  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("acme");
  resourceOwner->add_owned_resources()->set_domain("acme-ra.com");
  resourceOwner->add_owned_resources()->set_domain("acme-rb.co.uk");
  resourceOwner->add_owned_properties("acme-pa.com");
  resourceOwner->add_owned_properties("acme-pb.co.uk");

  resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("bbco");
  resourceOwner->add_owned_resources()->set_domain("bbco-ra.com");
  resourceOwner->add_owned_resources()->set_domain("bbco-rb.co.ch");
  resourceOwner->add_owned_properties("bbco-pa.com");
  resourceOwner->add_owned_properties("bbco-pb.co.uk");

  for (auto owner : mdl.resource_owners()) {
    matcher.AddRules(owner, /*excluded_domains=*/{},
                     /*create_bypass_matcher=*/true);
  }

  const MatchTest& p = GetParam();
  GURL request_url(base::StrCat({"https://", p.req}));
  net::SchemefulSite top_frame_site(GURL(base::StrCat({"https://", p.top})));
  EXPECT_EQ(p.result,
            matcher.Matches(request_url, top_frame_site, MdlType::kIncognito,
                            p.skip_bypass_check));
}

const std::vector<MatchTest> kMatchTests = {
    // First-party requests should never be proxied.
    MatchTest{
        "1PRsrcHost",
        "acme-ra.com",
        "acme-ra.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "1PPropHost",
        "bbco-pb.co.uk",
        "bbco-pb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "1POtherHost",
        "somehost.com",
        "somehost.com",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },

    // "First-party" is defined as schemefully same-site.
    MatchTest{
        "1PSameSiteOther1",
        "www.somehost.com",
        "somehost.com",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "1PSameSiteOther2",
        "somehost.com",
        "www.somehost.com",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "1PSameSiteRsrc1",
        "www.acme-ra.com",
        "acme-ra.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "1PSameSiteRsrc2",
        "acme-ra.com",
        "www.acme-ra.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "1PSameSiteRsrcSub1",
        "sub.sub.acme-ra.com",
        "acme-ra.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "1PSameSiteRsrcSub2",
        "acme-ra.com",
        "sub.sub.acme-ra.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "1PSameSiteProp1",
        "www.bbco-pb.co.uk",
        "bbco-pb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "1PSameSiteProp2",
        "bbco-pb.co.uk",
        "www.bbco-pb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },

    // Third-party requests for hosts not appearing in the MDL should never be
    // proxied, regardless of the top-level.
    MatchTest{
        "3POtherReqInOther",
        "somehost.com",
        "otherhost.com",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "3POtherReqInRsrc",
        "somehost.com",
        "acme-rb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "3POtherReqInProp",
        "somehost.com",
        "bbco-pb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },

    // Third-party requests for resources (including subdomains) in the MDL
    // should be proxied (with exceptions below).
    MatchTest{
        "3PRsrcInOther",
        "acme-ra.com",
        "somehost.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },
    MatchTest{
        "3PRsrcInOtherRsrc",
        "acme-ra.com",
        "bbco-rb.co.ch",
        false,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },
    MatchTest{
        "3PRsrcInOtherProp",
        "acme-ra.com",
        "bbco-pa.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },
    MatchTest{
        "3PSubRsrc",
        "sub.acme-ra.com",
        "somehost.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },
    MatchTest{
        "3PSub2Rsrc",
        "sub.sub.acme-ra.com",
        "somehost.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },

    // Third-party requests for properties in the MDL should not be proxied
    // if bypass policy is kFirstPartyToTopLevelFrame.
    MatchTest{
        "3PPropInOther",
        "acme-pa.com",
        "somehost.com",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "3PPropInOtherRsrc",
        "acme-pa.com",
        "bbco-rb.co.ch",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "3PPropInOtherProp",
        "acme-pa.com",
        "bbco-pa.com",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "3PPropInSameRsrc",
        "acme-pa.com",
        "acme-rb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },
    MatchTest{
        "3PPropInSameProp",
        "acme-pa.com",
        "acme-pb.co.uk",
        false,
        UrlMatcherWithBypassResult::kNoMatch,
    },

    // As an exception, third-party requests for resources (including
    // subdomains) in the MDL should be not be proxied when the top-level site
    // is a property with the same owner as the resource if bypass policy is
    // kFirstPartyToTopLevelFrame.
    MatchTest{
        "3PRsrcInPropSameOwner",
        "acme-ra.com",
        "acme-pa.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "3PRsrcInRsrcSameOwner",
        "acme-ra.com",
        "acme-rb.co.uk",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "3PRsrcInSubRsrcSameOwner",
        "acme-ra.com",
        "sub.acme-rb.co.uk",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "3PSubRsrcInSubRsrcSameOwner",
        "sub.acme-ra.com",
        "sub.acme-rb.co.uk",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "3PSubSameOwner",
        "sub.acme-ra.com",
        "acme-pa.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },
    MatchTest{
        "3PSubSubSameOwner",
        "sub.sub.acme-ra.com",
        "acme-pa.com",
        false,
        UrlMatcherWithBypassResult::kMatchAndBypass,
    },

    // Skip the bypass check.
    MatchTest{
        "MatchWithSameSiteAndBypass",
        "acme-ra.com",
        "acme-ra.com",
        true,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },
    MatchTest{
        "MatchWithSameOwnerAndBypass",
        "acme-ra.com",
        "acme-pa.com",
        true,
        UrlMatcherWithBypassResult::kMatchAndNoBypass,
    },
    MatchTest{
        "NoMatchWithSameOwnerAndBypass",
        "safe.com",
        "acme-pa.com",
        true,
        UrlMatcherWithBypassResult::kNoMatch,
    },
};

INSTANTIATE_TEST_SUITE_P(All,
                         UrlMatcherWithBypassMatchTest,
                         testing::ValuesIn(kMatchTests),
                         [](const testing::TestParamInfo<MatchTest>& info) {
                           return info.param.name;
                         });

}  // namespace ip_protection
