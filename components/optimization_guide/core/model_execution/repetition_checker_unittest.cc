// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/repetition_checker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

TEST(OptimizationGuideRepetitionCheckerTest, HasRepeatingSuffix) {
  struct TestCase {
    int min_chars;
    int num_repeats;
    const char* text;
    bool has_repeats;
  } cases[] = {
      {3, 2, "abcabc", true},
      {3, 2, "abcacc", false},
      {3, 3, "abcabc", false},
      {3, 3, "abcabcabc", true},
      {3, 3, "abcdabcabc", false},
      {3, 2, "abcdabcd", true},
      {10, 2, "abcdabcd", false},
      {10, 2, "some text. This is repeating. This is repeating.", true},
      {10, 3, "some text. This is repeating. This is repeating.", false},
      {10, 3,
       "some text. This is repeating. This is repeating. This is repeating",
       true},
      {10, 2, "some text. This is repeating. This is repeating. end text",
       false},
      {1, 2, "aaaaaaaaaaaaaaaaa", true},
      {1, 10, "aaaaaaaaaaaaaaaaa", true},
      {1, 10, "aaaaaaabaaaaaaaaa", false},
      {1, 10, "aaaaaaaaaaaaaaaa a", false},
      {2, 10, "a a a a a a a a a a a a a ", true},
      {2, 10, "a a a a a ", false},
      {3, 0, "abcabc", false},
  };
  for (const auto& test_case : cases) {
    EXPECT_EQ(HasRepeatingSuffix(test_case.min_chars, test_case.num_repeats,
                                 test_case.text),
              test_case.has_repeats)
        << test_case.min_chars << " " << test_case.num_repeats << " "
        << test_case.text;
  }
}

}  // namespace
}  // namespace optimization_guide
