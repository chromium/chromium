// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/test/bind.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

TEST(TransferableResourceTrackerTest, IdInRange) {
  ResourceId starting_id = 12345u;
  TransferableResourceTracker tracker(starting_id);

  bool resource1_released = false;
  auto resource1 = tracker.AddMailboxResource(
      gpu::Mailbox::GenerateForSharedImage(), gpu::SyncToken(), gfx::Size(1, 2),
      SingleReleaseCallback::Create(base::BindLambdaForTesting(
          [&resource1_released](const gpu::SyncToken&, bool) {
            ASSERT_FALSE(resource1_released);
            resource1_released = true;
          })));

  EXPECT_GE(resource1.id, starting_id);

  bool resource2_released = false;
  auto resource2 = tracker.AddMailboxResource(
      gpu::Mailbox::GenerateForSharedImage(), gpu::SyncToken(), gfx::Size(1, 2),
      SingleReleaseCallback::Create(base::BindLambdaForTesting(
          [&resource2_released](const gpu::SyncToken&, bool) {
            ASSERT_FALSE(resource2_released);
            resource2_released = true;
          })));

  EXPECT_GE(resource2.id, resource1.id);

  tracker.UnrefResource(resource1.id);
  EXPECT_TRUE(resource1_released);

  tracker.RefResource(resource2.id);
  tracker.UnrefResource(resource2.id);
  EXPECT_FALSE(resource2_released);
  tracker.UnrefResource(resource2.id);
  EXPECT_TRUE(resource2_released);
}

TEST(TransferableResourceTrackerTest, ExhaustedIdLoops) {
  ResourceId starting_id = std::numeric_limits<ResourceId>::max() - 3u;
  TransferableResourceTracker tracker(starting_id);

  ResourceId last_id = 0;
  for (int i = 0; i < 10; ++i) {
    bool resource_released = false;
    auto resource = tracker.AddMailboxResource(
        gpu::Mailbox::GenerateForSharedImage(), gpu::SyncToken(),
        gfx::Size(1, 2),
        SingleReleaseCallback::Create(base::BindLambdaForTesting(
            [&resource_released](const gpu::SyncToken&, bool) {
              ASSERT_FALSE(resource_released);
              resource_released = true;
            })));

    EXPECT_GE(resource.id, starting_id);
    EXPECT_NE(resource.id, last_id);
    last_id = resource.id;
    tracker.UnrefResource(resource.id);
    EXPECT_TRUE(resource_released);
  }
}

TEST(TransferableResourceTrackerTest, ExhaustedIdLoopsButSkipsUnavailableIds) {
  ResourceId starting_id = std::numeric_limits<ResourceId>::max() - 3u;
  TransferableResourceTracker tracker(starting_id);

  auto reserved_resource = tracker.AddMailboxResource(
      gpu::Mailbox::GenerateForSharedImage(), gpu::SyncToken(), gfx::Size(1, 2),
      SingleReleaseCallback::Create(
          base::BindOnce([](const gpu::SyncToken&, bool) {})));
  EXPECT_GE(reserved_resource.id, starting_id);

  ResourceId last_id = 0;
  for (int i = 0; i < 10; ++i) {
    bool resource_released = false;
    auto resource = tracker.AddMailboxResource(
        gpu::Mailbox::GenerateForSharedImage(), gpu::SyncToken(),
        gfx::Size(1, 2),
        SingleReleaseCallback::Create(base::BindLambdaForTesting(
            [&resource_released](const gpu::SyncToken&, bool) {
              ASSERT_FALSE(resource_released);
              resource_released = true;
            })));

    EXPECT_GE(resource.id, starting_id);
    EXPECT_NE(resource.id, last_id);
    EXPECT_NE(resource.id, reserved_resource.id);
    last_id = resource.id;
    tracker.UnrefResource(resource.id);
    EXPECT_TRUE(resource_released);
  }
}

}  // namespace viz
