// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/fuzzy_pattern_matching.h"

#include <string_view>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace url_pattern_index {

TEST(SubresourceFilterFuzzyPatternMatchingTest, StartsWithFuzzy) {
  const struct {
    const char* text;
    const char* subpattern;
    bool expected_starts_with;
  } kTestCases[] = {
      {"abc", "", true},       {"abc", "a", true},     {"abc", "ab", true},
      {"abc", "abc", true},    {"abc", "abcd", false}, {"abc", "abc^^", false},
      {"abc", "abcd^", false}, {"abc", "ab^", false},  {"abc", "bc", false},
      {"abc", "bc^", false},   {"abc", "^abc", false},
  };
  // TODO(pkalinnikov): Make end-of-string match '^' again.

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test: " << test_case.text
                 << "; Subpattern: " << test_case.subpattern);

    const bool starts_with =
        StartsWithFuzzy(test_case.text, test_case.subpattern);
    EXPECT_EQ(test_case.expected_starts_with, starts_with);
  }
}

TEST(SubresourceFilterFuzzyPatternMatchingTest, EndsWithFuzzy) {
  const struct {
    const char* text;
    const char* subpattern;
    bool expected_ends_with;
  } kTestCases[] = {
      {"abc", "", true},       {"abc", "c", true},     {"abc", "bc", true},
      {"abc", "abc", true},    {"abc", "0abc", false}, {"abc", "abc^^", false},
      {"abc", "abcd^", false}, {"abc", "ab^", false},  {"abc", "ab", false},
      {"abc", "^abc", false},
  };
  // TODO(pkalinnikov): Make end-of-string match '^' again.

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test: " << test_case.text
                 << "; Subpattern: " << test_case.subpattern);

    const bool ends_with = EndsWithFuzzy(test_case.text, test_case.subpattern);
    EXPECT_EQ(test_case.expected_ends_with, ends_with);
  }
}

TEST(SubresourceFilterFuzzyPatternMatchingTest, FindFuzzy) {
  const struct {
    std::string_view text;
    std::string_view subpattern;
    std::vector<size_t> expected_occurrences;
  } kTestCases[] = {
      {"abcd", "", {0, 1, 2, 3, 4}},
      {"abcd", "de", std::vector<size_t>()},
      {"abcd", "ab", {0}},
      {"abcd", "bc", {1}},
      {"abcd", "cd", {2}},

      {"a/bc/a/b", "", {0, 1, 2, 3, 4, 5, 6, 7, 8}},
      {"a/bc/a/b", "de", std::vector<size_t>()},
      {"a/bc/a/b", "a/", {0, 5}},
      {"a/bc/a/c", "a/c", {5}},
      {"a/bc/a/c", "a^c", {5}},
      {"a/bc/a/c", "a?c", std::vector<size_t>()},

      {"ab^cd", "ab/cd", std::vector<size_t>()},
      {"ab^cd", "b/c", std::vector<size_t>()},
      {"ab^cd", "ab^cd", {0}},
      {"ab^cd", "b^c", {1}},
      {"ab^b/b", "b/b", {3}},

      {"a/a/a/a", "a/a", {0, 2, 4}},
      {"a/a/a/a", "^a^a^a", {1}},
      {"a/a/a/a", "^a^a?a", std::vector<size_t>()},
      {"a/a/a/a", "?a?a?a", std::vector<size_t>()},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Test: " << test_case.text
                 << "; Subpattern: " << test_case.subpattern);

    std::vector<size_t> occurrences;
    for (size_t position = 0; position <= test_case.text.size(); ++position) {
      position = FindFuzzy(test_case.text, test_case.subpattern, position);
      if (position == std::string_view::npos) {
        break;
      }
      occurrences.push_back(position);
    }

    EXPECT_EQ(test_case.expected_occurrences, occurrences);
  }
}

}  // namespace url_pattern_index
