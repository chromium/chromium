// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_observer.h"
#include "components/record_replay/core/browser/task_store.h"
#include "url/gurl.h"

namespace record_replay {

TaskService::TaskService(TaskStore* task_store,
                         TaskParametersExtractor* task_parameters_extractor)
    : task_store_(task_store),
      task_parameters_extractor_(task_parameters_extractor) {}

TaskService::~TaskService() = default;

void TaskService::OnURLVisited(const GURL& visited_url) {
  if (!task_store_) {
    return;
  }

  // If there is already an observer, we don't need to get task definitions and
  // create a new one as we only support one task at a time.
  if (observer_) {
    observer_->OnURLVisited(visited_url);
    return;
  }

  task_store_->GetTaskDefinitionsByUrl(
      visited_url.spec(),
      base::BindOnce(&TaskService::OnTaskDefinitionsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), visited_url));
}

void TaskService::OnTaskDefinitionsRetrieved(
    const GURL& visited_url,
    std::vector<TaskDefinition> task_definitions) {
  for (const TaskDefinition& definition : task_definitions) {
    if (definition.url() == visited_url.spec()) {
      // TODO(crbug.com/517099841): Split the observation logic from the
      // execution logic.
      observer_ = std::make_unique<TaskObserver>(
          definition,
          base::BindRepeating(&TaskService::OnTaskCompleted,
                              weak_ptr_factory_.GetWeakPtr()),
          task_parameters_extractor_);
      observer_->StartObserving(visited_url);
      observer_->OnURLVisited(visited_url);
    }
  }
}

void TaskService::OnTaskCompleted(const TaskObservation& observation) {
  // TODO(crbug.com/): Persist the observation instead of the definition.
  task_store_->SaveTaskDefinition(
      /*task_definition_id=*/std::nullopt, observation.definition(),
      base::DoNothing());
  observer_.reset();
}

void TaskService::OnExecutionAccepted(const TaskDefinition& definition,
                                      const TaskParameterValues& values) {
  // TODO(crbug.com/514303674): Handle user accepting task execution callback
  // from UI.
}

}  // namespace record_replay
