// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/style_rule_matcher.h"

#include <memory>
#include <string_view>
#include <vector>

#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/style_rule_indexer.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

using testing::StyleRuleParams;

class StyleRuleMatcherTest : public ::testing::Test {
 protected:
  StyleRuleMatcherTest() : indexer_(&builder_) {}

  bool AddStyleRule(const testing::StyleRuleParams& params) {
    return indexer_.AddStyleRuleFromProto(testing::CreateStyleRule(params));
  }

  void Finish() {
    auto offset = indexer_.Finish();
    builder_.Finish(offset);

    const flat::StyleRuleIndex* index =
        flatbuffers::GetRoot<flat::StyleRuleIndex>(builder_.GetBufferPointer());
    matcher_ = std::make_unique<StyleRuleMatcher>(index);
  }

  flatbuffers::FlatBufferBuilder builder_;
  StyleRuleIndexer indexer_;
  std::unique_ptr<StyleRuleMatcher> matcher_;
};

TEST_F(StyleRuleMatcherTest, StyleRules) {
  AddStyleRule(StyleRuleParams().SetSelector("#ad-1").SetIds({"ad-1"}));
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-2")
                   .SetDomains({"example.com"})
                   .SetIds({"ad-2"}));

  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-4")
                   .SetDomains({"sub.example.com", "other.com"})
                   .SetIds({"ad-4"}));
  Finish();

  std::vector<std::string_view> rules;

  matcher_->GetDomainSelectors(
      url::Origin::Create(GURL("http://non-example.com")), rules);
  EXPECT_TRUE(rules.empty());

  rules.clear();
  matcher_->GetDomainSelectors(url::Origin::Create(GURL("http://example.com")),
                               rules);
  EXPECT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-2", rules[0]);

  rules.clear();
  matcher_->GetDomainSelectors(
      url::Origin::Create(GURL("http://sub.example.com")), rules);
  EXPECT_EQ(2u, rules.size());
  // Order depends on domain map sorting.
  EXPECT_EQ("#ad-4", rules[0]);
  EXPECT_EQ("#ad-2", rules[1]);

  rules.clear();
  matcher_->GetDomainSelectors(url::Origin::Create(GURL("http://other.com")),
                               rules);
  EXPECT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-4", rules[0]);
}

TEST_F(StyleRuleMatcherTest, ClassMatching) {
  AddStyleRule(
      StyleRuleParams().SetSelector(".class-1").SetClasses({"class-1"}));
  AddStyleRule(
      StyleRuleParams().SetSelector(".class-2").SetClasses({"class-2"}));
  Finish();

  std::vector<std::string_view> rules;

  // Lookup by class "class-1"
  matcher_->GetSelectorsByClass(url::Origin(), "class-1",
                                GetStyleRuleHash("class-1"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class-1", rules[0]);

  // Lookup by class "class-3" (non-existent)
  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "class-3",
                                GetStyleRuleHash("class-3"), rules);
  EXPECT_TRUE(rules.empty());
}

TEST_F(StyleRuleMatcherTest, IdMatching) {
  AddStyleRule(StyleRuleParams().SetSelector("#id-1").SetIds({"id-1"}));
  AddStyleRule(StyleRuleParams().SetSelector("#id-2").SetIds({"id-2"}));
  Finish();

  std::vector<std::string_view> rules;

  // Lookup by ID "id-1"
  matcher_->GetSelectorsById(url::Origin(), "id-1", GetStyleRuleHash("id-1"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#id-1", rules[0]);

  // Lookup by ID "id-3" (non-existent)
  rules.clear();
  matcher_->GetSelectorsById(url::Origin(), "id-3", GetStyleRuleHash("id-3"),
                             rules);
  EXPECT_TRUE(rules.empty());
}

TEST_F(StyleRuleMatcherTest, Exclusions) {
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-1")
                   .SetDomains({"example.com"})
                   .SetExclusion(true)
                   .SetIds({"ad-1"}));
  AddStyleRule(StyleRuleParams().SetSelector("#ad-1").SetIds({"ad-1"}));

  AddStyleRule(StyleRuleParams()
                   .SetSelector(".class-1")
                   .SetDomains({"example.com"})
                   .SetExclusion(true)
                   .SetClasses({"class-1"}));
  AddStyleRule(
      StyleRuleParams().SetSelector(".class-1").SetClasses({"class-1"}));
  Finish();

  std::vector<std::string_view> rules;

  url::Origin example_origin = url::Origin::Create(GURL("http://example.com"));
  url::Origin other_origin =
      url::Origin::Create(GURL("http://other-domain.com"));

  // 1. ID Lookups

  // On other-domain.com: rule is NOT excluded.
  matcher_->GetSelectorsById(other_origin, "ad-1", GetStyleRuleHash("ad-1"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-1", rules[0]);

  // On example.com: rule is EXCLUDED.
  rules.clear();
  matcher_->GetSelectorsById(example_origin, "ad-1", GetStyleRuleHash("ad-1"),
                             rules);
  EXPECT_TRUE(rules.empty());

  // Repeat lookup on example.com (should hit cache).
  rules.clear();
  matcher_->GetSelectorsById(example_origin, "ad-1", GetStyleRuleHash("ad-1"),
                             rules);
  EXPECT_TRUE(rules.empty());

  // Repeat lookup on other-domain.com (should update cache).
  rules.clear();
  matcher_->GetSelectorsById(other_origin, "ad-1", GetStyleRuleHash("ad-1"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-1", rules[0]);

  // 2. Class Lookups

  // On other-domain.com: rule is NOT excluded.
  rules.clear();
  matcher_->GetSelectorsByClass(other_origin, "class-1",
                                GetStyleRuleHash("class-1"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class-1", rules[0]);

  // On example.com: rule is EXCLUDED.
  rules.clear();
  matcher_->GetSelectorsByClass(example_origin, "class-1",
                                GetStyleRuleHash("class-1"), rules);
  EXPECT_TRUE(rules.empty());

  // Repeat lookup on example.com (should hit cache).
  rules.clear();
  matcher_->GetSelectorsByClass(example_origin, "class-1",
                                GetStyleRuleHash("class-1"), rules);
  EXPECT_TRUE(rules.empty());

  // Repeat lookup on other-domain.com (should update cache).
  rules.clear();
  matcher_->GetSelectorsByClass(other_origin, "class-1",
                                GetStyleRuleHash("class-1"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class-1", rules[0]);
}

TEST_F(StyleRuleMatcherTest, CombinedClassAndId) {
  AddStyleRule(StyleRuleParams()
                   .SetSelector(".class-1#id-1")
                   .SetClasses({"class-1"})
                   .SetIds({"id-1"}));
  Finish();

  std::vector<std::string_view> rules;

  // Lookup by class
  matcher_->GetSelectorsByClass(url::Origin(), "class-1",
                                GetStyleRuleHash("class-1"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class-1#id-1", rules[0]);

  // Lookup by ID
  rules.clear();
  matcher_->GetSelectorsById(url::Origin(), "id-1", GetStyleRuleHash("id-1"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class-1#id-1", rules[0]);
}

TEST_F(StyleRuleMatcherTest, OpaqueOriginWithoutPrecursor) {
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-1")
                   .SetDomains({"example.com"})
                   .SetIds({"ad-1"}));
  Finish();

  std::vector<std::string_view> rules;

  // Opaque origin without precursor should not match domain-specific selectors.
  url::Origin opaque_origin;
  EXPECT_TRUE(opaque_origin.opaque());
  EXPECT_TRUE(opaque_origin.GetTupleOrPrecursorTupleIfOpaque().host().empty());
  matcher_->GetDomainSelectors(opaque_origin, rules);
  EXPECT_TRUE(rules.empty());
}

TEST_F(StyleRuleMatcherTest, OpaqueOriginWithPrecursor) {
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-1")
                   .SetDomains({"example.com"})
                   .SetIds({"ad-1"}));
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-2")
                   .SetDomains({"example.com"})
                   .SetExclusion(true)
                   .SetIds({"ad-2"}));
  AddStyleRule(StyleRuleParams().SetSelector("#ad-2").SetIds({"ad-2"}));
  Finish();

  std::vector<std::string_view> rules;

  // Opaque origin with precursor should match domain-specific selectors of
  // precursor.
  url::Origin base_origin = url::Origin::Create(GURL("http://example.com"));
  url::Origin opaque_origin =
      url::Origin::Resolve(GURL("data:text/html,foo"), base_origin);
  EXPECT_TRUE(opaque_origin.opaque());
  EXPECT_EQ("example.com",
            opaque_origin.GetTupleOrPrecursorTupleIfOpaque().host());

  matcher_->GetDomainSelectors(opaque_origin, rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-1", rules[0]);

  // Test exclusions with opaque origin.
  rules.clear();
  matcher_->GetSelectorsById(opaque_origin, "ad-2", GetStyleRuleHash("ad-2"),
                             rules);
  EXPECT_TRUE(rules.empty());  // Should be excluded on example.com (precursor)

  // Test on another origin (opaque but different precursor).
  url::Origin other_base = url::Origin::Create(GURL("http://other.com"));
  url::Origin other_opaque =
      url::Origin::Resolve(GURL("data:text/html,foo"), other_base);
  rules.clear();
  matcher_->GetSelectorsById(other_opaque, "ad-2", GetStyleRuleHash("ad-2"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#ad-2", rules[0]);  // Should NOT be excluded on other.com
}

TEST_F(StyleRuleMatcherTest, BloomFilterShortCircuit) {
  AddStyleRule(
      StyleRuleParams().SetSelector(".class-1").SetClasses({"class-1"}));
  Finish();

  std::vector<std::string_view> rules;

  // Lookup by class "class-1" with CORRECT hash should find it.
  matcher_->GetSelectorsByClass(url::Origin(), "class-1",
                                GetStyleRuleHash("class-1"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class-1", rules[0]);

  // Lookup by class "class-1" with INCORRECT hash should NOT find it
  // (short-circuit). We use the hash of "non-existent" which is highly unlikely
  // to be in the filter.
  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "class-1",
                                GetStyleRuleHash("non-existent"), rules);
  EXPECT_TRUE(rules.empty());
}

TEST_F(StyleRuleMatcherTest, PreParsedAnchorIndexing) {
  // Test that the indexer trusts the anchors provided in the proto.
  AddStyleRule(StyleRuleParams().SetSelector("#fast-id").SetIds({"fast-id"}));
  AddStyleRule(
      StyleRuleParams().SetSelector(".fast-class").SetClasses({"fast-class"}));
  AddStyleRule(StyleRuleParams()
                   .SetSelector(".parent > .child")
                   .SetClasses({"parent", "child"}));

  AddStyleRule(StyleRuleParams()
                   .SetSelector(".fast-class-with-attr[attr=val]")
                   .SetClasses({"fast-class-with-attr"}));
  AddStyleRule(StyleRuleParams()
                   .SetSelector("div[attr=site-specific]")
                   .SetDomains({"site.com"}));
  Finish();

  std::vector<std::string_view> rules;
  matcher_->GetDomainSelectors(url::Origin::Create(GURL("http://example.com")),
                               rules);

  // Global selectors are not in the domain index.
  EXPECT_EQ(0u, rules.size());

  // Verified lookup by ID.
  rules.clear();
  matcher_->GetSelectorsById(url::Origin(), "fast-id",
                             GetStyleRuleHash("fast-id"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#fast-id", rules[0]);

  // Verified lookup by Class.
  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "fast-class",
                                GetStyleRuleHash("fast-class"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".fast-class", rules[0]);

  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "child",
                                GetStyleRuleHash("child"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".parent > .child", rules[0]);

  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "parent",
                                GetStyleRuleHash("parent"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".parent > .child", rules[0]);

  // Verified site-specific selector is in the domain index.
  rules.clear();
  matcher_->GetDomainSelectors(url::Origin::Create(GURL("http://site.com")),
                               rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("div[attr=site-specific]", rules[0]);
}

TEST_F(StyleRuleMatcherTest, MultipleRulesAndKeys) {
  AddStyleRule(StyleRuleParams().SetSelector("#id1").SetIds({"id1"}));
  AddStyleRule(StyleRuleParams().SetSelector(".class1").SetClasses({"class1"}));
  AddStyleRule(StyleRuleParams()
                   .SetSelector(".class1.class2")
                   .SetClasses({"class1", "class2"}));
  Finish();

  std::vector<std::string_view> rules;

  // Lookup by class "class1"
  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "class1",
                                GetStyleRuleHash("class1"), rules);
  ASSERT_EQ(2u, rules.size());
  EXPECT_EQ(".class1", rules[0]);
  EXPECT_EQ(".class1.class2", rules[1]);

  // Lookup by class "class2"
  rules.clear();
  matcher_->GetSelectorsByClass(url::Origin(), "class2",
                                GetStyleRuleHash("class2"), rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ(".class1.class2", rules[0]);

  // Lookup by ID "id1"
  rules.clear();
  matcher_->GetSelectorsById(url::Origin(), "id1", GetStyleRuleHash("id1"),
                             rules);
  ASSERT_EQ(1u, rules.size());
  EXPECT_EQ("#id1", rules[0]);
}

TEST_F(StyleRuleMatcherTest, MaybeHasStyleRule) {
  AddStyleRule(StyleRuleParams().SetSelector("#ad-1").SetIds({"ad-1"}));
  AddStyleRule(
      StyleRuleParams().SetSelector(".class-1").SetClasses({"class-1"}));
  Finish();

  // Hashes that exist should return true.
  EXPECT_TRUE(matcher_->MaybeHasStyleRule(GetStyleRuleHash("ad-1")));
  EXPECT_TRUE(matcher_->MaybeHasStyleRule(GetStyleRuleHash("class-1")));

  // Hash that does not exist should return false.
  EXPECT_FALSE(matcher_->MaybeHasStyleRule(GetStyleRuleHash("non-existent")));
}

// This test documents the existing behavior that suffix stripping applies to IP
// addresses as well as domains. It is not particularly important that it
// behaves this way, but this test ensures we are aware of it.
TEST_F(StyleRuleMatcherTest, IPAddressDomain) {
  AddStyleRule(StyleRuleParams()
                   .SetSelector("#ad-1")
                   .SetDomains({"127.0.0.1"})
                   .SetIds({"ad-1"}));
  AddStyleRule(
      StyleRuleParams().SetSelector("#ad-2").SetDomains({"0.0.1"}).SetIds(
          {"ad-2"}));
  Finish();

  std::vector<std::string_view> rules;
  url::Origin ip_origin = url::Origin::Create(GURL("http://127.0.0.1"));

  matcher_->GetDomainSelectors(ip_origin, rules);
  // Suffix stripping applies to IP addresses too.
  ASSERT_EQ(2u, rules.size());
  EXPECT_EQ("#ad-1", rules[0]);
  EXPECT_EQ("#ad-2", rules[1]);
}

}  // namespace subresource_filter
