// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/rule.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

absl::optional<Rule> MakeRule(const std::string& value) {
  auto dict = base::JSONReader::Read(value);
  EXPECT_TRUE(dict) << value << " is not valid JSON";
  return Rule::Create(*dict);
}

}  // namespace

TEST(DataControlsRuleTest, InvalidValues) {
  ASSERT_FALSE(Rule::Create(base::Value(1)));
  ASSERT_FALSE(Rule::Create(base::Value(-1)));
  ASSERT_FALSE(Rule::Create(base::Value(true)));
  ASSERT_FALSE(Rule::Create(base::Value(false)));
  ASSERT_FALSE(Rule::Create(base::Value(1.1)));
  ASSERT_FALSE(Rule::Create(base::Value(-1.1)));
  ASSERT_FALSE(Rule::Create(base::Value("simple string")));
  ASSERT_FALSE(Rule::Create(base::Value(u"wide string")));
  ASSERT_FALSE(Rule::Create(base::Value("")));
  ASSERT_FALSE(Rule::Create(base::Value("{}")));
  ASSERT_FALSE(Rule::Create(base::Value("{\"bad_dict\": true}")));
  ASSERT_FALSE(Rule::Create(base::Value(std::vector<char>({1, 2, 3, 4}))));
}

TEST(DataControlsRuleTest, InvalidConditions) {
  // First parameter should be "sources", second one should be "destinations".
  constexpr char kTemplate[] = R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    %s
    %s
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })";

  // No sources/destinations conditions shouldn't make a rule.
  ASSERT_FALSE(MakeRule(base::StringPrintf(kTemplate, "", "")));

  // Rules with only invalid sources shouldn't be created.
  ASSERT_FALSE(
      MakeRule(base::StringPrintf(kTemplate, R"("sources": {},)", "")));
  ASSERT_FALSE(MakeRule(
      base::StringPrintf(kTemplate, R"("sources": {"fake_key": 1234},)", "")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, R"("sources": {"urls": [1, 2, 3, 4]},)", "")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, R"("sources": {"urls": ["not_a_real:pattern"]},)", "")));

  // Rules with only invalid destinations shouldn't be created.
  ASSERT_FALSE(
      MakeRule(base::StringPrintf(kTemplate, "", R"("destinations": {},)")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "", R"("destinations": {"fake_key": 1234},)")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "", R"("destinations": {"urls": [1, 2, 3, 4]},)")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "", R"("destinations": {"urls": ["not_a_real:pattern"]},)")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "", R"("destinations": {"components": [1, 2, 3, 4]},)")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "",
      R"("destinations": {"components": ["not_a_real_component"]},)")));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(DataControlsRuleTest, ValidSourcesInvalidDestinationsConditions) {
  // Rules with a valid sources but invalid destinations should be created for
  // forward compatibility.
  constexpr char kTemplate[] = R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "sources": { "urls": ["*"] },
    %s
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })";
  ASSERT_TRUE(MakeRule(base::StringPrintf(kTemplate, "")));
  ASSERT_TRUE(
      MakeRule(base::StringPrintf(kTemplate, R"("destinations": {},)")));
  ASSERT_TRUE(MakeRule(
      base::StringPrintf(kTemplate, R"("destinations": {"fake_key": 1234},)")));
  ASSERT_TRUE(MakeRule(base::StringPrintf(
      kTemplate, R"("destinations": {"urls": [1, 2, 3, 4]},)")));
  ASSERT_TRUE(MakeRule(base::StringPrintf(
      kTemplate, R"("destinations": {"urls": ["not_a_real:pattern"]},)")));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(MakeRule(base::StringPrintf(
      kTemplate, R"("destinations": {"components": [1, 2, 3, 4]},)")));
  ASSERT_TRUE(MakeRule(base::StringPrintf(
      kTemplate,
      R"("destinations": {"components": ["not_a_real_component"]},)")));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(DataControlsRuleTest, InvalidSourcesValidDestinationsConditions) {
  // Rules with a valid destinations but valid destinations should be created
  // for forward compatibility.
  constexpr char kTemplate[] = R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    %s
    "destinations": { "urls": ["*"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })";
  ASSERT_TRUE(MakeRule(base::StringPrintf(kTemplate, "")));
  ASSERT_TRUE(MakeRule(base::StringPrintf(kTemplate, R"("sources": {},)")));
  ASSERT_TRUE(MakeRule(
      base::StringPrintf(kTemplate, R"("sources": {"fake_key": 1234},)")));
  ASSERT_TRUE(MakeRule(
      base::StringPrintf(kTemplate, R"("sources": {"urls": [1, 2, 3, 4]},)")));
  ASSERT_TRUE(MakeRule(base::StringPrintf(
      kTemplate, R"("sources": {"urls": ["not_a_real:pattern"]},)")));
}

TEST(DataControlsRuleTest, NoRestrictions) {
  ASSERT_FALSE(MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "urls": ["*"] }
  })"));
}

TEST(DataControlsRuleTest, InvalidRestrictions) {
  constexpr char kTemplate[] = R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "urls": ["*"] },
    "restrictions": %s
  })";
  ASSERT_FALSE(MakeRule(base::StringPrintf(kTemplate, "1234")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(kTemplate, "{}")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(kTemplate, "[]")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(kTemplate, R"("foo")")));
  ASSERT_FALSE(
      MakeRule(base::StringPrintf(kTemplate, R"(["not_a_real_restriction"])")));
}

TEST(DataControlsRuleTest, Restrictions) {
  auto rule = MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "urls": ["*"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" },
      { "class": "SCREENSHOT", "level": "WARN" },
      { "class": "PRINTING", "level": "ALLOW" },
      { "class": "PRIVACY_SCREEN", "level": "REPORT" }
    ]
  })");
  ASSERT_TRUE(rule);

  ActionContext context = {.destination = {.url = GURL("https://google.com")}};
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard, context),
            Rule::Level::kBlock);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kScreenshot, context),
            Rule::Level::kWarn);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kPrinting, context),
            Rule::Level::kAllow);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kPrivacyScreen, context),
            Rule::Level::kReport);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kScreenShare, context),
            Rule::Level::kNotSet);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kFiles, context),
            Rule::Level::kNotSet);
}

TEST(DataControlsRuleTest, Accessors) {
  auto rule = MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "urls": ["*"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })");
  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->name(), "Block pastes");
  ASSERT_EQ(rule->rule_id(), "1234");
  ASSERT_EQ(rule->description(), "A test rule to block pastes");
}

TEST(DataControlsRuleTest, SourceUrls) {
  auto rule = MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "sources": { "urls": ["google.com"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })");
  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard,
                           {.source = {.url = GURL("https://google.com")}}),
            Rule::Level::kBlock);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard,
                           {.source = {.url = GURL("https://chrome.com")}}),
            Rule::Level::kNotSet);
}

TEST(DataControlsRuleTest, DestinationUrls) {
  auto rule = MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "urls": ["google.com"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })");
  ASSERT_TRUE(rule);

  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {.destination = {.url = GURL("https://google.com")}}),
      Rule::Level::kBlock);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {.destination = {.url = GURL("https://chrome.com")}}),
      Rule::Level::kNotSet);
}

TEST(DataControlsRuleTest, SourceAndDestinationUrls) {
  auto rule = MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "sources": { "urls": ["chrome.com"] },
    "destinations": { "urls": ["google.com"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })");
  ASSERT_TRUE(rule);

  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {
                         .source = {.url = GURL("https://google.com")},
                         .destination = {.url = GURL("https://google.com")},
                     }),
      Rule::Level::kNotSet);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {
                         .source = {.url = GURL("https://google.com")},
                         .destination = {.url = GURL("https://chrome.com")},
                     }),
      Rule::Level::kNotSet);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {
                         .source = {.url = GURL("https://chrome.com")},
                         .destination = {.url = GURL("https://google.com")},
                     }),
      Rule::Level::kBlock);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {
                         .source = {.url = GURL("https://chrome.com")},
                         .destination = {.url = GURL("https://chrome.com")},
                     }),
      Rule::Level::kNotSet);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(DataControlsRuleTest, DestinationComponent) {
  // A "FOO" component is included to validate that compatibility with future
  // components works and doesn't interfere with the rest of the rule.
  auto rule = MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "components": ["FOO", "ARC"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" }
    ]
  })");
  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard,
                           {.destination = {.component = Component::kArc}}),
            Rule::Level::kBlock);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {.destination = {.component = Component::kCrostini}}),
      Rule::Level::kNotSet);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {.destination = {.component = Component::kPluginVm}}),
      Rule::Level::kNotSet);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard,
                           {.destination = {.component = Component::kUsb}}),
            Rule::Level::kNotSet);
  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard,
                           {.destination = {.component = Component::kDrive}}),
            Rule::Level::kNotSet);
  ASSERT_EQ(
      rule->GetLevel(Rule::Restriction::kClipboard,
                     {.destination = {.component = Component::kOneDrive}}),
      Rule::Level::kNotSet);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace data_controls
