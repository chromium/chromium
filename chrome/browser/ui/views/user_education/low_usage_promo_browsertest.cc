// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/low_usage_promo.h"

#include <sstream>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class LowUsagePromoBrowsertest
    : public policy::PolicyTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  LowUsagePromoBrowsertest() = default;
  ~LowUsagePromoBrowsertest() override = default;

  bool GetAiEnabled() const { return std::get<0>(GetParam()); }

  bool GetUseAlternateSearchProvider() const { return std::get<1>(GetParam()); }

  void SetUp() override {
    const std::string enabled = GetAiEnabled() ? "true" : "false";
    feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHDesktopReEngagementFeature,
        {{"include_ai", enabled}});
    PolicyTest::SetUp();
  }

  void SetUpOnMainThread() override {
    if (GetUseAlternateSearchProvider()) {
      policy::PolicyMap policies;
      policies.Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
      policies.Set(policy::key::kDefaultSearchProviderKeyword,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value("testsearch"),
                   nullptr);
      policies.Set(policy::key::kDefaultSearchProviderSearchURL,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("http://search.example/search?q={searchTerms}"),
                   nullptr);
      policies.Set(policy::key::kDefaultSearchProviderNewTabURL,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("http://search.example/newtab"), nullptr);
      UpdateProviderPolicy(policies);
      CHECK(!search::DefaultSearchProviderIsGoogle(browser()->profile()));
    }
  }

  bool HasPromo(const user_education::FeaturePromoSpecification& spec,
                int string_id) {
    for (const auto& promo : spec.rotating_promos()) {
      if (promo && promo->bubble_body_string_id() == string_id) {
        return true;
      }
    }
    return false;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

std::string TestParamToString(
    testing::TestParamInfo<std::tuple<bool, bool>> param) {
  std::ostringstream oss;
  if (std::get<0>(param.param)) {
    oss << "AI_";
  } else {
    oss << "NoAI_";
  }
  if (std::get<1>(param.param)) {
    oss << "AltSearch";
  } else {
    oss << "GoogleSearch";
  }
  return oss.str();
}

INSTANTIATE_TEST_SUITE_P(,
                         LowUsagePromoBrowsertest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         TestParamToString);

IN_PROC_BROWSER_TEST_P(LowUsagePromoBrowsertest, EnsureCorrectPromos) {
  const auto spec = CreateLowUsagePromoSpecification(browser()->profile());
  EXPECT_EQ(GetAiEnabled(), HasPromo(spec, IDS_REENGAGEMENT_PROMO_EXPLORE_AI));
  EXPECT_NE(GetAiEnabled(), HasPromo(spec, IDS_REENGAGEMENT_PROMO_AI_BENEFITS));
  EXPECT_NE(GetUseAlternateSearchProvider(),
            HasPromo(spec, IDS_REENGAGEMENT_PROMO_CUSTOMIZE_COLOR));
}
