// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameters_extractor.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/record_replay/core/browser/task_definition.pb.h"

namespace record_replay {

TaskParametersExtractor::TaskParametersExtractor() = default;
TaskParametersExtractor::~TaskParametersExtractor() = default;

void TaskParametersExtractor::StartExtraction(TaskDefinition task_definition) {
  active_task_definition_ = std::move(task_definition);
}

std::string TaskParametersExtractor::GetSelectorForKey(
    const std::string& expected_key) const {
  // TODO(crbug.com/511996748): Implement dynamic selector lookup from the
  // TaskDefinition extraction strategy instead of hard-coded dummy mapping.
  if (expected_key == "key1") {
    return "#ui-id-1";
  }
  if (expected_key == "key2") {
    return "#ui-id-2";
  }
  return "";
}

std::map<std::string, std::string>
TaskParametersExtractor::GetParameterValueSelectorsForUrl(const GURL& url) {
  std::map<std::string, std::string> parameter_value_selectors;
  // TODO(crbug.com/511996748): Refine the logic of URL comparison. We also need
  // to check URLs of different steps.
  if (!active_task_definition_.has_value() ||
      GURL(active_task_definition_->url()) != url) {
    return parameter_value_selectors;
  }

  // TODO(crbug.com/511996748): Instead of iterating over all steps - collect
  // selectors only for the step that corresponds to the current URL.
  for (const TaskStep& step : active_task_definition_->task_steps()) {
    for (const TaskParameter& task_parameter : step.parameters()) {
      std::string css_selector = GetSelectorForKey(task_parameter.key());
      if (!css_selector.empty()) {
        parameter_value_selectors[task_parameter.key()] = css_selector;
      }
    }
  }

  return parameter_value_selectors;
}

void TaskParametersExtractor::StoreExtractedValue(const std::string& key,
                                                  const std::string& value) {
  if (!value.empty()) {
    // Note: Storing a value will overwrite any already present value for the
    // key. This is expected if extraction runs multiple times or updates.
    extracted_values_[key] = value;
  }
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
      auto it = extracted_values_.find(param->key());
      std::string value =
          (it != extracted_values_.end()) ? it->second : "dummy_value";
      param->set_value(value);
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback), true));
}

base::WeakPtr<TaskParametersExtractor> TaskParametersExtractor::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TaskParametersExtractor::FinishExtraction() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_task_definition_.reset();
  extracted_values_.clear();
}

}  // namespace record_replay
