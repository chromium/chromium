// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/or_condition.h"

#include <algorithm>

#include "base/memory/ptr_util.h"

namespace data_controls {

// static
std::unique_ptr<Condition> OrCondition::Create(
    std::vector<std::unique_ptr<const Condition>> conditions) {
  if (conditions.empty()) {
    return nullptr;
  }

  return base::WrapUnique(new OrCondition(std::move(conditions)));
}

OrCondition::~OrCondition() = default;

bool OrCondition::IsTriggered(const ActionContext& action_context) const {
  return std::any_of(
      conditions_.begin(), conditions_.end(),
      [&action_context](const std::unique_ptr<const Condition>& condition) {
        return condition->IsTriggered(action_context);
      });
}

OrCondition::OrCondition(
    std::vector<std::unique_ptr<const Condition>> conditions)
    : conditions_(std::move(conditions)) {}

}  // namespace data_controls
