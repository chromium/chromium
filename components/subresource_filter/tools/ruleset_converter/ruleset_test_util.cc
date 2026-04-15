// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/ruleset_test_util.h"

#include <fstream>
#include <ios>
#include <istream>

#include "base/files/file_util.h"
#include "components/subresource_filter/tools/rule_parser/rule_parser.h"
#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TestRulesetContents::TestRulesetContents() = default;
TestRulesetContents::~TestRulesetContents() = default;

TestRulesetContents::TestRulesetContents(const TestRulesetContents& other)
    : url_rules(other.url_rules), style_rules(other.style_rules) {}

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
      case url_pattern_index::proto::RULE_TYPE_STYLE: {
        auto proto = parser.style_rule().ToProtobuf();
        if (!DeleteStyleRuleOrAmend(&proto)) {
          style_rules.push_back(proto);
        }
        break;
      }
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
  style_rules.insert(style_rules.end(), other.style_rules.begin(),
                     other.style_rules.end());
}

bool TestRulesetContents::operator==(const TestRulesetContents& other) const {
  if (url_rules.size() != other.url_rules.size() ||
      style_rules.size() != other.style_rules.size()) {
    return false;
  }
  for (size_t i = 0; i < url_rules.size(); ++i) {
    if (!AreUrlRulesEqual(url_rules[i], other.url_rules[i])) {
      return false;
    }
  }
  for (size_t i = 0; i < style_rules.size(); ++i) {
    if (!AreStyleRulesEqual(style_rules[i], other.style_rules[i])) {
      return false;
    }
  }
  return true;
}

std::ostream& operator<<(std::ostream& out,
                         const TestRulesetContents& contents) {
  for (const auto& rule : contents.url_rules) {
    out << ToString(rule) << "\n";
  }
  for (const auto& rule : contents.style_rules) {
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

  for (const auto& rule : contents.url_rules) {
    EXPECT_TRUE(output->PutUrlRule(rule));
  }
  for (const auto& rule : contents.style_rules) {
    EXPECT_TRUE(output->PutStyleRule(rule));
  }
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
      CHECK_EQ(url_pattern_index::proto::RULE_TYPE_STYLE, rule_type);
      contents.style_rules.push_back(input->GetStyleRule());
    }
  }
  return contents;
}

bool AreUrlRulesEqual(const url_pattern_index::proto::UrlRule& first,
                      const url_pattern_index::proto::UrlRule& second) {
  return first.SerializeAsString() == second.SerializeAsString();
}

bool AreStyleRulesEqual(const url_pattern_index::proto::StyleRule& first,
                        const url_pattern_index::proto::StyleRule& second) {
  url_pattern_index::proto::StyleRule first_copy = first;
  url_pattern_index::proto::StyleRule second_copy = second;
  DeleteStyleRuleOrAmend(&first_copy);
  DeleteStyleRuleOrAmend(&second_copy);
  return first_copy.SerializeAsString() == second_copy.SerializeAsString();
}

}  // namespace subresource_filter
