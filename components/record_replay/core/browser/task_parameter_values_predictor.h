// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETER_VALUES_PREDICTOR_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETER_VALUES_PREDICTOR_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"

namespace record_replay {

class TaskDefinition;
class TaskParameter;

// Predicts parameter values for a provided task definition.
class TaskParameterValuesPredictor {
 public:
  TaskParameterValuesPredictor();
  TaskParameterValuesPredictor(const TaskParameterValuesPredictor&) = delete;
  TaskParameterValuesPredictor& operator=(const TaskParameterValuesPredictor&) =
      delete;
  ~TaskParameterValuesPredictor();

  // Predicts the values for a given `task_definition`.
  void Predict(
      const TaskDefinition& task_definition,
      base::OnceCallback<void(std::optional<std::vector<TaskParameter>>)>
          completion_callback);
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETER_VALUES_PREDICTOR_H_
