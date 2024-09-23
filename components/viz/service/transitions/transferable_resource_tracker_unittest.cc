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
  CompositorFrameTransitionDirective::SharedElement element;
  auto directive = CompositorFrameTransitionDirective::CreateSave(
      blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/false, 1,
      {element}, {});
  auto frame = SurfaceSavedFrame::CreateForTesting(std::move(directive));
  frame->CompleteSavedFrameForTesting();
  return frame;
}

}  // namespace

class TransferableResourceTrackerTest : public testing::Test {
 public:
  void SetNextId(TransferableResourceTracker* tracker, uint32_t id) {
    tracker->id_tracker_->set_next_id_for_test(id);
  }

  // Returns if there is a SharedBitmap in SharedBitmapManager for |resource|.
  bool HasBitmapResource(const TransferableResource& resource) {
    DCHECK(resource.is_software);
    SharedBitmapId id = resource.shared_bitmap_id();
    return !!shared_bitmap_manager_.GetSharedBitmapFromId(
        gfx::Size(1, 1), SinglePlaneFormat::kRGBA_8888, id);
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  ReservedResourceIdTracker id_tracker_;
};

TEST_F(TransferableResourceTrackerTest, IdInRange) {
  TransferableResourceTracker tracker(&shared_bitmap_manager_, &id_tracker_);

  auto result = CreateFrameWithResult();
  auto frame1 =
      tracker.ImportResources(result->TakeResult(), result->directive());
  ASSERT_EQ(frame1.shared.size(), 1u);
  const auto& resource1 = frame1.shared.at(0);
  EXPECT_TRUE(HasBitmapResource(resource1->resource));

  EXPECT_GE(resource1->resource.id, kVizReservedRangeStartId);

  result = CreateFrameWithResult();
  auto frame2 =
      tracker.ImportResources(result->TakeResult(), result->directive());
  ASSERT_EQ(frame2.shared.size(), 1u);
  const auto& resource2 = frame2.shared.at(0);
  EXPECT_TRUE(HasBitmapResource(resource2->resource));

  EXPECT_GE(resource2->resource.id, resource1->resource.id);

  tracker.ReturnFrame(frame1);
  EXPECT_FALSE(HasBitmapResource(resource1->resource));

  tracker.RefResource(resource2->resource.id);
  tracker.ReturnFrame(frame2);
  EXPECT_TRUE(HasBitmapResource(resource2->resource));
  tracker.UnrefResource(resource2->resource.id, 1, gpu::SyncToken());
  EXPECT_FALSE(HasBitmapResource(resource2->resource));
}

TEST_F(TransferableResourceTrackerTest, ExhaustedIdLoops) {
  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "The test only makes sense if ResourceId is uint32_t");
  uint32_t next_id = std::numeric_limits<uint32_t>::max() - 3u;
  TransferableResourceTracker tracker(&shared_bitmap_manager_, &id_tracker_);
  SetNextId(&tracker, next_id);

  ResourceId last_id = kInvalidResourceId;
  std::vector<TransferableResourceTracker::ResourceFrame> frames;
  for (int i = 0; i < 10; ++i) {
    auto result = CreateFrameWithResult();
    auto frame =
        tracker.ImportResources(result->TakeResult(), result->directive());
    ASSERT_EQ(frame.shared.size(), 1u);
    const auto& resource = frame.shared.at(0);
    EXPECT_TRUE(HasBitmapResource(resource->resource));

    EXPECT_GE(resource->resource.id, kVizReservedRangeStartId);
    EXPECT_NE(resource->resource.id, last_id);
    last_id = resource->resource.id;
    frames.push_back(std::move(frame));
  }
  for (auto& frame : frames) {
    tracker.ReturnFrame(frame);
    EXPECT_FALSE(HasBitmapResource(frame.shared.at(0)->resource));
  }
}

TEST_F(TransferableResourceTrackerTest, UnrefWithCount) {
  TransferableResourceTracker tracker(&shared_bitmap_manager_, &id_tracker_);
  auto result = CreateFrameWithResult();
  auto frame =
      tracker.ImportResources(result->TakeResult(), result->directive());
  ASSERT_EQ(frame.shared.size(), 1u);
  const auto& resource = frame.shared.at(0);
  for (int i = 0; i < 1000; ++i)
    tracker.RefResource(resource->resource.id);
  ASSERT_FALSE(tracker.is_empty());
  tracker.UnrefResource(resource->resource.id, 1, gpu::SyncToken());
  EXPECT_FALSE(tracker.is_empty());
  tracker.UnrefResource(resource->resource.id, 1, gpu::SyncToken());
  EXPECT_FALSE(tracker.is_empty());
  tracker.UnrefResource(resource->resource.id, 999, gpu::SyncToken());
  EXPECT_TRUE(tracker.is_empty());
}

TEST_F(TransferableResourceTrackerTest,
       ExhaustedIdLoopsButSkipsUnavailableIds) {
  static_assert(std::is_same<decltype(kInvalidResourceId.GetUnsafeValue()),
                             uint32_t>::value,
                "The test only makes sense if ResourceId is uint32_t");
  TransferableResourceTracker tracker(&shared_bitmap_manager_, &id_tracker_);

  auto result = CreateFrameWithResult();
  auto reserved =
      tracker.ImportResources(result->TakeResult(), result->directive());
  ASSERT_EQ(reserved.shared.size(), 1u);
  const auto& resource = reserved.shared.at(0);
  EXPECT_GE(resource->resource.id, kVizReservedRangeStartId);

  uint32_t next_id = std::numeric_limits<uint32_t>::max() - 3u;
  SetNextId(&tracker, next_id);

  ResourceId last_id = kInvalidResourceId;
  std::vector<TransferableResourceTracker::ResourceFrame> frames;
  for (int i = 0; i < 10; ++i) {
    result = CreateFrameWithResult();
    auto frame =
        tracker.ImportResources(result->TakeResult(), result->directive());
    ASSERT_EQ(frame.shared.size(), 1u);
    const auto& new_resource = frame.shared.at(0);
    EXPECT_TRUE(HasBitmapResource(new_resource->resource));

    EXPECT_GE(new_resource->resource.id, kVizReservedRangeStartId);
    EXPECT_NE(new_resource->resource.id, last_id);
    EXPECT_NE(new_resource->resource.id, resource->resource.id);
    last_id = new_resource->resource.id;
    frames.push_back(std::move(frame));
  }
  for (auto& frame : frames) {
    tracker.ReturnFrame(frame);
    EXPECT_FALSE(HasBitmapResource(frame.shared.at(0)->resource));
  }
}

}  // namespace viz
