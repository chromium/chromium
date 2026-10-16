// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_dogfood_feedback.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox_dogfood_feedback::internal {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

BASE_FEATURE(kOmniboxDogfoodFeedbackTestEnabled,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kOmniboxDogfoodFeedbackTestDisabled,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kUnrelatedDogfoodFeedbackTestFeature,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Checks that the filter keeps names containing "omnibox" or "webui" in any
// case, and nothing else.
TEST(OmniboxDogfoodFeedbackTest, IsRelevantFeature) {
  EXPECT_TRUE(IsRelevantFeature("OmniboxAimDetachWebContentsOnHide"));
  EXPECT_TRUE(IsRelevantFeature("EnergyEffectInOmnibox"));
  EXPECT_TRUE(IsRelevantFeature("Omnibox"));
  EXPECT_TRUE(IsRelevantFeature("omnibox"));

  EXPECT_TRUE(IsRelevantFeature("WebUIOmniboxFullPopup"));
  EXPECT_TRUE(IsRelevantFeature("OmniboxFullWebUIEvictOnHide"));
  EXPECT_TRUE(IsRelevantFeature("WebUILocationBar"));
  EXPECT_TRUE(IsRelevantFeature("WebUIToolbarFrameEvictionOptOut"));
  EXPECT_TRUE(IsRelevantFeature("WebUIReloadButton"));
  EXPECT_TRUE(IsRelevantFeature("WebUITabStrip"));
  EXPECT_TRUE(IsRelevantFeature("InitialWebUIWithoutSpellCheckForNtp"));
  EXPECT_TRUE(IsRelevantFeature("SomeWebuiFeature"));
  EXPECT_TRUE(IsRelevantFeature("webui"));

  EXPECT_FALSE(IsRelevantFeature(""));
  EXPECT_FALSE(IsRelevantFeature("Webium"));
  EXPECT_FALSE(IsRelevantFeature("WebiumMetricsMapping"));
  EXPECT_FALSE(IsRelevantFeature("WebUsb"));
  EXPECT_FALSE(IsRelevantFeature("TabStrip"));
}

// Checks that relevant enabled features are listed with their trial, group and
// params, relevant disabled features are listed, and unrelated ones are not.
TEST(OmniboxDogfoodFeedbackTest, DiagnosticsListRelevantOverrides) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kOmniboxDogfoodFeedbackTestEnabled, {{"foo", "bar"}, {"baz", "1"}}},
       {kUnrelatedDogfoodFeedbackTestFeature, {}}},
      {kOmniboxDogfoodFeedbackTestDisabled});

  const std::string diagnostics = GetDiagnostics();

  EXPECT_TRUE(
      diagnostics.starts_with("Omnibox dogfood feedback diagnostics:\n"))
      << diagnostics;
  base::FieldTrial* trial =
      base::FeatureList::GetFieldTrial(kOmniboxDogfoodFeedbackTestEnabled);
  ASSERT_TRUE(trial);
  EXPECT_THAT(
      diagnostics,
      HasSubstr(base::StrCat(
          {"OmniboxDogfoodFeedbackTestEnabled: enabled (", trial->trial_name(),
           "/", trial->GetGroupNameWithoutActivation(), ") baz=1 foo=bar\n"})));
  EXPECT_THAT(diagnostics,
              HasSubstr("OmniboxDogfoodFeedbackTestDisabled: disabled\n"));
  EXPECT_THAT(diagnostics, Not(HasSubstr("UnrelatedDogfoodFeedbackTest")));
}

// Checks the output for a feature enabled with params on the command line,
// which is also how chrome://flags variations are applied. ScopedFeatureList
// keeps the overrides the test runner was started with (some bots pass
// --disable-features for WebUI features), so the output can hold more lines
// than the ones set here; check each line rather than the whole string.
TEST(OmniboxDogfoodFeedbackTest, DiagnosticsListCommandLineOverrides) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(
      "OmniboxDogfoodFeedbackTestEnabled:foo/bar,"
      "UnrelatedDogfoodFeedbackTestFeature",
      "OmniboxDogfoodFeedbackTestDisabled");

  const std::string diagnostics = GetDiagnostics();

  EXPECT_TRUE(
      diagnostics.starts_with("Omnibox dogfood feedback diagnostics:\n"))
      << diagnostics;
  EXPECT_THAT(diagnostics,
              HasSubstr("\nOmniboxDogfoodFeedbackTestEnabled: enabled "
                        "(StudyOmniboxDogfoodFeedbackTestEnabled/"
                        "GroupOmniboxDogfoodFeedbackTestEnabled) foo=bar\n"));
  EXPECT_THAT(diagnostics,
              HasSubstr("\nOmniboxDogfoodFeedbackTestDisabled: disabled\n"));
  EXPECT_THAT(diagnostics, Not(HasSubstr("UnrelatedDogfoodFeedbackTest")));
}

// Checks that a feature keeping its default state while tied to a trial (a "*"
// override, like a client in a study's Default group) is reported as default,
// not enabled.
TEST(OmniboxDogfoodFeedbackTest, DiagnosticsReportDefaultStateOverrides) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(
      "*OmniboxDogfoodFeedbackTestEnabled<DogfoodStudy.Default:foo/bar", "");
  ASSERT_TRUE(
      base::FeatureList::GetFieldTrial(kOmniboxDogfoodFeedbackTestEnabled));
  ASSERT_FALSE(
      base::FeatureList::IsEnabled(kOmniboxDogfoodFeedbackTestEnabled));

  const std::string diagnostics = GetDiagnostics();

  EXPECT_THAT(diagnostics,
              Not(HasSubstr("OmniboxDogfoodFeedbackTestEnabled: enabled")));
  EXPECT_THAT(diagnostics,
              HasSubstr("OmniboxDogfoodFeedbackTestEnabled: default "
                        "(DogfoodStudy/Default) foo=bar\n"));
}

// Checks that reading trial params for the diagnostics doesn't activate the
// trial.
TEST(OmniboxDogfoodFeedbackTest, DiagnosticsDoNotActivateTrials) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kOmniboxDogfoodFeedbackTestEnabled, {{"foo", "bar"}});
  base::FieldTrial* trial =
      base::FeatureList::GetFieldTrial(kOmniboxDogfoodFeedbackTestEnabled);
  ASSERT_TRUE(trial);
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));

  EXPECT_THAT(GetDiagnostics(), HasSubstr(" foo=bar\n"));

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
}

}  // namespace
}  // namespace omnibox_dogfood_feedback::internal
