// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/search_metadata.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/i18n/string_search.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

// A simple wrapper for testing FindAndHighlightWrapper(). It just converts the
// query text parameter to FixedPatternStringSearchIgnoringCaseAndAccents.
bool FindAndHighlightWrapper(
    const std::string& text,
    const std::string& query_text,
    std::string* highlighted_text) {
  std::vector<std::unique_ptr<
      base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>
      queries;
  queries.push_back(std::make_unique<
                    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
      base::UTF8ToUTF16(query_text)));
  return FindAndHighlight(text, queries, highlighted_text);
}

}  // namespace

TEST(SearchMetadataSimpleTest, FindAndHighlight_ZeroMatches) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlightWrapper("text", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyText) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlightWrapper("", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyQuery) {
  std::vector<std::unique_ptr<
      base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>
      queries;

  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello", queries, &highlighted_text));
  EXPECT_EQ("hello", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_FullMatch) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("hello", "hello", &highlighted_text));
  EXPECT_EQ("<b>hello</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_StartWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("hello, world", "hello",
                                     &highlighted_text));
  EXPECT_EQ("<b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EndWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("hello, world", "world",
                                     &highlighted_text));
  EXPECT_EQ("hello, <b>world</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_InTheMiddle) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("yo hello, world", "hello",
                                     &highlighted_text));
  EXPECT_EQ("yo <b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MultipeMatches) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("yoyoyoyoy", "yoy", &highlighted_text));
  // Only the first match is highlighted.
  EXPECT_EQ("<b>yoy</b>oyoyoy", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_IgnoreCase) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("HeLLo", "hello", &highlighted_text));
  EXPECT_EQ("<b>HeLLo</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_IgnoreCaseNonASCII) {
  std::string highlighted_text;

  // Case and accent ignorance in Greek. Find "socra" in "Socra'tes".
  EXPECT_TRUE(FindAndHighlightWrapper(
      "\xCE\xA3\xCF\x89\xCE\xBA\xCF\x81\xCE\xAC\xCF\x84\xCE\xB7\xCF\x82",
      "\xCF\x83\xCF\x89\xCE\xBA\xCF\x81\xCE\xB1", &highlighted_text));
  EXPECT_EQ(
      "<b>\xCE\xA3\xCF\x89\xCE\xBA\xCF\x81\xCE\xAC</b>\xCF\x84\xCE\xB7\xCF\x82",
      highlighted_text);

  // In Japanese characters.
  // Find Hiragana "pi" + "(small)ya" in Katakana "hi" + semi-voiced-mark + "ya"
  EXPECT_TRUE(FindAndHighlightWrapper(
      "\xE3\x81\xB2\xE3\x82\x9A\xE3\x82\x83\xE3\x83\xBC",
      "\xE3\x83\x94\xE3\x83\xA4",
      &highlighted_text));
  EXPECT_EQ(
      "<b>\xE3\x81\xB2\xE3\x82\x9A\xE3\x82\x83</b>\xE3\x83\xBC",
      highlighted_text);
}

TEST(SearchMetadataSimpleTest, MultiTextBySingleQuery) {
  std::vector<std::unique_ptr<
      base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>
      queries;
  queries.push_back(std::make_unique<
                    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
      base::UTF8ToUTF16("hello")));

  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello", queries, &highlighted_text));
  EXPECT_EQ("<b>hello</b>", highlighted_text);
  EXPECT_FALSE(FindAndHighlight("goodbye", queries, &highlighted_text));
  EXPECT_TRUE(FindAndHighlight("1hello2", queries, &highlighted_text));
  EXPECT_EQ("1<b>hello</b>2", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("<hello>", "hello", &highlighted_text));
  EXPECT_EQ("&lt;<b>hello</b>&gt;", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MoreMetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("a&b&c&d", "b&c", &highlighted_text));
  EXPECT_EQ("a&amp;<b>b&amp;c</b>&amp;d", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_SurrogatePair) {
  std::string highlighted_text;
  // \xF0\x9F\x98\x81 (U+1F601) is a surrogate pair for smile icon of emoji.
  EXPECT_TRUE(FindAndHighlightWrapper("hi\xF0\x9F\x98\x81hello",
                                      "i\xF0\x9F\x98\x81", &highlighted_text));
  EXPECT_EQ("h<b>i\xF0\x9F\x98\x81</b>hello", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MultipleQueries) {
  std::vector<std::unique_ptr<
      base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>
      queries;
  queries.push_back(std::make_unique<
                    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
      base::UTF8ToUTF16("hello")));
  queries.push_back(std::make_unique<
                    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
      base::UTF8ToUTF16("good")));

  std::string highlighted_text;
  EXPECT_TRUE(
      FindAndHighlight("good morning, hello", queries, &highlighted_text));
  EXPECT_EQ("<b>good</b> morning, <b>hello</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_OverlappingHighlights) {
  std::vector<std::unique_ptr<
      base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>
      queries;
  queries.push_back(std::make_unique<
                    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
      base::UTF8ToUTF16("morning")));
  queries.push_back(std::make_unique<
                    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
      base::UTF8ToUTF16("ing,")));

  std::string highlighted_text;
  EXPECT_TRUE(
      FindAndHighlight("good morning, hello", queries, &highlighted_text));
  EXPECT_EQ("good <b>morning,</b> hello", highlighted_text);
}

}  // namespace internal
}  // namespace drive
