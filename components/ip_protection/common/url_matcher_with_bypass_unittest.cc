// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/url_matcher_with_bypass.h"

#include <optional>
#include <vector>

#include "base/strings/strcat.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {
using ::masked_domain_list::MaskedDomainList;

struct MatchTest {
  std::string name;
  std::string req;
  std::string top;
  bool skip_bypass_check;
  UrlMatcherWithBypassResult result;
};

}  // namespace

class UrlMatcherWithBypassTest : public ::testing::Test {};

TEST_F(UrlMatcherWithBypassTest, PartitionMapKey) {
  auto PartitionMapKey = &UrlMatcherWithBypass::PartitionMapKey;
  EXPECT_EQ(PartitionMapKey("com"), "com");
  EXPECT_EQ(PartitionMapKey("foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("www.tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("foo.co.uk"), "co.uk");
}

TEST_F(UrlMatcherWithBypassTest, BuildBypassMatcher_Dedupes) {
  auto resource_owner = masked_domain_list::ResourceOwner();
  resource_owner.add_owned_properties("example.com");
  resource_owner.add_owned_properties("example2.com");
  auto* resource = resource_owner.add_owned_resources();
  resource->set_domain("example.com");
  auto bypass_matcher =
      UrlMatcherWithBypass::BuildBypassMatcher(resource_owner);

  // 2 distinct domains become 4 rules because of subdomain matching rules.
  EXPECT_EQ(bypass_matcher->rules().size(), 4u);
}

TEST_F(UrlMatcherWithBypassTest, AddRulesWithoutBypass_BypassCheckIsSkipped) {
  UrlMatcherWithBypass matcher;

  matcher.AddRulesWithoutBypass({"example.com"});
  EXPECT_EQ(matcher.Matches(GURL("http://example.com"),
                            /*top_frame_site=*/std::nullopt,
                            /*skip_bypass_check=*/true),
            UrlMatcherWithBypassResult::kMatchAndNoBypass);
}

TEST_F(UrlMatcherWithBypassTest,
       AddRulesWithoutBypass_BypassCheckIsNotSkipped) {
  UrlMatcherWithBypass matcher;

  matcher.AddRulesWithoutBypass({"example.com"});
  EXPECT_EQ(matcher.Matches(GURL("http://example.com"),
                            net::SchemefulSite(GURL("http://top.frame.com")),
                            /*skip_bypass_check=*/false),
            UrlMatcherWithBypassResult::kMatchAndNoBypass);
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
    std::set<std::string> domains;
    for (auto resource : owner.owned_resources()) {
      domains.emplace(resource.domain());
    }
    matcher.AddMaskedDomainListRules(domains, owner);
  }

  const MatchTest& p = GetParam();
  GURL request_url(base::StrCat({"https://", p.req}));
  net::SchemefulSite top_frame_site(GURL(base::StrCat({"https://", p.top})));
  EXPECT_EQ(p.result,
            matcher.Matches(request_url, top_frame_site, p.skip_bypass_check));
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
