// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameters_extractor.h"

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class TaskParametersExtractorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TaskParametersExtractor extractor_;
};

TEST_F(TaskParametersExtractorTest, ExtractsParameters) {
  // 1. Create a dummy TaskDefinition with steps and parameters.
  TaskDefinition task_definition;
  task_definition.set_url("https://example.com/");
  task_definition.set_title("Test Task");

  // Step index 0
  TaskStep* step0 = task_definition.add_task_steps();
  step0->set_step_index(0);
  step0->set_url("https://example.com/step1");
  step0->set_description("Step 1");

  TaskParameter* param0_1 = step0->add_parameters();
  param0_1->set_key("key1");
  param0_1->set_name("param1");
  param0_1->set_type("string");

  TaskParameter* param0_2 = step0->add_parameters();
  param0_2->set_key("key2");
  param0_2->set_name("param2");
  param0_2->set_type("string");

  extractor_.StartExtraction(task_definition);

  // 2. Query selectors and verify they are correct.
  std::map<std::string, std::string> parameter_value_selectors =
      extractor_.GetParameterValueSelectorsForUrl(GURL("https://example.com/"));

  ASSERT_EQ(parameter_value_selectors.size(), 2U);
  EXPECT_EQ(parameter_value_selectors["key1"], "#ui-id-1");
  EXPECT_EQ(parameter_value_selectors["key2"], "#ui-id-2");

  // 3. Store extracted values first.
  extractor_.StoreExtractedValue("key1", "Value 1");
  extractor_.StoreExtractedValue("key2", "Value 2");

  // 4. Call FillExtractedParametersTo.
  TaskObservation observation;
  base::test::TestFuture<bool> future;
  extractor_.FillExtractedParametersTo(&observation, future.GetCallback());
  EXPECT_TRUE(future.Get());

  EXPECT_EQ(observation.definition().task_steps_size(), 1);
  auto step0_data = observation.definition().task_steps(0);
  ASSERT_EQ(step0_data.parameters_size(), 2);
  EXPECT_EQ(step0_data.parameters(0).key(), "key1");
  EXPECT_EQ(step0_data.parameters(0).value(), "Value 1");
  EXPECT_EQ(step0_data.parameters(1).key(), "key2");
  EXPECT_EQ(step0_data.parameters(1).value(), "Value 2");

  extractor_.FinishExtraction();
}

TEST_F(TaskParametersExtractorTest, FillDummyDataForAllExpectedKeys) {
  // 1. Create a dummy TaskDefinition with steps and parameters.
  TaskDefinition task_definition;
  task_definition.set_url("https://example.com");
  task_definition.set_title("Test Task");

  // Step index 0
  TaskStep* step0 = task_definition.add_task_steps();
  step0->set_step_index(0);
  step0->set_url("https://example.com/step1");
  step0->set_description("Step 1");

  TaskParameter* param0_1 = step0->add_parameters();
  param0_1->set_key("key1");
  param0_1->set_name("param1");
  param0_1->set_type("string");

  TaskParameter* param0_2 = step0->add_parameters();
  param0_2->set_key("key2");
  param0_2->set_name("param2");
  param0_2->set_type("string");

  // Step index 1
  TaskStep* step1 = task_definition.add_task_steps();
  step1->set_step_index(1);
  step1->set_url("https://example.com/step2");
  step1->set_description("Step 2");

  TaskParameter* param1_1 = step1->add_parameters();
  param1_1->set_key("key3");
  param1_1->set_name("param3");
  param1_1->set_type("string");

  // 2. Start extraction session.
  extractor_.StartExtraction(std::move(task_definition));

  // 3. Call FillExtractedParametersTo and wait for async completion.
  TaskObservation observation;
  base::test::TestFuture<bool> future;

  extractor_.FillExtractedParametersTo(&observation, future.GetCallback());
  EXPECT_TRUE(future.Get());

  // 4. Verify TaskObservation is populated with dummy values for all
  // parameters.
  ASSERT_TRUE(observation.has_definition());
  ASSERT_EQ(observation.definition().task_steps_size(), 2);

  // Step index 0 parameters values
  ASSERT_EQ(observation.definition().task_steps(0).parameters_size(), 2);
  EXPECT_EQ(observation.definition().task_steps(0).parameters(0).value(),
            "dummy_value");
  EXPECT_EQ(observation.definition().task_steps(0).parameters(1).value(),
            "dummy_value");

  // Step index 1 parameters values
  ASSERT_EQ(observation.definition().task_steps(1).parameters_size(), 1);
  EXPECT_EQ(observation.definition().task_steps(1).parameters(0).value(),
            "dummy_value");

  // 5. Finish session.
  extractor_.FinishExtraction();
}

TEST_F(TaskParametersExtractorTest, FillFailsForNoActiveSession) {
  TaskObservation observation;
  base::test::TestFuture<bool> future;

  extractor_.FillExtractedParametersTo(&observation, future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(observation.has_definition());
}

}  // namespace record_replay
