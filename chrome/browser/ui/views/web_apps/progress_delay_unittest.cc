// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/progress_delay.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class ProgressDelayTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ProgressDelayTest, ProgressSteps) {
  double current_progress = 0;
  bool completed = false;

  auto delay = std::make_unique<ProgressDelay>(base::Seconds(1), 10);

  delay->Start(base::BindLambdaForTesting([&](std::optional<double> progress) {
    if (progress.has_value()) {
      current_progress = progress.value();
    } else {
      completed = true;
    }
  }));

  // Initially no movement.
  EXPECT_EQ(0, current_progress);
  EXPECT_FALSE(completed);

  // Advance time to take a step.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_DOUBLE_EQ(0.1, current_progress);
  EXPECT_FALSE(completed);

  // Advance to complete.
  task_environment_.FastForwardBy(base::Milliseconds(900));
  EXPECT_DOUBLE_EQ(1.0, current_progress);
  EXPECT_TRUE(completed);
}

}  // namespace web_app
