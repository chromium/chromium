// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

#import "base/test/scoped_feature_list.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

using AutofillFeaturesIOSTest = PlatformTest;

// Tests that `kAutofillSuggestionPipelineIOS` with different stage parameters
// correctly configures the cumulative suggestion pipeline predicates.
TEST_F(AutofillFeaturesIOSTest, SuggestionPipelineStagePredicates) {
  struct TestCase {
    const char* stage_param;
    AutofillSuggestionPipelineStage expected_stage;
    bool expected_bam_enabled;
  };

  const TestCase kTestCases[] = {
      {"0", AutofillSuggestionPipelineStage::kLegacy, false},
      {"1", AutofillSuggestionPipelineStage::kBAMAutofill, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Stage param: " << test_case.stage_param);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kAutofillSuggestionPipelineIOS,
        {{kAutofillSuggestionPipelineStageParamName, test_case.stage_param}});

    EXPECT_EQ(test_case.expected_stage, GetAutofillSuggestionPipelineStage());
    EXPECT_EQ(test_case.expected_bam_enabled,
              IsAutofillBAMSuggestionFlowEnabled());
  }
}

// Tests that the suggestion pipeline stays on the legacy stage when
// `kAutofillSuggestionPipelineIOS` is disabled.
TEST_F(AutofillFeaturesIOSTest, SuggestionPipelineDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAutofillSuggestionPipelineIOS);

  EXPECT_EQ(AutofillSuggestionPipelineStage::kLegacy,
            GetAutofillSuggestionPipelineStage());
  EXPECT_FALSE(IsAutofillBAMSuggestionFlowEnabled());
}

// Tests that enabling `kAutofillSuggestionPipelineIOS` without a stage
// parameter (e.g. "Enabled" in chrome://flags) or with an unknown stage value
// falls back to `AutofillSuggestionPipelineStage::kLegacy`.
TEST_F(AutofillFeaturesIOSTest, SuggestionPipelineDefaultStage) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kAutofillSuggestionPipelineIOS);
    EXPECT_EQ(AutofillSuggestionPipelineStage::kLegacy,
              GetAutofillSuggestionPipelineStage());
    EXPECT_FALSE(IsAutofillBAMSuggestionFlowEnabled());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kAutofillSuggestionPipelineIOS,
        {{kAutofillSuggestionPipelineStageParamName, "invalid"}});
    EXPECT_EQ(AutofillSuggestionPipelineStage::kLegacy,
              GetAutofillSuggestionPipelineStage());
    EXPECT_FALSE(IsAutofillBAMSuggestionFlowEnabled());
  }
}

}  // namespace autofill
