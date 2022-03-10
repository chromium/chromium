// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/dummy_model_execution_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
class DummyModelExecutionManagerTest : public testing::Test {
 public:
  DummyModelExecutionManagerTest() = default;
  ~DummyModelExecutionManagerTest() override = default;

  void TearDown() override {
    model_execution_manager_.reset();
    // Allow for the background class to be destroyed.
    RunUntilIdle();
  }

  void CreateModelExecutionManager() {
    model_execution_manager_ = std::make_unique<DummyModelExecutionManager>();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void ExecuteModel() {
    base::RunLoop loop;
    model_execution_manager_->ExecuteModel(
        proto::SegmentInfo(),
        base::BindOnce(&DummyModelExecutionManagerTest::OnExecutionCallback,
                       base::Unretained(this), loop.QuitClosure()));
    loop.Run();
  }

  void OnExecutionCallback(
      base::RepeatingClosure closure,
      const std::pair<float, ModelExecutionStatus>& actual) {
    EXPECT_EQ(ModelExecutionStatus::kExecutionError, actual.second);
    EXPECT_EQ(0, actual.first);
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyModelExecutionManager> model_execution_manager_;
};

TEST_F(DummyModelExecutionManagerTest, AlwaysSameResult) {
  CreateModelExecutionManager();
  ExecuteModel();
}

}  // namespace segmentation_platform
