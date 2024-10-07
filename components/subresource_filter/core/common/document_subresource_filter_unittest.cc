// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/document_subresource_filter.h"

#include <string_view>

#include "base/files/file.h"
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
  mojom::ActivationState activation_state;
  activation_state.activation_level = kDryRun;
  activation_state.measure_performance = true;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset());

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
}

TEST_F(DocumentSubresourceFilterTest, MatchingRuleDryRun) {
  mojom::ActivationState activation_state;
  activation_state.activation_level = kDryRun;
  activation_state.measure_performance = false;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset());

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
  auto test_impl = [this](bool measure_performance) {
    mojom::ActivationState activation_state;
    activation_state.activation_level = kEnabled;
    activation_state.measure_performance = measure_performance;
    DocumentSubresourceFilter filter(url::Origin(), activation_state,
                                     ruleset());

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
  test_impl(false /* measure_performance */);
}

TEST_F(DocumentSubresourceFilterTest, MatchingRuleEnabled) {
  mojom::ActivationState activation_state;
  activation_state.activation_level = kEnabled;
  activation_state.measure_performance = false;
  DocumentSubresourceFilter filter(url::Origin(), activation_state, ruleset());

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

}  // namespace subresource_filter
