// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_observer.h"
#include "components/record_replay/core/browser/task_parameters_extractor.h"
#include "components/record_replay/core/common/test_support/mock_task_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {

using ::testing::NiceMock;

class TaskServiceTest : public testing::Test {
 protected:
  TaskServiceTest()
      : task_service_(&mock_task_store_, nullptr, base::DoNothing()) {}
  ~TaskServiceTest() override = default;

  NiceMock<MockTaskStore> mock_task_store_;
  TaskService task_service_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TaskServiceTest, CanInstantiate) {
  TaskService task_service(nullptr, nullptr, base::DoNothing());
  // Check that we can instantiate it successfully.
  EXPECT_TRUE(true);
}

TEST_F(TaskServiceTest, OnURLVisitedRetrievesTaskDefinitions) {
  NiceMock<MockTaskStore> mock_task_store;
  TaskService task_service(&mock_task_store, nullptr, base::DoNothing());

  GURL url("https://example.com");
  EXPECT_CALL(mock_task_store,
              GetTaskDefinitionsByUrl(url.spec(), ::testing::_))
      .WillOnce(
          [](std::string url,
             base::OnceCallback<void(std::vector<TaskDefinition>)> callback) {
            std::vector<TaskDefinition> task_definitions;
            TaskDefinition task_definition;
            task_definition.set_id(42);
            task_definition.set_url(url);
            task_definition.set_title("Test Task");
            task_definition.set_description("Test Description");
            task_definitions.push_back(std::move(task_definition));
            std::move(callback).Run(std::move(task_definitions));
          });

  task_service.OnURLVisited(url);
}

TEST_F(TaskServiceTest, RegisterAndObserveTaskFlow) {
  // 1. Verify that initially there are no observers.
  EXPECT_EQ(task_service_.getObserverForTesting(), nullptr);

  // 2. Set up mock recording data manager expectations.
  TaskDefinition definition;
  definition.set_title("Test Journey");
  definition.set_url("https://example.com/start");
  TaskStep* step = definition.add_task_steps();
  step->set_url("https://example.com/end");

  EXPECT_CALL(
      mock_task_store_,
      GetTaskDefinitionsByUrl("https://example.com/unrelated", ::testing::_))
      .WillRepeatedly(
          [](std::string url,
             base::OnceCallback<void(std::vector<TaskDefinition>)> callback) {
            std::move(callback).Run({});
          });

  EXPECT_CALL(mock_task_store_, GetTaskDefinitionsByUrl(
                                    "https://example.com/start", ::testing::_))
      .WillRepeatedly(
          [definition](
              std::string url,
              base::OnceCallback<void(std::vector<TaskDefinition>)> callback) {
            std::vector<TaskDefinition> task_definitions;
            TaskDefinition def = definition;
            def.set_id(42);
            task_definitions.push_back(std::move(def));
            std::move(callback).Run(std::move(task_definitions));
          });

  // 3. Visit a different URL and verify no observer is created.
  GURL unrelated_url("https://example.com/unrelated");
  task_service_.OnURLVisited(unrelated_url);
  EXPECT_EQ(task_service_.getObserverForTesting(), nullptr);

  // 4. Visit the start URL and verify a TaskObserver is created and owned by
  // TaskService.
  GURL start_url("https://example.com/start");
  task_service_.OnURLVisited(start_url);
  ASSERT_NE(task_service_.getObserverForTesting(), nullptr);

  // Verify matched/copied task definition details.
  const TaskObserver* observer = task_service_.getObserverForTesting().get();
  ASSERT_NE(observer, nullptr);
  EXPECT_EQ(observer->observation().definition().title(), "Test Journey");
  EXPECT_EQ(observer->observation().definition().url(),
            "https://example.com/start");

  // 5. Complete the task and check that task completed details are propagated
  // correctly.
  TaskObservation completed_obs;
  completed_obs.set_id(42);
  TaskDefinition* completed_def = completed_obs.mutable_definition();
  completed_def->set_title("Test Journey");
  completed_def->set_url("https://example.com/start");
  completed_def->set_description("Completed successfully");

  EXPECT_CALL(mock_task_store_, SaveObservation(::testing::_, ::testing::_))
      .WillOnce([completed_obs](TaskObservation observation,
                                base::OnceCallback<void(int64_t)> callback) {
        EXPECT_EQ(observation.definition().title(), "Test Journey");
        EXPECT_EQ(observation.definition().url(), "https://example.com/start");
        EXPECT_EQ(observation.definition().description(),
                  "Completed successfully");
        std::move(callback).Run(completed_obs.id());
      });

  task_service_.OnTaskCompleted(completed_obs);
  EXPECT_EQ(task_service_.getObserverForTesting(), nullptr);
}

TEST_F(TaskServiceTest, TaskFlowWithParametersExtractor) {
  TaskParametersExtractor extractor;
  TaskService task_service(&mock_task_store_, &extractor, base::DoNothing());

  // Set up a task definition with steps and parameters.
  TaskDefinition definition;
  definition.set_id(101);
  definition.set_title("Journey with Parameters");
  definition.set_url("https://example.com/start");

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_url("https://example.com/final");
  TaskParameter* param = step->add_parameters();
  param->set_key("key1");
  param->set_name("param1");

  // Register mock expectation to retrieve the task when start URL is visited.
  EXPECT_CALL(mock_task_store_, GetTaskDefinitionsByUrl(
                                    "https://example.com/start", ::testing::_))
      .WillOnce(
          [definition](
              std::string url,
              base::OnceCallback<void(std::vector<TaskDefinition>)> callback) {
            std::vector<TaskDefinition> task_definitions;
            task_definitions.push_back(definition);
            std::move(callback).Run(std::move(task_definitions));
          });

  // 1. Visit start URL. TaskObserver should be created.
  GURL start_url("https://example.com/start");
  task_service.OnURLVisited(start_url);

  ASSERT_NE(task_service.getObserverForTesting(), nullptr);

  // 2. Simulate parameters being extracted by the page.
  extractor.StoreExtractedValue("key1", "value_from_dom");

  // 3. Visit final URL which triggers asynchronous parameters filling, then
  // completion. Expect that mock_task_store_.SaveObservation will be
  // called.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_task_store_, SaveObservation(::testing::_, ::testing::_))
      .WillOnce([&run_loop](TaskObservation observation,
                            base::OnceCallback<void(int64_t)> callback) {
        // Verify the value was correctly extracted and filled into the step
        // parameter!
        ASSERT_EQ(observation.definition().task_steps_size(), 1);
        EXPECT_EQ(observation.definition().task_steps(0).parameters(0).value(),
                  "value_from_dom");
        std::move(callback).Run(101);
        run_loop.Quit();
      });

  GURL final_url("https://example.com/final");
  task_service.OnURLVisited(final_url);

  run_loop.Run();

  // 4. Verify observer is reset and extractor has completed (FinishExtraction
  // is called).
  EXPECT_EQ(task_service.getObserverForTesting(), nullptr);
  // Let's try to fill parameters to a new observation to verify the session has
  // finished.
  TaskObservation observation;
  base::test::TestFuture<bool> future;
  extractor.FillExtractedParametersTo(&observation, future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace record_replay
