// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_STORE_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_STORE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/task_definition.pb.h"

namespace record_replay {

// Manages persistent storage for recording protos, task definitions, and
// task observations.
//
// Tied to the lifecycle of a `Profile`.
class TaskStore : public KeyedService {
 public:
  TaskStore() = default;
  TaskStore(const TaskStore&) = delete;
  TaskStore& operator=(const TaskStore&) = delete;
  TaskStore(TaskStore&&) = delete;
  TaskStore& operator=(TaskStore&&) = delete;
  ~TaskStore() override = default;

  // Adds a recording to the database.
  virtual void AddRecording(Recording recording,
                            base::OnceCallback<void(int64_t)> callback) = 0;

  // Retrieves every Recording that matches the given `url`.
  virtual void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) = 0;

  // Handles both insertion (when task_definition_id is nullopt) and updates.
  virtual void SaveTaskDefinition(
      std::optional<int64_t> task_definition_id,
      TaskDefinition task_definition,
      base::OnceCallback<void(int64_t)> callback) = 0;

  // Retrieves the task definition for a given ID, if it exists.
  virtual void GetTaskDefinition(
      int64_t task_definition_id,
      base::OnceCallback<void(std::optional<TaskDefinition>)> callback) = 0;

  // Retrieves all task definitions for a site, returning their proto data.
  virtual void GetTaskDefinitionsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<TaskDefinition>)> callback) = 0;

  virtual void DeleteTaskDefinition(int64_t task_definition_id,
                                    base::OnceCallback<void(bool)> callback) {}

  // Full CRUD for Observations (Create/Update are unified under Save)
  virtual void SaveObservation(TaskObservation observation,
                               base::OnceCallback<void(int64_t)> callback) {}

  virtual void GetObservationsForDefinition(
      int64_t task_definition_id,
      base::OnceCallback<void(std::vector<TaskObservation>)> callback) {}

  virtual void DeleteObservation(int64_t observation_id,
                                 base::OnceCallback<void(bool)> callback) {}
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_STORE_H_
