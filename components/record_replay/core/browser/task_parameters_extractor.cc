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

std::optional<std::pair<std::string, std::string>>
TaskParametersExtractor::GetParameterKeyAndCssSelector(
    const TaskParameter& task_parameter) const {
  if (task_parameter.key().empty() ||
      !task_parameter.has_extraction_strategy() ||
      !task_parameter.extraction_strategy().has_dom_css_selector() ||
      task_parameter.extraction_strategy().dom_css_selector().empty()) {
    return std::nullopt;
  }
  return std::make_pair(
      task_parameter.key(),
      task_parameter.extraction_strategy().dom_css_selector());
}

std::map<std::string, std::string>
TaskParametersExtractor::GetParameterValueSelectorsForUrl(const GURL& url) {
  std::map<std::string, std::string> parameter_value_selectors;
  if (!active_task_definition_.has_value()) {
    return parameter_value_selectors;
  }

  for (const TaskStep& step : active_task_definition_->task_steps()) {
    if (!step.url().empty() && GURL(step.url()) == url) {
      for (const TaskParameter& task_parameter : step.parameters()) {
        if (auto key_and_selector =
                GetParameterKeyAndCssSelector(task_parameter)) {
          auto [key, selector] = *key_and_selector;
          parameter_value_selectors[key] = selector;
        }
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
