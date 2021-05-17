// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"

#include "components/autofill_assistant/browser/selector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

class RequiredFieldTest : public testing::Test {
 public:
  void SetUp() override {}
};

TEST_F(RequiredFieldTest, HasValue) {
  RequiredField required_field;
  EXPECT_FALSE(required_field.HasValue());

  required_field.proto.mutable_value_expression()->add_chunk()->set_text(
      "value");
  EXPECT_TRUE(required_field.HasValue());

  required_field.proto.mutable_value_expression()->clear_chunk();
  ValueExpressionRegexp value_expression_re2;
  value_expression_re2.mutable_value_expression()->add_chunk()->set_text("^$");
  *required_field.proto.mutable_option_comparison_value_expression_re2() =
      value_expression_re2;
  EXPECT_TRUE(required_field.HasValue());
}

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmpty) {
  RequiredField required_field;
  required_field.status = RequiredField::NOT_EMPTY;
  required_field.proto.mutable_value_expression()->add_chunk()->set_text(
      "value");

  EXPECT_FALSE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmptyToBeCleared) {
  RequiredField required_field;
  required_field.status = RequiredField::NOT_EMPTY;

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_TRUE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForEmpty) {
  RequiredField required_field;
  required_field.status = RequiredField::EMPTY;
  required_field.proto.mutable_value_expression()->add_chunk()->set_text(
      "value");

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_TRUE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmptyForced) {
  RequiredField required_field;
  required_field.proto.set_forced(true);
  required_field.status = RequiredField::NOT_EMPTY;
  required_field.proto.mutable_value_expression()->add_chunk()->set_text(
      "value");

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForEmptyWithClick) {
  RequiredField required_field;
  required_field.status = RequiredField::EMPTY;
  *required_field.proto.mutable_option_element_to_click() =
      ToSelectorProto("#element");

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForEmptyOptional) {
  RequiredField required_field;
  required_field.proto.set_is_optional(true);
  required_field.status = RequiredField::EMPTY;
  required_field.proto.mutable_value_expression()->add_chunk()->set_text(
      "value");

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForEmptyWithOptionComparison) {
  RequiredField required_field;
  required_field.status = RequiredField::EMPTY;
  ValueExpressionRegexp value_expression_re2;
  value_expression_re2.mutable_value_expression()->add_chunk()->set_text("^$");
  *required_field.proto.mutable_option_comparison_value_expression_re2() =
      value_expression_re2;

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_TRUE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmptyWithOptionComparison) {
  RequiredField required_field;
  required_field.status = RequiredField::NOT_EMPTY;
  ValueExpressionRegexp value_expression_re2;
  value_expression_re2.mutable_value_expression()->add_chunk()->set_text("^$");
  *required_field.proto.mutable_option_comparison_value_expression_re2() =
      value_expression_re2;

  EXPECT_FALSE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

}  // namespace
}  // namespace autofill_assistant
