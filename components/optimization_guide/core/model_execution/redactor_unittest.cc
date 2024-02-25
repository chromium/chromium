// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/redactor.h"

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace optimization_guide {

using proto::RedactBehavior;

Rule CreateRule(
    const std::string& regex,
    RedactBehavior behavior = RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT,
    std::optional<std::string> replacement_string = std::nullopt,
    std::optional<int> min_pattern_length = std::nullopt,
    std::optional<int> max_pattern_length = std::nullopt,
    std::optional<int> group = std::nullopt) {
  Rule rule;
  rule.regex = regex;
  rule.behavior = behavior;
  rule.replacement_string = std::move(replacement_string);
  rule.matching_group = std::move(group);
  rule.min_pattern_length = std::move(min_pattern_length);
  rule.max_pattern_length = std::move(max_pattern_length);
  return rule;
}

TEST(RedactorTest, RedactMultipleHitsNotPresentInInput) {
  Redactor redactor({CreateRule("ab")});
  std::string output("ab cab");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("[##] c[##]", output);
}

TEST(RedactorTest, RedactMultipleHits) {
  Redactor redactor({CreateRule("ab")});
  std::string output("ab cab");
  redactor.Redact("zabq", output);
  EXPECT_EQ("ab cab", output);
}

TEST(RedactorTest, RedactMultipleHitsMultipleRegex) {
  Redactor redactor({CreateRule("ab"), CreateRule("z")});
  std::string output("ab zcab");
  redactor.Redact(std::string(), output);
  EXPECT_EQ("[##] [#]c[##]", output);
}

TEST(RedactorTest, RedactNotAtEnd) {
  Redactor redactor({CreateRule("ab")});
  std::string output("abc");
  redactor.Redact(std::string(), output);
  EXPECT_EQ("[##]c", output);
}

TEST(RedactorTest, RedactAlways) {
  Redactor redactor({CreateRule("ab", RedactBehavior::REDACT_ALWAYS)});
  std::string output("abc");
  redactor.Redact("ab", output);
  EXPECT_EQ("[##]c", output);
}

TEST(RedactorTest, Reject) {
  Redactor redactor({CreateRule("ab", RedactBehavior::REJECT)});
  std::string output("abc");
  EXPECT_EQ(RedactResult::kReject, redactor.Redact(std::string(), output));
}

TEST(RedactorTest, RedactWithReplacmentText) {
  Redactor redactor({CreateRule("ab", RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT,
                                "|redacted)")});
  std::string output("ab cab");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("|redacted) c|redacted)", output);
}

TEST(RedactorTest, DontRedactIfMatchTooMuch) {
  Redactor redactor(
      {CreateRule("a*", RedactBehavior::REDACT_ALWAYS, std::string(), 2, 4)});
  const std::string original_output("baaaaaaac");
  std::string output(original_output);
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  // No redact should happen because too much matched.
  EXPECT_EQ(original_output, output);
}

TEST(RedactorTest, DontRedactIfMatchTooLittle) {
  Redactor redactor(
      {CreateRule("a*", RedactBehavior::REDACT_ALWAYS, std::string(), 2, 4)});
  const std::string original_output("bad");
  std::string output(original_output);
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  // No redact should happen because it didn't match enough.
  EXPECT_EQ(original_output, output);
}

TEST(RedactorTest, MatchLimits) {
  Redactor redactor(
      {CreateRule("a*", RedactBehavior::REDACT_ALWAYS, std::nullopt, 2, 4)});
  const std::string original_output("baaad");
  std::string output(original_output);
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("b[###]d", output);
}

TEST(RedactorTest, ReplaceGroup) {
  Redactor redactor({CreateRule("(?:a)(b+)", RedactBehavior::REDACT_ALWAYS,
                                std::nullopt, 2, 4, 1)});
  std::string output("abbbcd");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("a[###]cd", output);
}

TEST(RedactorTest, ReplaceGroup2) {
  Redactor redactor({CreateRule("(a)(b+)", RedactBehavior::REDACT_ALWAYS,
                                std::nullopt, 2, 4, 2)});
  std::string output("abbbcd");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("a[###]cd", output);
}

}  // namespace optimization_guide
