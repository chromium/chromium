// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_COMMON_PREREAD_EXPERIMENT_GROUP_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_COMMON_PREREAD_EXPERIMENT_GROUP_H_

#include <stdint.h>

namespace startup_metric_utils {

enum class PrereadExperimentGroup : int32_t {
  // Experiment group not assigned, or not a client where prereading is
  // considered.
  kUnknown = 0,
  // Prereading of the main DLL is done in parallel.
  kParallel,
  // Control group: Prereading is done synchronously.
  kSynchronous,
};

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_COMMON_PREREAD_EXPERIMENT_GROUP_H_
