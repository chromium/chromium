// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETERS_EXTRACTOR_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETERS_EXTRACTOR_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/common/aliases.h"
#include "url/gurl.h"

namespace record_replay {

// Component responsible for caching task parameters. Implemented as a
// KeyedService which stores extracted parameter values and populates them into
// TaskObservations.
class TaskParametersExtractor : public KeyedService {
 public:
  TaskParametersExtractor();
  TaskParametersExtractor(const TaskParametersExtractor&) = delete;
  TaskParametersExtractor& operator=(const TaskParametersExtractor&) = delete;
  ~TaskParametersExtractor() override;

  base::WeakPtr<TaskParametersExtractor> GetWeakPtr();

  // Starts parameter extraction for the given TaskDefinition.
  void StartExtraction(TaskDefinition task_definition);

  // Returns a map of expected parameter keys and their corresponding CSS
  // selectors if the active task is configured for the given URL.
  std::map<std::string, std::string> GetParameterValueSelectorsForUrl(
      const GURL& url);

  // Stores a successfully extracted value for a parameter key.
  void StoreExtractedValue(const std::string& key, const std::string& value);

  // Fills currently cached parameter values into the given task observation
  // object and immediately runs the callback.
  //
  // NOTE: There is a potential race condition. If this is called before
  // asynchronous Mojo extractions have completed, the observation will only be
  // populated with currently cached values (or remain empty if no extractions
  // resolved yet).
  void FillExtractedParametersTo(
      TaskObservation* observation,
      base::OnceCallback<void(bool)> completion_callback);

  // Stops the task parameter values extraction.
  void FinishExtraction();

 private:
  // Returns a pair of (parameter_key, css_selector) for the given
  // TaskParameter if valid.
  std::optional<std::pair<std::string, std::string>>
  GetParameterKeyAndCssSelector(const TaskParameter& task_parameter) const;

  // Active task definition under observation, if any.
  std::optional<TaskDefinition> active_task_definition_;

  // Cached parameter values extracted from the DOM: key -> value.
  std::map<std::string, std::string> extracted_values_;

  base::WeakPtrFactory<TaskParametersExtractor> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETERS_EXTRACTOR_H_
