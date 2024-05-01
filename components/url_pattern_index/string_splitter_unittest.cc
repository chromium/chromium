// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/string_splitter.h"

#include <string>
#include <string_view>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace url_pattern_index {

namespace {
bool IsTestSeparator(char c) {
  return c == ' ' || c == '\t' || c == ',';
}
}

TEST(StringSplitterTest, SplitWithEmptyResult) {
  const char* const kStrings[] = {
      "", " ", "\t", ",", " \t ", ",,,,", "\t\t\t",
  };

  for (const char* string : kStrings) {
    auto splitter = CreateStringSplitter(string, IsTestSeparator);
    // Explicitly verify both operator== and operator!=.
    EXPECT_TRUE(splitter.begin() == splitter.end());
    EXPECT_FALSE(splitter.begin() != splitter.end());
  }
}

TEST(StringSplitterTest, SplitOneWord) {
  const char* const kLongStrings[] = {
      "word",     " word ",   " word",   "word ",   ",word,",
      "\tword\t", "  word  ", "word   ", "   word", ", word, \t",
  };
  const char* const kShortStrings[] = {
      "w", " w ", " w", "w ", "  w  ", "  w", "w  ", ", w, ", "w, \t",
  };

  const char kLongWord[] = "word";
  const char kShortWord[] = "w";

  auto expect_word = [](const char* text, const char* word) {
    auto splitter = CreateStringSplitter(text, IsTestSeparator);
    // Explicitly verify both operator== and operator!=.
    EXPECT_TRUE(splitter.begin() != splitter.end());
    EXPECT_FALSE(splitter.begin() == splitter.end());

    EXPECT_EQ(splitter.end(), ++splitter.begin());
    EXPECT_EQ(word, *splitter.begin());

    auto iterator = splitter.begin();
    EXPECT_EQ(splitter.begin(), iterator++);
    EXPECT_EQ(splitter.end(), iterator);
  };

  for (const char* string : kLongStrings)
    expect_word(string, kLongWord);
  for (const char* string : kShortStrings)
    expect_word(string, kShortWord);
}

TEST(StringSplitterTest, SplitThreeWords) {
  const char* const kStrings[] = {
      "one two three",     " one two three ",   "   one  two, three",
      "one,two\t\t three", "one, two, three, ",
  };
  const std::vector<std::string_view> kResults = {
      "one",
      "two",
      "three",
  };

  for (const char* string : kStrings) {
    auto splitter = CreateStringSplitter(string, IsTestSeparator);
    std::vector<std::string_view> tokens(splitter.begin(), splitter.end());
    EXPECT_EQ(kResults, tokens);
  }
}

}  // namespace url_pattern_index
