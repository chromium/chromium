// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/redactor.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using proto::RedactBehavior;

Rule CreateRule(
    const std::string& regex,
    RedactBehavior behavior = RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT,
    std::optional<std::string> replacement_string = std::nullopt) {
  Rule rule;
  rule.regex = regex;
  rule.behavior = behavior;
  rule.replacement_string = std::move(replacement_string);
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

}  // namespace optimization_guide
