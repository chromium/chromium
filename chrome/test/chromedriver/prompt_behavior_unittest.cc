// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/prompt_behavior.h"

#include <string>
#include <tuple>

#include "base/json/json_writer.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
static const std::unordered_map<std::string, PromptHandlerConfiguration>
    kPromptBehaviorToConfigurationMap = {
        {prompt_behavior::kAccept, {PromptHandlerType::kAccept, false}},
        {prompt_behavior::kAcceptAndNotify, {PromptHandlerType::kAccept, true}},
        {prompt_behavior::kDismiss, {PromptHandlerType::kDismiss, false}},
        {prompt_behavior::kDismissAndNotify,
         {PromptHandlerType::kDismiss, true}},
        {prompt_behavior::kIgnore, {PromptHandlerType::kIgnore, true}},
};
}  // namespace

void AssertPromptBehavior(
    PromptBehavior* prompt_behavior,
    std::vector<std::tuple<std::string, PromptHandlerType, bool>>
        expectations) {
  for (auto [dialog_type, expected_handler_type, expected_notify] :
       expectations) {
    PromptHandlerConfiguration actual_handler_configuration;
    Status status = prompt_behavior->GetConfiguration(
        dialog_type, actual_handler_configuration);
    ASSERT_EQ(kOk, status.code());
    ASSERT_EQ(expected_handler_type, actual_handler_configuration.type);
    ASSERT_EQ(expected_notify, actual_handler_configuration.notify);
  }
}

void AssertJsonEqual(const std::string& expected_str,
                     base::Value actual_value) {
  std::string expected_normalized_str, actual_normalized_str;
  ASSERT_TRUE(base::JSONWriter::Write(
      base::Value(base::test::ParseJsonDict(expected_str)),
      &expected_normalized_str));
  ASSERT_TRUE(base::JSONWriter::Write(actual_value, &actual_normalized_str));
  ASSERT_EQ(expected_normalized_str, actual_normalized_str);
}

TEST(PromptBehaviorTest, StrEmpty) {
  PromptBehavior prompt_behavior;
  Status status =
      PromptBehavior::Create(true, base::Value(""), prompt_behavior);
  ASSERT_TRUE(status.IsError());
}

TEST(PromptBehaviorTest, InvalidType) {
  PromptBehavior prompt_behavior;
  Status status =
      PromptBehavior::Create(true, base::Value(123), prompt_behavior);
  ASSERT_TRUE(status.IsError());
}

TEST(PromptBehaviorTest, StrInvalid) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      true, base::Value("some invalid string"), prompt_behavior);
  ASSERT_TRUE(status.IsError());
}

TEST(PromptBehaviorTest, StrAcceptLegacy) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      false, base::Value(prompt_behavior::kAccept), prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kAccept, true},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kAccept, true},
          {dialog_types::kPrompt, PromptHandlerType::kAccept, true},
      });
}

TEST(PromptBehaviorTest, StrAccept) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      true, base::Value(prompt_behavior::kAccept), prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kAccept, false},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kAccept, false},
          {dialog_types::kPrompt, PromptHandlerType::kAccept, false},
      });
}

TEST(PromptBehaviorTest, StrAcceptAndNotify) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      true, base::Value(prompt_behavior::kAcceptAndNotify), prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kAccept, true},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kAccept, true},
          {dialog_types::kPrompt, PromptHandlerType::kAccept, true},
      });
}

TEST(PromptBehaviorTest, StrDismiss) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      true, base::Value(prompt_behavior::kDismiss), prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kDismiss, false},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kDismiss, false},
          {dialog_types::kPrompt, PromptHandlerType::kDismiss, false},
      });
}

TEST(PromptBehaviorTest, StrDismissAndNotify) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      true, base::Value(prompt_behavior::kDismissAndNotify), prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kDismiss, true},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kDismiss, true},
          {dialog_types::kPrompt, PromptHandlerType::kDismiss, true},
      });
}

TEST(PromptBehaviorTest, StrIgnore) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(
      true, base::Value(prompt_behavior::kIgnore), prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kIgnore, true},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kIgnore, true},
          {dialog_types::kPrompt, PromptHandlerType::kIgnore, true},
      });
}

TEST(PromptBehaviorTest, EmptyDict) {
  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(true, base::Value(base::Value::Dict()),
                                         prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  AssertPromptBehavior(
      &prompt_behavior,
      {
          {dialog_types::kAlert, PromptHandlerType::kDismiss, true},
          {dialog_types::kBeforeUnload, PromptHandlerType::kAccept, false},
          {dialog_types::kConfirm, PromptHandlerType::kDismiss, true},
          {dialog_types::kPrompt, PromptHandlerType::kDismiss, true},
      });
}

class PromptBehaviorCreateDictInvariantTest
    : public testing::TestWithParam<
          std::tuple<std::string, std::string, std::string>> {};

INSTANTIATE_TEST_SUITE_P(
    CreateDict,
    PromptBehaviorCreateDictInvariantTest,
    testing::Combine(
        // prompt_type_in_capability
        testing::Values(dialog_types::kAlert,
                        dialog_types::kBeforeUnload,
                        dialog_types::kConfirm,
                        dialog_types::kPrompt),
        // prompt_type_to_check
        testing::Values(dialog_types::kAlert,
                        dialog_types::kBeforeUnload,
                        dialog_types::kConfirm,
                        dialog_types::kPrompt),
        // handler_type_str
        testing::Values(prompt_behavior::kAccept,
                        prompt_behavior::kDismiss,
                        prompt_behavior::kIgnore,
                        prompt_behavior::kAcceptAndNotify,
                        prompt_behavior::kDismissAndNotify)));

TEST_P(PromptBehaviorCreateDictInvariantTest, CreateDictWithDefault) {
  auto [prompt_type_in_capability, prompt_type_to_check, handler_type_str] =
      GetParam();
  std::string default_prompt_type = handler_type_str == prompt_behavior::kAccept
                                        ? prompt_behavior::kDismiss
                                        : prompt_behavior::kAccept;
  base::Value::Dict requested_capability;
  requested_capability.Set("default", default_prompt_type);
  requested_capability.Set(prompt_type_in_capability, handler_type_str);

  PromptBehavior prompt_behavior;

  Status status = PromptBehavior::Create(
      true, base::Value(std::move(requested_capability)), prompt_behavior);
  ASSERT_TRUE(status.IsOk());

  std::vector<std::tuple<std::string, PromptHandlerType, bool>> expectations;
  PromptHandlerConfiguration expected_configuration =
      prompt_type_to_check == prompt_type_in_capability
          ? kPromptBehaviorToConfigurationMap.at(handler_type_str)
          : kPromptBehaviorToConfigurationMap.at(default_prompt_type);
  expectations.emplace_back(prompt_type_to_check, expected_configuration.type,
                            expected_configuration.notify);

  AssertPromptBehavior(&prompt_behavior, expectations);
}

TEST_P(PromptBehaviorCreateDictInvariantTest, CreateDictWithoutDefault) {
  auto [prompt_type_in_capability, prompt_type_to_check, handler_type_str] =
      GetParam();
  std::string expected_behavior =
      prompt_type_to_check == prompt_type_in_capability ? handler_type_str
      : prompt_type_to_check == dialog_types::kBeforeUnload
          // beforeUnload is accepted be default.
          ? prompt_behavior::kAccept
          // other prompts are dismissed by default.
          : prompt_behavior::kDismissAndNotify;

  base::Value::Dict requested_capability;
  requested_capability.Set(prompt_type_in_capability, handler_type_str);

  PromptBehavior prompt_behavior;

  Status status = PromptBehavior::Create(
      true, base::Value(std::move(requested_capability)), prompt_behavior);
  ASSERT_TRUE(status.IsOk());

  std::vector<std::tuple<std::string, PromptHandlerType, bool>> expectations;
  PromptHandlerConfiguration expected_configuration =
      kPromptBehaviorToConfigurationMap.at(expected_behavior);
  expectations.emplace_back(prompt_type_to_check, expected_configuration.type,
                            expected_configuration.notify);

  AssertPromptBehavior(&prompt_behavior, expectations);
}

class PromptBehaviorStrCapabilityViewTest
    : public testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(StrCapabilityView,
                         PromptBehaviorStrCapabilityViewTest,
                         // capability_str
                         testing::Values(prompt_behavior::kAccept,
                                         prompt_behavior::kDismiss,
                                         prompt_behavior::kIgnore,
                                         prompt_behavior::kAcceptAndNotify,
                                         prompt_behavior::kDismissAndNotify));

TEST_P(PromptBehaviorStrCapabilityViewTest, StrCapabilityView) {
  std::string capability_str = GetParam();

  PromptBehavior prompt_behavior;
  Status status = PromptBehavior::Create(true, base::Value(capability_str),
                                         prompt_behavior);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(capability_str, prompt_behavior.CapabilityView().GetString());
}

class PromptBehaviorDictCapabilityViewInvariantTest
    : public testing::TestWithParam<std::string> {};

INSTANTIATE_TEST_SUITE_P(DictCapabilityView,
                         PromptBehaviorDictCapabilityViewInvariantTest,
                         // capability_json
                         testing::Values("{}",
                                         R"({
                                           "default":"ignore",
                                           "alert":"dismiss",
                                           "beforeUnload":"accept",
                                           "confirm":"accept and notify",
                                           "prompt":"dismiss and notify"
                                         })"));

TEST_P(PromptBehaviorDictCapabilityViewInvariantTest, DictCapabilityView) {
  std::string capability_json = GetParam();

  PromptBehavior prompt_behavior;

  Status status = PromptBehavior::Create(
      true, base::Value(base::test::ParseJsonDict(capability_json)),
      prompt_behavior);
  ASSERT_TRUE(status.IsOk());

  AssertJsonEqual(capability_json, prompt_behavior.CapabilityView());
}

class PromptBehaviorStrMapperOptionsViewInvariantTest
    : public testing::TestWithParam<std::tuple<std::string, std::string>> {};

INSTANTIATE_TEST_SUITE_P(
    StrMapperOptionsView,
    PromptBehaviorStrMapperOptionsViewInvariantTest,
    // {capability_str, expected_capability_json}
    testing::Values(std::make_tuple(prompt_behavior::kAccept,
                                    R"({
                                      "alert":"accept",
                                      "beforeUnload":"accept",
                                      "confirm":"accept",
                                      "prompt":"accept"
                                    })"),
                    std::make_tuple(prompt_behavior::kAcceptAndNotify,
                                    R"({
                                      "alert":"accept",
                                      "beforeUnload":"accept",
                                      "confirm":"accept",
                                      "prompt":"accept"
                                    })"),
                    std::make_tuple(prompt_behavior::kDismiss,
                                    R"({
                                      "alert":"dismiss",
                                      "beforeUnload":"accept",
                                      "confirm":"dismiss",
                                      "prompt":"dismiss"
                                    })"),
                    std::make_tuple(prompt_behavior::kDismissAndNotify,
                                    R"({
                                      "alert":"dismiss",
                                      "beforeUnload":"accept",
                                      "confirm":"dismiss",
                                      "prompt":"dismiss"
                                    })"),
                    std::make_tuple(prompt_behavior::kIgnore,
                                    R"({
                                      "alert":"ignore",
                                      "beforeUnload":"accept",
                                      "confirm":"ignore",
                                      "prompt":"ignore"
                                    })")));

TEST_P(PromptBehaviorStrMapperOptionsViewInvariantTest, StrMapperOptionsView) {
  auto [capability_str, expected_capability_json] = GetParam();
  //  for (auto capability_json : test_cases) {
  //    auto [capability_str, expected_capability_str] = capability_json;
  PromptBehavior prompt_behavior;

  Status status = PromptBehavior::Create(true, base::Value(capability_str),
                                         prompt_behavior);
  ASSERT_TRUE(status.IsOk());

  AssertJsonEqual(expected_capability_json,
                  prompt_behavior.MapperOptionsView());
  //  }
}

class PromptBehaviorDictMapperOptionsViewInvariantTest
    : public testing::TestWithParam<std::tuple<std::string, std::string>> {};

INSTANTIATE_TEST_SUITE_P(
    DictMapperOptionsView,
    PromptBehaviorDictMapperOptionsViewInvariantTest,
    // {capability_json, expected_capability_json}
    testing::Values(std::make_tuple(R"({"default":"accept"})",
                                    R"({
                                      "alert":"accept",
                                      "beforeUnload":"accept",
                                      "confirm":"accept",
                                      "prompt":"accept"
                                    })"),
                    std::make_tuple(R"({"default":"dismiss"})",
                                    R"({
                                      "alert":"dismiss",
                                      "beforeUnload":"dismiss",
                                      "confirm":"dismiss",
                                      "prompt":"dismiss"
                                    })"),
                    std::make_tuple(R"({"default":"ignore"})",
                                    R"({
                                      "alert":"ignore",
                                      "beforeUnload":"ignore",
                                      "confirm":"ignore",
                                      "prompt":"ignore"
                                    })"),
                    std::make_tuple(R"({
                                      "default":"ignore",
                                      "alert":"accept",
                                    })",
                                    R"({
                                      "alert":"accept",
                                      "beforeUnload":"ignore",
                                      "confirm":"ignore",
                                      "prompt":"ignore"
                                    })"),
                    std::make_tuple(R"({
                                      "default":"ignore",
                                      "beforeUnload":"accept",
                                    })",
                                    R"({
                                      "alert":"ignore",
                                      "beforeUnload":"accept",
                                      "confirm":"ignore",
                                      "prompt":"ignore"
                                    })"),
                    std::make_tuple(R"({
                                      "default":"ignore",
                                      "confirm":"accept",
                                    })",
                                    R"({
                                      "alert":"ignore",
                                      "beforeUnload":"ignore",
                                      "confirm":"accept",
                                      "prompt":"ignore"
                                    })"),
                    std::make_tuple(R"({
                                      "default":"ignore",
                                      "prompt":"accept",
                                    })",
                                    R"({
                                      "alert":"ignore",
                                      "beforeUnload":"ignore",
                                      "confirm":"ignore",
                                      "prompt":"accept"
                                    })")));
TEST_P(PromptBehaviorDictMapperOptionsViewInvariantTest,
       DictMapperOptionsView) {
  auto [capability_json, expected_capability_json] = GetParam();
  PromptBehavior prompt_behavior;

  Status status = PromptBehavior::Create(
      true, base::Value(base::test::ParseJsonDict(capability_json)),
      prompt_behavior);
  ASSERT_TRUE(status.IsOk());

  AssertJsonEqual(expected_capability_json,
                  prompt_behavior.MapperOptionsView());
}
