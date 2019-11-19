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
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/display/types/display_snapshot.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Return;

namespace viz {

class StubGpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  explicit StubGpuMemoryBufferImpl(size_t* set_color_space_count)
      : set_color_space_count_(set_color_space_count) {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override { return false; }
  void* memory(size_t plane) override { return nullptr; }
  void Unmap() override {}
  gfx::Size GetSize() const override { return gfx::Size(); }
  gfx::BufferFormat GetFormat() const override {
    return gfx::BufferFormat::BGRX_8888;
  }
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
    if (!surface_handle) {
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

const unsigned int kBufferQueueInternalformat = GL_RGBA;
const gfx::BufferFormat kBufferQueueFormat = gfx::BufferFormat::RGBA_8888;

class BufferQueueTest : public ::testing::Test {
 public:
  BufferQueueTest() {}

  void SetUp() override {
    InitWithContext(std::make_unique<TestGLES2Interface>());
  }

  void InitWithContext(std::unique_ptr<TestGLES2Interface> context) {
    context_provider_ = TestContextProvider::Create(std::move(context));
    context_provider_->BindToCurrentThread();
    gpu_memory_buffer_manager_.reset(new StubGpuMemoryBufferManager);
    mock_output_surface_ =
        new BufferQueue(context_provider_->ContextGL(), kBufferQueueFormat,
                        gpu_memory_buffer_manager_.get(), kFakeSurfaceHandle,
                        context_provider_->ContextCapabilities());
    output_surface_.reset(mock_output_surface_);
  }

  unsigned current_surface() {
    return output_surface_->current_surface_
               ? output_surface_->current_surface_->image
               : 0;
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
    if (current_surface())
      n++;
    return n;
  }

  // Check that each buffer is unique if present.
  void CheckUnique() {
    std::set<unsigned> buffers;
    EXPECT_TRUE(InsertUnique(&buffers, current_surface()));
    if (displayed_frame())
      EXPECT_TRUE(InsertUnique(&buffers, displayed_frame()->image));
    for (auto& surface : available_surfaces())
      EXPECT_TRUE(InsertUnique(&buffers, surface->image));
    for (auto& surface : in_flight_surfaces()) {
      if (surface)
        EXPECT_TRUE(InsertUnique(&buffers, surface->image));
    }
  }

  void SwapBuffers(const gfx::Rect& damage) {
    output_surface_->SwapBuffers(damage);
  }

  void SwapBuffers() { SwapBuffers(gfx::Rect(output_surface_->size_)); }

  void SendDamagedFrame(const gfx::Rect& damage) {
    // We don't care about the GL-level implementation here, just how it uses
    // damage rects.
    unsigned stencil;
    EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
    SwapBuffers(damage);
    output_surface_->PageFlipComplete();
  }

  void SendFullFrame() { SendDamagedFrame(gfx::Rect(output_surface_->size_)); }

 protected:
  bool InsertUnique(std::set<unsigned>* set, unsigned value) {
    if (!value)
      return true;
    if (set->find(value) != set->end())
      return false;
    set->insert(value);
    return true;
  }

  scoped_refptr<TestContextProvider> context_provider_;
  std::unique_ptr<StubGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<BufferQueue> output_surface_;
  BufferQueue* mock_output_surface_;
};

namespace {
const gfx::Size screen_size = gfx::Size(30, 30);
const gfx::Rect screen_rect = gfx::Rect(screen_size);
const gfx::Rect small_damage = gfx::Rect(gfx::Size(10, 10));
const gfx::Rect large_damage = gfx::Rect(gfx::Size(20, 20));
const gfx::Rect overlapping_damage = gfx::Rect(gfx::Size(5, 20));

GLuint CreateImageDefault() {
  static GLuint id = 0;
  return ++id;
}

class MockedContext : public TestGLES2Interface {
 public:
  MockedContext() {
    ON_CALL(*this, CreateImageCHROMIUM(_, _, _, _))
        .WillByDefault(testing::InvokeWithoutArgs(&CreateImageDefault));
  }
  MOCK_METHOD2(BindTexture, void(GLenum, GLuint));
  MOCK_METHOD2(BindTexImage2DCHROMIUM, void(GLenum, GLint));
  MOCK_METHOD4(CreateImageCHROMIUM,
               GLuint(ClientBuffer, GLsizei, GLsizei, GLenum));
  MOCK_METHOD1(DestroyImageCHROMIUM, void(GLuint));
};

class BufferQueueMockedContextTest : public BufferQueueTest {
 public:
  void SetUp() override {
    context_ = new MockedContext();
    InitWithContext(base::WrapUnique<TestGLES2Interface>(context_));
  }

 protected:
  MockedContext* context_;
};

scoped_refptr<TestContextProvider> CreateMockedContextProvider(
    MockedContext** context) {
  std::unique_ptr<MockedContext> owned_context(new MockedContext);
  *context = owned_context.get();
  scoped_refptr<TestContextProvider> context_provider =
      TestContextProvider::Create(std::move(owned_context));
  context_provider->BindToCurrentThread();
  return context_provider;
}

std::unique_ptr<BufferQueue> CreateBufferQueue(
    gpu::gles2::GLES2Interface* gl,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gpu::Capabilities& capabilities) {
  std::unique_ptr<BufferQueue> buffer_queue(
      new BufferQueue(gl, kBufferQueueFormat, gpu_memory_buffer_manager,
                      kFakeSurfaceHandle, capabilities));
  return buffer_queue;
}

TEST(BufferQueueStandaloneTest, BufferCreation) {
  MockedContext* context;
  scoped_refptr<TestContextProvider> context_provider =
      CreateMockedContextProvider(&context);
  std::unique_ptr<StubGpuMemoryBufferManager> gpu_memory_buffer_manager(
      new StubGpuMemoryBufferManager);
  std::unique_ptr<BufferQueue> output_surface = CreateBufferQueue(
      context_provider->ContextGL(), gpu_memory_buffer_manager.get(),
      context_provider->ContextCapabilities());

  GLenum target = output_surface->texture_target();

  EXPECT_CALL(*context, BindTexture(target, Ne(0U)));
  EXPECT_CALL(*context, DestroyImageCHROMIUM(1));
  Expectation image =
      EXPECT_CALL(*context,
                  CreateImageCHROMIUM(_, 0, 0, kBufferQueueInternalformat))
          .WillOnce(Return(1));
  Expectation tex = EXPECT_CALL(*context, BindTexture(target, Ne(0U)));
  Expectation bind_tex =
      EXPECT_CALL(*context, BindTexImage2DCHROMIUM(target, 1))
          .After(tex, image);

  unsigned stencil;
  EXPECT_GT(output_surface->GetCurrentBuffer(&stencil), 0u);
}

TEST_F(BufferQueueTest, PartialSwapReuse) {
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(large_damage);
  // Verify that the damage has propagated.
  EXPECT_EQ(next_frame()->damage, large_damage);
}

TEST_F(BufferQueueTest, PartialSwapFullFrame) {
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendFullFrame();
  SendFullFrame();
  EXPECT_EQ(next_frame()->damage, screen_rect);
}

// Make sure that each time we swap buffers, the damage gets propagated to the
// previously swapped buffers.
TEST_F(BufferQueueTest, PartialSwapWithTripleBuffering) {
  unsigned stencil = 0;
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  SendFullFrame();
  SendFullFrame();
  // Let's triple buffer.
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  SwapBuffers(small_damage);
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(3, CountBuffers());
  // The whole buffer needs to be redrawn since it's a newly allocated buffer
  EXPECT_EQ(output_surface_->CurrentBufferDamage(), screen_rect);

  SendDamagedFrame(overlapping_damage);
  // The next buffer should include damage from |overlapping_damage| and
  // |small_damage|.
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  const auto current_buffer_damage = output_surface_->CurrentBufferDamage();
  EXPECT_TRUE(current_buffer_damage.Contains(overlapping_damage));
  EXPECT_TRUE(current_buffer_damage.Contains(small_damage));

  // Let's make sure the damage is not trivially the whole screen.
  EXPECT_NE(current_buffer_damage, screen_rect);
}

TEST_F(BufferQueueTest, PartialSwapOverlapping) {
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));

  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(overlapping_damage);
  EXPECT_EQ(next_frame()->damage, overlapping_damage);
}

TEST_F(BufferQueueTest, MultipleGetCurrentBufferCalls) {
  // Check that multiple bind calls do not create or change surfaces.
  unsigned stencil;
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(1, CountBuffers());
  unsigned int fb = current_surface();
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(1, CountBuffers());
  EXPECT_EQ(fb, current_surface());
}

TEST_F(BufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  EXPECT_EQ(0, CountBuffers());
  unsigned stencil;
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(1, CountBuffers());
  EXPECT_NE(0U, current_surface());
  EXPECT_FALSE(displayed_frame());
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  SwapBuffers();
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  output_surface_->PageFlipComplete();
  CheckUnique();
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_EQ(1U, available_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_TRUE(available_surfaces().empty());
}

TEST_F(BufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));

  // This bit is the same sequence tested in the doublebuffering case.
  unsigned stencil;
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_FALSE(displayed_frame());
  SwapBuffers();
  output_surface_->PageFlipComplete();
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  SwapBuffers();

  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  output_surface_->PageFlipComplete();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame()->texture);
  EXPECT_EQ(1U, available_surfaces().size());
}

TEST_F(BufferQueueTest, CheckEmptySwap) {
  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  EXPECT_EQ(0, CountBuffers());
  unsigned stencil;
  unsigned texture = output_surface_->GetCurrentBuffer(&stencil);
  EXPECT_GT(texture, 0u);
  EXPECT_EQ(1, CountBuffers());
  EXPECT_NE(0U, current_surface());
  EXPECT_FALSE(displayed_frame());

  SwapBuffers();
  // Make sure we won't be drawing to the texture we just sent for scanout.
  unsigned new_texture = output_surface_->GetCurrentBuffer(&stencil);
  EXPECT_GT(new_texture, 0u);
  EXPECT_NE(texture, new_texture);

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
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned stencil;
    EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
    SwapBuffers();
  }

  EXPECT_EQ(kSwapCount, in_flight_surfaces().size());
  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned int next_texture_id = in_flight_surfaces().front()->texture;
    output_surface_->PageFlipComplete();
    EXPECT_EQ(displayed_frame()->texture, next_texture_id);
  }
}

TEST_F(BufferQueueTest, ReshapeWithInFlightSurfaces) {
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned stencil;
    EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
    SwapBuffers();
  }

  EXPECT_TRUE(output_surface_->Reshape(gfx::Size(10, 20), 1.0f,
                                       gfx::ColorSpace(), false));
  EXPECT_EQ(3u, in_flight_surfaces().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->PageFlipComplete();
    EXPECT_FALSE(displayed_frame());
  }

  // The dummy surfacess left should be discarded.
  EXPECT_EQ(0u, available_surfaces().size());
}

TEST_F(BufferQueueTest, SwapAfterReshape) {
  DCHECK_EQ(0u, gpu_memory_buffer_manager_->set_color_space_count());
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned stencil;
    EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
    SwapBuffers();
  }
  DCHECK_EQ(kSwapCount, gpu_memory_buffer_manager_->set_color_space_count());

  EXPECT_TRUE(output_surface_->Reshape(gfx::Size(10, 20), 1.0f,
                                       gfx::ColorSpace(), false));
  DCHECK_EQ(kSwapCount, gpu_memory_buffer_manager_->set_color_space_count());

  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned stencil;
    EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
    SwapBuffers();
  }
  DCHECK_EQ(2 * kSwapCount,
            gpu_memory_buffer_manager_->set_color_space_count());
  EXPECT_EQ(2 * kSwapCount, in_flight_surfaces().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->PageFlipComplete();
    EXPECT_FALSE(displayed_frame());
  }

  CheckUnique();

  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned int next_texture_id = in_flight_surfaces().front()->texture;
    output_surface_->PageFlipComplete();
    EXPECT_EQ(displayed_frame()->texture, next_texture_id);
    EXPECT_TRUE(displayed_frame());
  }

  DCHECK_EQ(2 * kSwapCount,
            gpu_memory_buffer_manager_->set_color_space_count());
  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned stencil;
    EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
    SwapBuffers();
    output_surface_->PageFlipComplete();
  }
  DCHECK_EQ(2 * kSwapCount,
            gpu_memory_buffer_manager_->set_color_space_count());
}

TEST_F(BufferQueueTest, AllocateFails) {
  EXPECT_TRUE(
      output_surface_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false));

  // Succeed in the two swaps.
  unsigned stencil;
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_TRUE(current_frame());
  SwapBuffers(screen_rect);

  // Fail the next surface allocation.
  gpu_memory_buffer_manager_->set_allocate_succeeds(false);
  EXPECT_EQ(0u, output_surface_->GetCurrentBuffer(&stencil));
  EXPECT_FALSE(current_frame());
  SwapBuffers(screen_rect);
  EXPECT_FALSE(current_frame());

  gpu_memory_buffer_manager_->set_allocate_succeeds(true);
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  SwapBuffers(small_damage);

  // Destroy the just-created buffer, and try another swap.
  output_surface_->PageFlipComplete();
  in_flight_surfaces().back().reset();
  EXPECT_EQ(2u, in_flight_surfaces().size());
  for (auto& surface : in_flight_surfaces())
    EXPECT_FALSE(surface);
  EXPECT_GT(output_surface_->GetCurrentBuffer(&stencil), 0u);
  EXPECT_TRUE(current_frame());
  EXPECT_TRUE(displayed_frame());
  SwapBuffers(small_damage);
}

}  // namespace
}  // namespace viz
