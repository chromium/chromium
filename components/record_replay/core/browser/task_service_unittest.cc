// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {
namespace {

class TaskServiceTest : public testing::Test {
 public:
  TaskServiceTest() = default;
  ~TaskServiceTest() override = default;
};

TEST_F(TaskServiceTest, CanInstantiate) {
  TaskService task_service;
  // The service is currently concrete and has placeholder methods. Check that
  // we can instantiate it successfully.
  EXPECT_TRUE(true);
}

}  // namespace
}  // namespace record_replay
