// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/not_condition.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace data_controls {

namespace {

class TrueCondition : public Condition {
 public:
  bool CanBeEvaluated(const ActionContext& action_context) const override {
    return true;
  }
  bool IsTriggered(const ActionContext& action_context) const override {
    return true;
  }
  TrueCondition() = default;
  ~TrueCondition() override = default;
};

class FalseCondition : public Condition {
 public:
  bool CanBeEvaluated(const ActionContext& action_context) const override {
    return true;
  }
  bool IsTriggered(const ActionContext& action_context) const override {
    return false;
  }
  FalseCondition() = default;
  ~FalseCondition() override = default;
};

// Helpers to make the test logic more readable.
std::unique_ptr<const Condition> True() {
  return std::make_unique<TrueCondition>();
}
std::unique_ptr<const Condition> False() {
  return std::make_unique<FalseCondition>();
}
std::unique_ptr<const Condition> Not(std::unique_ptr<const Condition> cond) {
  return NotCondition::Create(std::move(cond));
}

}  // namespace

TEST(DataControlsNotConditionTest, InvalidValues) {
  ASSERT_FALSE(Not(nullptr));
  ASSERT_FALSE(Not(Not(nullptr)));
}

TEST(DataControlsNotConditionTest, BasicTests) {
  EXPECT_FALSE(Not(True())->IsTriggered({}));
  EXPECT_TRUE(Not(False())->IsTriggered({}));

  // Nested "not"s should cancel out.
  EXPECT_TRUE(Not(Not(True()))->IsTriggered({}));
  EXPECT_FALSE(Not(Not(False()))->IsTriggered({}));
}

}  // namespace data_controls
