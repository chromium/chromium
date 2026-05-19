// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameters_extractor.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace record_replay {

TaskParametersExtractor::TaskParametersExtractor() = default;
TaskParametersExtractor::~TaskParametersExtractor() = default;

void TaskParametersExtractor::StartExtraction(TaskDefinition task_definition) {
  active_task_definition_ = std::move(task_definition);
}

void TaskParametersExtractor::FillExtractedParametersTo(
    TaskObservation* observation,
    base::OnceCallback<void(bool)> completion_callback) {
  if (!active_task_definition_.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback), false));
    return;
  }

  // Copy the active task definition into the observation.
  *observation->mutable_definition() = *active_task_definition_;

  // Populate all step-specific parameters inside the observation definition
  // with dummy values.
  for (int i = 0; i < observation->mutable_definition()->task_steps_size();
       ++i) {
    TaskStep* step = observation->mutable_definition()->mutable_task_steps(i);
    for (int j = 0; j < step->parameters_size(); ++j) {
      TaskParameter* param = step->mutable_parameters(j);
      param->set_value("dummy_value");
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback), true));
}

void TaskParametersExtractor::FinishExtraction() {
  active_task_definition_.reset();
}

}  // namespace record_replay
