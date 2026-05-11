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
    TaskData* task_data,
    base::OnceCallback<void(bool)> completion_callback) {
  if (!active_task_definition_.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback), false));
    return;
  }

  // Iterate over all steps and populate all expected keys with dummy values.
  for (const auto& [step_index, step_anno] : active_task_definition_->steps()) {
    auto& step_values =
        *(*task_data->mutable_step_data())[step_index].mutable_values();
    for (const std::string& expected_key : step_anno.expected_data_keys()) {
      step_values.insert({expected_key, "dummy_value"});
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback), true));
}

void TaskParametersExtractor::FinishExtraction() {
  active_task_definition_.reset();
}

}  // namespace record_replay
