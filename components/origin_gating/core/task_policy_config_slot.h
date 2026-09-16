// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_TASK_POLICY_CONFIG_SLOT_H_
#define COMPONENTS_ORIGIN_GATING_CORE_TASK_POLICY_CONFIG_SLOT_H_

#include <optional>

#include "base/types/optional_ref.h"
#include "components/origin_gating/core/task_policy_config.h"

namespace origin_gating {

// A slot that optionally holds a TaskPolicyConfig.
class TaskPolicyConfigSlot {
 public:
  TaskPolicyConfigSlot();
  TaskPolicyConfigSlot(const TaskPolicyConfigSlot&) = delete;
  TaskPolicyConfigSlot& operator=(const TaskPolicyConfigSlot&) = delete;
  TaskPolicyConfigSlot(TaskPolicyConfigSlot&&) = delete;
  TaskPolicyConfigSlot& operator=(TaskPolicyConfigSlot&&) = delete;
  ~TaskPolicyConfigSlot();

  // Assigns the `config` to this instance. This method is a no-op except for
  // the first time it is called.
  void Assign(TaskPolicyConfig config);

  bool has_value() const { return config_.has_value(); }

  const TaskPolicyConfig& value() const { return config_.value(); }

 private:
  std::optional<TaskPolicyConfig> config_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_TASK_POLICY_CONFIG_SLOT_H_
