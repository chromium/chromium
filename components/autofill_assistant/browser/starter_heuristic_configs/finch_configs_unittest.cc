// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_configs.h"
#include <array>
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant::finch_configs {
namespace {

constexpr std::array<const base::Feature*, 9> kHeuristicFeatures = {
    &features::kAutofillAssistantUrlHeuristic1,
    &features::kAutofillAssistantUrlHeuristic2,
    &features::kAutofillAssistantUrlHeuristic3,
    &features::kAutofillAssistantUrlHeuristic4,
    &features::kAutofillAssistantUrlHeuristic5,
    &features::kAutofillAssistantUrlHeuristic6,
    &features::kAutofillAssistantUrlHeuristic7,
    &features::kAutofillAssistantUrlHeuristic8,
    &features::kAutofillAssistantUrlHeuristic9};

TEST(FinchConfigsTest, AllUrlHeuristicsAreDisabledByDefault) {
  for (const auto* feature : kHeuristicFeatures) {
    EXPECT_FALSE(base::FeatureList::IsEnabled(*feature));
  }
}

TEST(FinchConfigsTest, UrlHeuristicsAreDistinct) {
  std::vector<base::test::FeatureRefAndParams> features_and_params;
  for (size_t i = 0; i < kHeuristicFeatures.size(); ++i) {
    features_and_params.push_back(
        {*kHeuristicFeatures[i],
         {{"json_parameters",
           base::StrCat({"{", R"("intent": ")", base::NumberToString(i + 1),
                         R"(", "heuristics":[]})"})}}});
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      features_and_params, /* disabled_features = */ {});

  EXPECT_EQ(GetOrCreateUrlHeuristic1()->GetIntent(), "1");
  EXPECT_EQ(GetOrCreateUrlHeuristic2()->GetIntent(), "2");
  EXPECT_EQ(GetOrCreateUrlHeuristic3()->GetIntent(), "3");
  EXPECT_EQ(GetOrCreateUrlHeuristic4()->GetIntent(), "4");
  EXPECT_EQ(GetOrCreateUrlHeuristic5()->GetIntent(), "5");
  EXPECT_EQ(GetOrCreateUrlHeuristic6()->GetIntent(), "6");
  EXPECT_EQ(GetOrCreateUrlHeuristic7()->GetIntent(), "7");
  EXPECT_EQ(GetOrCreateUrlHeuristic8()->GetIntent(), "8");
  EXPECT_EQ(GetOrCreateUrlHeuristic9()->GetIntent(), "9");
}

}  // namespace
}  // namespace autofill_assistant::finch_configs
