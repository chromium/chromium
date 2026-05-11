// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace record_replay {

// Manages persistent storage for recording protos, task definitions, and
// sensitive task data.
//
// Tied to the lifecycle of a `Profile`.
class RecordingDataManager : public KeyedService {
 public:
  RecordingDataManager() = default;
  RecordingDataManager(const RecordingDataManager&) = delete;
  RecordingDataManager& operator=(const RecordingDataManager&) = delete;
  RecordingDataManager(RecordingDataManager&&) = delete;
  RecordingDataManager& operator=(RecordingDataManager&&) = delete;
  ~RecordingDataManager() override = default;

  // Adds a recording to the database.
  virtual void AddRecording(Recording recording,
                            base::OnceCallback<void(int64_t)> callback) = 0;

  // Retrieves every Recording that matches the given `url`.
  virtual void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) = 0;

  // Handles both insertion (when task_definition_id is nullopt) and updates.
  virtual void SaveTaskDefinition(std::optional<int64_t> task_definition_id,
                                  TaskDefinition task_definition,
                                  std::string target_url,
                                  std::optional<int64_t> recording_id,
                                  base::OnceClosure callback) = 0;

  // Retrieves the task definition for a given ID, if it exists.
  virtual void GetTaskDefinition(
      int64_t task_definition_id,
      base::OnceCallback<void(std::optional<TaskDefinition>)> callback) = 0;

  // Retrieves all task definitions for a site, returning their IDs and proto
  // data.
  virtual void GetTaskDefinitionsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<std::pair<int64_t, TaskDefinition>>)>
          callback) = 0;

  // Saves or updates task data for a task definition.
  virtual void SaveTaskData(int64_t task_definition_id,
                            TaskData data,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Retrieves task data for a task definition.
  virtual void GetTaskData(
      int64_t task_definition_id,
      base::OnceCallback<void(std::optional<TaskData>)> callback) = 0;

  // Deletes task data for a task definition.
  virtual void DeleteTaskData(int64_t task_definition_id,
                              base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_H_
