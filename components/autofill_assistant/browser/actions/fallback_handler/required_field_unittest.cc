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

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmpty) {
  RequiredField required_field;
  required_field.status = RequiredField::NOT_EMPTY;
  required_field.value_expression = "value";

  EXPECT_FALSE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmptyToBeCleared) {
  RequiredField required_field;
  required_field.status = RequiredField::NOT_EMPTY;
  required_field.value_expression = std::string();

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_TRUE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForEmpty) {
  RequiredField required_field;
  required_field.status = RequiredField::EMPTY;
  required_field.value_expression = "value";

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_TRUE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForNotEmptyForced) {
  RequiredField required_field;
  required_field.forced = true;
  required_field.status = RequiredField::NOT_EMPTY;
  required_field.value_expression = "value";

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

TEST_F(RequiredFieldTest, ShouldFallbackForEmptyWithClick) {
  RequiredField required_field;
  required_field.status = RequiredField::EMPTY;
  required_field.fallback_click_element = Selector({"#element"});

  EXPECT_TRUE(required_field.ShouldFallback(true));
  EXPECT_FALSE(required_field.ShouldFallback(false));
}

}  // namespace
}  // namespace autofill_assistant
