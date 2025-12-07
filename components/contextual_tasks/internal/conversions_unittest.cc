// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/conversions.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(ContextualTasksConversionsTest, ToThreadType) {
  EXPECT_EQ(ThreadType::kAiMode,
            ToThreadType(sync_pb::AiThreadSpecifics::AI_MODE));
  EXPECT_EQ(ThreadType::kUnknown,
            ToThreadType(sync_pb::AiThreadSpecifics::UNKNOWN));
}

}  // namespace contextual_tasks
