// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameter_values_predictor.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class MockTaskStore : public TaskStore {
 public:
  MockTaskStore() = default;
  ~MockTaskStore() override = default;

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
  MOCK_METHOD(void,
              SaveObservation,
              (TaskObservation observation,
               base::OnceCallback<void(int64_t)> callback),
              (override));
  MOCK_METHOD(void,
              GetObservationsForDefinition,
              (int64_t task_definition_id,
               base::OnceCallback<void(std::vector<TaskObservation>)> callback),
              (override));
  MOCK_METHOD(void,
              DeleteObservation,
              (int64_t observation_id, base::OnceCallback<void(bool)> callback),
              (override));
};

class TaskParameterValuesPredictorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TaskParameterValuesPredictorTest,
       PredictReturnsNulloptWhenNoStoreOrNoId) {
  // Case A: Null store pointer
  {
    TaskDefinition task_definition;
    task_definition.set_id(42);

    TaskParameterValuesPredictor predictor(nullptr);
    base::test::TestFuture<std::optional<std::vector<TaskParameter>>> future;
    predictor.Predict(task_definition, future.GetCallback());

    std::optional<std::vector<TaskParameter>> result = future.Get();
    EXPECT_FALSE(result.has_value());
  }

  // Case B: Task definition has no ID
  {
    StrictMock<MockTaskStore> mock_store;
    TaskDefinition task_definition;
    task_definition.set_url("https://example.com/");

    TaskParameterValuesPredictor predictor(&mock_store);
    base::test::TestFuture<std::optional<std::vector<TaskParameter>>> future;
    predictor.Predict(task_definition, future.GetCallback());

    std::optional<std::vector<TaskParameter>> result = future.Get();
    EXPECT_FALSE(result.has_value());
  }
}

TEST_F(TaskParameterValuesPredictorTest,
       PredictReturnsParametersFromFirstObservation) {
  StrictMock<MockTaskStore> mock_store;

  TaskDefinition task_definition;
  task_definition.set_id(42);
  task_definition.set_url("https://example.com/");
  task_definition.set_title("Test Task");

  // Set up mock seeder database query responses:
  EXPECT_CALL(mock_store, GetObservationsForDefinition(42, _))
      .WillOnce(
          [](int64_t id,
             base::OnceCallback<void(std::vector<TaskObservation>)> callback) {
            std::vector<TaskObservation> observations;

            // First Observation (should be returned!)
            TaskObservation observation1;
            observation1.set_id(100);
            TaskDefinition* def1 = observation1.mutable_definition();
            def1->set_id(42);
            TaskStep* step1 = def1->add_task_steps();
            step1->set_step_index(0);
            TaskParameter* param1 = step1->add_parameters();
            param1->set_key("departure_date");
            param1->set_value("Wed, May 27");
            observations.push_back(std::move(observation1));

            // Second Observation (should be ignored!)
            TaskObservation observation2;
            observation2.set_id(101);
            TaskDefinition* def2 = observation2.mutable_definition();
            def2->set_id(42);
            TaskStep* step2 = def2->add_task_steps();
            step2->set_step_index(0);
            TaskParameter* param2 = step2->add_parameters();
            param2->set_key("departure_date");
            param2->set_value("Thu, May 28");
            observations.push_back(std::move(observation2));

            std::move(callback).Run(std::move(observations));
          });

  TaskParameterValuesPredictor predictor(&mock_store);
  base::test::TestFuture<std::optional<std::vector<TaskParameter>>> future;
  predictor.Predict(task_definition, future.GetCallback());

  std::optional<std::vector<TaskParameter>> result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1U);
  EXPECT_EQ((*result)[0].key(), "departure_date");
  EXPECT_EQ((*result)[0].value(), "Wed, May 27");
}

}  // namespace
}  // namespace record_replay
