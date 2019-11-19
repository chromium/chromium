// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/interprocess_frame_pool.h"

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

TEST(InterprocessFramePoolTest, FramesConfiguredCorrectly) {
  InterprocessFramePool pool(1);
  const scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  ASSERT_EQ(kSize, frame->coded_size());
  ASSERT_EQ(gfx::Rect(kSize), frame->visible_rect());
  ASSERT_EQ(kSize, frame->natural_size());
  ASSERT_TRUE(frame->IsMappable());
}

TEST(InterprocessFramePool, UsesAvailableBuffersIfPossible) {
  constexpr gfx::Size kSmallerSize =
      gfx::Size(kSize.width() / 2, kSize.height() / 2);
  constexpr gfx::Size kBiggerSize =
      gfx::Size(kSize.width() * 2, kSize.height() * 2);

  InterprocessFramePool pool(1);

  // Reserve a frame of baseline size and then free it to return it to the pool.
  scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  size_t baseline_bytes_allocated;
  {
    auto handle = pool.CloneHandleForDelivery(frame.get());
    ExpectValidHandleForDelivery(handle);
    baseline_bytes_allocated = handle.GetSize();
  }
  frame = nullptr;  // Returns frame to pool.

  // Now, attempt to reserve a smaller-sized frame. Expect that the same buffer
  // is backing the frame because it's large enough.
  frame = pool.ReserveVideoFrame(kFormat, kSmallerSize);
  ASSERT_TRUE(frame);
  {
    auto handle = pool.CloneHandleForDelivery(frame.get());
    ExpectValidHandleForDelivery(handle);
    EXPECT_EQ(baseline_bytes_allocated, handle.GetSize());
  }
  frame = nullptr;  // Returns frame to pool.

  // Now, attempt to reserve a larger-than-baseline-sized frame. Expect that a
  // different buffer is backing the frame because a larger one had to be
  // allocated.
  frame = pool.ReserveVideoFrame(kFormat, kBiggerSize);
  ASSERT_TRUE(frame);
  size_t larger_buffer_bytes_allocated;
  {
    auto handle = pool.CloneHandleForDelivery(frame.get());
    ExpectValidHandleForDelivery(handle);
    larger_buffer_bytes_allocated = handle.GetSize();
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
      auto handle = pool.CloneHandleForDelivery(frame.get());
      ExpectValidHandleForDelivery(handle);
      EXPECT_EQ(larger_buffer_bytes_allocated, handle.GetSize());
    }
    frame = nullptr;  // Returns frame to pool.
  }
}

TEST(InterprocessFramePoolTest, ReachesCapacityLimit) {
  InterprocessFramePool pool(2);
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

// Returns true iff each plane of the given |frame| is filled with
// |values[plane]|.
bool PlanesAreFilledWithValues(const VideoFrame& frame, const uint8_t* values) {
  static_assert(VideoFrame::kUPlane == (VideoFrame::kYPlane + 1) &&
                    VideoFrame::kVPlane == (VideoFrame::kUPlane + 1),
                "enum values changed, will break code below");
  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
    const uint8_t expected_value = values[plane - VideoFrame::kYPlane];
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

TEST(InterprocessFramePoolTest, ResurrectFrameThatIsNotInUse) {
  InterprocessFramePool pool(2);
  const gfx::ColorSpace kArbitraryColorSpace = gfx::ColorSpace::CreateREC709();

  // Reserve a frame, populate it, mark it, and release it.
  scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  const uint8_t kValues[3] = {0x11, 0x22, 0x33};
  media::FillYUV(frame.get(), kValues[0], kValues[1], kValues[2]);
  frame->set_color_space(kArbitraryColorSpace);
  pool.MarkFrame(*frame);
  frame = nullptr;  // Returns frame to pool.

  ASSERT_TRUE(pool.HasMarkedFrameWithSize(kSize));
  const gfx::Size kDifferentSize(kSize.width() - 2, kSize.height() + 2);
  ASSERT_FALSE(pool.HasMarkedFrameWithSize(kDifferentSize));

  // Resurrect the frame and expect it to still have the same content, size,
  // format, and color space. Release and repeat that a few times.
  for (int i = 0; i < 3; ++i) {
    frame = pool.ResurrectOrDuplicateContentFromMarkedFrame();
    ASSERT_TRUE(frame);
    ASSERT_EQ(kFormat, frame->format());
    ASSERT_EQ(kSize, frame->coded_size());
    ASSERT_EQ(kSize, frame->visible_rect().size());
    ASSERT_EQ(kSize, frame->natural_size());
    ASSERT_EQ(kArbitraryColorSpace, frame->ColorSpace());
    ASSERT_TRUE(PlanesAreFilledWithValues(*frame, kValues));
    frame = nullptr;
  }
}

TEST(InterprocessFramePoolTest, ResurrectContentFromFrameThatIsStillInUse) {
  InterprocessFramePool pool(2);
  const gfx::ColorSpace kArbitraryColorSpace = gfx::ColorSpace::CreateREC709();

  // Reserve a frame, populate it, mark it, and hold on to it.
  scoped_refptr<media::VideoFrame> frame =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame);
  const uint8_t kValues[3] = {0x11, 0x22, 0x33};
  media::FillYUV(frame.get(), kValues[0], kValues[1], kValues[2]);
  frame->set_color_space(kArbitraryColorSpace);
  pool.MarkFrame(*frame);

  ASSERT_TRUE(pool.HasMarkedFrameWithSize(kSize));
  const gfx::Size kDifferentSize(kSize.width() - 2, kSize.height() + 2);
  ASSERT_FALSE(pool.HasMarkedFrameWithSize(kDifferentSize));

  scoped_refptr<media::VideoFrame> frame2 =
      pool.ResurrectOrDuplicateContentFromMarkedFrame();
  ASSERT_TRUE(frame2);
  ASSERT_NE(frame, frame2);
  ASSERT_NE(frame->data(0), frame2->data(0));
  ASSERT_EQ(kFormat, frame2->format());
  ASSERT_EQ(kSize, frame2->coded_size());
  ASSERT_EQ(kSize, frame2->visible_rect().size());
  ASSERT_EQ(kSize, frame2->natural_size());
  ASSERT_EQ(kArbitraryColorSpace, frame2->ColorSpace());
  ASSERT_TRUE(PlanesAreFilledWithValues(*frame2, kValues));
}

TEST(InterprocessFramePoolTest, ResurrectWhenAtCapacity) {
  InterprocessFramePool pool(2);
  const gfx::ColorSpace kArbitraryColorSpace = gfx::ColorSpace::CreateREC709();

  // Reserve two frames and hold on to them
  scoped_refptr<media::VideoFrame> frame1 =
      pool.ReserveVideoFrame(kFormat, kSize);
  scoped_refptr<media::VideoFrame> frame2 =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame1);
  ASSERT_TRUE(frame2);
  // Fill and mark one of them
  const uint8_t kValues[3] = {0x11, 0x22, 0x33};
  media::FillYUV(frame1.get(), kValues[0], kValues[1], kValues[2]);
  frame1->set_color_space(kArbitraryColorSpace);
  pool.MarkFrame(*frame1);

  // Attempt to resurrect. This should fail, because the pool is already at
  // capacity.
  scoped_refptr<media::VideoFrame> frame3 =
      pool.ResurrectOrDuplicateContentFromMarkedFrame();
  ASSERT_FALSE(frame3);

  // Release the first frame
  frame1 = nullptr;

  // Now, resurrecting should work again.
  frame3 = pool.ResurrectOrDuplicateContentFromMarkedFrame();
  ASSERT_TRUE(frame3);
  ASSERT_EQ(kFormat, frame3->format());
  ASSERT_EQ(kSize, frame3->coded_size());
  ASSERT_EQ(kArbitraryColorSpace, frame3->ColorSpace());
  ASSERT_TRUE(PlanesAreFilledWithValues(*frame3, kValues));
}

TEST(InterprocessFramePoolTest, ResurrectWhenNoFrameMarked) {
  InterprocessFramePool pool(2);

  // Attempt to resurrect before any frame was ever reserved.
  scoped_refptr<media::VideoFrame> frame =
      pool.ResurrectOrDuplicateContentFromMarkedFrame();
  ASSERT_FALSE(frame);

  // Reserve a frame and release it without marking it.
  scoped_refptr<media::VideoFrame> frame2 =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame2);
  frame2 = nullptr;  // Returns frame to pool.

  // Attempt to resurrect. This should fail, because no frame was marked.
  scoped_refptr<media::VideoFrame> frame3 =
      pool.ResurrectOrDuplicateContentFromMarkedFrame();
  ASSERT_FALSE(frame3);
}

TEST(InterprocessFramePoolTest, FrameMarkingIsLostWhenBufferIsReallocated) {
  InterprocessFramePool pool(2);

  // Reserve enough frames to hit capacity.
  scoped_refptr<media::VideoFrame> frame1 =
      pool.ReserveVideoFrame(kFormat, kSize);
  scoped_refptr<media::VideoFrame> frame2 =
      pool.ReserveVideoFrame(kFormat, kSize);
  ASSERT_TRUE(frame1);
  ASSERT_TRUE(frame2);

  // Mark one of them
  pool.MarkFrame(*frame1);
  ASSERT_TRUE(pool.HasMarkedFrameWithSize(kSize));

  // Release all frames
  frame1 = nullptr;
  frame2 = nullptr;

  // Reserve all frames again but this time request a bigger size.
  // This should lead to all buffers being reallocated and the marking being
  // lost.
  gfx::Size kBiggerSize(kSize.width() + 2, kSize.height() + 2);
  frame1 = pool.ReserveVideoFrame(kFormat, kBiggerSize);
  frame2 = pool.ReserveVideoFrame(kFormat, kBiggerSize);
  ASSERT_TRUE(frame1);
  ASSERT_TRUE(frame2);

  ASSERT_FALSE(pool.HasMarkedFrameWithSize(kSize));
  ASSERT_FALSE(pool.HasMarkedFrameWithSize(kBiggerSize));
}

TEST(InterprocessFramePoolTest, ReportsCorrectUtilization) {
  InterprocessFramePool pool(2);
  ASSERT_EQ(0.0f, pool.GetUtilization());

  // Run through a typical sequence twice: Once for normal frame reservation,
  // and the second time for a resurrected frame.
  for (int i = 0; i < 2; ++i) {
    // Reserve the frame and expect 1/2 the pool to be utilized.
    scoped_refptr<media::VideoFrame> frame =
        (i == 0) ? pool.ReserveVideoFrame(kFormat, kSize)
                 : pool.ResurrectOrDuplicateContentFromMarkedFrame();
    ASSERT_TRUE(frame);
    ASSERT_EQ(0.5f, pool.GetUtilization());

    // Signal that the frame will be delivered. This should not change the
    // utilization.
    {
      auto handle = pool.CloneHandleForDelivery(frame.get());
      ExpectValidHandleForDelivery(handle);
    }
    ASSERT_EQ(0.5f, pool.GetUtilization());

    // Mark the frame for later resurrection.
    pool.MarkFrame(*frame);

    // Finally, release the frame to indicate it has been delivered and is no
    // longer in-use by downstream consumers. This should cause the utilization
    // to go back down to zero.
    frame = nullptr;
    ASSERT_EQ(0.0f, pool.GetUtilization());
  }
}

}  // namespace
}  // namespace viz
