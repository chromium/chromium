// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameter_values_predictor.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_store.h"

namespace record_replay {

TaskParameterValuesPredictor::TaskParameterValuesPredictor(
    TaskStore* task_store)
    : task_store_(task_store) {}

TaskParameterValuesPredictor::~TaskParameterValuesPredictor() = default;

void TaskParameterValuesPredictor::Predict(
    const TaskDefinition& task_definition,
    base::OnceCallback<void(std::optional<std::vector<TaskParameter>>)>
        completion_callback) {
  // TODO(crbug.com/517857125): Implement some logic for passing param values
  // based on historical observations.
  if (!task_store_ || !task_definition.has_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback), std::nullopt));
    return;
  }

  task_store_->GetObservationsForDefinition(
      task_definition.id(),
      base::BindOnce(&TaskParameterValuesPredictor::OnObservationsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_callback)));
}

void TaskParameterValuesPredictor::OnObservationsRetrieved(
    base::OnceCallback<void(std::optional<std::vector<TaskParameter>>)>
        completion_callback,
    std::vector<TaskObservation> observations) {
  if (observations.empty()) {
    std::move(completion_callback).Run(std::nullopt);
    return;
  }

  const TaskObservation& first_observation = observations[0];
  std::vector<TaskParameter> flat_parameters;
  for (const TaskStep& step : first_observation.definition().task_steps()) {
    for (const TaskParameter& param : step.parameters()) {
      flat_parameters.push_back(param);
    }
  }

  std::move(completion_callback).Run(std::move(flat_parameters));
}

}  // namespace record_replay
