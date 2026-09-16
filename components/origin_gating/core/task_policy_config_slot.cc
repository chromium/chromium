// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/task_policy_config_slot.h"

#include <utility>

#include "base/types/optional_ref.h"
#include "components/origin_gating/core/task_policy_config.h"

namespace origin_gating {

TaskPolicyConfigSlot::TaskPolicyConfigSlot() = default;
TaskPolicyConfigSlot::~TaskPolicyConfigSlot() = default;

void TaskPolicyConfigSlot::Assign(TaskPolicyConfig config) {
  if (config_.has_value()) {
    return;
  }
  config_.emplace(std::move(config));
}

}  // namespace origin_gating
