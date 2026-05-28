// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_message_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

using internal::kDateKeysKey;
using internal::kDateStringKey;
using internal::kDetailsOrderKey;
using internal::kEndKey;
using internal::kOneKey;
using internal::kOtherKey;
using internal::kPluralsKey;
using internal::kStartKey;
using internal::kTemplateKey;

namespace {

FilterAttributeUiLabel CreateUiLabel(std::string key,
                                     std::u16string label,
                                     std::string value) {
  return FilterAttributeUiLabel(FilterSuggestionCandidateAttribute(key, label),
                                FilterAttribute(key, value));
}

struct TestCase {
  const char* description;
  const char* json_config;
  std::vector<FilterAttributeUiLabel> attribute_ui_labels;
  std::optional<std::u16string> expected_message;
};

class FilterSuggestionMessageUtilTest
    : public testing::TestWithParam<TestCase> {};

TEST_P(FilterSuggestionMessageUtilTest, GenerateMessage) {
  const TestCase& test_case = GetParam();

  std::optional<base::Value> config_val =
      base::JSONReader::Read(test_case.json_config, 0);
  ASSERT_TRUE(config_val.has_value())
      << "Failed to parse JSON for: " << test_case.description;
  ASSERT_TRUE(config_val->is_dict());

  base::DictValue root_dict;
  for (auto [key, value] : config_val->GetDict()) {
    root_dict.Set(key, value.Clone());
  }

  std::optional<std::u16string> result = GenerateMessageWithConfig(
      "task1", test_case.attribute_ui_labels, root_dict);

  if (test_case.expected_message) {
    ASSERT_TRUE(result.has_value())
        << "Expected value but got nullopt for: " << test_case.description;
    EXPECT_EQ(*result, *test_case.expected_message)
        << "Failed for: " << test_case.description;
  } else {
    EXPECT_EQ(result, std::nullopt)
        << "Expected nullopt but got value for: " << test_case.description;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FilterSuggestionMessageUtilTest,
    testing::Values(
        TestCase{
            .description = "Template Substitution",
            .json_config = R"({"task1": {"template": "Hello {NAME}"}})",
            .attribute_ui_labels = {CreateUiLabel("NAME", u"Name", "World")},
            .expected_message = u"Hello World"},
        TestCase{.description = "Plurals One",
                 .json_config = R"({
              "task1": {
                "template": "Status",
                "details_order": ["C"],
                "plurals": {"C": {"one": "1 item", "other": "{VALUE} items"}}
              }
            })",
                 .attribute_ui_labels = {CreateUiLabel("C", u"Count", "1")},
                 .expected_message = u"Status 1 item"},
        TestCase{.description = "Plurals Other",
                 .json_config = R"({
              "task1": {
                "template": "Status",
                "details_order": ["C"],
                "plurals": {"C": {"one": "1 item", "other": "{VALUE} items"}}
              }
            })",
                 .attribute_ui_labels = {CreateUiLabel("C", u"Count", "5")},
                 .expected_message = u"Status 5 items"},
        TestCase{.description = "Plurals Zero Skipped",
                 .json_config = R"({
              "task1": {
                "template": "Status",
                "details_order": ["C"],
                "plurals": {"C": {"one": "1 item", "other": "{VALUE} items"}}
              }
            })",
                 .attribute_ui_labels = {CreateUiLabel("C", u"Count", "0")},
                 .expected_message = u"Status"},
        TestCase{.description = "Missing Key Skipped",
                 .json_config = R"({
              "task1": {
                "template": "Status",
                "details_order": ["MISSING_KEY"]
              }
            })",
                 .attribute_ui_labels = {CreateUiLabel("C", u"Count", "5")},
                 .expected_message = u"Status"},
        TestCase{
            .description = "Raw Fallback",
            .json_config = R"({
              "task1": {
                "template": "Template",
                "details_order": ["KEY1", "KEY2"]
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("KEY1", u"Label1", "value1"),
                                    CreateUiLabel("KEY2", u"Label2", "value2")},
            .expected_message = u"Template value1 • value2"},
        TestCase{.description = "Missing Template",
                 .json_config = R"({"task1": {}})",
                 .attribute_ui_labels = {},
                 .expected_message = std::nullopt},
        TestCase{
            .description = "Date Range Same Month",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "2026-06-15")},
            .expected_message = u"Trip Jun 10 - 15"},
        TestCase{
            .description = "Date Range Different Months",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "2026-07-15")},
            .expected_message = u"Trip Jun 10 - Jul 15"},
        TestCase{
            .description = "Date Range No End Date Key",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10")},
            .expected_message = u"Trip Jun 10"},
        TestCase{
            .description = "Date Range Invalid End Date Key",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "invalid-date")},
            .expected_message = u"Trip"},
        TestCase{
            .description = "Date Range Missing Start Attribute",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("E", u"End", "2026-06-15")},
            .expected_message = u"Trip"},
        TestCase{
            .description = "Date Range Same Day",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "2026-06-10")},
            .expected_message = u"Trip Jun 10"},
        TestCase{
            .description = "Date Range Different Years Same Month",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "2027-06-15")},
            .expected_message = u"Trip Jun 10, 2026 - Jun 15, 2027"},
        TestCase{
            .description = "Date Range No Leading Zero",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-05"),
                                    CreateUiLabel("E", u"End", "2026-06-15")},
            .expected_message = u"Trip Jun 5 - 15"},
        TestCase{
            .description = "Date Range Cross Year Concise",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-12-25"),
                                    CreateUiLabel("E", u"End", "2027-01-10")},
            .expected_message = u"Trip Dec 25 - Jan 10"},
        TestCase{
            .description = "Date Range Exactly One Year",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "2027-06-10")},
            .expected_message = u"Trip Jun 10, 2026 - Jun 10, 2027"},
        TestCase{
            .description = "Date Range Less Than One Year",
            .json_config = R"({
              "task1": {
                "template": "Trip",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "2026-06-10"),
                                    CreateUiLabel("E", u"End", "2027-06-09")},
            .expected_message = u"Trip Jun 10 - Jun 9"},
        TestCase{
            .description = "Invalid Date Format Returns Empty",
            .json_config = R"({
              "task1": {
                "template": "Stays",
                "details_order": ["DATE_STRING"],
                "date_keys": {"start": "S", "end": "E"}
              }
            })",
            .attribute_ui_labels = {CreateUiLabel("S", u"Start", "not-a-date"),
                                    CreateUiLabel("E", u"End", "2026-06-15")},
            .expected_message = u"Stays"}));

}  // namespace

}  // namespace multistep_filter
