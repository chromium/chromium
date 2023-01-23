// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/fragment_directives_utils.h"

#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

TEST(TextFragmentsUtilsTest, SplitUrlTextFragmentDirective_NoDelimiter) {
  GURL webpage_url;
  std::string highlight_directive;
  std::string url = "https://www.example.com/hello/";

  EXPECT_FALSE(
      SplitUrlTextFragmentDirective(url, &webpage_url, &highlight_directive));
  EXPECT_TRUE(webpage_url.spec().empty());
  EXPECT_TRUE(highlight_directive.empty());
}

TEST(TextFragmentsUtilsTest,
     SplitUrlTextFragmentDirective_NormalUrlWithFragment) {
  GURL webpage_url;
  std::string highlight_directive;

  std::string url = "https://www.example.com/hello/#:~:text=blahblah";
  EXPECT_TRUE(
      SplitUrlTextFragmentDirective(url, &webpage_url, &highlight_directive));
  EXPECT_EQ("https://www.example.com/hello/", webpage_url.spec());
  EXPECT_EQ("blahblah", highlight_directive);
}

TEST(TextFragmentsUtilsTest, SplitUrlTextFragmentDirective_JustFragment) {
  GURL webpage_url;
  std::string highlight_directive;
  std::string url = "#:~:text=blueblue";

  EXPECT_TRUE(
      SplitUrlTextFragmentDirective(url, &webpage_url, &highlight_directive));
  EXPECT_EQ("", webpage_url.spec());
  EXPECT_EQ("blueblue", highlight_directive);
}

TEST(TextFragmentsUtilsTest,
     SplitUrlTextFragmentDirective_OtherAnchorBeforeFragment) {
  GURL webpage_url;
  std::string highlight_directive;
  std::string url = "https://www.example.com/hello/#AnAnchor&:~:text=blahblah";

  EXPECT_TRUE(
      SplitUrlTextFragmentDirective(url, &webpage_url, &highlight_directive));
  EXPECT_EQ("https://www.example.com/hello/#AnAnchor", webpage_url.spec());
  EXPECT_EQ("blahblah", highlight_directive);
}

TEST(TextFragmentsUtilsTest, ParseTextFragments) {
  GURL url_with_fragment(
      "https://www.example.com/#idFrag:~:text=text%201&text=text%202");
  base::Value result = ParseTextFragments(url_with_fragment);
  ASSERT_EQ(2u, result.GetList().size());
  EXPECT_EQ("text 1",
            *result.GetList()[0].GetDict().FindString(kFragmentTextStartKey));
  EXPECT_EQ("text 2",
            *result.GetList()[1].GetDict().FindString(kFragmentTextStartKey));

  GURL url_no_fragment("www.example.com");
  base::Value empty_result = ParseTextFragments(url_no_fragment);
  EXPECT_TRUE(empty_result.is_none());
}

TEST(TextFragmentsUtilsTest, ExtractTextFragments) {
  std::vector<std::string> expected = {"test1", "test2", "test3"};
  // Ensure presence/absence of a trailing & doesn't break anything
  EXPECT_EQ(expected,
            ExtractTextFragments("#id:~:text=test1&text=test2&text=test3"));
  EXPECT_EQ(expected,
            ExtractTextFragments("#id:~:text=test1&text=test2&text=test3&"));

  // Test that empty tokens (&& or &text=&) are discarded
  EXPECT_EQ(expected, ExtractTextFragments(
                          "#id:~:text=test1&&text=test2&text=&text=test3"));

  expected.clear();
  EXPECT_EQ(expected, ExtractTextFragments("#idButNoTextFragmentsHere"));
  EXPECT_EQ(expected, ExtractTextFragments(""));
}

TEST(TextFragmentsUtilsTest, RemoveTextFragments) {
  GURL url_with_fragment(
      "https://www.example.com/#idFrag:~:text=text%201&text=text%202");
  GURL result = RemoveFragmentSelectorDirectives(url_with_fragment);
  EXPECT_EQ("https://www.example.com/#idFrag", result.spec());
}

TEST(TextFragmentsUtilsTest,
     RemoveTextFragmentsAndSelectorFragmentsAndKeepOtherSelectors) {
  GURL url_with_fragment(R"(
      https://www.example.org#:~:text=foo&lorem_directive_string&selector(type=CssSelector,value=img[src$="example.org"]))");
  GURL url_with_fragment_removed(R"(
      https://www.example.org#:~:lorem_directive_string)");
  GURL result = RemoveFragmentSelectorDirectives(url_with_fragment);
  EXPECT_EQ(url_with_fragment_removed.spec(), result.spec());

  url_with_fragment = GURL{
      R"(https://www.example.org#:~:lorem_directive_string&selector(type=CssSelector,value=img[src$="example.org"]))"};
  result = RemoveFragmentSelectorDirectives(url_with_fragment);
  EXPECT_EQ(url_with_fragment_removed.spec(), result.spec());
}

TEST(TextFragmentsUtilsTest, RemoveTextFragmentsWithNoFragments) {
  GURL url_without_fragment("https://www.example.com/no_fragment");
  GURL result = RemoveFragmentSelectorDirectives(url_without_fragment);
  EXPECT_EQ("https://www.example.com/no_fragment", result.spec());
}

TEST(TextFragmentsUtilsTest, RemoveTextFragmentsParameterBeforeDelimiter) {
  GURL url("https://www.example.com/?text=foo#:~:bar");
  GURL result = RemoveFragmentSelectorDirectives(url);
  EXPECT_EQ("https://www.example.com/?text=foo#:~:bar", result.spec());
}

TEST(TextFragmentsUtilsTest, RemoveTextFragmentsParameterIsSubstring) {
  GURL url("https://www.example.com/#:~:case_insensitive_text=foo");
  GURL result = RemoveFragmentSelectorDirectives(url);
  EXPECT_EQ("https://www.example.com/#:~:case_insensitive_text=foo",
            result.spec());
}

TEST(TextFragmentsUtilsTest, RemoveTextFragmentsWithNonTextFragment) {
  GURL url("https://example.com/?not_a_frag_directive:~:#no_text_fragments");
  GURL result = RemoveFragmentSelectorDirectives(url);
  EXPECT_EQ("https://example.com/?not_a_frag_directive:~:#no_text_fragments",
            result.spec());
}

TEST(TextFragmentsUtilsTest, RemoveTextFragmentsMultipleDelimiters) {
  GURL url(
      "https://example.com/"
      "?not_a_frag_directive:~:#no_text_fragments:~:text=test,frag");
  GURL result = RemoveFragmentSelectorDirectives(url);
  EXPECT_EQ("https://example.com/?not_a_frag_directive:~:#no_text_fragments",
            result.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesOneFragment) {
  GURL base_url("https://www.chromium.org");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ("https://www.chromium.org/#:~:text=only%20start",
            created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesURLWithPound) {
  GURL base_url("https://www.chromium.org/#");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ("https://www.chromium.org/#:~:text=only%20start",
            created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesURLWithPoundAndValue) {
  GURL base_url("https://www.chromium.org/#SomeAnchor");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ("https://www.chromium.org/#SomeAnchor:~:text=only%20start",
            created_url.spec());
}

TEST(TextFragmentsUtilsTest,
     AppendFragmentDirectivesURLWithPoundAndExistingFragment) {
  GURL base_url("https://www.chromium.org/#:~:text=some%20value");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#:~:text=only%20start",
      created_url.spec());
}

TEST(TextFragmentsUtilsTest,
     AppendFragmentDirectivesURLWithPoundAndExistingFragmentAndAnchor) {
  GURL base_url("https://www.chromium.org/#SomeAnchor:~:text=some%20value");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#SomeAnchor:~:text=only%20start",
      created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesTwoFragments) {
  GURL base_url("https://www.chromium.org");
  TextFragment first_test_fragment("only start");
  TextFragment second_test_fragment("only,- start #2");

  GURL created_url = AppendFragmentDirectives(
      base_url, {first_test_fragment, second_test_fragment});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#:~:text=only%20start&text=only%2C%2D%20start%20%232",
      created_url.spec());
}

TEST(TextFragmentsUtilsTest,
     AppendSelectorsURLWithPoundAndExistingFragmentAndAnchor) {
  GURL base_url("https://www.chromium.org/#SomeAnchor:~:text=some%20value");
  std::string test_selector("only%20start");

  GURL created_url = AppendSelectors(base_url, {test_selector});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#SomeAnchor:~:text=only%20start",
      created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendSelectorsTwoFragments) {
  GURL base_url("https://www.chromium.org");
  std::string first_test_selector("only start");
  std::string second_test_selector("only%2C%2D%20start%20%232");

  GURL created_url =
      AppendSelectors(base_url, {first_test_selector, second_test_selector});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#:~:text=only%20start&text=only%2C%2D%20start%20%232",
      created_url.spec());
}

}  // namespace
}  // namespace shared_highlighting
