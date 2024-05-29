// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/redactor.h"
#include <initializer_list>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace optimization_guide {

using proto::RedactBehavior;

Redactor CreateRedactor(std::initializer_list<proto::RedactRule> rules) {
  proto::RedactRules proto_rules;
  for (auto& rule : rules) {
    proto_rules.add_rules()->CopyFrom(rule);
  }
  return Redactor::FromProto(proto_rules);
}

proto::RedactRule CreateRule(
    const std::string& regex,
    RedactBehavior behavior = RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT,
    std::optional<std::string> replacement_string = std::nullopt,
    std::optional<int> min_pattern_length = std::nullopt,
    std::optional<int> max_pattern_length = std::nullopt,
    std::optional<int> group = std::nullopt) {
  proto::RedactRule rule;
  rule.set_regex(regex);
  rule.set_behavior(behavior);
  if (replacement_string) {
    rule.set_replacement_string(*replacement_string);
  }
  if (group) {
    rule.set_group_index(*group);
  }
  if (min_pattern_length) {
    rule.set_min_pattern_length(*min_pattern_length);
  }
  if (max_pattern_length) {
    rule.set_max_pattern_length(*max_pattern_length);
  }
  return rule;
}

TEST(RedactorTest, RedactMultipleHitsNotPresentInInput) {
  auto redactor = CreateRedactor({CreateRule("ab")});
  std::string output("ab cab");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("[##] c[##]", output);
}

TEST(RedactorTest, RedactMultipleHits) {
  auto redactor = CreateRedactor({CreateRule("ab")});
  std::string output("ab cab");
  redactor.Redact("zabq", output);
  EXPECT_EQ("ab cab", output);
}

TEST(RedactorTest, RedactMultipleHitsMultipleRegex) {
  auto redactor = CreateRedactor({CreateRule("ab"), CreateRule("z")});
  std::string output("ab zcab");
  redactor.Redact(std::string(), output);
  EXPECT_EQ("[##] [#]c[##]", output);
}

TEST(RedactorTest, RedactNotAtEnd) {
  auto redactor = CreateRedactor({CreateRule("ab")});
  std::string output("abc");
  redactor.Redact(std::string(), output);
  EXPECT_EQ("[##]c", output);
}

TEST(RedactorTest, RedactAlways) {
  auto redactor =
      CreateRedactor({CreateRule("ab", RedactBehavior::REDACT_ALWAYS)});
  std::string output("abc");
  redactor.Redact("ab", output);
  EXPECT_EQ("[##]c", output);
}

TEST(RedactorTest, Reject) {
  auto redactor = CreateRedactor({CreateRule("ab", RedactBehavior::REJECT)});
  std::string output("abc");
  EXPECT_EQ(RedactResult::kReject, redactor.Redact(std::string(), output));
}

TEST(RedactorTest, RedactWithReplacmentText) {
  auto redactor = CreateRedactor({CreateRule(
      "ab", RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT, "|redacted)")});
  std::string output("ab cab");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("|redacted) c|redacted)", output);
}

TEST(RedactorTest, DontRedactIfMatchTooMuch) {
  auto redactor = CreateRedactor(
      {CreateRule("a*", RedactBehavior::REDACT_ALWAYS, std::string(), 2, 4)});
  const std::string original_output("baaaaaaac");
  std::string output(original_output);
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  // No redact should happen because too much matched.
  EXPECT_EQ(original_output, output);
}

TEST(RedactorTest, DontRedactIfMatchTooLittle) {
  auto redactor = CreateRedactor(
      {CreateRule("a*", RedactBehavior::REDACT_ALWAYS, std::string(), 2, 4)});
  const std::string original_output("bad");
  std::string output(original_output);
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  // No redact should happen because it didn't match enough.
  EXPECT_EQ(original_output, output);
}

TEST(RedactorTest, MatchLimits) {
  auto redactor = CreateRedactor(
      {CreateRule("a*", RedactBehavior::REDACT_ALWAYS, std::nullopt, 2, 4)});
  const std::string original_output("baaad");
  std::string output(original_output);
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("b[###]d", output);
}

TEST(RedactorTest, ReplaceGroup) {
  auto redactor = CreateRedactor({CreateRule(
      "(?:a)(b+)", RedactBehavior::REDACT_ALWAYS, std::nullopt, 2, 4, 1)});
  std::string output("abbbcd");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("a[###]cd", output);
}

TEST(RedactorTest, ReplaceGroup2) {
  auto redactor = CreateRedactor({CreateRule(
      "(a)(b+)", RedactBehavior::REDACT_ALWAYS, std::nullopt, 2, 4, 2)});
  std::string output("abbbcd");
  EXPECT_EQ(RedactResult::kContinue, redactor.Redact(std::string(), output));
  EXPECT_EQ("a[###]cd", output);
}

}  // namespace optimization_guide
