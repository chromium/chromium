// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETERS_EXTRACTOR_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETERS_EXTRACTOR_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace record_replay {

// Component responsible for extracting and caching task parameters. Currently
// implemented as a skeleton that populates dummy data.
// TODO(crbug.com/511996748): Implement tracking of web page loads and
// performing real parameter extraction.
class TaskParametersExtractor : public KeyedService {
 public:
  TaskParametersExtractor();
  TaskParametersExtractor(const TaskParametersExtractor&) = delete;
  TaskParametersExtractor& operator=(const TaskParametersExtractor&) = delete;
  ~TaskParametersExtractor() override;

  // Starts parameter extraction for the given TaskDefinition.
  void StartExtraction(TaskDefinition task_definition);

  // Fills extracted parameter values into the given task
  // observation object. Upon completion - calls the
  // completion callback.
  void FillExtractedParametersTo(
      TaskData* task_data,
      base::OnceCallback<void(bool)> completion_callback);

  // Stops the task parameter values extraction.
  void FinishExtraction();

 private:
  // Active task definition under observation, if any.
  std::optional<TaskDefinition> active_task_definition_;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_PARAMETERS_EXTRACTOR_H_
