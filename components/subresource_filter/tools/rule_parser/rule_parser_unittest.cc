// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/subresource_filter/tools/rule_parser/rule_parser.h"

#include <string>
#include <string_view>
#include <vector>

#include "components/subresource_filter/tools/rule_parser/rule.h"
#include "components/subresource_filter/tools/rule_parser/rule_options.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

void ParseAndExpectUrlRule(std::string_view line,
                           const UrlRule& expected_rule) {
  UrlRule canonicalized_rule = expected_rule;
  canonicalized_rule.Canonicalize();

  RuleParser parser;
  EXPECT_NE(url_pattern_index::proto::RULE_TYPE_UNSPECIFIED,
            parser.Parse(line));
  EXPECT_EQ(RuleParser::ParseError::NONE, parser.parse_error().error_code);
  EXPECT_TRUE(parser.parse_error().line.empty());
  EXPECT_EQ(std::string::npos, parser.parse_error().error_index);

  EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_URL, parser.rule_type());
  EXPECT_EQ(canonicalized_rule, parser.url_rule());
}

void ParseAndExpectCssRule(std::string_view line,
                           const CssRule& expected_rule) {
  CssRule canonicalized_rule = expected_rule;
  canonicalized_rule.Canonicalize();

  RuleParser parser;
  EXPECT_NE(url_pattern_index::proto::RULE_TYPE_UNSPECIFIED,
            parser.Parse(line));
  EXPECT_EQ(RuleParser::ParseError::NONE, parser.parse_error().error_code);
  EXPECT_TRUE(parser.parse_error().line.empty());
  EXPECT_EQ(std::string::npos, parser.parse_error().error_index);

  EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_CSS, parser.rule_type());
  EXPECT_EQ(canonicalized_rule, parser.css_rule());
}

}  // namespace

TEST(RuleParserTest, ParseComment) {
  RuleParser parser;

  static const char* kLines[] = {
      "! this is a comment", "   ! this is a comment too", "[ and this",
      "    [ as well as this",
  };

  for (const char* line : kLines) {
    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_COMMENT, parser.Parse(line));
    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_COMMENT, parser.rule_type());
  }
}

TEST(RuleParserTest, ParseUrlRule) {
  static const char* kLine = "?param=";
  UrlRule expected_rule;
  expected_rule.url_pattern = kLine;
  expected_rule.url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;

  ParseAndExpectUrlRule(kLine, expected_rule);
}

TEST(RuleParserTest, UrlRuleMatchCase) {
  const struct {
    const char* line;
    bool expected_match_case;
  } kTestCases[] = {
      {"example.com$image", false}, {"example.com$image,match-case", true},
  };
  RuleParser parser;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test_case.line);
    ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_URL,
              parser.Parse(test_case.line));
    EXPECT_EQ(test_case.expected_match_case, parser.url_rule().match_case);
  }
}

TEST(RuleParserTest, ParseAllowlistUrlRule) {
  static const char* kLine = "@@?param=";
  UrlRule expected_rule;
  expected_rule.is_allowlist = true;
  expected_rule.url_pattern = kLine + 2;
  expected_rule.url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;

  ParseAndExpectUrlRule(kLine, expected_rule);
}

TEST(RuleParserTest, ParseEmptyOptions) {
  static const char* kLine = "&param=value$";
  UrlRule expected_rule;
  expected_rule.url_pattern.assign(kLine, strlen(kLine) - 1);
  expected_rule.url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;

  ParseAndExpectUrlRule(kLine, expected_rule);
}

TEST(RuleParserTest, ParseSingleTypeOptions) {
  static const std::string kRulePrefix = "?param=$";
  for (const auto& type_info : kElementTypes) {
    UrlRule expected_rule;
    expected_rule.url_pattern = "?param=";
    expected_rule.url_pattern_type =
        url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
    expected_rule.type_mask = type_mask_for(type_info.type);

    ParseAndExpectUrlRule(kRulePrefix + type_info.name, expected_rule);
  }
}

TEST(RuleParserTest, ParseSingleInverseTypeOptions) {
  static const std::string kRulePrefix = "?param=$";

  for (const auto& type_info : kElementTypes) {
    UrlRule expected_rule;
    expected_rule.url_pattern = "?param=";
    expected_rule.url_pattern_type =
        url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
    expected_rule.type_mask = type_mask_for(type_info.type);

    ParseAndExpectUrlRule(kRulePrefix + type_info.name, expected_rule);
  }
}

TEST(RuleParserTest, ParseMultipleTypeOptions) {
  static const char* kLine = "?param=$script,image,~stylesheet";

  UrlRule expected_rule;
  expected_rule.url_pattern = "?param=";
  expected_rule.url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
  expected_rule.type_mask =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_SCRIPT) |
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_IMAGE);

  ParseAndExpectUrlRule(kLine, expected_rule);
}

TEST(RuleParserTest, ParseContradictingTypeOptions) {
  static const char* kLines[2] = {
      "?param=$image,~image", "?param=$popup,image,~image",
  };

  for (size_t i = 0; i < 2; ++i) {
    UrlRule expected_rule;
    expected_rule.url_pattern = "?param=";
    expected_rule.url_pattern_type =
        url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
    expected_rule.type_mask = 0;
    if (i == 1) {
      expected_rule.type_mask |=
          type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_POPUP);
    }
    ParseAndExpectUrlRule(kLines[i], expected_rule);
  }
}

TEST(RuleParserTest, ParseUrlRuleOptions) {
  static const char* kLine =
      "?param=$popup,match-case,domain=example1.com|example2.org,~third-party";

  UrlRule expected_rule;
  expected_rule.url_pattern = "?param=";
  expected_rule.url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
  expected_rule.match_case = true;
  expected_rule.type_mask =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_POPUP);
  expected_rule.is_third_party = TriState::NO;
  expected_rule.domains.push_back("example1.com");
  expected_rule.domains.push_back("example2.org");

  ParseAndExpectUrlRule(kLine, expected_rule);
}

TEST(RuleParserTest, ParseUrlRuleAnchors) {
  const std::string kLine = "example.com";
  static const struct {
    AnchorType type;
    const char* literal;
  } kAnchors[] = {
      {url_pattern_index::proto::ANCHOR_TYPE_NONE, ""},
      {url_pattern_index::proto::ANCHOR_TYPE_BOUNDARY, "|"},
      {url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN, "||"},
  };

  for (const auto& left_anchor : kAnchors) {
    UrlRule expected_rule;
    expected_rule.url_pattern = kLine;
    expected_rule.anchor_left = left_anchor.type;

    for (const auto& right_anchor : kAnchors) {
      if (right_anchor.type == url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN)
        continue;
      expected_rule.anchor_right = right_anchor.type;
      std::string line = left_anchor.literal + kLine + right_anchor.literal;
      if (left_anchor.type != url_pattern_index::proto::ANCHOR_TYPE_NONE ||
          right_anchor.type != url_pattern_index::proto::ANCHOR_TYPE_NONE) {
        expected_rule.url_pattern_type =
            url_pattern_index::proto::URL_PATTERN_TYPE_WILDCARDED;
      } else {
        expected_rule.url_pattern_type =
            url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
      }
      ParseAndExpectUrlRule(line, expected_rule);
    }
  }
}

TEST(RuleParserTest, ParseUrlRuleWithBookmark) {
  static const char* kLine = "@@example.com^*#-$image";
  UrlRule expected_rule;
  expected_rule.is_allowlist = true;
  expected_rule.type_mask =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_IMAGE);
  expected_rule.url_pattern = "example.com^*#-";
  expected_rule.url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_WILDCARDED;

  ParseAndExpectUrlRule(kLine, expected_rule);
}

TEST(RuleParserTest, ParseUrlRuleWithElementAndActivationTypes) {
  constexpr auto kImage =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_IMAGE);
  constexpr auto kFont =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_FONT);
  constexpr auto kScript =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_SCRIPT);
  constexpr auto kPopup =
      type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_POPUP);
  constexpr auto kDocument =
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT);
  constexpr auto kGenericBlock =
      type_mask_for(url_pattern_index::proto::ACTIVATION_TYPE_GENERICBLOCK);

  const struct {
    const char* rule;
    TypeMask expected_type_mask;
  } kTestCases[] = {
      {"ex.com", kDefaultElementTypes},
      {"ex.com$", kDefaultElementTypes},
      {"ex.com$popup", kPopup},
      {"ex.com$~popup", kDefaultElementTypes},
      {"ex.com$image", kImage},
      {"ex.com$~image", kDefaultElementTypes & ~kImage},
      {"ex.com$image,font", kImage | kFont},
      {"ex.com$image,popup", kImage | kPopup},
      {"ex.com$~font,~script", kDefaultElementTypes & ~kFont & ~kScript},
      {"ex.com$subdocument,~subdocument", 0},
      {"@@ex.com$document", kDocument},
      {"@@ex.com$document,genericblock", kDocument | kGenericBlock},
      {"@@ex.com$document,image", kDocument | kImage},
      {"@@ex.com$document,image,popup", kDocument | kImage | kPopup},
      {"@@ex.com$document,~image", kDocument},
      {"@@ex.com$~image,document",
       (kDefaultElementTypes & ~kImage) | kDocument},
  };

  RuleParser parser;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test_case.rule);
    ASSERT_EQ(url_pattern_index::proto::RULE_TYPE_URL,
              parser.Parse(test_case.rule));
    EXPECT_EQ(test_case.expected_type_mask, parser.url_rule().type_mask);
  }
}

TEST(RuleParserTest, ParseUrlRuleWithRegexp) {
  const struct {
    const char* line;
    const char* expected_url_pattern;
  } kTestCases[] = {
      {"/.*substring.*/", "/.*substring.*/"},
      {"/.*substring$/", "/.*substring$/"},
      {"/.*substring.*/$image", "/.*substring.*/"},
      {"/.*[$#].*/$image", "/.*[$#].*/"},
      {"/.*$script/$image", "/.*$script/"},
      {"@@/^.*$/$script,domain=example.com", "/^.*$/"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test.line);

    RuleParser parser;
    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_URL, parser.Parse(test.line));
    EXPECT_EQ(url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP,
              parser.url_rule().url_pattern_type);
    EXPECT_EQ(test.expected_url_pattern, parser.url_rule().url_pattern);
  }
}

TEST(RuleParserTest, ParseCssRules) {
  static const std::string kCssSelector = "div.blocked_class";
  static const std::string kTestDomain = "example.com";
  static const std::string kTestSubDomain = "however.example.com";

  CssRule expected_rule;
  expected_rule.css_selector = kCssSelector;

  ParseAndExpectCssRule("##" + kCssSelector, expected_rule);
  ParseAndExpectCssRule(" ##" + kCssSelector, expected_rule);
  ParseAndExpectCssRule(" \t\t##" + kCssSelector, expected_rule);

  expected_rule.domains.push_back(kTestDomain);
  ParseAndExpectCssRule(kTestDomain + "##" + kCssSelector, expected_rule);

  expected_rule.is_allowlist = true;
  ParseAndExpectCssRule(kTestDomain + "#@#" + kCssSelector, expected_rule);

  expected_rule.is_allowlist = false;
  expected_rule.domains.push_back("~" + kTestSubDomain);
  ParseAndExpectCssRule(
      kTestDomain + ",~" + kTestSubDomain + "##" + kCssSelector, expected_rule);

  expected_rule.is_allowlist = true;
  ParseAndExpectCssRule(
      kTestDomain + ",~" + kTestSubDomain + "#@#" + kCssSelector,
      expected_rule);
}

TEST(RuleParserTest, ParseErrors) {
  RuleParser parser;

  struct {
    const char* rule;
    RuleParser::ParseError::ErrorCode expected_code;
    size_t expected_index;
  } kRulesAndExpectations[] = {
      {"", RuleParser::ParseError::EMPTY_RULE, 0},
      {"    ", RuleParser::ParseError::EMPTY_RULE, 0},
      {"   \t ", RuleParser::ParseError::EMPTY_RULE, 0},
      {"@", RuleParser::ParseError::BAD_ALLOWLIST_SYNTAX, 1},
      {"@http://example.com", RuleParser::ParseError::BAD_ALLOWLIST_SYNTAX, 1},
      {"?opt=$unknown_option", RuleParser::ParseError::UNKNOWN_OPTION, 6},
      {"example.com$image,dtd", RuleParser::ParseError::DEPRECATED_OPTION, 18},
      {"?opt=$elemhide", RuleParser::ParseError::ALLOWLIST_ONLY_OPTION, 6},
      {"?opt=$generichide", RuleParser::ParseError::ALLOWLIST_ONLY_OPTION, 6},
      {"?opt=$generblock", RuleParser::ParseError::ALLOWLIST_ONLY_OPTION, 6},
      {"?opt=$~document", RuleParser::ParseError::ALLOWLIST_ONLY_OPTION, 7},
      {"~host$sitekey=1", RuleParser::ParseError::UNSUPPORTED_FEATURE, 6},
      {"?param=$~match-case", RuleParser::ParseError::NOT_A_TRISTATE_OPTION, 9},
      {"?param=$domain", RuleParser::ParseError::NO_VALUE_PROVIDED, 8},
      {"#@!", RuleParser::ParseError::WRONG_CSS_RULE_DELIM, 2},
      {"#@#", RuleParser::ParseError::EMPTY_CSS_SELECTOR, 3},
      {"example.com##", RuleParser::ParseError::EMPTY_CSS_SELECTOR, 13},
  };

  for (const auto& rule_and_expectation : kRulesAndExpectations) {
    SCOPED_TRACE(testing::Message() << "Rule: " << rule_and_expectation.rule);

    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_UNSPECIFIED,
              parser.Parse(rule_and_expectation.rule));
    EXPECT_NE(RuleParser::ParseError::NONE, parser.parse_error().error_code);
    EXPECT_EQ(rule_and_expectation.rule, parser.parse_error().line);
    EXPECT_EQ(rule_and_expectation.expected_index,
              parser.parse_error().error_index);
  }
}

TEST(RuleParserTest, ParseNonEmptyUrlRuleWithEmptyUrlPattern) {
  const std::string kRules[] = {
      "|", "||", "|||", "@@", "@@|", "@@||", "@@|||", "$image", "@@|$image",
  };

  RuleParser parser;
  for (const std::string& rule : kRules) {
    SCOPED_TRACE(testing::Message() << "Rule: " << rule);

    EXPECT_EQ(url_pattern_index::proto::RULE_TYPE_URL, parser.Parse(rule));
    EXPECT_TRUE(parser.url_rule().url_pattern.empty());
  }
}

}  // namespace subresource_filter
