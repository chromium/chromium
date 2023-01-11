// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_request_scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

TEST(VariationsRequestSchedulerTest, ScheduleFetchShortly) {
  base::test::SingleThreadTaskEnvironment task_environment;

  const base::RepeatingClosure task = base::DoNothing();
  VariationsRequestScheduler scheduler(task);
  EXPECT_FALSE(scheduler.one_shot_timer_.IsRunning());

  scheduler.ScheduleFetchShortly();
  EXPECT_TRUE(scheduler.one_shot_timer_.IsRunning());
}

}  // namespace variations
