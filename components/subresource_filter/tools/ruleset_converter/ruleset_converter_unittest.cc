// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/ruleset_converter.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

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

base::CommandLine::StringType AsciiToNativeString(std::string ascii) {
#if BUILDFLAG(IS_WIN)
  return base::ASCIIToWide(ascii);
#else
  return ascii;
#endif
}

class RulesetConverterTest : public testing::Test {
 public:
  RulesetConverterTest() { test_content_.AppendRules(GetSomeRules()); }
  TestRulesetContents test_content_;
};

TEST_F(RulesetConverterTest, InputNotFound_Fails) {
  RulesetConverter converter;
  EXPECT_FALSE(converter.SetInputFiles(AsciiToNativeString("/not-a-file")));
}

TEST_F(RulesetConverterTest, NoInputFiles_Fails) {
  RulesetConverter converter;
  EXPECT_FALSE(converter.SetInputFiles(AsciiToNativeString("")));
}

TEST_F(RulesetConverterTest, OutputDirNotFound_Fails) {
  RulesetConverter converter;
  base::FilePath path(FILE_PATH_LITERAL("/not-a-dir/output"));
  EXPECT_FALSE(converter.SetOutputFile(path));
}

TEST_F(RulesetConverterTest, BadInputFormat_Fails) {
  RulesetConverter converter;
  EXPECT_FALSE(converter.SetInputFormat("badformat"));
}

TEST_F(RulesetConverterTest, BadOutputFormat_Fails) {
  RulesetConverter converter;
  EXPECT_FALSE(converter.SetOutputFormat("badformat"));
}

TEST_F(RulesetConverterTest, BadChromeVersions_Fail) {
  RulesetConverter converter;
  EXPECT_FALSE(converter.SetChromeVersion("not-a-number"));
  EXPECT_FALSE(converter.SetChromeVersion("1"));
  EXPECT_FALSE(converter.SetChromeVersion("60"));
}

TEST_F(RulesetConverterTest, NoInput_Fails) {
  RulesetConverter converter;
  base::FilePath path(FILE_PATH_LITERAL("/output"));
  EXPECT_TRUE(converter.SetOutputFile(path));
  EXPECT_FALSE(converter.Convert());
}

TEST_F(RulesetConverterTest, NoOutput_Fails) {
  RulesetConverter converter;
  ScopedTempRulesetFile ruleset_file(RulesetFormat::kFilterList);
  ruleset_file.WriteRuleset(test_content_);
  EXPECT_TRUE(converter.SetInputFiles(
      AsciiToNativeString(ruleset_file.ruleset_path().AsUTF8Unsafe())));
  EXPECT_FALSE(converter.Convert());
}

TEST_F(RulesetConverterTest, InputAndOneOutput_Succeeds) {
  RulesetConverter converter;
  ScopedTempRulesetFile input_file(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_file(RulesetFormat::kUnindexedRuleset);

  input_file.WriteRuleset(test_content_);
  EXPECT_TRUE(converter.SetInputFiles(
      AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
  EXPECT_TRUE(converter.SetOutputFile(output_file.ruleset_path()));
  EXPECT_TRUE(converter.Convert());
}

TEST_F(RulesetConverterTest, MultipleInputs) {
  RulesetConverter converter;
  ScopedTempRulesetFile input_1(RulesetFormat::kFilterList);
  ScopedTempRulesetFile input_2(RulesetFormat::kFilterList);
  ScopedTempRulesetFile input_3(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_file(RulesetFormat::kFilterList);

  input_1.WriteRuleset(test_content_);

  TestRulesetContents content_2;
  content_2.AppendRules({"foo.com", "eggs*waffles.org"});
  input_2.WriteRuleset(content_2);

  TestRulesetContents content_3;
  content_3.AppendRules({"@@bar.net/some/path"});
  input_3.WriteRuleset(content_3);

  std::string joined = base::JoinString({input_1.ruleset_path().AsUTF8Unsafe(),
                                         input_2.ruleset_path().AsUTF8Unsafe(),
                                         input_3.ruleset_path().AsUTF8Unsafe()},
                                        ",");
  EXPECT_TRUE(converter.SetInputFiles(AsciiToNativeString(joined)));
  EXPECT_TRUE(converter.SetOutputFile(output_file.ruleset_path()));
  EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
  EXPECT_TRUE(converter.Convert());

  TestRulesetContents expected_output = test_content_;
  expected_output.AppendParsedRules(content_2);
  expected_output.AppendParsedRules(content_3);
  EXPECT_EQ(expected_output, output_file.ReadContents());
}

TEST_F(RulesetConverterTest, MultipleOutputs) {
  RulesetConverter converter;
  ScopedTempRulesetFile input_file(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_url(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_css(RulesetFormat::kFilterList);

  input_file.WriteRuleset(test_content_);

  EXPECT_TRUE(converter.SetInputFiles(
      AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
  EXPECT_TRUE(converter.SetOutputFileUrl(output_url.ruleset_path()));
  EXPECT_TRUE(converter.SetOutputFileCss(output_url.ruleset_path()));
  EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
  EXPECT_TRUE(converter.Convert());

  TestRulesetContents expected_url_output;
  expected_url_output.url_rules = test_content_.url_rules;
  TestRulesetContents expected_css_output;
  expected_url_output.css_rules = test_content_.css_rules;

  EXPECT_EQ(expected_url_output, output_url.ReadContents());
  EXPECT_EQ(expected_css_output, output_css.ReadContents());
}

TEST_F(RulesetConverterTest, UrlOutput) {
  RulesetConverter converter;
  ScopedTempRulesetFile input_file(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_url(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_css(RulesetFormat::kFilterList);

  input_file.WriteRuleset(test_content_);

  EXPECT_TRUE(converter.SetInputFiles(
      AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
  EXPECT_TRUE(converter.SetOutputFileUrl(output_url.ruleset_path()));
  EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
  EXPECT_TRUE(converter.Convert());

  TestRulesetContents expected_url_output;
  expected_url_output.url_rules = test_content_.url_rules;

  // Expect no CSS output.
  TestRulesetContents expected_css_output;

  EXPECT_EQ(expected_url_output, output_url.ReadContents());
  EXPECT_EQ(expected_css_output, output_css.ReadContents());
}

TEST_F(RulesetConverterTest, CssOutput) {
  RulesetConverter converter;
  ScopedTempRulesetFile input_file(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_url(RulesetFormat::kFilterList);
  ScopedTempRulesetFile output_css(RulesetFormat::kFilterList);

  input_file.WriteRuleset(test_content_);

  EXPECT_TRUE(converter.SetInputFiles(
      AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
  EXPECT_TRUE(converter.SetOutputFileCss(output_css.ruleset_path()));
  EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
  EXPECT_TRUE(converter.Convert());

  // Expect no URL output.
  TestRulesetContents expected_url_output;

  TestRulesetContents expected_css_output;
  expected_css_output.css_rules = test_content_.css_rules;

  EXPECT_EQ(expected_url_output, output_url.ReadContents());
  EXPECT_EQ(expected_css_output, output_css.ReadContents());
}

TEST_F(RulesetConverterTest, ChromeVersions) {
  ScopedTempRulesetFile input_file(RulesetFormat::kFilterList);

  std::vector<std::string> kUnfriendlyRules{
      // No Chrome version supports regexes.
      "/a[0-9].com/$image",

      // No Chrome version supports badly defined rules.
      "a.com$image,~image",

      // No Chrome version supports popups.
      "a.com$popup",

      // Only genericblock/document activations are supported.
      "@@a.com$generichide", "@@a.com$elemhide",

      // Chrome 59+ supports websocket.
      "host.com$websocket",
  };
  TestRulesetContents unfriendly_contents;
  unfriendly_contents.AppendRules(kUnfriendlyRules);
  input_file.WriteRuleset(unfriendly_contents);
  {
    RulesetConverter converter;
    ScopedTempRulesetFile output_file(RulesetFormat::kFilterList);
    EXPECT_TRUE(converter.SetInputFiles(
        AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
    EXPECT_TRUE(converter.SetOutputFile(output_file.ruleset_path()));
    EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
    EXPECT_TRUE(converter.SetChromeVersion("0"));
    EXPECT_TRUE(converter.Convert());
    EXPECT_EQ(unfriendly_contents, output_file.ReadContents());
  }
  {
    RulesetConverter converter;
    ScopedTempRulesetFile output_file(RulesetFormat::kFilterList);
    EXPECT_TRUE(converter.SetInputFiles(
        AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
    EXPECT_TRUE(converter.SetOutputFile(output_file.ruleset_path()));
    EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
    EXPECT_TRUE(converter.SetChromeVersion("54"));
    EXPECT_TRUE(converter.Convert());
    EXPECT_EQ(0u, output_file.ReadContents().url_rules.size());
  }
  {
    RulesetConverter converter;
    ScopedTempRulesetFile output_file(RulesetFormat::kFilterList);
    EXPECT_TRUE(converter.SetInputFiles(
        AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
    EXPECT_TRUE(converter.SetOutputFile(output_file.ruleset_path()));
    EXPECT_TRUE(converter.SetOutputFormat("filter-list"));
    EXPECT_TRUE(converter.SetChromeVersion("59"));
    EXPECT_TRUE(converter.Convert());

    TestRulesetContents expected_contents;
    expected_contents.AppendRules({"host.com$websocket"});
    EXPECT_EQ(expected_contents, output_file.ReadContents());
  }
}

TEST_F(RulesetConverterTest, FormatConversions) {
  std::vector<std::string> kSupportedFormats = {"filter-list",
                                                "unindexed-ruleset", "proto"};
  for (const auto& input_format : kSupportedFormats) {
    for (const auto& output_format : kSupportedFormats) {
      SCOPED_TRACE(testing::Message() << "input: " << input_format
                                      << " output: " << output_format);
      RulesetConverter converter;
      ScopedTempRulesetFile input_file(ParseFlag(input_format));
      input_file.WriteRuleset(test_content_);

      ScopedTempRulesetFile output_file(ParseFlag(output_format));
      EXPECT_TRUE(converter.SetInputFiles(
          AsciiToNativeString(input_file.ruleset_path().AsUTF8Unsafe())));
      EXPECT_TRUE(converter.SetOutputFile(output_file.ruleset_path()));
      EXPECT_TRUE(converter.SetInputFormat(input_format));
      EXPECT_TRUE(converter.SetOutputFormat(output_format));
      EXPECT_TRUE(converter.Convert());

      TestRulesetContents input_contents = input_file.ReadContents();
      TestRulesetContents output_contents = output_file.ReadContents();
      TestRulesetContents expected_output;

      // Unindexed format strips CSS rules.
      if (output_format == "unindexed-ruleset" &&
          input_format != output_format) {
        expected_output.url_rules = input_contents.url_rules;
      } else {
        expected_output = input_contents;
      }
      EXPECT_EQ(expected_output, output_contents);
    }
  }
}

}  // namespace subresource_filter
