// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/not_condition.h"
#include "base/memory/ptr_util.h"

namespace data_controls {

// static
std::unique_ptr<const Condition> NotCondition::Create(
    std::unique_ptr<const Condition> condition) {
  if (!condition) {
    return nullptr;
  }

  return base::WrapUnique(new NotCondition(std::move(condition)));
}

NotCondition::~NotCondition() = default;

bool NotCondition::CanBeEvaluated(const ActionContext& action_context) const {
  // If the condition wrapped in a "not" can't be evaluated, then the "not"
  // itself shouldn't be evaluated.
  return condition_->CanBeEvaluated(action_context);
}

bool NotCondition::IsTriggered(const ActionContext& action_context) const {
  if (!CanBeEvaluated(action_context)) {
    return false;
  }
  return !condition_->IsTriggered(action_context);
}

NotCondition::NotCondition(std::unique_ptr<const Condition> condition)
    : condition_(std::move(condition)) {
  DCHECK(condition_);
}

}  // namespace data_controls
