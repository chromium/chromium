// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/tools/rule_parser/rule_parser.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_test_util.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

// Returns a small number of predefined rules in text format.
std::vector<std::string> GetSomeRules() {
  return std::vector<std::string>{
      "example.com",
      "||ex.com$image",
      "|http://example.com/?key=value$~third-party,domain=ex.com",
      "&key1=value1&key2=value2|$script,image,font",
      "domain1.com,domain1.com###id",
      "@@allowlisted.com$document,domain=example.com|~sub.example.com",
      "###absolute_evil_id",
      "@@allowlisted.com$match-case,document,domain=another.example.com",
      "domain.com,~sub.domain.com,sub.sub.domain.com#@#id",
      "#@#absolute_good_id",
      "host$websocket",
  };
}

// Returns some rules which Chrome doesn't support fully or partially, mixed
// with a couple of supported rules, or otherwise weird ones.
std::vector<std::string> GetSomeChromeUnfriendlyRules() {
  return std::vector<std::string>{
      "/a[0-9].com/$image",
      "a.com$image,popup",
      "a.com$popup",
      "a.com$~image",
      "a.com$~popup",
      "a.com$~image,~popup",
      "@@a.com$subdocument,document",
      "@@a.com$document,generichide",
      "@@a.com$document",
      "@@a.com$genericblock",
      "@@a.com$elemhide",
      "@@a.com$generichide",
      "@@a.com$elemhide,generichide",
      "@@a.com$image,elemhide,generichide",
      "a.com$image,~image",
  };
}

// Generates and returns many rules in text format.
std::vector<std::string> GetManyRules() {
  constexpr size_t kNumberOfUrlRules = 10123;
  constexpr size_t kNumberOfCssRules = 5321;

  std::vector<std::string> text_rules;

  for (size_t i = 0; i != kNumberOfUrlRules; ++i) {
    std::string text_rule;
    if (!(i & 3))
      text_rule += "@@";
    if (i & 1)
      text_rule += "sub.";
    text_rule += "example" + base::NumberToString(i) + ".com";
    text_rule += '$';
    text_rule += (i & 7) ? "script" : "image";
    if (i & 1)
      text_rule += ",domain=example.com|~but_not.example.com";
    text_rules.push_back(text_rule);
  }

  for (size_t i = 0; i != kNumberOfCssRules; ++i) {
    std::string text_rule = "domain.com";
    if (i & 1)
      text_rule += ",~but_not.domain.com";
    text_rule += (i & 3) ? "##" : "#@#";
    text_rule += "#id" + base::NumberToString(i);
    text_rules.push_back(text_rule);
  }

  return text_rules;
}

// Reads the provided |ruleset_file| skipping every second rule (independently
// for URL and CSS rules), and EXPECTs that it contains exactly all the rules
// from |expected_contents| in the same order.
void ReadHalfRulesOfTestRulesetAndExpectContents(
    const ScopedTempRulesetFile& ruleset_file,
    const TestRulesetContents& expected_contents) {
  std::unique_ptr<RuleInputStream> input = ruleset_file.OpenForInput();

  TestRulesetContents contents;

  bool take_url_rule = true;
  bool take_css_rule = true;
  url_pattern_index::proto::RuleType rule_type =
      url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  while ((rule_type = input->FetchNextRule()) !=
         url_pattern_index::proto::RULE_TYPE_UNSPECIFIED) {
    if (rule_type == url_pattern_index::proto::RULE_TYPE_URL) {
      if (take_url_rule)
        contents.url_rules.push_back(input->GetUrlRule());
      take_url_rule = !take_url_rule;
    } else {
      ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_CSS, rule_type);
      if (take_css_rule)
        contents.css_rules.push_back(input->GetCssRule());
      take_css_rule = !take_css_rule;
    }
  }

  EXPECT_EQ(contents, expected_contents);
}

}  // namespace

TEST(RuleStreamTest, WriteAndReadRuleset) {
  for (int small_or_big = 0; small_or_big < 2; ++small_or_big) {
    TestRulesetContents contents;
    if (small_or_big)
      contents.AppendRules(GetManyRules());
    else
      contents.AppendRules(GetSomeRules());

    TestRulesetContents only_url_rules;
    only_url_rules.url_rules = contents.url_rules;

    for (auto format : {RulesetFormat::kFilterList, RulesetFormat::kProto,
                        RulesetFormat::kUnindexedRuleset}) {
      ScopedTempRulesetFile ruleset_file(format);
      ruleset_file.WriteRuleset(contents);
      // Note: kUnindexedRuleset discards CSS rules, test it differently.
      EXPECT_EQ(ruleset_file.ReadContents(),
                format == RulesetFormat::kUnindexedRuleset ? only_url_rules
                                                           : contents);
    }
  }
}

TEST(RuleStreamTest, WriteAndReadHalfRuleset) {
  TestRulesetContents contents;
  contents.AppendRules(GetManyRules());

  TestRulesetContents half_contents;
  for (size_t i = 0, size = contents.url_rules.size(); i < size; i += 2)
    half_contents.url_rules.push_back(contents.url_rules[i]);
  for (size_t i = 0, size = contents.css_rules.size(); i < size; i += 2)
    half_contents.css_rules.push_back(contents.css_rules[i]);

  TestRulesetContents half_url_rules;
  half_url_rules.url_rules = half_contents.url_rules;

  for (auto format : {RulesetFormat::kFilterList, RulesetFormat::kProto,
                      RulesetFormat::kUnindexedRuleset}) {
    ScopedTempRulesetFile ruleset_file(format);
    ruleset_file.WriteRuleset(contents);
    // Note: kUnindexedRuleset discards CSS rules, test it differently.
    ReadHalfRulesOfTestRulesetAndExpectContents(
        ruleset_file, format == RulesetFormat::kUnindexedRuleset
                          ? half_url_rules
                          : half_contents);
  }
}

TEST(RuleStreamTest, TransferAllRulesToSameStream) {
  TestRulesetContents contents;
  contents.AppendRules(GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  source_ruleset.WriteRuleset(contents);

  std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
  std::unique_ptr<RuleOutputStream> output = target_ruleset.OpenForOutput();
  TransferRules(input.get(), output.get(), output.get());
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  EXPECT_EQ(target_ruleset.ReadContents(), contents);
}

TEST(RuleStreamTest, TransferUrlRulesToOneStream) {
  TestRulesetContents contents;
  contents.AppendRules(GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  source_ruleset.WriteRuleset(contents);

  std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
  std::unique_ptr<RuleOutputStream> output = target_ruleset.OpenForOutput();
  TransferRules(input.get(), output.get(), nullptr);
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  contents.css_rules.clear();
  EXPECT_EQ(target_ruleset.ReadContents(), contents);
}

TEST(RuleStreamTest, TransferCssRulesToOneStream) {
  TestRulesetContents contents;
  contents.AppendRules(GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  source_ruleset.WriteRuleset(contents);

  std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
  std::unique_ptr<RuleOutputStream> output = target_ruleset.OpenForOutput();
  TransferRules(input.get(), nullptr, output.get());
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  contents.url_rules.clear();
  EXPECT_EQ(target_ruleset.ReadContents(), contents);
}

TEST(RuleStreamTest, TransferAllRulesToDifferentStreams) {
  TestRulesetContents contents;
  contents.AppendRules(GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset_url(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset_css(RulesetFormat::kFilterList);
  source_ruleset.WriteRuleset(contents);

  std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
  std::unique_ptr<RuleOutputStream> output_url =
      target_ruleset_url.OpenForOutput();
  std::unique_ptr<RuleOutputStream> output_css =
      target_ruleset_css.OpenForOutput();
  TransferRules(input.get(), output_url.get(), output_css.get());
  EXPECT_TRUE(output_url->Finish());
  EXPECT_TRUE(output_css->Finish());
  input.reset();
  output_url.reset();
  output_css.reset();

  TestRulesetContents only_url_rules;
  only_url_rules.url_rules = contents.url_rules;
  EXPECT_EQ(target_ruleset_url.ReadContents(), only_url_rules);

  contents.url_rules.clear();
  EXPECT_EQ(target_ruleset_css.ReadContents(), contents);
}

TEST(RuleStreamTest, TransferRulesAndDiscardRegexpRules) {
  TestRulesetContents contents;
  contents.AppendRules(GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
  source_ruleset.WriteRuleset(contents);

  std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
  std::unique_ptr<RuleOutputStream> output = target_ruleset.OpenForOutput();
  TransferRules(input.get(), output.get(), nullptr, 54 /* chrome_version */);
  EXPECT_TRUE(output->Finish());
  input.reset();
  output.reset();

  std::erase_if(contents.url_rules,
                [](const url_pattern_index::proto::UrlRule& rule) {
                  return rule.url_pattern_type() ==
                         url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP;
                });
  contents.css_rules.clear();
  EXPECT_EQ(target_ruleset.ReadContents(), contents);
}

TEST(RuleStreamTest, TransferRulesChromeVersion) {
  TestRulesetContents contents;
  contents.AppendRules(GetSomeChromeUnfriendlyRules());
  contents.AppendRules(GetManyRules());

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  source_ruleset.WriteRuleset(contents);

  for (int chrome_version : {0, 54, 59}) {
    TestRulesetContents expected_contents;
    for (url_pattern_index::proto::UrlRule url_rule : contents.url_rules) {
      if (DeleteUrlRuleOrAmend(&url_rule, chrome_version))
        continue;
      expected_contents.url_rules.push_back(url_rule);
    }

    ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);
    std::unique_ptr<RuleOutputStream> output = target_ruleset.OpenForOutput();
    std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
    TransferRules(input.get(), output.get(), nullptr, chrome_version);
    EXPECT_TRUE(output->Finish());
    input.reset();
    output.reset();

    EXPECT_EQ(target_ruleset.ReadContents(), expected_contents);
  }
}

TEST(RuleStreamTest, TransferRulesFromFilterListWithUnsupportedOptions) {
  std::vector<std::string> text_rules = GetSomeRules();
  const size_t number_of_correct_rules = text_rules.size();

  // Insert several rules with non-critical parse errors.
  text_rules.insert(text_rules.begin(), "host1$donottrack");
  text_rules.push_back("");
  text_rules.insert(text_rules.begin() + text_rules.size() / 2,
                    "host3$collapse");

  ScopedTempRulesetFile source_ruleset(RulesetFormat::kFilterList);
  ScopedTempRulesetFile target_ruleset(RulesetFormat::kFilterList);

  // Output all the rules to the |source_ruleset| file.
  std::string joined_rules = base::JoinString(text_rules, "\n");
  base::WriteFile(source_ruleset.ruleset_path(), joined_rules);

  // Filter out the rules with parse errors, and save the rest to |contents|.
  TestRulesetContents contents;
  contents.AppendRules(text_rules, true /* allow_errors */);

  // Make sure all the rules with no errors were transferred.
  {
    std::unique_ptr<RuleInputStream> input = source_ruleset.OpenForInput();
    std::unique_ptr<RuleOutputStream> output = target_ruleset.OpenForOutput();
    TransferRules(input.get(), output.get(), output.get());
    EXPECT_TRUE(output->Finish());
  }

  EXPECT_EQ(number_of_correct_rules,
            contents.url_rules.size() + contents.css_rules.size());
  EXPECT_EQ(target_ruleset.ReadContents(), contents);
}

TEST(RuleStreamTest, DeleteUrlRuleOrAmend) {
  const struct TestCase {
    const char* rule;
    const char* chrome_54_rule;
    const char* chrome_59_rule;
  } kTestCases[] = {
      {"/a[0-9].com/$image", nullptr, nullptr},
      {"a.com$image,popup", "a.com$image,~popup", "#54"},
      {"a.com$popup", nullptr, nullptr},
      {"a.com$~image",
       "a.com$~image,~popup,~websocket,~webtransport,~webbundle", "#0"},
      {"a.com$~popup", "a.com$~popup,~websocket,~webtransport,~webbundle",
       "a.com"},
      {"a.com$~image,~popup",
       "a.com$~image,~popup,~websocket,~webtransport,~webbundle", "#0"},
      {"@@a.com$subdocument,document", "#0", "#0"},
      {"@@a.com$document,generichide", "@@a.com$document", "#54"},
      {"@@a.com$document", "#0", "#0"},
      {"@@a.com$genericblock", "#0", "#0"},
      {"@@a.com$elemhide", nullptr, nullptr},
      {"@@a.com$generichide", nullptr, nullptr},
      {"@@a.com$elemhide,generichide", nullptr, nullptr},
      {"@@a.com$image,elemhide,generichide", "@@a.com$image", "#54"},
      {"a.com$image,~image", nullptr, nullptr},
  };

  auto get_target_rule = [](const TestCase& test, std::string target_rule) {
    RuleParser parser;
    if (target_rule == "#0")
      target_rule = test.rule;
    else if (target_rule == "#54")
      target_rule = test.chrome_54_rule;
    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_URL,
              parser.Parse(target_rule));
    return parser.url_rule().ToProtobuf();
  };

  RuleParser parser;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.rule);
    ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_URL, parser.Parse(test.rule));
    const url_pattern_index::proto::UrlRule current_rule =
        parser.url_rule().ToProtobuf();

    url_pattern_index::proto::UrlRule modified_rule = current_rule;
    EXPECT_FALSE(DeleteUrlRuleOrAmend(&modified_rule, 0));
    EXPECT_TRUE(AreUrlRulesEqual(modified_rule, current_rule));

    modified_rule = current_rule;
    EXPECT_EQ(!test.chrome_54_rule, DeleteUrlRuleOrAmend(&modified_rule, 54));
    if (test.chrome_54_rule) {
      EXPECT_TRUE(AreUrlRulesEqual(modified_rule,
                                   get_target_rule(test, test.chrome_54_rule)));
    }

    modified_rule = current_rule;
    EXPECT_EQ(!test.chrome_59_rule, DeleteUrlRuleOrAmend(&modified_rule, 59));
    if (test.chrome_59_rule) {
      EXPECT_TRUE(AreUrlRulesEqual(modified_rule,
                                   get_target_rule(test, test.chrome_59_rule)));
    }
  }
}

}  // namespace subresource_filter
