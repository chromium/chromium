// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/task/task_manager.h"

namespace download {

TaskManager::TaskParams::TaskParams()
    : require_unmetered_network(false),
      require_charging(false),
      optimal_battery_percentage(0),
      window_start_time_seconds(0),
      window_end_time_seconds(0) {}

bool TaskManager::TaskParams::operator==(
    const TaskManager::TaskParams& other) const {
  return require_unmetered_network == other.require_unmetered_network &&
         require_charging == other.require_charging &&
         optimal_battery_percentage == other.optimal_battery_percentage &&
         window_start_time_seconds == other.window_start_time_seconds &&
         window_end_time_seconds == other.window_end_time_seconds;
}

}  // namespace download
