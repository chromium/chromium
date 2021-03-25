// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

#include <map>
#include <string>

#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

TEST(TriggerContextTest, Empty) {
  TriggerContext empty;
  EXPECT_THAT(empty.GetScriptParameters().ToProto(), IsEmpty());
  EXPECT_EQ(empty.GetExperimentIds(), std::string());
}

TEST(TriggerContextTest, Create) {
  TriggerContext context = {
      std::make_unique<ScriptParameters>(std::map<std::string, std::string>{
          {"key_a", "value_a"}, {"key_b", "value_b"}}),
      "exps",
      /* is_cct = */ true,
      /* onboarding_shown = */ true,
      /* is_direct_action = */ true,
      /* initial_url = */ "https://www.example.com"};
  EXPECT_THAT(context.GetScriptParameters().ToProto(),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"key_a", "value_a"}, {"key_b", "value_b"}})));
  EXPECT_EQ(context.GetExperimentIds(), "exps");
  EXPECT_TRUE(context.GetCCT());
  EXPECT_TRUE(context.GetOnboardingShown());
  EXPECT_TRUE(context.GetDirectAction());
  EXPECT_EQ(context.GetInitialUrl(), "https://www.example.com");

  context.SetOnboardingShown(false);
  EXPECT_FALSE(context.GetOnboardingShown());
}

TEST(TriggerContextTest, MergeEmpty) {
  TriggerContext empty;
  TriggerContext merged = {{&empty, &empty}};
  EXPECT_THAT(merged.GetScriptParameters().ToProto(), IsEmpty());
  EXPECT_TRUE(merged.GetExperimentIds().empty());
  EXPECT_FALSE(merged.GetCCT());
  EXPECT_FALSE(merged.GetOnboardingShown());
  EXPECT_FALSE(merged.GetDirectAction());
}

TEST(TriggerContextTest, MergeEmptyWithNonEmpty) {
  TriggerContext::Options options;
  options.experiment_ids = "exp1";
  TriggerContext context = {
      std::make_unique<ScriptParameters>(
          std::map<std::string, std::string>{{"key_a", "value_a"}}),
      options};
  TriggerContext empty;
  TriggerContext merged = {{&empty, &context}};
  EXPECT_THAT(merged.GetScriptParameters().ToProto(),
              UnorderedElementsAreArray(
                  std::map<std::string, std::string>({{"key_a", "value_a"}})));
  EXPECT_EQ(merged.GetExperimentIds(), "exp1");
  EXPECT_FALSE(merged.GetCCT());
  EXPECT_FALSE(merged.GetOnboardingShown());
  EXPECT_FALSE(merged.GetDirectAction());
}

TEST(TriggerContextTest, MergeNonEmptyWithNonEmpty) {
  TriggerContext::Options options1;
  options1.experiment_ids = "exp1";
  TriggerContext context1 = {
      std::make_unique<ScriptParameters>(
          std::map<std::string, std::string>{{"key_a", "value_a"}}),
      options1};
  TriggerContext context2 = {
      std::make_unique<ScriptParameters>(std::map<std::string, std::string>{
          {"key_a", "value_a_changed"}, {"key_b", "value_b"}}),
      "exp2",
      /* is_cct = */ true,
      /* onboarding_shown = */ true,
      /* is_direct_action = */ true,
      /* initial_url = */ "https://www.example.com"};

  // Adding empty to make sure empty contexts are properly skipped.
  TriggerContext empty;
  TriggerContext merged = {{&empty, &context1, &empty, &context2, &empty}};
  EXPECT_THAT(merged.GetScriptParameters().ToProto(),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"key_a", "value_a"}, {"key_b", "value_b"}})));
  EXPECT_EQ(merged.GetExperimentIds(), "exp1,exp2");
  EXPECT_TRUE(merged.GetCCT());
  EXPECT_TRUE(merged.GetOnboardingShown());
  EXPECT_TRUE(merged.GetDirectAction());
  EXPECT_EQ(merged.GetInitialUrl(), "https://www.example.com");
}

TEST(TriggerContextTest, HasExperimentId) {
  TriggerContext::Options options;
  options.experiment_ids = "1,2,3";
  TriggerContext context = {std::make_unique<ScriptParameters>(), options};

  EXPECT_TRUE(context.HasExperimentId("2"));
  EXPECT_FALSE(context.HasExperimentId("4"));

  TriggerContext::Options other_options;
  other_options.experiment_ids = "4,5,6";
  TriggerContext other_context = {std::make_unique<ScriptParameters>(),
                                  other_options};
  EXPECT_TRUE(other_context.HasExperimentId("4"));
  EXPECT_FALSE(other_context.HasExperimentId("2"));

  TriggerContext merged = {{&context, &other_context}};
  EXPECT_TRUE(merged.HasExperimentId("2"));
  EXPECT_TRUE(merged.HasExperimentId("4"));
  EXPECT_FALSE(merged.HasExperimentId("7"));

  // Double commas should not allow empty element to match.
  options.experiment_ids = "1,,2";
  TriggerContext double_comma = {{}, options};
  EXPECT_TRUE(double_comma.HasExperimentId("2"));
  EXPECT_FALSE(double_comma.HasExperimentId(""));

  // Empty context should not allow empty element to match.
  TriggerContext empty;
  EXPECT_FALSE(empty.HasExperimentId(""));

  // Lone comma does not create empty elements.
  options.experiment_ids = ",";
  TriggerContext lone_comma = {{}, options};
  EXPECT_FALSE(lone_comma.HasExperimentId(""));

  // Single element should match.
  options.experiment_ids = "1";
  TriggerContext single_element = {{}, options};
  EXPECT_TRUE(single_element.HasExperimentId("1"));
}

}  // namespace autofill_assistant
