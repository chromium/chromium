// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_parameter_values_predictor.h"

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {

class TaskParameterValuesPredictorTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TaskParameterValuesPredictorTest, PredictReturnsNullopt) {
  TaskDefinition task_definition;
  task_definition.set_url("https://example.com/");
  task_definition.set_title("Test Task");

  TaskParameterValuesPredictor predictor;
  base::test::TestFuture<std::optional<std::vector<TaskParameter>>> future;
  predictor.Predict(task_definition, future.GetCallback());

  std::optional<std::vector<TaskParameter>> result = future.Get();
  EXPECT_FALSE(result.has_value());
}

}  // namespace record_replay
