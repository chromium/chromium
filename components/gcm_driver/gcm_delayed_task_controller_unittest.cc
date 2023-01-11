// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_delayed_task_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

class GCMDelayedTaskControllerTest : public testing::Test {
 public:
  GCMDelayedTaskControllerTest();
  ~GCMDelayedTaskControllerTest() override;

  void TestTask();

  GCMDelayedTaskController* controller() { return controller_.get(); }

  int number_of_triggered_tasks() const { return number_of_triggered_tasks_; }

 private:
  std::unique_ptr<GCMDelayedTaskController> controller_;
  int number_of_triggered_tasks_;
};

GCMDelayedTaskControllerTest::GCMDelayedTaskControllerTest()
    : controller_(new GCMDelayedTaskController), number_of_triggered_tasks_(0) {
}

GCMDelayedTaskControllerTest::~GCMDelayedTaskControllerTest() {
}

void GCMDelayedTaskControllerTest::TestTask() {
  ++number_of_triggered_tasks_;
}

// Tests that a newly created controller forced tasks to be delayed, while
// calling SetReady allows tasks to execute.
TEST_F(GCMDelayedTaskControllerTest, SetReadyWithNoTasks) {
  EXPECT_FALSE(controller()->CanRunTaskWithoutDelay());
  EXPECT_EQ(0, number_of_triggered_tasks());

  controller()->SetReady();
  EXPECT_TRUE(controller()->CanRunTaskWithoutDelay());
  EXPECT_EQ(0, number_of_triggered_tasks());
}

// Tests that tasks are triggered when controlles is set to ready.
TEST_F(GCMDelayedTaskControllerTest, PendingTasksTriggeredWhenSetReady) {
  controller()->AddTask(base::BindOnce(&GCMDelayedTaskControllerTest::TestTask,
                                       base::Unretained(this)));
  controller()->AddTask(base::BindOnce(&GCMDelayedTaskControllerTest::TestTask,
                                       base::Unretained(this)));

  controller()->SetReady();
  EXPECT_EQ(2, number_of_triggered_tasks());
}

}  // namespace gcm
