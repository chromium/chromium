// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_INFO_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_INFO_H_

#include <stdint.h>
#include <string>

#include "components/background_task_scheduler/task_ids.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace background_task {

// Specifies information regarding periodic tasks.
struct PeriodicInfo {
  PeriodicInfo();
  ~PeriodicInfo();

  int64_t interval_ms;
  int64_t flex_ms;
  bool expires_after_window_end_time;
};

// Specifies information regarding one-off tasks.
struct OneOffInfo {
  OneOffInfo();
  ~OneOffInfo();

  int64_t window_start_time_ms;
  int64_t window_end_time_ms;
  bool expires_after_window_end_time;
};

// Specifies information regarding exact tasks.
struct ExactInfo {
  ExactInfo();
  ~ExactInfo();

  int64_t trigger_at_ms;
};

// TaskInfo represents a request to run a specific BackgroundTask given
// the required parameters, such as whether a special type of network is
// available.
struct TaskInfo {
  TaskInfo(int task_id, const PeriodicInfo& timing_info);
  TaskInfo(int task_id, const OneOffInfo& timing_info);
  // TODO(crbug.com/1190755): Either remove this or make sure it's compatible
  // with Android S.
  // Warning: This functionality might get removed, check with OWNERS before
  // using this in new code: //components/background_task_scheduler/OWNERS.
  TaskInfo(int task_id, const ExactInfo& timing_info);

  TaskInfo(const TaskInfo&) = delete;
  TaskInfo& operator=(const TaskInfo&) = delete;

  ~TaskInfo();

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.components.background_task_scheduler)
  enum NetworkType {
    // This task has no requirements for network connectivity. Default.
    NONE = 0,
    // This task requires network connectivity.
    ANY = 1,
    // This task requires network connectivity that is unmetered.
    UNMETERED = 2,
  };

  int task_id;
  NetworkType network_type;
  bool requires_charging;
  bool is_persisted;
  bool update_current;
  std::string extras;

  absl::optional<PeriodicInfo> periodic_info;
  absl::optional<OneOffInfo> one_off_info;
  absl::optional<ExactInfo> exact_info;
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_INFO_H_
