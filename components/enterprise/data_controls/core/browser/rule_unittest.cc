// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/rule.h"

#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

std::optional<Rule> MakeRule(const std::string& value) {
  auto dict = base::JSONReader::Read(value);
  EXPECT_TRUE(dict) << value << " is not valid JSON";
  return Rule::Create(*dict);
}

class DataControlsRuleTest : public testing::Test {
 public:
  explicit DataControlsRuleTest(bool desktop_feature_enabled = true,
                                bool screenshot_feature_enabled = true) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (desktop_feature_enabled) {
      enabled_features.push_back(kEnableDesktopDataControls);
    } else {
      disabled_features.push_back(kEnableDesktopDataControls);
    }

    if (screenshot_feature_enabled) {
      enabled_features.push_back(kEnableScreenshotProtection);
    } else {
      disabled_features.push_back(kEnableScreenshotProtection);
    }

    scoped_features_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  base::test::ScopedFeatureList scoped_features_;
};

class DataControlsFeaturesRuleTest
    : public DataControlsRuleTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DataControlsFeaturesRuleTest()
      : DataControlsRuleTest(desktop_feature_enabled(),
                             screenshot_feature_enabled()) {}

  bool desktop_feature_enabled() const { return std::get<0>(GetParam()); }
  bool screenshot_feature_enabled() const { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsFeaturesRuleTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

struct AndOrNotTestCase {
  const char* conditions;
  ActionContext context;
};

// Test to validate that a valid set of conditions in a rule will return
// opposite "IsTriggered" results when those conditions are nested in a "not"
// attribute. This is parametrized with conditions and a corresponding context
// to trigger them.
class DataControlsRuleNotTest
    : public DataControlsRuleTest,
      public testing::WithParamInterface<AndOrNotTestCase> {
 public:
  std::string normal_rule_string() {
    return base::StringPrintf(R"(
    {
      "name": "Normal rule",
      "rule_id": "1234",
      %s,
      "restrictions": [
        { "class": "CLIPBOARD", "level": "BLOCK" }
      ]
    })",
                              GetParam().conditions);
  }

  std::string negative_rule_string() {
    return base::StringPrintf(R"(
    {
      "name": "Negative rule",
      "rule_id": "5678",
      "not": {
        %s
      },
      "restrictions": [
        { "class": "CLIPBOARD", "level": "BLOCK" }
      ]
    })",
                              GetParam().conditions);
  }
};

// Test to validate that a valid set of conditions in a rule will trigger when
// inserted into an "and" attribute. This is parametrized with conditions and a
// corresponding context to trigger them.
class DataControlsRuleAndTest
    : public DataControlsRuleTest,
      public testing::WithParamInterface<AndOrNotTestCase> {
 public:
  std::string rule_string() {
    return base::StringPrintf(R"(
    {
      "name": "Normal rule",
      "rule_id": "1234",
      "and": [
        %s
      ],
      "restrictions": [
        { "class": "CLIPBOARD", "level": "BLOCK" }
      ]
    })",
                              GetParam().conditions);
  }
};

// Test to validate that a valid set of conditions in a rule will trigger when
// inserted into an "or" attribute. This is parametrized with conditions and a
// corresponding context to trigger them.
class DataControlsRuleOrTest
    : public DataControlsRuleTest,
      public testing::WithParamInterface<AndOrNotTestCase> {
 public:
  std::string rule_string() {
    return base::StringPrintf(R"(
    {
      "name": "Normal rule",
      "rule_id": "1234",
      "or": [
        %s
      ],
      "restrictions": [
        { "class": "CLIPBOARD", "level": "BLOCK" }
      ]
    })",
                              GetParam().conditions);
  }
};

// These helpers are implemented as functions instead of simple constants
// because some sub-types of ActionContext (namely GURL) don't support being
// statically instantiated.
std::vector<AndOrNotTestCase> NotTestCases() {
  return {
      {.conditions = R"("sources": {"incognito":true})",
       .context = {.source = {.url = GURL("https://chrome.com"),
                              .incognito = true}}},
      {.conditions = R"("sources": {"os_clipboard":true})",
       .context = {.source = {.os_clipboard = true}}},
      {.conditions = R"("sources": {"urls":["google.com"]})",
       .context = {.source = {.url = GURL("https://google.com")}}},
      {.conditions = R"("destinations": {"incognito":true})",
       .context = {.destination = {.url = GURL("https://chrome.com"),
                                   .incognito = true}}},
      {.conditions = R"("destinations": {"os_clipboard":true})",
       .context = {.destination = {.os_clipboard = true}}},
      {.conditions = R"("destinations": {"urls":["google.com"]})",
       .context = {.destination = {.url = GURL("https://google.com")}}},
  };
}

std::vector<AndOrNotTestCase> AndTestCases() {
  return {
      {.conditions = R"(
        {"sources": {"incognito":true}},
        {"sources": {"urls": ["google.com"]}})",
       .context = {.source = {.url = GURL("https://google.com"),
                              .incognito = true}}},
      {.conditions = R"(
        {"not": {"sources": {"incognito":false}}},
        {"sources": {"urls": ["google.com"]}})",
       .context = {.source = {.url = GURL("https://google.com"),
                              .incognito = true}}},
      {.conditions = R"(
        {"destinations": {"incognito": true}},
        {"sources": {"os_clipboard": true}})",
       .context = {.source = {.os_clipboard = true},
                   .destination = {.url = GURL("https://google.com"),
                                   .incognito = true}}},
      {.conditions = R"(
        {"not": {"destinations": {"incognito": false}}},
        {"sources": {"os_clipboard": true}})",
       .context = {.source = {.os_clipboard = true},
                   .destination = {.url = GURL("https://google.com"),
                                   .incognito = true}}},
      {.conditions = R"(
        {"or": [
          {"sources": {"incognito":true}},
          {"sources": {"urls": ["google.com"]}}
        ]},
        {"not": { "destinations": {"incognito": true} } })",
       .context = {.source = {.url = GURL("https://google.com")},
                   .destination = {.url = GURL("https://google.com"),
                                   .incognito = false}}},
      {.conditions = R"(
        {"or": [
          {"sources": {"incognito":true}},
          {"sources": {"urls": ["google.com"]}}
        ]},
        {"not": { "destinations": {"incognito": true} } })",
       .context = {.source = {.url = GURL("https://chrome.com"),
                              .incognito = true},
                   .destination = {.url = GURL("https://chrome.com"),
                                   .incognito = false}}},
  };
}

std::vector<AndOrNotTestCase> OrTestCases() {
  return {
      {.conditions = R"(
        {"sources": {"incognito":true}},
        {"sources": {"urls": ["google.com"]}})",
       .context = {.source = {.url = GURL("https://chrome.com"),
                              .incognito = true}}},
      {.conditions = R"(
        {"sources": {"incognito":true}},
        {"sources": {"urls": ["google.com"]}})",
       .context = {.source = {.url = GURL("https://google.com")}}},
      {.conditions = R"(
        {"destinations": {"os_clipboard":true}},
        {"destinations": { "not": {"urls": ["google.com"]}}})",
       .context = {.destination = {.os_clipboard = true}}},
      {.conditions = R"(
        {"destinations": {"os_clipboard":true}},
        {"destinations": {"urls": ["google.com"]}})",
       .context = {.destination = {.url = GURL("https://google.com")}}},
      {.conditions = R"(
        {"and": [
          {"sources": {"incognito":true}},
          {"sources": {"urls": ["google.com"]}}
        ]},
        {"destinations": {"incognito": true} })",
       .context = {.source = {.url = GURL("https://google.com"),
                              .incognito = true}}},
      {.conditions = R"(
        {"and": [
          {"sources": {"incognito":true}},
          {"sources": {"urls": ["google.com"]}}
        ]},
        {"not": { "destinations": {"incognito": false} } })",
       .context =
           {
               .source = {.url = GURL("https://chrome.com")},
               .destination = {.url = GURL("https://chrome.com"),
                               .incognito = true},
           }},
  };
}

INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsRuleNotTest,
                         testing::ValuesIn(NotTestCases()));
INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsRuleAndTest,
                         testing::ValuesIn(AndTestCases()));
INSTANTIATE_TEST_SUITE_P(All,
                         DataControlsRuleOrTest,
                         testing::ValuesIn(OrTestCases()));

}  // namespace

TEST_F(DataControlsRuleTest, InvalidValues) {
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

TEST_F(DataControlsRuleTest, InvalidConditions) {
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

  // Rules with invalid boolean attributes shouldn't be created.
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "",
      R"("not": [{"sources": {"urls": ["not.is.not.an.array"]}}],)")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "",
      R"("and": {"sources": {"urls": ["and.is.not.a.dict"]}},)")));
  ASSERT_FALSE(MakeRule(base::StringPrintf(
      kTemplate, "", R"("or": {"sources": {"urls": ["or.is.not.a.dict"]}},)")));
}

TEST_F(DataControlsRuleTest, ValidSourcesInvalidDestinationsConditions) {
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

TEST_F(DataControlsRuleTest, InvalidSourcesValidDestinationsConditions) {
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

TEST_F(DataControlsRuleTest, NoRestrictions) {
  ASSERT_FALSE(MakeRule(R"({
    "name": "Block pastes",
    "rule_id": "1234",
    "description": "A test rule to block pastes",
    "destinations": { "urls": ["*"] }
  })"));
}

TEST_F(DataControlsRuleTest, InvalidRestrictions) {
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

TEST_F(DataControlsRuleTest, Restrictions) {
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

TEST_F(DataControlsRuleTest, Accessors) {
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

TEST_F(DataControlsRuleTest, SourceUrls) {
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

TEST_F(DataControlsRuleTest, DestinationUrls) {
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

TEST_F(DataControlsRuleTest, SourceAndDestinationUrls) {
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
TEST_F(DataControlsRuleTest, DestinationComponent) {
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

TEST_P(DataControlsFeaturesRuleTest, ScreenshotRules) {
  auto rule = MakeRule(R"({
    "name": "Block screenshots",
    "rule_id": "1234",
    "description": "A test rule to block screenshots",
    "sources": { "urls": ["*"] },
    "restrictions": [
      { "class": "SCREENSHOT", "level": "BLOCK" }
    ]
  })");
  if (screenshot_feature_enabled()) {
    ASSERT_TRUE(rule);
    ASSERT_EQ(rule->GetLevel(Rule::Restriction::kScreenshot,
                             {.source = {.url = GURL("https://google.com")}}),
              Rule::Level::kBlock);
  } else {
    ASSERT_FALSE(rule);
  }
}

TEST_P(DataControlsFeaturesRuleTest, NonScreenshotRules) {
  auto rule = MakeRule(R"({
    "name": "Block stuff",
    "rule_id": "1234",
    "description": "A test rule to block some non-screenshot actions",
    "destinations": { "urls": ["*"] },
    "restrictions": [
      { "class": "CLIPBOARD", "level": "BLOCK" },
      { "class": "PRINTING", "level": "ALLOW" },
      { "class": "PRIVACY_SCREEN", "level": "REPORT" }
    ]
  })");
  if (desktop_feature_enabled()) {
    ASSERT_TRUE(rule);
    ActionContext context = {
        .destination = {.url = GURL("https://google.com")}};
    ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard, context),
              Rule::Level::kBlock);
    ASSERT_EQ(rule->GetLevel(Rule::Restriction::kPrinting, context),
              Rule::Level::kAllow);
    ASSERT_EQ(rule->GetLevel(Rule::Restriction::kPrivacyScreen, context),
              Rule::Level::kReport);
    ASSERT_EQ(rule->GetLevel(Rule::Restriction::kScreenshot, context),
              Rule::Level::kNotSet);
  } else {
    ASSERT_FALSE(rule);
  }
}

TEST_P(DataControlsRuleNotTest, TriggeringContext) {
  auto normal_rule = MakeRule(normal_rule_string());
  auto negative_rule = MakeRule(negative_rule_string());

  ASSERT_TRUE(normal_rule);
  ASSERT_TRUE(negative_rule);

  ASSERT_EQ(
      normal_rule->GetLevel(Rule::Restriction::kClipboard, GetParam().context),
      Rule::Level::kBlock);
  ASSERT_EQ(negative_rule->GetLevel(Rule::Restriction::kClipboard,
                                    GetParam().context),
            Rule::Level::kNotSet);
}

TEST_P(DataControlsRuleNotTest, NonTriggeringContext) {
  auto normal_rule = MakeRule(normal_rule_string());
  auto negative_rule = MakeRule(negative_rule_string());

  ASSERT_TRUE(normal_rule);
  ASSERT_TRUE(negative_rule);

  ASSERT_EQ(normal_rule->GetLevel(
                Rule::Restriction::kClipboard,
                {.source = {.url = GURL("https://chrome.com")},
                 .destination = {.url = GURL("https://chrome.com")}}),
            Rule::Level::kNotSet);
  ASSERT_EQ(negative_rule->GetLevel(
                Rule::Restriction::kClipboard,
                {.source = {.url = GURL("https://chrome.com")},
                 .destination = {.url = GURL("https://chrome.com")}}),
            Rule::Level::kBlock);
}

TEST_P(DataControlsRuleAndTest, TriggeringContext) {
  auto rule = MakeRule(rule_string());

  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard, GetParam().context),
            Rule::Level::kBlock);
}

TEST_P(DataControlsRuleAndTest, NonTriggeringContext) {
  auto rule = MakeRule(rule_string());

  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard, {}),
            Rule::Level::kNotSet);
}

TEST_P(DataControlsRuleOrTest, TriggeringContext) {
  auto rule = MakeRule(rule_string());

  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard, GetParam().context),
            Rule::Level::kBlock);
}

TEST_P(DataControlsRuleOrTest, NonTriggeringContext) {
  auto rule = MakeRule(rule_string());

  ASSERT_TRUE(rule);

  ASSERT_EQ(rule->GetLevel(Rule::Restriction::kClipboard, {}),
            Rule::Level::kNotSet);
}

}  // namespace data_controls
