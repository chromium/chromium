// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_observer.h"
#include "url/gurl.h"

namespace record_replay {

TaskService::TaskService(RecordingDataManager* recording_data_manager)
    : recording_data_manager_(recording_data_manager) {}

TaskService::~TaskService() = default;

void TaskService::OnURLVisited(const GURL& visited_url) {
  if (!recording_data_manager_) {
    return;
  }

  // If there is already an observer, we don't need to get task definitions and
  // create a new one as we only support one task at a time.
  if (observer_) {
    observer_->OnURLVisited(visited_url);
    return;
  }

  recording_data_manager_->GetTaskDefinitionsByUrl(
      visited_url.spec(),
      base::BindOnce(&TaskService::OnTaskDefinitionsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), visited_url));
}

void TaskService::OnTaskDefinitionsRetrieved(
    const GURL& visited_url,
    std::vector<std::pair<int64_t, TaskDefinition>> task_definitions) {
  for (const auto& pair : task_definitions) {
    const auto& definition = pair.second;
    if (definition.url() == visited_url.spec()) {
      observer_ = std::make_unique<TaskObserver>(
          definition, base::BindRepeating(&TaskService::OnTaskCompleted,
                                          weak_ptr_factory_.GetWeakPtr()));
      observer_->StartObserving(visited_url);
      observer_->OnURLVisited(visited_url);
    }
  }
}

void TaskService::OnTaskCompleted(const TaskObservation& observation) {
  recording_data_manager_->SaveTaskDefinition(
      /*task_definition_id=*/std::nullopt, observation.definition(),
      observation.definition().url(),
      /*recording_id=*/std::nullopt,
      // TODO(crbug.com/515729820): Implement a callback that uses the newly
      // stored observation.
      base::DoNothing());
  observer_.reset();
}

void TaskService::OnExecutionAccepted(const TaskDefinition& definition,
                                      const TaskParameterValues& values) {
  // TODO(crbug.com/514303674): Handle user accepting task execution callback
  // from UI.
}

}  // namespace record_replay
