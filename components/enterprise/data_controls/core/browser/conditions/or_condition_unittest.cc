// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/or_condition.h"

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

TEST(DataControlsOrConditionTest, InvalidValues) {
  ASSERT_FALSE(OrCondition::Create({}));
}

TEST(DataControlsOrConditionTest, BasicTests) {
  const std::tuple<std::vector<bool>, bool> kTestValues[] = {
      // The following cases evaluate to false because all sub-conditions
      // evaluate to false.
      {{false}, false},
      {{false, false}, false},
      {{false, false, false}, false},

      // The following cases evaluate to true because at least one
      // sub-condition evaluates to true.
      {{true}, true},
      {{false, true}, true},
      {{true, false}, true},
      {{true, true}, true},
      {{false, false, true}, true},
      {{false, true, false}, true},
      {{false, true, true}, true},
      {{true, false, false}, true},
      {{true, false, true}, true},
      {{true, true, false}, true},
      {{true, true, true}, true},
  };
  for (auto [or_conditions, expected] : kTestValues) {
    std::vector<std::unique_ptr<const Condition>> conditions;
    for (bool value : or_conditions) {
      if (value) {
        conditions.push_back(std::make_unique<TrueCondition>());
      } else {
        conditions.push_back(std::make_unique<FalseCondition>());
      }
    }
    auto or_condition = OrCondition::Create(std::move(conditions));
    ASSERT_TRUE(or_condition);
    ASSERT_EQ(or_condition->IsTriggered({}), expected);
  }
}

}  // namespace data_controls
