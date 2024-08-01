// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/frame_sinks/video_capture/shared_memory_video_frame_pool.h"

#include <memory>

#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using media::VideoFrame;

namespace viz {
namespace {

constexpr gfx::Size kSize = gfx::Size(32, 18);
constexpr media::VideoPixelFormat kFormat = media::PIXEL_FORMAT_I420;

void ExpectValidHandleForDelivery(
    const base::ReadOnlySharedMemoryRegion& region) {
  EXPECT_TRUE(region.IsValid());
  constexpr int kI420BitsPerPixel = 12;
  EXPECT_LE(static_cast<size_t>(kSize.GetArea() * kI420BitsPerPixel / 8),
            region.GetSize());
}

TEST(SharedMemoryVideoFramePoolTest, FramesConfiguredCorrectly) {
  SharedMemoryVideoFramePool pool(1);
  const scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  ASSERT_EQ(kSize, frame->coded_size());
  ASSERT_EQ(gfx::Rect(kSize), frame->visible_rect());
  ASSERT_EQ(kSize, frame->natural_size());
  ASSERT_TRUE(frame->IsMappable());
}

TEST(SharedMemoryVideoFramePoolTest, UsesAvailableBuffersIfPossible) {
  constexpr gfx::Size kSmallerSize =
      gfx::Size(kSize.width() / 2, kSize.height() / 2);
  constexpr gfx::Size kBiggerSize =
      gfx::Size(kSize.width() * 2, kSize.height() * 2);

  SharedMemoryVideoFramePool pool(1);

  // Reserve a frame of baseline size and then free it to return it to the pool.
  scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  size_t baseline_bytes_allocated;
  {
    auto handle = pool.CloneHandleForDelivery(*frame);
    ExpectValidHandleForDelivery(handle->get_read_only_shmem_region());
    baseline_bytes_allocated = handle->get_read_only_shmem_region().GetSize();
  }
  frame = nullptr;  // Returns frame to pool.

  // Now, attempt to reserve a smaller-sized frame. Expect that the same buffer
  // is backing the frame because it's large enough.
  frame = pool.ReserveVideoFrame(kFormat, kSmallerSize);
  ASSERT_TRUE(frame);
  {
    auto handle = pool.CloneHandleForDelivery(*frame);
    ExpectValidHandleForDelivery(handle->get_read_only_shmem_region());
    EXPECT_EQ(baseline_bytes_allocated,
              handle->get_read_only_shmem_region().GetSize());
  }
  frame = nullptr;  // Returns frame to pool.

  // Now, attempt to reserve a larger-than-baseline-sized frame. Expect that a
  // different buffer is backing the frame because a larger one had to be
  // allocated.
  frame = pool.ReserveVideoFrame(kFormat, kBiggerSize);
  ASSERT_TRUE(frame);
  size_t larger_buffer_bytes_allocated;
  {
    auto handle = pool.CloneHandleForDelivery(*frame);
    ExpectValidHandleForDelivery(handle->get_read_only_shmem_region());
    larger_buffer_bytes_allocated =
        handle->get_read_only_shmem_region().GetSize();
    EXPECT_LT(baseline_bytes_allocated, larger_buffer_bytes_allocated);
  }
  frame = nullptr;  // Returns frame to pool.

  // Finally, if either a baseline-sized or a smaller-than-baseline-sized frame
  // is reserved, expect that the same larger buffer is backing the frames.
  constexpr gfx::Size kTheSmallerSizes[] = {kSmallerSize, kSize};
  for (const auto& size : kTheSmallerSizes) {
    frame = pool.ReserveVideoFrame(kFormat, size);
    ASSERT_TRUE(frame);
    {
      auto handle = pool.CloneHandleForDelivery(*frame);
      ExpectValidHandleForDelivery(handle->get_read_only_shmem_region());
      EXPECT_EQ(larger_buffer_bytes_allocated,
                handle->get_read_only_shmem_region().GetSize());
    }
    frame = nullptr;  // Returns frame to pool.
  }
}

TEST(SharedMemoryVideoFramePoolTest, ReachesCapacityLimit) {
  SharedMemoryVideoFramePool pool(2);
  scoped_refptr<media::VideoFrame> frames[5];

  // Reserve two frames from a pool of capacity 2.
  frames[0] = pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frames[0]);
  frames[1] = pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frames[1]);

  // Now, try to reserve a third frame. This should fail (return null).
  frames[2] = pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_FALSE(frames[2]);

  // Release the first frame. Then, retry reserving a frame. This should
  // succeed.
  frames[0] = nullptr;
  frames[3] = pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frames[3]);

  // Finally, try to reserve yet another frame. This should fail.
  frames[4] = pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_FALSE(frames[4]);
}

TEST(SharedMemoryVideoFramePoolTest, ReportsCorrectUtilization) {
  SharedMemoryVideoFramePool pool(2);
  ASSERT_EQ(0u, pool.GetNumberOfReservedFrames());
  ASSERT_EQ(0.0f, pool.GetUtilization());

  // Reserve the frame and expect 1/2 the pool to be utilized.
  scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  ASSERT_EQ(1u, pool.GetNumberOfReservedFrames());
  ASSERT_EQ(0.5f, pool.GetUtilization());

  // Signal that the frame will be delivered. This should not change the
  // utilization.
  {
    auto handle = pool.CloneHandleForDelivery(*frame);
    ExpectValidHandleForDelivery(handle->get_read_only_shmem_region());
  }
  ASSERT_EQ(1u, pool.GetNumberOfReservedFrames());
  ASSERT_EQ(0.5f, pool.GetUtilization());

  // Finally, release the frame to indicate it has been delivered and is no
  // longer in-use by downstream consumers. This should cause the utilization
  // to go back down to zero.
  frame = nullptr;
  ASSERT_EQ(0u, pool.GetNumberOfReservedFrames());
  ASSERT_EQ(0.0f, pool.GetUtilization());
}

// Returns true iff each plane of the given |frame| is filled with
// |values[plane]|.
bool PlanesAreFilledWithValues(const VideoFrame& frame, const uint8_t* values) {
  static_assert(VideoFrame::Plane::kU == (VideoFrame::Plane::kY + 1) &&
                    VideoFrame::Plane::kV == (VideoFrame::Plane::kU + 1),
                "enum values changed, will break code below");
  for (int plane = VideoFrame::Plane::kY; plane <= VideoFrame::Plane::kV;
       ++plane) {
    const uint8_t expected_value = values[plane - VideoFrame::Plane::kY];
    for (int y = 0; y < frame.rows(plane); ++y) {
      const uint8_t* row = frame.visible_data(plane) + y * frame.stride(plane);
      for (int x = 0; x < frame.row_bytes(plane); ++x) {
        EXPECT_EQ(expected_value, row[x])
            << "at row " << y << " in plane " << plane;
        if (expected_value != row[x])
          return false;
      }
    }
  }
  return true;
}

TEST(SharedMemoryVideoFramePoolTest, FramesReturnedWhenPoolIsGone) {
  constexpr gfx::ColorSpace kArbitraryColorSpace1 =
      gfx::ColorSpace::CreateREC709();
  constexpr gfx::ColorSpace kArbitraryColorSpace2 =
      gfx::ColorSpace::CreateSRGB();

  // Create a pool dynamically, we will release it and verify that frames are
  // still usable.
  auto pool = std::make_unique<SharedMemoryVideoFramePool>(1);

  // Reserve a frame, populate it:
  scoped_refptr<media::VideoFrame> frame =
      pool->ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);

  constexpr uint8_t kValues1[3] = {0x11, 0x22, 0x33};
  media::FillYUV(frame.get(), kValues1[0], kValues1[1], kValues1[2]);
  frame->set_color_space(kArbitraryColorSpace1);

  // Release pool, the VideoFrame should still be alive.
  pool = nullptr;

  // Check that we can read the frame:
  ASSERT_TRUE(frame);
  ASSERT_EQ(kFormat, frame->format());
  ASSERT_EQ(kSize, frame->coded_size());
  ASSERT_EQ(kSize, frame->visible_rect().size());
  ASSERT_EQ(kSize, frame->natural_size());
  ASSERT_EQ(kArbitraryColorSpace1, frame->ColorSpace());
  ASSERT_TRUE(PlanesAreFilledWithValues(*frame, kValues1));

  // Check that we can write to the frame:
  constexpr uint8_t kValues2[3] = {0x44, 0x55, 0x66};
  media::FillYUV(frame.get(), kValues2[0], kValues2[1], kValues2[2]);
  frame->set_color_space(kArbitraryColorSpace2);

  // Read again, this time expect new values:
  ASSERT_EQ(kArbitraryColorSpace2, frame->ColorSpace());
  ASSERT_TRUE(PlanesAreFilledWithValues(*frame, kValues2));

  // Release the frame to force the dtor to run now:
  frame = nullptr;
}

}  // namespace
}  // namespace viz
