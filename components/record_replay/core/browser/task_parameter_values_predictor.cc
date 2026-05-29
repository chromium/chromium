// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameter_values_predictor.h"

#include "base/task/sequenced_task_runner.h"
#include "components/record_replay/core/browser/task_definition.pb.h"

namespace record_replay {

TaskParameterValuesPredictor::TaskParameterValuesPredictor() = default;

TaskParameterValuesPredictor::~TaskParameterValuesPredictor() = default;

void TaskParameterValuesPredictor::Predict(
    const TaskDefinition& task_definition,
    base::OnceCallback<void(std::optional<std::vector<TaskParameter>>)>
        completion_callback) {
  // TODO(crbug.com/517857125): Implement some logic for passing param values
  // based on historical observations.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback), std::nullopt));
}

}  // namespace record_replay
