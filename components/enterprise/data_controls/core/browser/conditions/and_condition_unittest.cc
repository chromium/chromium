// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/and_condition.h"

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

}  // namespace

TEST(DataControlsAndConditionTest, InvalidValues) {
  ASSERT_FALSE(AndCondition::Create({}));
}

TEST(DataControlsAndConditionTest, BasicTests) {
  const std::tuple<std::vector<bool>, bool> kTestValues[] = {
      // The following cases evaluate to true because all sub-conditions
      // evaluate to true.
      {{true}, true},
      {{true, true}, true},
      {{true, true, true}, true},

      // The following cases evaluate to false because at least one
      // sub-condition evaluates to false.
      {{false}, false},
      {{false, false}, false},
      {{false, true}, false},
      {{true, false}, false},
      {{false, false, false}, false},
      {{false, false, true}, false},
      {{false, true, false}, false},
      {{false, true, true}, false},
      {{true, false, false}, false},
      {{true, false, true}, false},
      {{true, true, false}, false},
  };
  for (auto [and_conditions, expected] : kTestValues) {
    std::vector<std::unique_ptr<const Condition>> conditions;
    for (bool value : and_conditions) {
      if (value) {
        conditions.push_back(std::make_unique<TrueCondition>());
      } else {
        conditions.push_back(std::make_unique<FalseCondition>());
      }
    }
    auto and_condition = AndCondition::Create(std::move(conditions));
    ASSERT_TRUE(and_condition);
    ASSERT_EQ(and_condition->IsTriggered({}), expected);
  }
}

}  // namespace data_controls
