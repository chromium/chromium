// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/buffer_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_snapshot.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Return;

namespace viz {

namespace {

constexpr gfx::BufferFormat kBufferQueueFormat = gfx::BufferFormat::RGBA_8888;
constexpr gfx::ColorSpace kBufferQueueColorSpace =
    gfx::ColorSpace::CreateSRGB();

}  // namespace

class FakeSyncTokenProvider : public BufferQueue::SyncTokenProvider {
 public:
  ~FakeSyncTokenProvider() override = default;
  gpu::SyncToken GenSyncToken() override { return gpu::SyncToken(); }
};

class StubGpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  explicit StubGpuMemoryBufferImpl(size_t* set_color_space_count)
      : set_color_space_count_(set_color_space_count) {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override { return false; }
  void* memory(size_t plane) override { return nullptr; }
  void Unmap() override {}
  gfx::Size GetSize() const override { return gfx::Size(); }
  gfx::BufferFormat GetFormat() const override { return kBufferQueueFormat; }
  int stride(size_t plane) const override { return 0; }
  gfx::GpuMemoryBufferType GetType() const override {
    return gfx::EMPTY_BUFFER;
  }
  gfx::GpuMemoryBufferId GetId() const override {
    return gfx::GpuMemoryBufferId(0);
  }
  void SetColorSpace(const gfx::ColorSpace& color_space) override {
    *set_color_space_count_ += 1;
  }
  gfx::GpuMemoryBufferHandle CloneHandle() const override {
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }
  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {}

  size_t* set_color_space_count_;
};

class StubGpuMemoryBufferManager : public TestGpuMemoryBufferManager {
 public:
  StubGpuMemoryBufferManager() : allocate_succeeds_(true) {}

  size_t set_color_space_count() const { return set_color_space_count_; }

  void set_allocate_succeeds(bool value) { allocate_succeeds_ = value; }

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override {
    if (surface_handle == gpu::kNullSurfaceHandle) {
      return TestGpuMemoryBufferManager::CreateGpuMemoryBuffer(
          size, format, usage, surface_handle);
    }
    if (allocate_succeeds_)
      return base::WrapUnique<gfx::GpuMemoryBuffer>(
          new StubGpuMemoryBufferImpl(&set_color_space_count_));
    return nullptr;
  }

 private:
  bool allocate_succeeds_;
  size_t set_color_space_count_ = 0;
};

#if defined(OS_WIN)
const gpu::SurfaceHandle kFakeSurfaceHandle =
    reinterpret_cast<gpu::SurfaceHandle>(1);
#else
const gpu::SurfaceHandle kFakeSurfaceHandle = 1;
#endif

class BufferQueueTest : public ::testing::Test,
                        public BufferQueue::SyncTokenProvider {
 public:
  BufferQueueTest() = default;

  void SetUp() override {
    InitWithSharedImageInterface(std::make_unique<TestSharedImageInterface>());
  }

  void TearDown() override { output_surface_.reset(); }

  gpu::SyncToken GenSyncToken() override { return gpu::SyncToken(); }

  void InitWithSharedImageInterface(
      std::unique_ptr<TestSharedImageInterface> sii) {
    context_provider_ = TestContextProvider::Create(std::move(sii));
    context_provider_->BindToCurrentThread();
    output_surface_.reset(new BufferQueue(
        context_provider_->SharedImageInterface(), kFakeSurfaceHandle));
    output_surface_->SetSyncTokenProvider(this);
  }

  gpu::Mailbox current_surface() {
    return output_surface_->current_surface_
               ? output_surface_->current_surface_->mailbox
               : gpu::Mailbox();
  }
  const std::vector<std::unique_ptr<BufferQueue::AllocatedSurface>>&
  available_surfaces() {
    return output_surface_->available_surfaces_;
  }
  base::circular_deque<std::unique_ptr<BufferQueue::AllocatedSurface>>&
  in_flight_surfaces() {
    return output_surface_->in_flight_surfaces_;
  }

  const BufferQueue::AllocatedSurface* displayed_frame() {
    return output_surface_->displayed_surface_.get();
  }
  const BufferQueue::AllocatedSurface* current_frame() {
    return output_surface_->current_surface_.get();
  }
  const BufferQueue::AllocatedSurface* next_frame() {
    return output_surface_->available_surfaces_.back().get();
  }
  const gfx::Size size() { return output_surface_->size_; }

  int CountBuffers() {
    int n = available_surfaces().size() + in_flight_surfaces().size() +
            (displayed_frame() ? 1 : 0);
    if (!current_surface().IsZero())
      n++;
    return n;
  }

  // Check that each buffer is unique if present.
  void CheckUnique() {
    std::set<gpu::Mailbox> buffers;
    EXPECT_TRUE(InsertUnique(&buffers, current_surface()));
    if (displayed_frame())
      EXPECT_TRUE(InsertUnique(&buffers, displayed_frame()->mailbox));
    for (auto& surface : available_surfaces())
      EXPECT_TRUE(InsertUnique(&buffers, surface->mailbox));
    for (auto& surface : in_flight_surfaces()) {
      if (surface)
        EXPECT_TRUE(InsertUnique(&buffers, surface->mailbox));
    }
  }

  void SwapBuffers(const gfx::Rect& damage) {
    output_surface_->SwapBuffers(damage);
  }

  void SwapBuffers() { SwapBuffers(gfx::Rect(output_surface_->size_)); }

  void SendDamagedFrame(const gfx::Rect& damage) {
    // We don't care about the GL-level implementation here, just how it uses
    // damage rects.
    gpu::SyncToken creation_sync_token;
    EXPECT_FALSE(
        output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
    SwapBuffers(damage);
    output_surface_->PageFlipComplete();
  }

  void SendFullFrame() { SendDamagedFrame(gfx::Rect(output_surface_->size_)); }

 protected:
  bool InsertUnique(std::set<gpu::Mailbox>* set, gpu::Mailbox value) {
    if (value.IsZero())
      return true;
    if (set->find(value) != set->end())
      return false;
    set->insert(value);
    return true;
  }

  scoped_refptr<TestContextProvider> context_provider_;
  std::unique_ptr<BufferQueue> output_surface_;
};

const gfx::Size screen_size = gfx::Size(30, 30);
const gfx::Rect screen_rect = gfx::Rect(screen_size);
const gfx::Rect small_damage = gfx::Rect(gfx::Size(10, 10));
const gfx::Rect large_damage = gfx::Rect(gfx::Size(20, 20));
const gfx::Rect overlapping_damage = gfx::Rect(gfx::Size(5, 20));

class MockedSharedImageInterface : public TestSharedImageInterface {
 public:
  MockedSharedImageInterface() {
    ON_CALL(*this, CreateSharedImage(_, _, _, _, _, _, _))
        .WillByDefault(Return(gpu::Mailbox()));
    // this, &MockedSharedImageInterface::TestCreateSharedImage));
  }
  MOCK_METHOD7(CreateSharedImage,
               gpu::Mailbox(ResourceFormat format,
                            const gfx::Size& size,
                            const gfx::ColorSpace& color_space,
                            GrSurfaceOrigin surface_origin,
                            SkAlphaType alpha_type,
                            uint32_t usage,
                            gpu::SurfaceHandle surface_handle));
  MOCK_METHOD2(UpdateSharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD2(DestroySharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  // Use this to call CreateSharedImage() defined in TestSharedImageInterface.
  gpu::Mailbox TestCreateSharedImage(ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage,
                                     gpu::SurfaceHandle surface_handle) {
    return TestSharedImageInterface::CreateSharedImage(
        format, size, color_space, surface_origin, alpha_type, usage,
        surface_handle);
  }
};

class BufferQueueMockedSharedImageInterfaceTest : public BufferQueueTest {
 public:
  void SetUp() override {
    sii_ = new MockedSharedImageInterface();
    InitWithSharedImageInterface(
        base::WrapUnique<TestSharedImageInterface>(sii_));
  }

 protected:
  MockedSharedImageInterface* sii_;
};

scoped_refptr<TestContextProvider> CreateMockedSharedImageInterfaceProvider(
    MockedSharedImageInterface** sii) {
  std::unique_ptr<MockedSharedImageInterface> owned_sii(
      new MockedSharedImageInterface);
  *sii = owned_sii.get();
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create(std::move(owned_sii));
  context_provider->BindToCurrentThread();
  return context_provider;
}

std::unique_ptr<BufferQueue> CreateBufferQueue(
    gpu::SharedImageInterface* sii,
    BufferQueue::SyncTokenProvider* sync_token_provider) {
  std::unique_ptr<BufferQueue> buffer_queue(
      new BufferQueue(sii, kFakeSurfaceHandle));
  buffer_queue->SetSyncTokenProvider(sync_token_provider);
  return buffer_queue;
}

TEST(BufferQueueStandaloneTest, BufferCreationAndDestruction) {
  MockedSharedImageInterface* sii;
  scoped_refptr<TestContextProvider> context_provider =
      CreateMockedSharedImageInterfaceProvider(&sii);
  std::unique_ptr<BufferQueue::SyncTokenProvider> sync_token_provider(
      new FakeSyncTokenProvider);
  std::unique_ptr<BufferQueue> output_surface = CreateBufferQueue(
      context_provider->SharedImageInterface(), sync_token_provider.get());

  EXPECT_TRUE(output_surface->Reshape(screen_size, kBufferQueueColorSpace,
                                      kBufferQueueFormat));

  const gpu::Mailbox expected_mailbox = gpu::Mailbox::GenerateForSharedImage();
  {
    testing::InSequence dummy;
    EXPECT_CALL(*sii, CreateSharedImage(
                          _, _, _, _, _,
                          gpu::SHARED_IMAGE_USAGE_SCANOUT |
                              gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT,
                          _))
        .WillOnce(Return(expected_mailbox));
    EXPECT_CALL(*sii, DestroySharedImage(_, expected_mailbox));
  }
  gpu::SyncToken creation_sync_token;
  EXPECT_EQ(expected_mailbox,
            output_surface->GetCurrentBuffer(&creation_sync_token));
}

TEST_F(BufferQueueTest, PartialSwapReuse) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(large_damage);
  // Verify that the damage has propagated.
  EXPECT_EQ(next_frame()->damage, large_damage);
}

TEST_F(BufferQueueTest, PartialSwapFullFrame) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendFullFrame();
  SendFullFrame();
  EXPECT_EQ(next_frame()->damage, screen_rect);
}

// Make sure that each time we swap buffers, the damage gets propagated to the
// previously swapped buffers.
TEST_F(BufferQueueTest, PartialSwapWithTripleBuffering) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  SendFullFrame();
  SendFullFrame();
  // Let's triple buffer.
  gpu::SyncToken creation_sync_token;
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  SwapBuffers(small_damage);
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(3, CountBuffers());
  // The whole buffer needs to be redrawn since it's a newly allocated buffer
  EXPECT_EQ(output_surface_->CurrentBufferDamage(), screen_rect);

  SendDamagedFrame(overlapping_damage);
  // The next buffer should include damage from |overlapping_damage| and
  // |small_damage|.
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  const auto current_buffer_damage = output_surface_->CurrentBufferDamage();
  EXPECT_TRUE(current_buffer_damage.Contains(overlapping_damage));
  EXPECT_TRUE(current_buffer_damage.Contains(small_damage));

  // Let's make sure the damage is not trivially the whole screen.
  EXPECT_NE(current_buffer_damage, screen_rect);
}

TEST_F(BufferQueueTest, PartialSwapOverlapping) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));

  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(overlapping_damage);
  EXPECT_EQ(next_frame()->damage, overlapping_damage);
}

TEST_F(BufferQueueTest, MultipleGetCurrentBufferCalls) {
  // It is not valid to call GetCurrentBuffer without having set an initial
  // size via Reshape.
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  // Check that multiple bind calls do not create or change surfaces.
  gpu::SyncToken creation_sync_token;
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(1, CountBuffers());
  gpu::Mailbox fb = current_surface();
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(1, CountBuffers());
  EXPECT_EQ(fb, current_surface());
}

TEST_F(BufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  EXPECT_EQ(0, CountBuffers());
  gpu::SyncToken creation_sync_token;
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(1, CountBuffers());
  EXPECT_FALSE(current_surface().IsZero());
  EXPECT_FALSE(displayed_frame());
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_FALSE(current_surface().IsZero());
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  SwapBuffers();
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  output_surface_->PageFlipComplete();
  CheckUnique();
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_EQ(1U, available_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_TRUE(available_surfaces().empty());
}

TEST_F(BufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));

  // This bit is the same sequence tested in the doublebuffering case.
  gpu::SyncToken creation_sync_token;
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_FALSE(displayed_frame());
  SwapBuffers();
  output_surface_->PageFlipComplete();
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  SwapBuffers();

  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_FALSE(current_surface().IsZero());
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_FALSE(current_surface().IsZero());
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_FALSE(displayed_frame()->mailbox.IsZero());
  EXPECT_EQ(1U, available_surfaces().size());
}

TEST_F(BufferQueueTest, CheckEmptySwap) {
  // It is not valid to call GetCurrentBuffer without having set an initial
  // size via Reshape.
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  EXPECT_EQ(0, CountBuffers());
  gpu::SyncToken creation_sync_token;
  gpu::Mailbox mailbox =
      output_surface_->GetCurrentBuffer(&creation_sync_token);
  EXPECT_FALSE(mailbox.IsZero());
  EXPECT_EQ(1, CountBuffers());
  EXPECT_FALSE(current_surface().IsZero());
  EXPECT_FALSE(displayed_frame());

  SwapBuffers();
  // Make sure we won't be drawing to the buffer we just sent for scanout.
  gpu::SyncToken new_creation_sync_token;
  gpu::Mailbox new_mailbox =
      output_surface_->GetCurrentBuffer(&new_creation_sync_token);
  EXPECT_FALSE(new_mailbox.IsZero());
  EXPECT_NE(mailbox, new_mailbox);

  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();

  // Test swapbuffers without calling BindFramebuffer. DirectRenderer skips
  // BindFramebuffer if not necessary.
  SwapBuffers();
  SwapBuffers();
  EXPECT_EQ(2U, in_flight_surfaces().size());

  output_surface_->PageFlipComplete();
  EXPECT_EQ(1U, in_flight_surfaces().size());

  output_surface_->PageFlipComplete();
  EXPECT_EQ(0U, in_flight_surfaces().size());
}

TEST_F(BufferQueueTest, CheckCorrectBufferOrdering) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::SyncToken creation_sync_token;
    EXPECT_FALSE(
        output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
    SwapBuffers();
  }

  EXPECT_EQ(kSwapCount, in_flight_surfaces().size());
  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::Mailbox next_mailbox = in_flight_surfaces().front()->mailbox;
    output_surface_->PageFlipComplete();
    EXPECT_EQ(displayed_frame()->mailbox, next_mailbox);
  }
}

TEST_F(BufferQueueTest, ReshapeWithInFlightSurfaces) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::SyncToken creation_sync_token;
    EXPECT_FALSE(
        output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
    SwapBuffers();
  }

  EXPECT_TRUE(output_surface_->Reshape(
      gfx::Size(10, 20), kBufferQueueColorSpace, kBufferQueueFormat));
  EXPECT_EQ(3u, in_flight_surfaces().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->PageFlipComplete();
    EXPECT_FALSE(displayed_frame());
  }

  // The dummy surfacess left should be discarded.
  EXPECT_EQ(0u, available_surfaces().size());
}

TEST_F(BufferQueueTest, SwapAfterReshape) {
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::SyncToken creation_sync_token;
    EXPECT_FALSE(
        output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
    SwapBuffers();
  }

  EXPECT_TRUE(output_surface_->Reshape(
      gfx::Size(10, 20), kBufferQueueColorSpace, kBufferQueueFormat));

  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::SyncToken creation_sync_token;
    EXPECT_FALSE(
        output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
    SwapBuffers();
  }
  EXPECT_EQ(2 * kSwapCount, in_flight_surfaces().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->PageFlipComplete();
    EXPECT_FALSE(displayed_frame());
  }

  CheckUnique();

  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::Mailbox next_mailbox = in_flight_surfaces().front()->mailbox;
    output_surface_->PageFlipComplete();
    EXPECT_EQ(displayed_frame()->mailbox, next_mailbox);
    EXPECT_TRUE(displayed_frame());
  }

  for (size_t i = 0; i < kSwapCount; ++i) {
    gpu::SyncToken creation_sync_token;
    EXPECT_FALSE(
        output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
    SwapBuffers();
    output_surface_->PageFlipComplete();
  }
}

TEST_F(BufferQueueMockedSharedImageInterfaceTest, AllocateFails) {
  const gpu::Mailbox expected_mailbox = gpu::Mailbox::GenerateForSharedImage();
  EXPECT_TRUE(output_surface_->Reshape(screen_size, kBufferQueueColorSpace,
                                       kBufferQueueFormat));
  EXPECT_CALL(*sii_, CreateSharedImage(
                         _, _, _, _, _,
                         gpu::SHARED_IMAGE_USAGE_SCANOUT |
                             gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT,
                         _))
      .WillOnce(Return(expected_mailbox))
      .WillOnce(Return(gpu::Mailbox()))  // Fail the next surface allocation.
      .WillRepeatedly(Return(expected_mailbox));

  EXPECT_CALL(*sii_, DestroySharedImage(_, expected_mailbox)).Times(3);

  // Succeed in the two swaps.
  gpu::SyncToken creation_sync_token;
  const gpu::Mailbox result_mailbox =
      output_surface_->GetCurrentBuffer(&creation_sync_token);
  EXPECT_FALSE(result_mailbox.IsZero());
  EXPECT_EQ(result_mailbox, expected_mailbox);
  EXPECT_TRUE(current_frame());
  SwapBuffers(screen_rect);

  EXPECT_TRUE(output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_FALSE(current_frame());
  SwapBuffers(screen_rect);
  EXPECT_FALSE(current_frame());

  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  SwapBuffers(small_damage);

  // Destroy the just-created buffer, and try another swap.
  output_surface_->PageFlipComplete();
  output_surface_->FreeSurface(
      std::move(output_surface_->in_flight_surfaces_.back()), gpu::SyncToken());
  EXPECT_EQ(2u, in_flight_surfaces().size());
  for (auto& surface : in_flight_surfaces())
    EXPECT_FALSE(surface);
  EXPECT_FALSE(
      output_surface_->GetCurrentBuffer(&creation_sync_token).IsZero());
  EXPECT_TRUE(current_frame());
  EXPECT_TRUE(displayed_frame());
  SwapBuffers(small_damage);
}

}  // namespace viz
