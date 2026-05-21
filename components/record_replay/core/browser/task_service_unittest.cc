// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {
namespace {

using ::testing::NiceMock;

class MockRecordingDataManager : public RecordingDataManager {
 public:
  MockRecordingDataManager() = default;
  ~MockRecordingDataManager() override = default;

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
               std::string target_url,
               std::optional<int64_t> recording_id,
               base::OnceClosure callback),
              (override));
  MOCK_METHOD(
      void,
      GetTaskDefinition,
      (int64_t task_definition_id,
       base::OnceCallback<void(std::optional<TaskDefinition>)> callback),
      (override));
  MOCK_METHOD(
      void,
      GetTaskDefinitionsByUrl,
      (std::string url,
       base::OnceCallback<void(std::vector<std::pair<int64_t, TaskDefinition>>)>
           callback),
      (override));
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

class TaskServiceTest : public testing::Test {
 public:
  TaskServiceTest() = default;
  ~TaskServiceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TaskServiceTest, CanInstantiate) {
  TaskService task_service(nullptr);
  // Check that we can instantiate it successfully.
  EXPECT_TRUE(true);
}

TEST_F(TaskServiceTest, OnURLVisitedRetrievesTaskDefinitions) {
  NiceMock<MockRecordingDataManager> mock_data_manager;
  TaskService task_service(&mock_data_manager);

  GURL url("https://example.com");
  EXPECT_CALL(mock_data_manager,
              GetTaskDefinitionsByUrl(url.spec(), ::testing::_))
      .WillOnce(
          [](std::string url,
             base::OnceCallback<void(
                 std::vector<std::pair<int64_t, TaskDefinition>>)> callback) {
            std::vector<std::pair<int64_t, TaskDefinition>> task_definitions;
            TaskDefinition task_definition;
            task_definition.set_url(url);
            task_definition.set_title("Test Task");
            task_definition.set_description("Test Description");
            task_definitions.emplace_back(42, task_definition);
            std::move(callback).Run(std::move(task_definitions));
          });

  task_service.OnURLVisited(url);
}

}  // namespace
}  // namespace record_replay
