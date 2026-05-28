// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_message_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

using internal::kDateStringKey;
using internal::kDetailsOrderKey;
using internal::kOneKey;
using internal::kOtherKey;
using internal::kPluralsKey;
using internal::kTemplateKey;

namespace {

FilterAttributeUiLabel CreateUiLabel(std::string key,
                                     std::u16string label,
                                     std::string value) {
  return FilterAttributeUiLabel(FilterSuggestionCandidateAttribute(key, label),
                                FilterAttribute(key, value));
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_TemplateSubstitution) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  task_dict.Set(kTemplateKey, "Hello {NAME}");
  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message = GenerateMessageWithConfig(
      "task1", {CreateUiLabel("NAME", u"Name", "World")}, root_dict);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(*message), "Hello World");
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_PluralsOne) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  task_dict.Set(kTemplateKey, "Status");

  base::ListValue details_order;
  details_order.Append("C");
  task_dict.Set(kDetailsOrderKey, std::move(details_order));

  base::DictValue plurals;
  base::DictValue c_rules;
  c_rules.Set(kOneKey, "1 item");
  c_rules.Set(kOtherKey, "{VALUE} items");
  plurals.Set("C", std::move(c_rules));
  task_dict.Set(kPluralsKey, std::move(plurals));

  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message = GenerateMessageWithConfig(
      "task1", {CreateUiLabel("C", u"Count", "1")}, root_dict);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(*message), "Status 1 item");
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_PluralsOther) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  task_dict.Set(kTemplateKey, "Status");

  base::ListValue details_order;
  details_order.Append("C");
  task_dict.Set(kDetailsOrderKey, std::move(details_order));

  base::DictValue plurals;
  base::DictValue c_rules;
  c_rules.Set(kOneKey, "1 item");
  c_rules.Set(kOtherKey, "{VALUE} items");
  plurals.Set("C", std::move(c_rules));
  task_dict.Set(kPluralsKey, std::move(plurals));

  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message = GenerateMessageWithConfig(
      "task1", {CreateUiLabel("C", u"Count", "5")}, root_dict);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(*message), "Status 5 items");
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_PluralsZeroSkipped) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  task_dict.Set(kTemplateKey, "Status");

  base::ListValue details_order;
  details_order.Append("C");
  task_dict.Set(kDetailsOrderKey, std::move(details_order));

  base::DictValue plurals;
  base::DictValue c_rules;
  c_rules.Set(kOneKey, "1 item");
  c_rules.Set(kOtherKey, "{VALUE} items");
  plurals.Set("C", std::move(c_rules));
  task_dict.Set(kPluralsKey, std::move(plurals));

  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message = GenerateMessageWithConfig(
      "task1", {CreateUiLabel("C", u"Count", "0")}, root_dict);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(*message), "Status");
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_MissingKeySkipped) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  task_dict.Set(kTemplateKey, "Status");

  base::ListValue details_order;
  details_order.Append("MISSING_KEY");
  task_dict.Set(kDetailsOrderKey, std::move(details_order));

  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message = GenerateMessageWithConfig(
      "task1", {CreateUiLabel("C", u"Count", "5")}, root_dict);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(*message), "Status");
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_RawFallback) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  task_dict.Set(kTemplateKey, "Template");

  base::ListValue details_order;
  details_order.Append("KEY1");
  details_order.Append("KEY2");
  task_dict.Set(kDetailsOrderKey, std::move(details_order));

  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message =
      GenerateMessageWithConfig("task1",
                                {CreateUiLabel("KEY1", u"Label1", "value1"),
                                 CreateUiLabel("KEY2", u"Label2", "value2")},
                                root_dict);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(base::UTF16ToUTF8(*message), "Template value1 • value2");
}

TEST(FilterSuggestionMessageUtilTest, GenerateMessage_MissingTemplate) {
  base::DictValue root_dict;
  base::DictValue task_dict;
  root_dict.Set("task1", std::move(task_dict));

  std::optional<std::u16string> message =
      GenerateMessageWithConfig("task1", {}, root_dict);
  EXPECT_EQ(message, std::nullopt);
}

}  // namespace

}  // namespace multistep_filter
