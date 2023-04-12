// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/surfaces/surface_saved_frame.h"
#include "components/viz/service/transitions/transferable_resource_tracker.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

std::unique_ptr<SurfaceSavedFrame> CreateFrameWithResult() {
  auto directive = CompositorFrameTransitionDirective::CreateSave(
      NavigationID::Null(), 1, {});
  auto frame = std::make_unique<SurfaceSavedFrame>(
      std::move(directive),
      base::BindRepeating([](const CompositorFrameTransitionDirective&) {}));
  frame->CompleteSavedFrameForTesting();
  return frame;
}

}  // namespace

class TransferableResourceTrackerTest : public testing::Test {
 public:
  void SetNextId(TransferableResourceTracker* tracker, uint32_t id) {
    tracker->next_id_ = id;
  }

  // Returns if there is a SharedBitmap in SharedBitmapManager for |resource|.
  bool HasBitmapResource(const TransferableResource& resource) {
    DCHECK(resource.is_software);
    SharedBitmapId id = resource.mailbox_holder.mailbox;
    return !!shared_bitmap_manager_.GetSharedBitmapFromId(
        gfx::Size(1, 1), SinglePlaneFormat::kRGBA_8888, id);
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
};

TEST_F(TransferableResourceTrackerTest, IdInRange) {
  TransferableResourceTracker tracker(&shared_bitmap_manager_);

  auto frame1 = tracker.ImportResources(CreateFrameWithResult());
  EXPECT_TRUE(HasBitmapResource(frame1.root.resource));

  EXPECT_GE(frame1.root.resource.id, kVizReservedRangeStartId);

  auto frame2 = tracker.ImportResources(CreateFrameWithResult());
  EXPECT_TRUE(HasBitmapResource(frame2.root.resource));

  EXPECT_GE(frame2.root.resource.id, frame1.root.resource.id);

  tracker.ReturnFrame(frame1);
  EXPECT_FALSE(HasBitmapResource(frame1.root.resource));

  tracker.RefResource(frame2.root.resource.id);
  tracker.ReturnFrame(frame2);
  EXPECT_TRUE(HasBitmapResource(frame2.root.resource));
  tracker.UnrefResource(frame2.root.resource.id, 1);
  EXPECT_FALSE(HasBitmapResource(frame2.root.resource));
}

TEST_F(TransferableResourceTrackerTest, ExhaustedIdLoops) {
  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "The test only makes sense if ResourceId is uint32_t");
  uint32_t next_id = std::numeric_limits<uint32_t>::max() - 3u;
  TransferableResourceTracker tracker(&shared_bitmap_manager_);
  SetNextId(&tracker, next_id);

  ResourceId last_id = kInvalidResourceId;
  for (int i = 0; i < 10; ++i) {
    auto frame = tracker.ImportResources(CreateFrameWithResult());
    EXPECT_TRUE(HasBitmapResource(frame.root.resource));

    EXPECT_GE(frame.root.resource.id, kVizReservedRangeStartId);
    EXPECT_NE(frame.root.resource.id, last_id);
    last_id = frame.root.resource.id;
    tracker.ReturnFrame(frame);
    EXPECT_FALSE(HasBitmapResource(frame.root.resource));
  }
}

TEST_F(TransferableResourceTrackerTest, UnrefWithCount) {
  TransferableResourceTracker tracker(&shared_bitmap_manager_);
  auto frame = tracker.ImportResources(CreateFrameWithResult());
  for (int i = 0; i < 1000; ++i)
    tracker.RefResource(frame.root.resource.id);
  ASSERT_FALSE(tracker.is_empty());
  tracker.UnrefResource(frame.root.resource.id, 1);
  EXPECT_FALSE(tracker.is_empty());
  tracker.UnrefResource(frame.root.resource.id, 1);
  EXPECT_FALSE(tracker.is_empty());
  tracker.UnrefResource(frame.root.resource.id, 999);
  EXPECT_TRUE(tracker.is_empty());
}

TEST_F(TransferableResourceTrackerTest,
       ExhaustedIdLoopsButSkipsUnavailableIds) {
  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "The test only makes sense if ResourceId is uint32_t");
  TransferableResourceTracker tracker(&shared_bitmap_manager_);

  auto reserved = tracker.ImportResources(CreateFrameWithResult());
  EXPECT_GE(reserved.root.resource.id, kVizReservedRangeStartId);

  uint32_t next_id = std::numeric_limits<uint32_t>::max() - 3u;
  SetNextId(&tracker, next_id);

  ResourceId last_id = kInvalidResourceId;
  for (int i = 0; i < 10; ++i) {
    auto frame = tracker.ImportResources(CreateFrameWithResult());
    EXPECT_TRUE(HasBitmapResource(frame.root.resource));

    EXPECT_GE(frame.root.resource.id, kVizReservedRangeStartId);
    EXPECT_NE(frame.root.resource.id, last_id);
    EXPECT_NE(frame.root.resource.id, reserved.root.resource.id);
    last_id = frame.root.resource.id;
    tracker.ReturnFrame(frame);
    EXPECT_FALSE(HasBitmapResource(frame.root.resource));
  }
}

}  // namespace viz
