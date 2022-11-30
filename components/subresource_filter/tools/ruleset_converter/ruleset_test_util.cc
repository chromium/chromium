// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/ruleset_test_util.h"

#include <fstream>
#include <ios>
#include <istream>

#include "base/files/file_util.h"
#include "components/subresource_filter/tools/rule_parser/rule_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TestRulesetContents::TestRulesetContents() = default;
TestRulesetContents::~TestRulesetContents() = default;

TestRulesetContents::TestRulesetContents(const TestRulesetContents& other)
    : url_rules(other.url_rules), css_rules(other.css_rules) {}

void TestRulesetContents::AppendRules(
    const std::vector<std::string>& text_rules,
    bool allow_errors) {
  RuleParser parser;
  for (const std::string& text_rule : text_rules) {
    url_pattern_index::proto::RuleType rule_type = parser.Parse(text_rule);
    switch (rule_type) {
      case url_pattern_index::proto::RULE_TYPE_URL:
        url_rules.push_back(parser.url_rule().ToProtobuf());
        break;
      case url_pattern_index::proto::RULE_TYPE_CSS:
        css_rules.push_back(parser.css_rule().ToProtobuf());
        break;
      case url_pattern_index::proto::RULE_TYPE_UNSPECIFIED:
        ASSERT_TRUE(allow_errors);
        break;
      default:
        ASSERT_TRUE(false);
    }
  }
}

void TestRulesetContents::AppendParsedRules(const TestRulesetContents& other) {
  url_rules.insert(url_rules.end(), other.url_rules.begin(),
                   other.url_rules.end());
  css_rules.insert(css_rules.end(), other.css_rules.begin(),
                   other.css_rules.end());
}

bool TestRulesetContents::operator==(const TestRulesetContents& other) const {
  if (url_rules.size() != other.url_rules.size() ||
      css_rules.size() != other.css_rules.size()) {
    return false;
  }
  for (size_t i = 0; i < url_rules.size(); ++i) {
    if (!AreUrlRulesEqual(url_rules[i], other.url_rules[i]))
      return false;
  }
  for (size_t i = 0; i < css_rules.size(); ++i) {
    if (!AreCssRulesEqual(css_rules[i], other.css_rules[i]))
      return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& out,
                         const TestRulesetContents& contents) {
  for (const auto& rule : contents.url_rules) {
    out << ToString(rule) << "\n";
  }
  for (const auto& rule : contents.css_rules) {
    out << ToString(rule) << "\n";
  }
  return out;
}

ScopedTempRulesetFile::ScopedTempRulesetFile(RulesetFormat format)
    : format_(format) {
  // Cannot ASSERT due to returning in constructor.
  CHECK(scoped_dir_.CreateUniqueTempDir());
  CHECK(base::CreateTemporaryFileInDir(scoped_dir_.GetPath(), &ruleset_path_));
}

ScopedTempRulesetFile::~ScopedTempRulesetFile() = default;

std::unique_ptr<RuleOutputStream> ScopedTempRulesetFile::OpenForOutput() const {
  return RuleOutputStream::Create(
      std::make_unique<std::ofstream>(ruleset_path().MaybeAsASCII(),
                                      std::ios::binary | std::ios::out),
      format());
}

// Opens the |ruleset_file| with already existing ruleset and returns the
// corresponding input stream, or nullptr if it failed to be created.
std::unique_ptr<RuleInputStream> ScopedTempRulesetFile::OpenForInput() const {
  return RuleInputStream::Create(
      std::make_unique<std::ifstream>(ruleset_path().MaybeAsASCII(),
                                      std::ios::binary | std::ios::in),
      format());
}

void ScopedTempRulesetFile::WriteRuleset(
    const TestRulesetContents& contents) const {
  std::unique_ptr<RuleOutputStream> output = OpenForOutput();
  ASSERT_NE(nullptr, output);

  for (const auto& rule : contents.url_rules)
    EXPECT_TRUE(output->PutUrlRule(rule));
  for (const auto& rule : contents.css_rules)
    EXPECT_TRUE(output->PutCssRule(rule));
  EXPECT_TRUE(output->Finish());
}

TestRulesetContents ScopedTempRulesetFile::ReadContents() const {
  std::unique_ptr<RuleInputStream> input = OpenForInput();
  TestRulesetContents contents;
  url_pattern_index::proto::RuleType rule_type =
      url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  while ((rule_type = input->FetchNextRule()) !=
         url_pattern_index::proto::RULE_TYPE_UNSPECIFIED) {
    if (rule_type == url_pattern_index::proto::RULE_TYPE_URL) {
      contents.url_rules.push_back(input->GetUrlRule());
    } else {
      CHECK_EQ(url_pattern_index::proto::RULE_TYPE_CSS, rule_type);
      contents.css_rules.push_back(input->GetCssRule());
    }
  }
  return contents;
}

bool AreUrlRulesEqual(const url_pattern_index::proto::UrlRule& first,
                      const url_pattern_index::proto::UrlRule& second) {
  return first.SerializeAsString() == second.SerializeAsString();
}

bool AreCssRulesEqual(const url_pattern_index::proto::CssRule& first,
                      const url_pattern_index::proto::CssRule& second) {
  return first.SerializeAsString() == second.SerializeAsString();
}

}  // namespace subresource_filter
