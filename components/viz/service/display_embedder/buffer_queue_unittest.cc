// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/buffer_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Contains;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Return;

namespace viz {

namespace {

constexpr SharedImageFormat kBufferQueueFormat = SinglePlaneFormat::kRGBA_8888;
constexpr gfx::ColorSpace kBufferQueueColorSpace =
    gfx::ColorSpace::CreateSRGB();

}  // namespace

#if BUILDFLAG(IS_WIN)
const gpu::SurfaceHandle kFakeSurfaceHandle =
    reinterpret_cast<gpu::SurfaceHandle>(1);
#else
const gpu::SurfaceHandle kFakeSurfaceHandle = 1;
#endif

class BufferQueueTest : public ::testing::Test {
 public:
  BufferQueueTest() = default;

  void SetUp() override {
    skia_output_surface_ = FakeSkiaOutputSurface::Create3d();
    buffer_queue_ = std::make_unique<BufferQueue>(skia_output_surface_.get(),
                                                  kFakeSurfaceHandle, 3);
  }

  void TearDown() override { buffer_queue_.reset(); }

  gpu::Mailbox current_buffer() {
    return buffer_queue_->current_buffer_
               ? buffer_queue_->current_buffer_->mailbox
               : gpu::Mailbox();
  }
  const base::circular_deque<std::unique_ptr<BufferQueue::AllocatedBuffer>>&
  available_buffers() {
    return buffer_queue_->available_buffers_;
  }
  base::circular_deque<std::unique_ptr<BufferQueue::AllocatedBuffer>>&
  in_flight_buffers() {
    return buffer_queue_->in_flight_buffers_;
  }

  const BufferQueue::AllocatedBuffer* displayed_frame() {
    return buffer_queue_->displayed_buffer_.get();
  }
  const BufferQueue::AllocatedBuffer* current_frame() {
    return buffer_queue_->current_buffer_.get();
  }
  const gfx::Size size() { return buffer_queue_->size_; }

  int CountBuffers() {
    int n = available_buffers().size() + in_flight_buffers().size() +
            (displayed_frame() ? 1 : 0);
    if (!current_buffer().IsZero())
      n++;
    return n;
  }

  // Check that each buffer is unique if present.
  bool CheckUnique() {
    std::set<gpu::Mailbox> buffers;
    if (!InsertUnique(&buffers, current_buffer())) {
      return false;
    }
    if (displayed_frame() &&
        !InsertUnique(&buffers, displayed_frame()->mailbox)) {
      return false;
    }
    for (auto& buffer : available_buffers()) {
      if (!InsertUnique(&buffers, buffer->mailbox)) {
        return false;
      }
    }
    for (auto& buffer : in_flight_buffers()) {
      if (buffer && !InsertUnique(&buffers, buffer->mailbox)) {
        return false;
      }
    }
    return true;
  }

  gpu::Mailbox SendDamagedFrame(const gfx::Rect& damage) {
    // We don't care about the GL-level implementation here, just how it uses
    // damage rects.
    auto mailbox = buffer_queue_->GetCurrentBuffer();
    buffer_queue_->SwapBuffers(damage);
    buffer_queue_->SwapBuffersComplete();
    return mailbox;
  }

  void SendFullFrame() { SendDamagedFrame(gfx::Rect(buffer_queue_->size_)); }

 protected:
  bool InsertUnique(std::set<gpu::Mailbox>* set, gpu::Mailbox value) {
    if (value.IsZero())
      return true;
    if (set->find(value) != set->end())
      return false;
    set->insert(value);
    return true;
  }

  std::unique_ptr<FakeSkiaOutputSurface> skia_output_surface_;
  std::unique_ptr<BufferQueue> buffer_queue_;
};

const gfx::Size screen_size = gfx::Size(30, 30);
const gfx::Rect screen_rect = gfx::Rect(screen_size);
const gfx::Rect small_damage = gfx::Rect(gfx::Size(10, 10));
const gfx::Rect large_damage = gfx::Rect(gfx::Size(20, 20));
const gfx::Rect overlapping_damage = gfx::Rect(gfx::Size(5, 20));

class MockedSkiaOutputSurface : public FakeSkiaOutputSurface {
 public:
  MockedSkiaOutputSurface() : FakeSkiaOutputSurface(nullptr) {}
  MOCK_METHOD7(CreateSharedImage,
               gpu::Mailbox(SharedImageFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            RenderPassAlphaType alpha_type,
                            gpu::SharedImageUsageSet usage,
                            std::string_view debug_label,
                            gpu::SurfaceHandle surface_handle));
  MOCK_METHOD1(DestroySharedImage, void(const gpu::Mailbox& mailbox));
};

TEST(BufferQueueStandaloneTest, BufferCreationAndDestruction) {
  auto mock_skia_output_surface = std::make_unique<MockedSkiaOutputSurface>();
  std::unique_ptr<BufferQueue> buffer_queue = std::make_unique<BufferQueue>(
      mock_skia_output_surface.get(), kFakeSurfaceHandle, 1);

  const gpu::Mailbox expected_mailbox = gpu::Mailbox::Generate();
  {
    testing::InSequence dummy;
    EXPECT_CALL(*mock_skia_output_surface,
                CreateSharedImage(_, _, _, _,

                                  gpu::SHARED_IMAGE_USAGE_SCANOUT |
                                      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                      gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE,
                                  _, _))
        .WillOnce(Return(expected_mailbox));
    EXPECT_CALL(*mock_skia_output_surface,
                DestroySharedImage(expected_mailbox));
  }

  EXPECT_TRUE(buffer_queue->Reshape(screen_size, kBufferQueueColorSpace,
                                    kBufferQueueFormat));
  EXPECT_EQ(expected_mailbox, buffer_queue->GetCurrentBuffer());
}

TEST_F(BufferQueueTest, PartialSwapReuse) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(large_damage);
  // Verify that the damage has propagated.
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), large_damage);
}

TEST_F(BufferQueueTest, PartialSwapFullFrame) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendFullFrame();
  SendFullFrame();
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), screen_rect);
}

// Make sure that each time we swap buffers, the damage gets propagated to the
// previously swapped buffers.
TEST_F(BufferQueueTest, PartialSwapWithTripleBuffering) {
  EXPECT_EQ(0, CountBuffers());
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  EXPECT_EQ(3, CountBuffers());

  SendFullFrame();
  SendFullFrame();
  // Let's triple buffer.
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  buffer_queue_->SwapBuffers(small_damage);
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  // The whole buffer needs to be redrawn since it's a newly allocated buffer
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), screen_rect);

  SendDamagedFrame(overlapping_damage);
  // The next buffer should include damage from |overlapping_damage| and
  // |small_damage|.
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  const auto current_buffer_damage = buffer_queue_->CurrentBufferDamage();
  EXPECT_TRUE(current_buffer_damage.Contains(overlapping_damage));
  EXPECT_TRUE(current_buffer_damage.Contains(small_damage));

  // Let's make sure the damage is not trivially the whole screen.
  EXPECT_NE(current_buffer_damage, screen_rect);
}

TEST_F(BufferQueueTest, PartialSwapOverlapping) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));

  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(overlapping_damage);
  // Expect small_damage UNION overlapping_damage
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), gfx::Rect(0, 0, 10, 20));
}

TEST_F(BufferQueueTest, MultipleGetCurrentBufferCalls) {
  // It is not valid to call GetCurrentBuffer without having set an initial
  // size via Reshape.
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  // Check that multiple bind calls do not create or change buffers.
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  gpu::Mailbox fb = buffer_queue_->GetCurrentBuffer();
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_EQ(fb, buffer_queue_->GetCurrentBuffer());
}

TEST_F(BufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.

  // Create a BufferQueue with only 2 buffers.
  buffer_queue_ = std::make_unique<BufferQueue>(skia_output_surface_.get(),
                                                kFakeSurfaceHandle, 2);

  EXPECT_EQ(0, CountBuffers());
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  EXPECT_EQ(2, CountBuffers());

  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_FALSE(displayed_frame());

  buffer_queue_->SwapBuffers(screen_rect);

  EXPECT_EQ(1U, in_flight_buffers().size());
  buffer_queue_->SwapBuffersComplete();

  EXPECT_EQ(0U, in_flight_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_TRUE(CheckUnique());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_EQ(0U, in_flight_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  buffer_queue_->SwapBuffers(screen_rect);
  EXPECT_TRUE(CheckUnique());
  EXPECT_EQ(1U, in_flight_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  buffer_queue_->SwapBuffersComplete();
  EXPECT_TRUE(CheckUnique());
  EXPECT_EQ(0U, in_flight_buffers().size());
  EXPECT_EQ(1U, available_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_TRUE(CheckUnique());
  EXPECT_TRUE(available_buffers().empty());
}

TEST_F(BufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.
  EXPECT_EQ(0, CountBuffers());
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  EXPECT_EQ(3, CountBuffers());

  // This bit is the same sequence tested in the doublebuffering case.
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_FALSE(displayed_frame());
  buffer_queue_->SwapBuffers(screen_rect);
  buffer_queue_->SwapBuffersComplete();
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  buffer_queue_->SwapBuffers(screen_rect);

  EXPECT_TRUE(CheckUnique());
  EXPECT_EQ(1U, in_flight_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_TRUE(CheckUnique());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_EQ(1U, in_flight_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  buffer_queue_->SwapBuffersComplete();
  EXPECT_TRUE(CheckUnique());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_EQ(0U, in_flight_buffers().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_EQ(1U, available_buffers().size());
}

TEST_F(BufferQueueTest, CheckEmptySwap) {
  // It is not valid to call GetCurrentBuffer without having set an initial
  // size via Reshape.
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  // Check empty swap flow, in which the damage is empty.
  gpu::Mailbox mailbox = buffer_queue_->GetCurrentBuffer();
  EXPECT_FALSE(mailbox.IsZero());
  EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
  EXPECT_FALSE(displayed_frame());

  buffer_queue_->SwapBuffers(gfx::Rect());
  // Make sure we won't be drawing to the buffer we just sent for scanout.
  gpu::Mailbox new_mailbox = buffer_queue_->GetCurrentBuffer();
  EXPECT_FALSE(new_mailbox.IsZero());
  EXPECT_NE(mailbox, new_mailbox);

  EXPECT_EQ(1U, in_flight_buffers().size());
  buffer_queue_->SwapBuffersComplete();

  buffer_queue_->SwapBuffers(gfx::Rect());
  // Test SwapBuffers() without calling GetCurrentBuffer().
  buffer_queue_->SwapBuffers(gfx::Rect());
  EXPECT_EQ(2U, in_flight_buffers().size());

  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(1U, in_flight_buffers().size());

  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(0U, in_flight_buffers().size());
}

TEST_F(BufferQueueTest, CheckCorrectBufferOrdering) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
    buffer_queue_->SwapBuffers(screen_rect);
  }

  EXPECT_EQ(kSwapCount, in_flight_buffers().size());
  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::Mailbox next_mailbox = in_flight_buffers().front()->mailbox;
    buffer_queue_->SwapBuffersComplete();
    EXPECT_EQ(displayed_frame()->mailbox, next_mailbox);
  }
}

TEST_F(BufferQueueTest, ReshapeWithInFlightBuffers) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
    buffer_queue_->SwapBuffers(screen_rect);
  }

  EXPECT_TRUE(buffer_queue_->Reshape(gfx::Size(10, 20), kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  EXPECT_EQ(3u, in_flight_buffers().size());
  EXPECT_EQ(3u, available_buffers().size());
  // The inflight images are destroyed, but the buffers are still around for
  // now, in addition to the newly created buffers.
  EXPECT_EQ(6, CountBuffers());

  for (size_t i = 0; i < kSwapCount; ++i) {
    buffer_queue_->SwapBuffersComplete();
    EXPECT_FALSE(displayed_frame());
  }

  // The dummy buffers left should be discarded.
  EXPECT_EQ(3u, available_buffers().size());
}

TEST_F(BufferQueueTest, SwapAfterReshape) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
    buffer_queue_->SwapBuffers(screen_rect);
  }

  EXPECT_TRUE(buffer_queue_->Reshape(gfx::Size(10, 20), kBufferQueueColorSpace,
                                     kBufferQueueFormat));

  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
    buffer_queue_->SwapBuffers(screen_rect);
  }
  EXPECT_EQ(2 * kSwapCount, in_flight_buffers().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    buffer_queue_->SwapBuffersComplete();
    EXPECT_FALSE(displayed_frame());
  }

  EXPECT_TRUE(CheckUnique());

  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::Mailbox next_mailbox = in_flight_buffers().front()->mailbox;
    buffer_queue_->SwapBuffersComplete();
    EXPECT_EQ(displayed_frame()->mailbox, next_mailbox);
    EXPECT_TRUE(displayed_frame());
  }

  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_FALSE(buffer_queue_->GetCurrentBuffer().IsZero());
    buffer_queue_->SwapBuffers(screen_rect);
    buffer_queue_->SwapBuffersComplete();
  }
}

TEST_F(BufferQueueTest, SwapBuffersSkipped) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);

  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);

  auto mailbox1 = buffer_queue_->GetCurrentBuffer();
  buffer_queue_->SwapBuffersSkipped(large_damage);
  auto mailbox2 = buffer_queue_->GetCurrentBuffer();

  // SwapBuffersSkipped() didn't advance the current buffer.
  EXPECT_EQ(mailbox1, mailbox2);
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);

  // Swap on the next frame with no additional damage.
  buffer_queue_->SwapBuffers(gfx::Rect());
  buffer_queue_->SwapBuffersComplete();

  // The next frame has the damage from the last SwapBuffersSkipped().
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), large_damage);
}

TEST_F(BufferQueueTest, EnsureMinNumberOfBuffers) {
  EXPECT_EQ(CountBuffers(), 0);

  buffer_queue_->EnsureMinNumberOfBuffers(4);

  // EnsureMinNumberOfBuffers does nothing before Reshape() is called.
  EXPECT_EQ(CountBuffers(), 0);

  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));

  EXPECT_EQ(CountBuffers(), 4);

  buffer_queue_->EnsureMinNumberOfBuffers(2);

  // EnsureMinNumberOfBuffers will never decrease the number of buffers.
  EXPECT_EQ(CountBuffers(), 4);

  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);

  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);

  buffer_queue_->EnsureMinNumberOfBuffers(5);

  EXPECT_EQ(CountBuffers(), 5);

  // 3 of the existing buffers will be in available_buffers_ already, with
  // existing small_damage.
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);
  SendDamagedFrame(small_damage);
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);
  SendDamagedFrame(small_damage);
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);
  SendDamagedFrame(small_damage);

  // The new buffer will come next with full damage.
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), screen_rect);
  SendDamagedFrame(small_damage);

  // Finally the buffer that was being displayed while we added the 5th buffer
  // will also have small damage.
  EXPECT_EQ(buffer_queue_->CurrentBufferDamage(), small_damage);
  SendDamagedFrame(small_damage);
}

TEST_F(BufferQueueTest, GetLastSwappedBuffer) {
  // No images allocated, so zero-mailbox is returned.
  EXPECT_TRUE(buffer_queue_->GetLastSwappedBuffer().IsZero());

  // After reshape we'll get the last buffer in the queue.
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  gpu::Mailbox last_swapped1 = buffer_queue_->GetLastSwappedBuffer();
  EXPECT_FALSE(last_swapped1.IsZero());

  // The last swapped buffer won't change until calling SwapBuffersComplete.
  gpu::Mailbox mailbox1 = buffer_queue_->GetCurrentBuffer();
  EXPECT_NE(last_swapped1, mailbox1);
  EXPECT_EQ(last_swapped1, buffer_queue_->GetLastSwappedBuffer());
  buffer_queue_->SwapBuffers(screen_rect);
  EXPECT_EQ(last_swapped1, buffer_queue_->GetLastSwappedBuffer());
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox1);

  // Swap another frame. Last swapped only updates after SwapBuffersComplete().
  gpu::Mailbox mailbox2 = buffer_queue_->GetCurrentBuffer();
  buffer_queue_->SwapBuffers(screen_rect);
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox1);
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox2);

  // Swap a third frame. Last swapped only updates after SwapBuffersComplete().
  gpu::Mailbox mailbox3 = buffer_queue_->GetCurrentBuffer();
  // The third mailbox is the first one we got from GetLastSwappedBuffer().
  EXPECT_EQ(mailbox3, last_swapped1);
  buffer_queue_->SwapBuffers(screen_rect);
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox2);
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox3);

  // Empty swap, Last swapped stays the same.
  buffer_queue_->SwapBuffers(gfx::Rect());
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox3);
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox3);

  // Swap a fourth frame. Last swapped only updates after SwapBuffersComplete().
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mailbox1);
  buffer_queue_->SwapBuffers(screen_rect);
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox3);
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mailbox1);
}

TEST_F(BufferQueueTest, RecreateBuffers) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  auto mb1 = SendDamagedFrame(small_damage);
  auto mb2 = SendDamagedFrame(small_damage);
  auto mb3 = SendDamagedFrame(small_damage);
  std::vector<gpu::Mailbox> original_buffers = {mb1, mb2, mb3};

  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb1);
  buffer_queue_->SwapBuffers(small_damage);
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb2);
  buffer_queue_->SwapBuffers(small_damage);

  buffer_queue_->RecreateBuffers();
  buffer_queue_->SwapBuffersComplete();  // mb1

  auto mb4 = buffer_queue_->GetCurrentBuffer();
  EXPECT_THAT(original_buffers, Not(Contains(mb4)));
  buffer_queue_->SwapBuffers(small_damage);

  buffer_queue_->SwapBuffersComplete();  // mb2

  auto mb5 = buffer_queue_->GetCurrentBuffer();
  EXPECT_THAT(original_buffers, Not(Contains(mb5)));
  buffer_queue_->SwapBuffers(small_damage);
  buffer_queue_->SwapBuffersComplete();  // mb4
  buffer_queue_->SwapBuffersComplete();  // mb5

  auto mb6 = SendDamagedFrame(small_damage);
  EXPECT_THAT(original_buffers, Not(Contains(mb6)));

  // New queue of buffers loops.
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb4);
}

TEST_F(BufferQueueTest, DestroyBuffers) {
  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  auto mb1 = SendDamagedFrame(small_damage);
  auto mb2 = SendDamagedFrame(small_damage);
  auto mb3 = SendDamagedFrame(small_damage);
  std::vector<gpu::Mailbox> original_buffers = {mb1, mb2, mb3};

  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb1);
  buffer_queue_->SwapBuffers(small_damage);
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb2);
  buffer_queue_->SwapBuffers(small_damage);

  buffer_queue_->DestroyBuffers();

  // All buffers are destroyed, and GetLastSwappedBuffer should not recreate
  // them.
  EXPECT_TRUE(buffer_queue_->GetLastSwappedBuffer().IsZero());
  buffer_queue_->SwapBuffersComplete();  // mb1
  EXPECT_TRUE(buffer_queue_->GetLastSwappedBuffer().IsZero());
  // Reshape should not reallocate buffers.
  EXPECT_TRUE(buffer_queue_->Reshape(gfx::Size(20, 20), kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  EXPECT_TRUE(buffer_queue_->GetLastSwappedBuffer().IsZero());

  // GetCurrentBuffer should create the new buffers.
  auto mb4 = buffer_queue_->GetCurrentBuffer();
  EXPECT_FALSE(mb4.IsZero());
  EXPECT_THAT(original_buffers, Not(Contains(mb4)));
  EXPECT_FALSE(buffer_queue_->GetLastSwappedBuffer().IsZero());
}

TEST_F(BufferQueueTest, SetPurgeable) {
  testing::MockFunction<void(const gpu::Mailbox&, bool)> mock;
  skia_output_surface_->SetSharedImagePurgeableCallback(
      base::BindLambdaForTesting(mock.AsStdFunction()));

  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));
  auto mb1 = SendDamagedFrame(small_damage);
  auto mb2 = SendDamagedFrame(small_damage);
  auto mb3 = SendDamagedFrame(small_damage);
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mb3);

  // Queue up `mb1` and `mb2` so they are in flight. `mb3` is still the last
  // swapped buffer.
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb1);
  buffer_queue_->SwapBuffers(small_damage);
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb2);
  buffer_queue_->SwapBuffers(small_damage);
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mb3);

  // Set buffers as purgeable.
  buffer_queue_->SetBuffersPurgeable();

  // When the next swap finishes `mb3` is available and gets marked purgeable.
  EXPECT_CALL(mock, Call(mb3, true));
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mb1);

  // When the next swap finishes `mb1` is available and gets marked purgeable.
  EXPECT_CALL(mock, Call(mb1, true));
  buffer_queue_->SwapBuffersComplete();
  EXPECT_EQ(buffer_queue_->GetLastSwappedBuffer(), mb2);

  // `mb2` is last swapped buffer now and there are no pending swaps. Push an
  // empty swap and complete that so `mb2` is available.
  EXPECT_CALL(mock, Call(mb2, true));
  buffer_queue_->SwapBuffers(small_damage);
  buffer_queue_->SwapBuffersComplete();

  // The next non-delegated draw will get a primary plane buffer. This will
  // cause all three buffers to be marked as not purgeable anymore.
  EXPECT_CALL(mock, Call(mb3, false));
  EXPECT_CALL(mock, Call(mb1, false));
  EXPECT_CALL(mock, Call(mb2, false));
  EXPECT_EQ(buffer_queue_->GetCurrentBuffer(), mb3);

  // Reset callback since it points to stack allocated mock.
  skia_output_surface_->SetSharedImagePurgeableCallback({});
}

TEST_F(BufferQueueTest, SetPurgeableThenReshape) {
  testing::MockFunction<void(const gpu::Mailbox&, bool)> mock;
  skia_output_surface_->SetSharedImagePurgeableCallback(
      base::BindLambdaForTesting(mock.AsStdFunction()));

  // This test will reshape before any buffers can be marked as purgeable.
  EXPECT_CALL(mock, Call(testing::_, testing::_)).Times(0);

  EXPECT_TRUE(buffer_queue_->Reshape(screen_size, kBufferQueueColorSpace,
                                     kBufferQueueFormat));

  // Swap three buffers. First buffer swap completes so there is one displayed
  // buffer and two in flight buffers.
  buffer_queue_->GetCurrentBuffer();
  buffer_queue_->SwapBuffers(small_damage);
  buffer_queue_->GetCurrentBuffer();
  buffer_queue_->SwapBuffers(small_damage);
  buffer_queue_->SwapBuffersComplete();
  buffer_queue_->GetCurrentBuffer();
  buffer_queue_->SwapBuffers(small_damage);
  EXPECT_FALSE(buffer_queue_->GetLastSwappedBuffer().IsZero());

  // Set the buffers as purgeable before the next swap buffers complete and then
  // immediately reshape. The reshape will cause buffers to be deleted but not
  // recreated at the new size until they will be used.
  buffer_queue_->SetBuffersPurgeable();
  EXPECT_TRUE(buffer_queue_->Reshape(gfx::Size(1, 1), kBufferQueueColorSpace,
                                     kBufferQueueFormat));

  // Complete the last two swaps. Since the reshape deleted all the buffers
  // they will not be marked as purgeable.
  buffer_queue_->SwapBuffersComplete();
  buffer_queue_->SwapBuffersComplete();

  // Reset callback since it points to stack allocated mock.
  skia_output_surface_->SetSharedImagePurgeableCallback({});
}

}  // namespace viz
