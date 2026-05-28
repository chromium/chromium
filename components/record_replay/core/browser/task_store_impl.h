// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_STORE_IMPL_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_STORE_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/task_database.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_store.h"

namespace record_replay {

// Concrete implementation for `TaskStore` using a SQLite database
// to save and load `Recording` protos and `TaskDefinition`s.
//
// Owned by `TaskStoreFactory` as a `KeyedService`, and thus tied to
// the lifecycle of a `Profile`.
// It runs on the UI thread but uses `TaskDatabase` on a background
// sequence for database I/O.
class TaskStoreImpl : public TaskStore {
 public:
  explicit TaskStoreImpl(base::FilePath profile_path);
  TaskStoreImpl(const TaskStoreImpl&) = delete;
  TaskStoreImpl& operator=(const TaskStoreImpl&) = delete;
  ~TaskStoreImpl() override;

  // TaskStore:
  void AddRecording(Recording recording,
                    base::OnceCallback<void(int64_t)> callback) override;
  void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) override;
  void SaveTaskDefinition(std::optional<int64_t> task_definition_id,
                          TaskDefinition task_definition,
                          base::OnceCallback<void(int64_t)> callback) override;
  void GetTaskDefinition(int64_t task_definition_id,
                         base::OnceCallback<void(std::optional<TaskDefinition>)>
                             callback) override;
  void GetTaskDefinitionsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<TaskDefinition>)> callback) override;
  void DeleteTaskDefinition(int64_t definition_id,
                            base::OnceCallback<void(bool)> callback) override;

  // Full CRUD for Observations
  void SaveObservation(TaskObservation observation,
                       base::OnceCallback<void(int64_t)> callback) override;
  void GetObservationsForDefinition(
      int64_t task_definition_id,
      base::OnceCallback<void(std::vector<TaskObservation>)> callback) override;
  void DeleteObservation(int64_t observation_id,
                         base::OnceCallback<void(bool)> callback) override;

 private:
  base::SequenceBound<TaskDatabase> db_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_STORE_IMPL_H_
