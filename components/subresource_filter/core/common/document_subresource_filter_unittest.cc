// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/document_subresource_filter.h"

#include <string_view>

#include "base/files/file.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

namespace {

constexpr auto kDryRun = mojom::ActivationLevel::kDryRun;
constexpr auto kEnabled = mojom::ActivationLevel::kEnabled;

constexpr auto kImageType = proto::ELEMENT_TYPE_IMAGE;
constexpr auto kSubdocumentType = proto::ELEMENT_TYPE_SUBDOCUMENT;

constexpr const char kTestAlphaURL[] = "http://example.com/alpha";
constexpr const char kTestAlphaDataURI[] = "data:text/plain,alpha";
constexpr const char kTestAlphaWSURI[] = "ws://example.com/alpha";
constexpr const char kTestBetaURL[] = "http://example.com/beta";

constexpr const char kSubresourceLoadEvaluationWallDurationHistogram[] =
    "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration";
constexpr const char kSubresourceLoadEvaluationCPUDurationHistogram[] =
    "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration";

constexpr const char kTestAlphaURLPathSuffix[] = "alpha";

}  // namespace

// Tests for DocumentSubresourceFilter class. ----------------------------------

class DocumentSubresourceFilterTest : public ::testing::Test {
 public:
  DocumentSubresourceFilterTest() = default;

  DocumentSubresourceFilterTest(const DocumentSubresourceFilterTest&) = delete;
  DocumentSubresourceFilterTest& operator=(
      const DocumentSubresourceFilterTest&) = delete;

 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        SetTestRulesetToDisallowURLsWithPathSuffix(kTestAlphaURLPathSuffix));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(std::string_view suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_ = MemoryMappedRuleset::CreateAndInitialize(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  const MemoryMappedRuleset* ruleset() { return ruleset_.get(); }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  scoped_refptr<const MemoryMappedRuleset> ruleset_;
};

TEST_F(DocumentSubresourceFilterTest, DryRun) {
  base::HistogramTester histogram_tester;
  mojom::ActivationState activation_state;
  activation_state.activation_level = kDryRun;
  activation_state.measure_performance = true;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset(),
                                   kSafeBrowsingRulesetConfig.uma_tag);

  EXPECT_EQ(LoadPolicy::WOULD_DISALLOW,
            filter.GetLoadPolicy(GURL(kTestAlphaURL), kImageType));
  EXPECT_EQ(LoadPolicy::ALLOW,
            filter.GetLoadPolicy(GURL(kTestAlphaDataURI), kImageType));
  EXPECT_EQ(
      LoadPolicy::WOULD_DISALLOW,
      filter.GetLoadPolicy(GURL(kTestAlphaWSURI), proto::ELEMENT_TYPE_OTHER));
  EXPECT_EQ(LoadPolicy::ALLOW,
            filter.GetLoadPolicy(GURL(kTestBetaURL), kImageType));
  EXPECT_EQ(LoadPolicy::WOULD_DISALLOW,
            filter.GetLoadPolicy(GURL(kTestAlphaURL), kSubdocumentType));
  EXPECT_EQ(LoadPolicy::ALLOW,
            filter.GetLoadPolicy(GURL(kTestBetaURL), kSubdocumentType));

  const auto& statistics = filter.statistics();
  EXPECT_EQ(6, statistics.num_loads_total);
  EXPECT_EQ(5, statistics.num_loads_evaluated);
  EXPECT_EQ(3, statistics.num_loads_matching_rules);
  EXPECT_EQ(0, statistics.num_loads_disallowed);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationWallDurationHistogram, 5);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationCPUDurationHistogram, 5);
}

TEST_F(DocumentSubresourceFilterTest, MatchingRuleDryRun) {
  mojom::ActivationState activation_state;
  activation_state.activation_level = kDryRun;
  activation_state.measure_performance = false;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset(),
                                   kSafeBrowsingRulesetConfig.uma_tag);

  EXPECT_NE(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestAlphaURL), kImageType));
  EXPECT_EQ(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestAlphaDataURI), kImageType));
  EXPECT_NE(nullptr, filter.FindMatchingUrlRule(GURL(kTestAlphaWSURI),
                                                proto::ELEMENT_TYPE_OTHER));
  EXPECT_EQ(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestBetaURL), kImageType));
  EXPECT_NE(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestAlphaURL), kSubdocumentType));
  EXPECT_EQ(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestBetaURL), kSubdocumentType));
}

TEST_F(DocumentSubresourceFilterTest, Enabled) {
  base::HistogramTester histogram_tester;

  auto test_impl = [this](bool measure_performance) {
    mojom::ActivationState activation_state;
    activation_state.activation_level = kEnabled;
    activation_state.measure_performance = measure_performance;
    DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset(),
                                     kSafeBrowsingRulesetConfig.uma_tag);

    EXPECT_EQ(LoadPolicy::DISALLOW,
              filter.GetLoadPolicy(GURL(kTestAlphaURL), kImageType));
    EXPECT_EQ(LoadPolicy::ALLOW,
              filter.GetLoadPolicy(GURL(kTestAlphaDataURI), kImageType));
    EXPECT_EQ(
        LoadPolicy::DISALLOW,
        filter.GetLoadPolicy(GURL(kTestAlphaWSURI), proto::ELEMENT_TYPE_OTHER));
    EXPECT_EQ(LoadPolicy::ALLOW,
              filter.GetLoadPolicy(GURL(kTestBetaURL), kImageType));
    EXPECT_EQ(LoadPolicy::DISALLOW,
              filter.GetLoadPolicy(GURL(kTestAlphaURL), kSubdocumentType));
    EXPECT_EQ(LoadPolicy::ALLOW,
              filter.GetLoadPolicy(GURL(kTestBetaURL), kSubdocumentType));

    const auto& statistics = filter.statistics();
    EXPECT_EQ(6, statistics.num_loads_total);
    EXPECT_EQ(5, statistics.num_loads_evaluated);
    EXPECT_EQ(3, statistics.num_loads_matching_rules);
    EXPECT_EQ(3, statistics.num_loads_disallowed);

    if (!measure_performance) {
      EXPECT_TRUE(statistics.evaluation_total_cpu_duration.is_zero());
      EXPECT_TRUE(statistics.evaluation_total_wall_duration.is_zero());
    }
    // Otherwise, don't expect |total_duration| to be non-zero, although it
    // practically is (when timer is supported).
  };

  test_impl(true /* measure_performance */);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationWallDurationHistogram, 5);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationCPUDurationHistogram, 5);
  test_impl(false /* measure_performance */);
  // No more histograms should have been emitted since the last run since
  // performance measurements are turned off.
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationWallDurationHistogram, 5);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadEvaluationCPUDurationHistogram, 5);
}

TEST_F(DocumentSubresourceFilterTest, MatchingRuleEnabled) {
  mojom::ActivationState activation_state;
  activation_state.activation_level = kEnabled;
  activation_state.measure_performance = false;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset(),
                                   kSafeBrowsingRulesetConfig.uma_tag);

  EXPECT_NE(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestAlphaURL), kImageType));
  EXPECT_EQ(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestAlphaDataURI), kImageType));
  EXPECT_NE(nullptr, filter.FindMatchingUrlRule(GURL(kTestAlphaWSURI),
                                                proto::ELEMENT_TYPE_OTHER));
  EXPECT_EQ(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestBetaURL), kImageType));
  EXPECT_NE(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestAlphaURL), kSubdocumentType));
  EXPECT_EQ(nullptr,
            filter.FindMatchingUrlRule(GURL(kTestBetaURL), kSubdocumentType));
}

TEST_F(DocumentSubresourceFilterTest, StyleRules) {
  RulesetIndexer indexer(0x12345678);
  {
    proto::StyleRule rule;
    rule.set_selector("#ad-div");
    rule.add_domains()->set_domain("example.com");
    rule.add_ids("ad-div");
    indexer.AddStyleRuleFromProto(rule);
  }
  {
    proto::StyleRule rule;
    rule.set_selector(".ad-class");
    rule.add_classes("ad-class");
    indexer.AddStyleRuleFromProto(rule);
  }
  indexer.Finish();

  testing::TestRuleset indexed_ruleset;
  testing::TestRulesetCreator ruleset_creator;
  ruleset_creator.CreateTestRulesetFromContents(
      std::vector<uint8_t>(indexer.data().begin(), indexer.data().end()),
      &indexed_ruleset);

  scoped_refptr<const MemoryMappedRuleset> ruleset =
      MemoryMappedRuleset::CreateAndInitialize(
          testing::TestRuleset::Open(indexed_ruleset));
  ASSERT_TRUE(ruleset);

  // Test with filtering enabled.
  mojom::ActivationState activation_state;
  activation_state.activation_level = kEnabled;
  DocumentSubresourceFilter filter(
      url::Origin::Create(GURL("http://example.com")), activation_state,
      ruleset.get(), kSafeBrowsingRulesetConfig.uma_tag);

  // MaybeHasStyleRule: only global rules are added to the Bloom filter.
  EXPECT_FALSE(filter.MaybeHasStyleRule(GetStyleRuleHash("ad-div")));
  EXPECT_TRUE(filter.MaybeHasStyleRule(GetStyleRuleHash("ad-class")));
  EXPECT_FALSE(filter.MaybeHasStyleRule(GetStyleRuleHash("nonexistent")));

  // GetDomainSelectors: should retrieve site-specific rules for this domain.
  std::vector<std::string_view> domain_selectors;
  filter.GetDomainSelectors(domain_selectors);
  ASSERT_EQ(1U, domain_selectors.size());
  EXPECT_EQ("#ad-div", domain_selectors[0]);

  // GetSelectorsByClass: should retrieve global class rules.
  std::vector<std::string_view> class_selectors;
  filter.GetSelectorsByClass("ad-class", GetStyleRuleHash("ad-class"),
                             class_selectors);
  ASSERT_EQ(1U, class_selectors.size());
  EXPECT_EQ(".ad-class", class_selectors[0]);

  // GetSelectorsById: should NOT retrieve site-specific ID rules (they are only
  // in domain_map).
  std::vector<std::string_view> id_selectors;
  filter.GetSelectorsById("ad-div", GetStyleRuleHash("ad-div"), id_selectors);
  EXPECT_TRUE(id_selectors.empty());

  // GetRulesetId
  EXPECT_EQ(0x12345678U, filter.GetRulesetId());

  // Test with filtering disabled for document.
  mojom::ActivationState disabled_state;
  disabled_state.activation_level = kEnabled;
  disabled_state.filtering_disabled_for_document = true;
  DocumentSubresourceFilter disabled_filter(
      url::Origin::Create(GURL("http://example.com")), disabled_state,
      ruleset.get(), kSafeBrowsingRulesetConfig.uma_tag);

  EXPECT_FALSE(disabled_filter.MaybeHasStyleRule(GetStyleRuleHash("ad-class")));

  std::vector<std::string_view> disabled_domain_selectors;
  disabled_filter.GetDomainSelectors(disabled_domain_selectors);
  EXPECT_TRUE(disabled_domain_selectors.empty());

  std::vector<std::string_view> disabled_class_selectors;
  disabled_filter.GetSelectorsByClass("ad-class", GetStyleRuleHash("ad-class"),
                                      disabled_class_selectors);
  EXPECT_TRUE(disabled_class_selectors.empty());

  std::vector<std::string_view> disabled_id_selectors;
  disabled_filter.GetSelectorsById("ad-div", GetStyleRuleHash("ad-div"),
                                   disabled_id_selectors);
  EXPECT_TRUE(disabled_id_selectors.empty());

  // Ruleset ID should still be available even if filtering is disabled.
  EXPECT_EQ(0x12345678U, disabled_filter.GetRulesetId());
}

}  // namespace subresource_filter
