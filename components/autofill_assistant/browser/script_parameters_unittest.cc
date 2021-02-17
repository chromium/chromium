// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_parameters.h"

#include "components/autofill_assistant/browser/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAreArray;

TEST(ScriptParametersTest, Create) {
  ScriptParameters parameters = {{{"key_a", "value_a"}, {"key_b", "value_b"}}};
  EXPECT_THAT(parameters.ToProto(),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"key_a", "value_a"}, {"key_b", "value_b"}})));
  EXPECT_THAT(parameters.GetParameter("key_a"), Eq("value_a"));
  EXPECT_THAT(parameters.GetParameter("key_b"), Eq("value_b"));
  EXPECT_THAT(parameters.GetParameter("not_found"), Eq(base::nullopt));
}

TEST(ScriptParametersTest, MergeEmpty) {
  ScriptParameters merged;
  EXPECT_THAT(merged.ToProto(), IsEmpty());
  merged.MergeWith(ScriptParameters());
  EXPECT_THAT(merged.ToProto(), IsEmpty());
}

TEST(ScriptParametersTest, MergeEmptyWithNonEmpty) {
  ScriptParameters empty;
  empty.MergeWith({{{"key_a", "value_a"}}});
  EXPECT_THAT(empty.ToProto(),
              UnorderedElementsAreArray(
                  std::map<std::string, std::string>({{"key_a", "value_a"}})));
}

TEST(ScriptParametersTest, MergeNonEmptyWithEmpty) {
  ScriptParameters parameters = {{{"key_a", "value_a"}}};
  parameters.MergeWith(ScriptParameters());
  EXPECT_THAT(parameters.ToProto(),
              UnorderedElementsAreArray(
                  std::map<std::string, std::string>({{"key_a", "value_a"}})));
}

TEST(ScriptParametersTest, MergeNonEmptyWithNonEmpty) {
  ScriptParameters parameters_a = {{{"key_a", "value_a"}}};
  ScriptParameters parameters_b = {
      {{"key_a", "value_a_changed"}, {"key_b", "value_b"}}};

  parameters_a.MergeWith(parameters_b);
  EXPECT_THAT(parameters_a.ToProto(),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"key_a", "value_a"}, {"key_b", "value_b"}})));
}

TEST(ScriptParametersTest, TriggerScriptAllowList) {
  ScriptParameters parameters = {{{"DEBUG_BUNDLE_ID", "12345"},
                                  {"key_a", "value_a"},
                                  {"DEBUG_BUNDLE_VERSION", "version"},
                                  {"DEBUG_SOCKET_ID", "678"},
                                  {"FALLBACK_BUNDLE_ID", "fallback_id"},
                                  {"key_b", "value_b"},
                                  {"FALLBACK_BUNDLE_VERSION", "fallback_ver"}}};

  EXPECT_THAT(parameters.ToProto(/* only_trigger_script_allowlisted = */ false),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"DEBUG_BUNDLE_ID", "12345"},
                   {"key_a", "value_a"},
                   {"DEBUG_BUNDLE_VERSION", "version"},
                   {"DEBUG_SOCKET_ID", "678"},
                   {"FALLBACK_BUNDLE_ID", "fallback_id"},
                   {"key_b", "value_b"},
                   {"FALLBACK_BUNDLE_VERSION", "fallback_ver"}})));

  EXPECT_THAT(parameters.ToProto(/* only_trigger_script_allowlisted = */ true),
              UnorderedElementsAreArray(std::map<std::string, std::string>(
                  {{"DEBUG_BUNDLE_ID", "12345"},
                   {"DEBUG_BUNDLE_VERSION", "version"},
                   {"DEBUG_SOCKET_ID", "678"},
                   {"FALLBACK_BUNDLE_ID", "fallback_id"},
                   {"FALLBACK_BUNDLE_VERSION", "fallback_ver"}})));
}

}  // namespace autofill_assistant
