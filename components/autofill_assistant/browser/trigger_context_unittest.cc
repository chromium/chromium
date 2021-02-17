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
      /* caller_account_hash = */ "hash"};
  EXPECT_THAT(context.GetScriptParameters().ToProto(),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"key_a", "value_a"}, {"key_b", "value_b"}})));
  EXPECT_EQ(context.GetExperimentIds(), "exps");
  EXPECT_TRUE(context.GetCCT());
  EXPECT_TRUE(context.GetOnboardingShown());
  EXPECT_TRUE(context.GetDirectAction());
  EXPECT_EQ(context.GetCallerAccountHash(), "hash");
}

TEST(TriggerContextTest, MergeEmpty) {
  TriggerContext empty;
  TriggerContext merged = {{&empty, &empty}};
  EXPECT_THAT(merged.GetScriptParameters().ToProto(), IsEmpty());
  EXPECT_TRUE(merged.GetExperimentIds().empty());
  EXPECT_FALSE(merged.GetCCT());
  EXPECT_FALSE(merged.GetOnboardingShown());
  EXPECT_FALSE(merged.GetDirectAction());
  EXPECT_TRUE(merged.GetCallerAccountHash().empty());
}

TEST(TriggerContextTest, MergeEmptyWithNonEmpty) {
  TriggerContext context = {
      std::make_unique<ScriptParameters>(
          std::map<std::string, std::string>{{"key_a", "value_a"}}),
      "exp1",
      /* is_cct = */ false,
      /* onboarding_shown = */ false,
      /* is_direct_action = */ false,
      /* caller_account_hash = */ std::string()};
  TriggerContext empty;
  TriggerContext merged = {{&empty, &context}};
  EXPECT_THAT(merged.GetScriptParameters().ToProto(),
              UnorderedElementsAreArray(
                  std::map<std::string, std::string>({{"key_a", "value_a"}})));
  EXPECT_EQ(merged.GetExperimentIds(), "exp1");
  EXPECT_FALSE(merged.GetCCT());
  EXPECT_FALSE(merged.GetOnboardingShown());
  EXPECT_FALSE(merged.GetDirectAction());
  EXPECT_TRUE(merged.GetCallerAccountHash().empty());
}

TEST(TriggerContextTest, MergeNonEmptyWithNonEmpty) {
  TriggerContext context1 = {
      std::make_unique<ScriptParameters>(
          std::map<std::string, std::string>{{"key_a", "value_a"}}),
      "exp1",
      /* is_cct = */ false,
      /* onboarding_shown = */ false,
      /* is_direct_action = */ false,
      /* caller_account_hash = */ std::string()};
  TriggerContext context2 = {
      std::make_unique<ScriptParameters>(std::map<std::string, std::string>{
          {"key_a", "value_a_changed"}, {"key_b", "value_b"}}),
      "exp2",
      /* is_cct = */ true,
      /* onboarding_shown = */ true,
      /* is_direct_action = */ true,
      /* caller_account_hash = */ "hash"};

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
  EXPECT_EQ(merged.GetCallerAccountHash(), "hash");
}

TEST(TriggerContextTest, HasExperimentId) {
  TriggerContext context = {std::make_unique<ScriptParameters>(),
                            "1,2,3",
                            false,
                            false,
                            false,
                            std::string()};

  EXPECT_TRUE(context.HasExperimentId("2"));
  EXPECT_FALSE(context.HasExperimentId("4"));

  TriggerContext other_context = {std::make_unique<ScriptParameters>(),
                                  "4,5,6",
                                  false,
                                  false,
                                  false,
                                  std::string()};
  EXPECT_TRUE(other_context.HasExperimentId("4"));
  EXPECT_FALSE(other_context.HasExperimentId("2"));

  TriggerContext merged = {{&context, &other_context}};
  EXPECT_TRUE(merged.HasExperimentId("2"));
  EXPECT_TRUE(merged.HasExperimentId("4"));
  EXPECT_FALSE(merged.HasExperimentId("7"));

  // Double commas should not allow empty element to match.
  TriggerContext double_comma = {{},    "1,,2", false,
                                 false, false,  std::string()};
  EXPECT_TRUE(double_comma.HasExperimentId("2"));
  EXPECT_FALSE(double_comma.HasExperimentId(""));

  // Empty context should not allow empty element to match.
  TriggerContext empty;
  EXPECT_FALSE(empty.HasExperimentId(""));

  // Lone comma does not create empty elements.
  TriggerContext lone_comma = {{}, ",", false, false, false, std::string()};
  EXPECT_FALSE(lone_comma.HasExperimentId(""));

  // Single element should match.
  TriggerContext single_element = {{}, "1", false, false, false, std::string()};
  EXPECT_TRUE(single_element.HasExperimentId("1"));
}

}  // namespace autofill_assistant
