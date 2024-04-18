// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_PRIORITY_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_PRIORITY_H_

#include <cstddef>
#include <string_view>

#include "base/component_export.h"

namespace unexportable_keys {

// Ordered list of priorities supported by the unexportable key task manager.
// The priorities are ordered from the lowest one to the highest one.
enum class BackgroundTaskPriority {
  // For non-urgent work, that will only execute if there's nothing else to do.
  kBestEffort = 0,

  // The result of these tasks are visible to the user (in the UI or as a
  // side-effect on the system) but they are not an immediate response to a user
  // interaction.
  kUserVisible = 1,

  // Tasks that affect the UI immediately after a user interaction.
  kUserBlocking = 2,

  kMaxValue = kUserBlocking
};

constexpr size_t kNumTaskPriorities =
    static_cast<size_t>(BackgroundTaskPriority::kMaxValue) + 1;

// Converts `BackgroundTaskPriority` to a histogram suffix string. The string is
// prepended with "." symbol so it can be directly concatenated with a base
// histogram name.
COMPONENT_EXPORT(UNEXPORTABLE_KEYS)
std::string_view GetBackgroundTaskPrioritySuffixForHistograms(
    BackgroundTaskPriority priority);

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_PRIORITY_H_
