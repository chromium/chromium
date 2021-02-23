// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

std::unique_ptr<SurfaceSavedFrame> CreateFrameWithResult(
    base::OnceCallback<void(const gpu::SyncToken&, bool)> callback) {
  CompositorFrameTransitionDirective directive(
      1, CompositorFrameTransitionDirective::Type::kSave);
  auto frame = std::make_unique<SurfaceSavedFrame>(directive);
  frame->CompleteSavedFrameForTesting(std::move(callback));
  return frame;
}

}  // namespace

class TransferableResourceTrackerTest : public testing::Test {
 public:
  void SetNextId(TransferableResourceTracker* tracker, uint32_t id) {
    tracker->next_id_ = id;
  }
};

TEST_F(TransferableResourceTrackerTest, IdInRange) {
  TransferableResourceTracker tracker;

  bool resource1_released = false;
  auto resource1 =
      tracker.ImportResource(CreateFrameWithResult(base::BindLambdaForTesting(
          [&resource1_released](const gpu::SyncToken&, bool) {
            ASSERT_FALSE(resource1_released);
            resource1_released = true;
          })));

  EXPECT_GE(resource1.id, kVizReservedRangeStartId);

  bool resource2_released = false;
  auto resource2 =
      tracker.ImportResource(CreateFrameWithResult(base::BindLambdaForTesting(
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

TEST_F(TransferableResourceTrackerTest, ExhaustedIdLoops) {
  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "The test only makes sense if ResourceId is uint32_t");
  uint32_t next_id = std::numeric_limits<uint32_t>::max() - 3u;
  TransferableResourceTracker tracker;
  SetNextId(&tracker, next_id);

  ResourceId last_id = kInvalidResourceId;
  for (int i = 0; i < 10; ++i) {
    bool resource_released = false;
    auto resource =
        tracker.ImportResource(CreateFrameWithResult(base::BindLambdaForTesting(
            [&resource_released](const gpu::SyncToken&, bool) {
              ASSERT_FALSE(resource_released);
              resource_released = true;
            })));

    EXPECT_GE(resource.id, kVizReservedRangeStartId);
    EXPECT_NE(resource.id, last_id);
    last_id = resource.id;
    tracker.UnrefResource(resource.id);
    EXPECT_TRUE(resource_released);
  }
}

TEST_F(TransferableResourceTrackerTest,
       ExhaustedIdLoopsButSkipsUnavailableIds) {
  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "The test only makes sense if ResourceId is uint32_t");
  TransferableResourceTracker tracker;

  auto reserved_resource = tracker.ImportResource(CreateFrameWithResult(
      base::BindOnce([](const gpu::SyncToken&, bool) {})));
  EXPECT_GE(reserved_resource.id, kVizReservedRangeStartId);

  uint32_t next_id = std::numeric_limits<uint32_t>::max() - 3u;
  SetNextId(&tracker, next_id);

  ResourceId last_id = kInvalidResourceId;
  for (int i = 0; i < 10; ++i) {
    bool resource_released = false;
    auto resource =
        tracker.ImportResource(CreateFrameWithResult(base::BindLambdaForTesting(
            [&resource_released](const gpu::SyncToken&, bool) {
              ASSERT_FALSE(resource_released);
              resource_released = true;
            })));

    EXPECT_GE(resource.id, kVizReservedRangeStartId);
    EXPECT_NE(resource.id, last_id);
    EXPECT_NE(resource.id, reserved_resource.id);
    last_id = resource.id;
    tracker.UnrefResource(resource.id);
    EXPECT_TRUE(resource_released);
  }
}

}  // namespace viz
