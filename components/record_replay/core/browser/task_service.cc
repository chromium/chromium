// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_service.h"

#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "url/gurl.h"

namespace record_replay {

TaskService::TaskService(RecordingDataManager* recording_data_manager)
    : recording_data_manager_(recording_data_manager) {}

TaskService::~TaskService() = default;

void TaskService::OnURLVisited(const GURL& visited_url) {
  // TODO(crbug.com/514303197): Check whether or not to start observing a task,
  // and create a TaskObserver if necessary.
}

void TaskService::OnTaskCompleted(const TaskDefinition& definition) {
  // TODO(crbug.com/514303497): Handle landing on a task-end URL.
}

void TaskService::OnExecutionAccepted(const TaskDefinition& definition,
                                      const TaskParameterValues& values) {
  // TODO(crbug.com/514303674): Handle user accepting task execution callback
  // from UI.
}

}  // namespace record_replay
