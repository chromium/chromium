// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETER_VALUES_PREDICTOR_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETER_VALUES_PREDICTOR_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace record_replay {

class TaskDefinition;
class TaskParameter;
class TaskStore;
class TaskObservation;

// Predicts parameter values for a provided task definition.
class TaskParameterValuesPredictor {
 public:
  explicit TaskParameterValuesPredictor(TaskStore* task_store);
  TaskParameterValuesPredictor(const TaskParameterValuesPredictor&) = delete;
  TaskParameterValuesPredictor& operator=(const TaskParameterValuesPredictor&) =
      delete;
  ~TaskParameterValuesPredictor();

  // Predicts the values for a given `task_definition`.
  void Predict(
      const TaskDefinition& task_definition,
      base::OnceCallback<void(std::optional<std::vector<TaskParameter>>)>
          completion_callback);

 private:
  void OnObservationsRetrieved(
      base::OnceCallback<void(std::optional<std::vector<TaskParameter>>)>
          completion_callback,
      std::vector<TaskObservation> observations);

  raw_ptr<TaskStore> task_store_;
  base::WeakPtrFactory<TaskParameterValuesPredictor> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETER_VALUES_PREDICTOR_H_
