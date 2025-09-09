// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/contextual_task.h"

#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(ContextualTaskTest, GetTaskId) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ContextualTask task(task_id);
  EXPECT_EQ(task_id, task.GetTaskId());
}

}  // namespace contextual_tasks
