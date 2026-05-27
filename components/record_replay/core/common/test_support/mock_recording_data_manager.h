// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_COMMON_TEST_SUPPORT_MOCK_RECORDING_DATA_MANAGER_H_
#define COMPONENTS_RECORD_REPLAY_CORE_COMMON_TEST_SUPPORT_MOCK_RECORDING_DATA_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace record_replay {

class MockRecordingDataManager : public RecordingDataManager {
 public:
  MockRecordingDataManager();
  MockRecordingDataManager(const MockRecordingDataManager&) = delete;
  MockRecordingDataManager& operator=(const MockRecordingDataManager&) = delete;
  ~MockRecordingDataManager() override;

  MOCK_METHOD(void,
              AddRecording,
              (Recording recording, base::OnceCallback<void(int64_t)> callback),
              (override));
  MOCK_METHOD(void,
              GetRecordingsByUrl,
              (std::string url,
               base::OnceCallback<void(std::vector<Recording>)> callback),
              (override));
  MOCK_METHOD(void,
              SaveTaskDefinition,
              (std::optional<int64_t> task_definition_id,
               TaskDefinition task_definition,
               base::OnceCallback<void(int64_t)> callback),
              (override));
  MOCK_METHOD(
      void,
      GetTaskDefinition,
      (int64_t task_definition_id,
       base::OnceCallback<void(std::optional<TaskDefinition>)> callback),
      (override));
  MOCK_METHOD(void,
              GetTaskDefinitionsByUrl,
              (std::string url,
               base::OnceCallback<void(std::vector<TaskDefinition>)> callback),
              (override));

  // Legacy Task Data mock targets (preserved for CL 1 / part4b base setup)
  MOCK_METHOD(void,
              SaveTaskData,
              (int64_t task_definition_id,
               TaskData data,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              GetTaskData,
              (int64_t task_definition_id,
               base::OnceCallback<void(std::optional<TaskData>)> callback),
              (override));
  MOCK_METHOD(void,
              DeleteTaskData,
              (int64_t task_definition_id,
               base::OnceCallback<void(bool)> callback),
              (override));
};

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_COMMON_TEST_SUPPORT_MOCK_RECORDING_DATA_MANAGER_H_
