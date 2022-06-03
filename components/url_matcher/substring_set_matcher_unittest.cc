// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/substring_set_matcher.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_matcher {

namespace {

void TestOnePattern(const std::string& test_string,
                    const std::string& pattern,
                    bool is_match) {
  std::string test =
      "TestOnePattern(" + test_string + ", " + pattern + ", " +
      (is_match ? "1" : "0") + ")";
  std::vector<StringPattern> patterns;
  patterns.emplace_back(pattern, 1);
  SubstringSetMatcher matcher(patterns);
  std::set<int> matches;
  matcher.Match(test_string, &matches);

  size_t expected_matches = (is_match ? 1 : 0);
  EXPECT_EQ(expected_matches, matches.size()) << test;
  EXPECT_EQ(is_match, matches.find(1) != matches.end()) << test;
}

void TestTwoPatterns(const std::string& test_string,
                     const std::string& pattern_1,
                     const std::string& pattern_2,
                     bool is_match_1,
                     bool is_match_2) {
  std::string test =
      "TestTwoPatterns(" + test_string + ", " + pattern_1 + ", " + pattern_2 +
      ", " + (is_match_1 ? "1" : "0") + ", " + (is_match_2 ? "1" : "0") + ")";
  ASSERT_NE(pattern_1, pattern_2);
  StringPattern substring_pattern_1(pattern_1, 1);
  StringPattern substring_pattern_2(pattern_2, 2);
  // In order to make sure that the order in which patterns are registered
  // does not make any difference we try both permutations.
  for (int permutation = 0; permutation < 2; ++permutation) {
    std::vector<const StringPattern*> patterns;
    if (permutation == 0) {
      patterns.push_back(&substring_pattern_1);
      patterns.push_back(&substring_pattern_2);
    } else {
      patterns.push_back(&substring_pattern_2);
      patterns.push_back(&substring_pattern_1);
    }
    SubstringSetMatcher matcher(patterns);
    std::set<int> matches;
    matcher.Match(test_string, &matches);

    size_t expected_matches = (is_match_1 ? 1 : 0) + (is_match_2 ? 1 : 0);
    EXPECT_EQ(expected_matches, matches.size()) << test;
    EXPECT_EQ(is_match_1, matches.find(1) != matches.end()) << test;
    EXPECT_EQ(is_match_2, matches.find(2) != matches.end()) << test;
  }
}

}  // namespace

TEST(SubstringSetMatcherTest, TestMatcher) {
  // Test overlapping patterns
  // String    abcde
  // Pattern 1  bc
  // Pattern 2   cd
  TestTwoPatterns("abcde", "bc", "cd", true, true);

  // Test subpatterns - part 1
  // String    abcde
  // Pattern 1  bc
  // Pattern 2  b
  TestTwoPatterns("abcde", "bc", "b", true, true);

  // Test subpatterns - part 2
  // String    abcde
  // Pattern 1  bc
  // Pattern 2   c
  TestTwoPatterns("abcde", "bc", "c", true, true);

  // Test identical matches
  // String    abcde
  // Pattern 1 abcde
  TestOnePattern("abcde", "abcde", true);

  // Test multiple matches
  // String    aaaaa
  // Pattern 1 a
  TestOnePattern("abcde", "a", true);

  // Test matches at beginning and end
  // String    abcde
  // Pattern 1 ab
  // Pattern 2    de
  TestTwoPatterns("abcde", "ab", "de", true, true);

  // Test non-match
  // String    abcde
  // Pattern 1        fg
  TestOnePattern("abcde", "fg", false);

  // Test empty pattern and too long pattern
  // String    abcde
  // Pattern 1
  // Pattern 2 abcdef
  TestTwoPatterns("abcde", std::string(), "abcdef", true, false);
}

TEST(SubstringSetMatcherTest, TestMatcher2) {
  StringPattern pattern_1("a", 1);
  StringPattern pattern_2("b", 2);
  StringPattern pattern_3("c", 3);

  std::vector<const StringPattern*> patterns = {&pattern_1, &pattern_2,
                                                &pattern_3};
  auto matcher = std::make_unique<SubstringSetMatcher>(patterns);

  std::set<int> matches;
  matcher->Match("abd", &matches);
  EXPECT_EQ(2u, matches.size());
  EXPECT_TRUE(matches.end() != matches.find(1));
  EXPECT_TRUE(matches.end() != matches.find(2));

  patterns = {&pattern_1, &pattern_3};
  matcher = std::make_unique<SubstringSetMatcher>(patterns);

  matches.clear();
  matcher->Match("abd", &matches);
  EXPECT_EQ(1u, matches.size());
  EXPECT_TRUE(matches.end() != matches.find(1));
  EXPECT_TRUE(matches.end() == matches.find(2));

  matcher = std::make_unique<SubstringSetMatcher>(
      std::vector<const StringPattern*>());
  EXPECT_TRUE(matcher->IsEmpty());
}

TEST(SubstringSetMatcherTest, TestMatcher3) {
  std::string text = "abcde";

  std::vector<StringPattern> patterns;
  int id = 0;
  // Add all substrings of this string, including empty string.
  patterns.emplace_back("", id++);
  for (size_t i = 0; i < text.length(); i++) {
    for (size_t j = i; j < text.length(); j++) {
      patterns.emplace_back(text.substr(i, j - i + 1), id++);
    }
  }

  SubstringSetMatcher matcher(patterns);
  std::set<int> matches;
  matcher.Match(text, &matches);
  EXPECT_EQ(patterns.size(), matches.size());
  for (const StringPattern& pattern : patterns) {
    EXPECT_TRUE(matches.find(pattern.id()) != matches.end())
        << pattern.pattern();
  }
}

TEST(SubstringSetMatcherTest, TestEmptyMatcher) {
  std::vector<StringPattern> patterns;
  SubstringSetMatcher matcher(patterns);
  std::set<int> matches;
  matcher.Match("abd", &matches);
  EXPECT_TRUE(matches.empty());
  EXPECT_TRUE(matcher.IsEmpty());
}

}  // namespace url_matcher
