// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_IMPL_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "components/record_replay/core/browser/task_database.h"

namespace record_replay {

// Concrete implementation for `RecordingDataManager` using a SQLite database
// to save and load `Recording` protos and `TaskDefinition`s.
//
// Owned by `RecordingDataManagerFactory` as a `KeyedService`, and thus tied to
// the lifecycle of a `Profile`.
// It runs on the UI thread but uses `TaskDatabase` on a background
// sequence for database I/O.
class RecordingDataManagerImpl : public RecordingDataManager {
 public:
  explicit RecordingDataManagerImpl(base::FilePath profile_path);
  RecordingDataManagerImpl(const RecordingDataManagerImpl&) = delete;
  RecordingDataManagerImpl& operator=(const RecordingDataManagerImpl&) = delete;
  ~RecordingDataManagerImpl() override;

  // RecordingDataManager:
  void AddRecording(Recording recording,
                    base::OnceCallback<void(int64_t)> callback) override;
  void GetRecordingsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<Recording>)> callback) override;
  void SaveTaskDefinition(std::optional<int64_t> task_definition_id,
                          TaskDefinition task_definition,
                          std::string target_url,
                          std::optional<int64_t> recording_id,
                          base::OnceClosure callback) override;
  void GetTaskDefinition(int64_t task_definition_id,
                         base::OnceCallback<void(std::optional<TaskDefinition>)>
                             callback) override;
  void GetTaskDefinitionsByUrl(
      std::string url,
      base::OnceCallback<void(std::vector<std::pair<int64_t, TaskDefinition>>)>
          callback) override;
  void SaveTaskData(int64_t task_definition_id,
                    TaskData data,
                    base::OnceCallback<void(bool)> callback) override;
  void GetTaskData(
      int64_t task_definition_id,
      base::OnceCallback<void(std::optional<TaskData>)> callback) override;
  void DeleteTaskData(int64_t task_definition_id,
                      base::OnceCallback<void(bool)> callback) override;

 private:
  base::SequenceBound<TaskDatabase> db_;
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_RECORDING_DATA_MANAGER_IMPL_H_
