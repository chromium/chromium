// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/and_condition.h"

#include <algorithm>

#include "base/memory/ptr_util.h"

namespace data_controls {

// static
std::unique_ptr<Condition> AndCondition::Create(
    std::vector<std::unique_ptr<const Condition>> conditions) {
  if (conditions.empty()) {
    return nullptr;
  }

  return base::WrapUnique(new AndCondition(std::move(conditions)));
}

AndCondition::~AndCondition() = default;

bool AndCondition::CanBeEvaluated(const ActionContext& action_context) const {
  // If any condition in `conditions_` cannot be evaluated, then we know the
  // overall "and" will never be triggerable, so it shouldn't be evaluated in
  // that case.
  return std::all_of(
      conditions_.begin(), conditions_.end(),
      [&action_context](const std::unique_ptr<const Condition>& condition) {
        return condition->CanBeEvaluated(action_context);
      });
}

bool AndCondition::IsTriggered(const ActionContext& action_context) const {
  if (!CanBeEvaluated(action_context)) {
    return false;
  }
  return std::all_of(
      conditions_.begin(), conditions_.end(),
      [&action_context](const std::unique_ptr<const Condition>& condition) {
        return condition->IsTriggered(action_context);
      });
}

AndCondition::AndCondition(
    std::vector<std::unique_ptr<const Condition>> conditions)
    : conditions_(std::move(conditions)) {}

}  // namespace data_controls
