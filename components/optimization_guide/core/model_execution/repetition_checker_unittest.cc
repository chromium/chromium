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

TEST(OptimizationGuideRepetitionCheckerTest, GetNumTrailingNewlines) {
  struct TestCase {
    const char* text;
    size_t num_trailing_newlines;
  } cases[] = {
      {"hello\n", 1},
      {"hello\n\n", 2},
      {"hello\n\n\n", 3},
      {"hello", 0},
      {"hello world", 0},
      {"hello\nworld", 0},
      {"\nhello", 0},
      {"hello\nworld\nagain", 0},
      {"", 0},
      {"\n", 1},
      {"\n\n\n", 3},
      {"hello \n", 1},
      {"hello\n ", 0},
      {"hello\n\t", 0},
  };
  for (const auto& test_case : cases) {
    const auto ret = GetNumTrailingNewlines(test_case.text);
    EXPECT_EQ(ret, test_case.num_trailing_newlines)
        << "Expect " << test_case.num_trailing_newlines
        << " but function returned " << ret << std::endl
        << test_case.text;
  }
}

TEST(OptimizationGuideRepetitionCheckerTest, CheckTextContainsNonNewline) {
  struct TestCase {
    const char* text;
    bool has_non_newline;
  } cases[] = {
      {"hello", true},   {"hello\nworld", true}, {"\nhello", true},
      {"a", true},       {"123", true},          {" ", true},
      {"\t", true},      {" \n", true},          {"\n\t\n", true},
      {"", false},       {"\n", false},          {"\n\n", false},
      {"\n\n\n", false},
  };
  for (const auto& test_case : cases) {
    const auto ret = CheckTextContainsNonNewline(test_case.text);
    EXPECT_EQ(ret, test_case.has_non_newline)
        << "Expect " << test_case.has_non_newline << " but function returned "
        << ret << std::endl
        << test_case.text;
  }
}

TEST(OptimizationGuideRepetitionCheckerTest, NewlineBuffer) {
  NewlineBuffer newline_buffer;
  NewlineBuffer::Chunk released_chunk;

  released_chunk = newline_buffer.Append("Hello!");
  EXPECT_EQ(released_chunk.text, "Hello!");
  EXPECT_EQ(released_chunk.num_tokens, 1u);

  released_chunk = newline_buffer.Append("This has a trailing newline\n");
  EXPECT_EQ(released_chunk.text, "This has a trailing newline");
  EXPECT_EQ(released_chunk.num_tokens, 1u);

  released_chunk = newline_buffer.Append("\n");
  EXPECT_EQ(released_chunk.text, "");
  EXPECT_EQ(released_chunk.num_tokens, 0u);

  released_chunk = newline_buffer.Append("\n\n");
  EXPECT_EQ(released_chunk.text, "");
  EXPECT_EQ(released_chunk.num_tokens, 0u);

  released_chunk = newline_buffer.Append("\nThe end!\n");
  EXPECT_EQ(released_chunk.text, "\n\n\n\n\nThe end!");
  EXPECT_EQ(released_chunk.num_tokens, 3u);

  released_chunk = newline_buffer.Append("");
  EXPECT_EQ(released_chunk.text, "");
  EXPECT_EQ(released_chunk.num_tokens, 0u);

  released_chunk = newline_buffer.Append("Bye!");
  EXPECT_EQ(released_chunk.text, "\nBye!");
  EXPECT_EQ(released_chunk.num_tokens, 2u);
}

}  // namespace
}  // namespace optimization_guide
