// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameters_extractor.h"

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/record_replay/core/browser/recording.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class TaskParametersExtractorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TaskParametersExtractor extractor_;
};

TEST_F(TaskParametersExtractorTest, FillDummyDataForAllExpectedKeys) {
  // 1. Create a dummy TaskDefinition with steps and expected keys.
  TaskDefinition task_definition;
  task_definition.set_url("https://example.com");
  task_definition.set_title("Test Task");

  // Step 1
  StepDefinition step1;
  step1.set_description("Step 1");
  step1.add_expected_data_keys("key1");
  step1.add_expected_data_keys("key2");
  (*task_definition.mutable_steps())[1] = step1;

  // Step 2
  StepDefinition step2;
  step2.set_description("Step 2");
  step2.add_expected_data_keys("key3");
  (*task_definition.mutable_steps())[2] = step2;

  // 2. Start extraction session.
  extractor_.StartExtraction(task_definition);

  // 3. Call FillExtractedParametersTo and wait for async completion.
  TaskData task_data;
  base::test::TestFuture<bool> future;

  extractor_.FillExtractedParametersTo(&task_data, future.GetCallback());
  EXPECT_TRUE(future.Get());

  // 4. Verify TaskData is populated with dummy values for all keys.
  EXPECT_EQ(task_data.step_data().size(), 2u);

  // Step 1 values
  auto step1_data = task_data.step_data().find(1);
  ASSERT_NE(step1_data, task_data.step_data().end());
  EXPECT_EQ(step1_data->second.values().size(), 2u);
  EXPECT_EQ(step1_data->second.values().at("key1"), "dummy_value");
  EXPECT_EQ(step1_data->second.values().at("key2"), "dummy_value");

  // Step 2 values
  auto step2_data = task_data.step_data().find(2);
  ASSERT_NE(step2_data, task_data.step_data().end());
  EXPECT_EQ(step2_data->second.values().size(), 1u);
  EXPECT_EQ(step2_data->second.values().at("key3"), "dummy_value");

  // 5. Finish session.
  extractor_.FinishExtraction();
}

TEST_F(TaskParametersExtractorTest, FillFailsForNoActiveSession) {
  TaskData task_data;
  base::test::TestFuture<bool> future;

  extractor_.FillExtractedParametersTo(&task_data, future.GetCallback());
  EXPECT_FALSE(future.Get());
  EXPECT_TRUE(task_data.step_data().empty());
}

}  // namespace record_replay
