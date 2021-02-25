// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/service/display_embedder/output_presenter_gl.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/test_shared_image_backing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gl/gl_surface_stub.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Return;

namespace viz {
namespace {

// These MACRO and TestOnGpu class make it easier to write tests that runs on
// GPU Thread
// Use TEST_F_GPU instead of TEST_F in the same manner and in your subclass
// of TestOnGpu implement SetUpOnMain/SetUpOnGpu and
// TearDownOnMain/TearDownOnGpu instead of SetUp and TearDown respectively.
//
// NOTE: Most likely you need to implement TearDownOnGpu instead of relying on
// destructor to ensure that necessary cleanup happens on GPU Thread.

// TODO(vasilyt): Extract this for others to use?

#define GTEST_TEST_GPU_(test_suite_name, test_name, parent_class, parent_id)  \
  class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                    \
      : public parent_class {                                                 \
   public:                                                                    \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() {}                   \
                                                                              \
   private:                                                                   \
    virtual void TestBodyOnGpu();                                             \
    static ::testing::TestInfo* const test_info_ GTEST_ATTRIBUTE_UNUSED_;     \
    GTEST_DISALLOW_COPY_AND_ASSIGN_(GTEST_TEST_CLASS_NAME_(test_suite_name,   \
                                                           test_name));       \
  };                                                                          \
                                                                              \
  ::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_suite_name,          \
                                                    test_name)::test_info_ =  \
      ::testing::internal::MakeAndRegisterTestInfo(                           \
          #test_suite_name, #test_name, nullptr, nullptr,                     \
          ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id), \
          ::testing::internal::SuiteApiResolver<                              \
              parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__),         \
          ::testing::internal::SuiteApiResolver<                              \
              parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__),      \
          new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(    \
              test_suite_name, test_name)>);                                  \
  void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBodyOnGpu()

#define TEST_F_GPU(test_fixture, test_name)              \
  GTEST_TEST_GPU_(test_fixture, test_name, test_fixture, \
                  ::testing::internal::GetTypeId<test_fixture>())

class TestOnGpu : public ::testing::Test {
 protected:
  TestOnGpu()
      : wait_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void TestBody() override {
    auto callback =
        base::BindLambdaForTesting([&]() { this->TestBodyOnGpu(); });
    ScheduleGpuTask(std::move(callback));
  }

  void SetUp() override {
    gpu_service_holder_ = TestGpuServiceHolder::GetInstance();
    SetUpOnMain();

    auto setup = base::BindLambdaForTesting([&]() { this->SetUpOnGpu(); });
    ScheduleGpuTask(setup);
  }

  void TearDown() override {
    auto teardown =
        base::BindLambdaForTesting([&]() { this->TearDownOnGpu(); });
    ScheduleGpuTask(teardown);

    TearDownOnMain();
  }

  void CallOnGpuAndUnblockMain(base::OnceClosure callback) {
    DCHECK(!wait_.IsSignaled());
    std::move(callback).Run();
    wait_.Signal();
  }

  void ScheduleGpuTask(base::OnceClosure callback) {
    auto wrap = base::BindOnce(&TestOnGpu::CallOnGpuAndUnblockMain,
                               base::Unretained(this), std::move(callback));
    gpu_service_holder_->ScheduleGpuTask(std::move(wrap));
    wait_.Wait();
  }

  virtual void SetUpOnMain() {}
  virtual void SetUpOnGpu() {}
  virtual void TearDownOnMain() {}
  virtual void TearDownOnGpu() {}
  virtual void TestBodyOnGpu() {}

  TestGpuServiceHolder* gpu_service_holder_;
  base::WaitableEvent wait_;
};

// Here starts SkiaOutputDeviceBufferQueue test related code

class TestSharedImageBackingFactory : public gpu::SharedImageBackingFactory {
 public:
  TestSharedImageBackingFactory() = default;
  ~TestSharedImageBackingFactory() override = default;

  // gpu::SharedImageBackingFactory implementation.
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      ResourceFormat format,
      gpu::SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override {
    size_t estimated_size =
        ResourceSizes::CheckedSizeInBytes<size_t>(size, format);
    return std::make_unique<gpu::TestSharedImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        estimated_size);
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    return std::make_unique<gpu::TestSharedImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        pixel_data.size());
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gpu::SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override {
    NOTREACHED();
    return nullptr;
  }
  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override {
    return false;
  }
};

class MockGLSurfaceAsync : public gl::GLSurfaceStub {
 public:
  bool SupportsAsyncSwap() override { return true; }

  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback) override {
    callbacks_.push_back(std::move(completion_callback));
  }

  void CommitOverlayPlanesAsync(
      SwapCompletionCallback completion_callback,
      PresentationCallback presentation_callback) override {
    callbacks_.push_back(std::move(completion_callback));
  }

  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    return true;
  }

  gfx::SurfaceOrigin GetOrigin() const override {
    return gfx::SurfaceOrigin::kTopLeft;
  }

  void SwapComplete() {
    DCHECK(!callbacks_.empty());
    std::move(callbacks_.front())
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
    callbacks_.pop_front();
  }

 protected:
  ~MockGLSurfaceAsync() override = default;
  base::circular_deque<SwapCompletionCallback> callbacks_;
};

class MemoryTrackerStub : public gpu::MemoryTracker {
 public:
  MemoryTrackerStub() = default;
  MemoryTrackerStub(const MemoryTrackerStub&) = delete;
  MemoryTrackerStub& operator=(const MemoryTrackerStub&) = delete;
  ~MemoryTrackerStub() override { DCHECK(!size_); }

  // MemoryTracker implementation:
  void TrackMemoryAllocatedChange(int64_t delta) override {
    DCHECK(delta >= 0 || size_ >= static_cast<uint64_t>(-delta));
    size_ += delta;
  }

  uint64_t GetSize() const override { return size_; }
  uint64_t ClientTracingId() const override { return client_tracing_id_; }
  int ClientId() const override {
    return gpu::ChannelIdFromCommandBufferId(command_buffer_id_);
  }
  uint64_t ContextGroupTracingId() const override {
    return command_buffer_id_.GetUnsafeValue();
  }

 private:
  gpu::CommandBufferId command_buffer_id_;
  const uint64_t client_tracing_id_ = 0;
  uint64_t size_ = 0;
};

}  // namespace

class SkiaOutputDeviceBufferQueueTest : public TestOnGpu {
 public:
  SkiaOutputDeviceBufferQueueTest() = default;

  void SetUpOnMain() override {
    gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
    dependency_ = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
        gpu_service_holder_->gpu_service(), surface_handle_);
  }

  void SetUpOnGpu() override {
    gl_surface_ = base::MakeRefCounted<MockGLSurfaceAsync>();
    memory_tracker_ = std::make_unique<MemoryTrackerStub>();
    shared_image_factory_ = std::make_unique<gpu::SharedImageFactory>(
        dependency_->GetGpuPreferences(),
        dependency_->GetGpuDriverBugWorkarounds(),
        dependency_->GetGpuFeatureInfo(),
        dependency_->GetSharedContextState().get(),
        dependency_->GetMailboxManager(), dependency_->GetSharedImageManager(),
        dependency_->GetGpuImageFactory(), memory_tracker_.get(), true),
    shared_image_factory_->RegisterSharedImageBackingFactoryForTesting(
        &test_backing_factory_);
    shared_image_representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(
            dependency_->GetSharedImageManager(), memory_tracker_.get());

    auto present_callback =
        base::DoNothing::Repeatedly<gpu::SwapBuffersCompleteParams,
                                    const gfx::Size&>();

    output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
        std::make_unique<OutputPresenterGL>(
            gl_surface_, dependency_.get(), shared_image_factory_.get(),
            shared_image_representation_factory_.get()),
        dependency_.get(), shared_image_representation_factory_.get(),
        memory_tracker_.get(), present_callback, false);
  }

  void TearDownOnGpu() override {
    output_device_.reset();
    shared_image_representation_factory_.reset();
    shared_image_factory_.reset();
    memory_tracker_.reset();
    gl_surface_.reset();
  }

  using Image = OutputPresenter::Image;

  const std::vector<std::unique_ptr<Image>>& images() {
    return output_device_->images_;
  }

  Image* current_image() { return output_device_->current_image_; }

  const base::circular_deque<Image*>& available_images() {
    return output_device_->available_images_;
  }

  Image* submitted_image() { return output_device_->submitted_image_; }

  Image* displayed_image() { return output_device_->displayed_image_; }

  base::circular_deque<std::unique_ptr<
      SkiaOutputDeviceBufferQueue::CancelableSwapCompletionCallback>>&
  swap_completion_callbacks() {
    return output_device_->swap_completion_callbacks_;
  }

  const gpu::MemoryTracker& memory_tracker() { return *memory_tracker_; }

  int CountBuffers() {
    int n = available_images().size() + swap_completion_callbacks().size();

    if (displayed_image())
      n++;
    if (current_image())
      n++;
    return n;
  }

  void CheckUnique() {
    std::set<Image*> images;
    for (auto* image : available_images())
      images.insert(image);

    if (displayed_image())
      images.insert(displayed_image());

    if (current_image())
      images.insert(current_image());

    EXPECT_EQ(images.size() + swap_completion_callbacks().size(),
              (size_t)CountBuffers());
  }

  Image* PaintPrimaryPlane() {
    std::vector<GrBackendSemaphore> end_semaphores;
    output_device_->BeginPaint(&end_semaphores);
    output_device_->EndPaint();
    return current_image();
  }

  Image* PaintAndSchedulePrimaryPlane() {
    PaintPrimaryPlane();
    SchedulePrimaryPlane();
    return current_image();
  }

  void SchedulePrimaryPlane() {
    output_device_->SchedulePrimaryPlane(
        OverlayProcessorInterface::OutputSurfaceOverlayPlane());
  }

  void ScheduleNoPrimaryPlane() {
    base::Optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
        no_plane;
    output_device_->SchedulePrimaryPlane(no_plane);
  }

  void SwapBuffers() {
    auto present_callback =
        base::DoNothing::Once<const gfx::PresentationFeedback&>();

    output_device_->SwapBuffers(std::move(present_callback),
                                OutputSurfaceFrame());
  }

  void CommitOverlayPlanes() {
    auto present_callback =
        base::DoNothing::Once<const gfx::PresentationFeedback&>();

    output_device_->CommitOverlayPlanes(std::move(present_callback),
                                        OutputSurfaceFrame());
  }

  void PageFlipComplete() { gl_surface_->SwapComplete(); }

 protected:
  std::unique_ptr<SkiaOutputSurfaceDependency> dependency_;
  scoped_refptr<MockGLSurfaceAsync> gl_surface_;
  std::unique_ptr<MemoryTrackerStub> memory_tracker_;
  TestSharedImageBackingFactory test_backing_factory_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<SkiaOutputDeviceBufferQueue> output_device_;
};

namespace {

const gfx::Size screen_size = gfx::Size(30, 30);

const gfx::BufferFormat kDefaultFormat = gfx::BufferFormat::RGBA_8888;

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, MultipleGetCurrentBufferCalls) {
  // Check that multiple bind calls do not create or change surfaces.

  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_NE(PaintPrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  auto* fb = current_image();
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  EXPECT_EQ(fb, current_image());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  EXPECT_NE(current_image(), nullptr);
  EXPECT_FALSE(displayed_image());
  SwapBuffers();
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  PageFlipComplete();
  EXPECT_EQ(0U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(0U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  SwapBuffers();
  CheckUnique();
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());

  PageFlipComplete();
  CheckUnique();
  EXPECT_EQ(0U, swap_completion_callbacks().size());
  EXPECT_EQ(2U, available_images().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1u, available_images().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_NE(0U, memory_tracker().GetSize());

  // This bit is the same sequence tested in the doublebuffering case.
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_FALSE(displayed_image());
  SwapBuffers();
  PageFlipComplete();
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  SwapBuffers();

  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  PageFlipComplete();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(0U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_EQ(1U, available_images().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckEmptySwap) {
  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);

  EXPECT_EQ(3, CountBuffers());
  EXPECT_NE(0U, memory_tracker().GetSize());
  auto* image = PaintAndSchedulePrimaryPlane();
  EXPECT_NE(image, nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(3, CountBuffers());
  EXPECT_NE(current_image(), nullptr);
  EXPECT_FALSE(displayed_image());

  SwapBuffers();
  // Make sure we won't be drawing to the texture we just sent for scanout.
  auto* new_image = PaintAndSchedulePrimaryPlane();
  EXPECT_NE(new_image, nullptr);
  EXPECT_NE(image, new_image);

  EXPECT_EQ(1U, swap_completion_callbacks().size());
  PageFlipComplete();

  // Test CommitOverlayPlanes without calling BeginPaint/EndPaint (i.e without
  // PaintAndSchedulePrimaryPlane)
  SwapBuffers();
  EXPECT_EQ(1U, swap_completion_callbacks().size());

  // Schedule the primary plane without drawing.
  SchedulePrimaryPlane();

  PageFlipComplete();
  EXPECT_EQ(0U, swap_completion_callbacks().size());

  EXPECT_EQ(current_image(), nullptr);
  CommitOverlayPlanes();
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  PageFlipComplete();
  EXPECT_EQ(0U, swap_completion_callbacks().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, NoPrimaryPlane) {
  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);

  // Do a swap and commit overlay planes with no primary plane.
  for (size_t i = 0; i < 2; ++i) {
    ScheduleNoPrimaryPlane();
    EXPECT_EQ(current_image(), nullptr);
    EXPECT_FALSE(displayed_image());
    if (i == 0)
      SwapBuffers();
    else if (i == 1)
      CommitOverlayPlanes();
    EXPECT_FALSE(displayed_image());
    PageFlipComplete();
  }

  // Do it again with a paint in between.
  for (size_t i = 0; i < 2; ++i) {
    PaintAndSchedulePrimaryPlane();
    EXPECT_NE(current_image(), nullptr);
    EXPECT_FALSE(displayed_image());
    SwapBuffers();
    PageFlipComplete();
    EXPECT_TRUE(displayed_image());

    ScheduleNoPrimaryPlane();
    EXPECT_EQ(current_image(), nullptr);
    if (i == 0)
      SwapBuffers();
    else if (i == 1)
      CommitOverlayPlanes();
    EXPECT_TRUE(displayed_image());
    PageFlipComplete();
    EXPECT_FALSE(displayed_image());
  }

  // Do a final commit with no primary.
  {
    ScheduleNoPrimaryPlane();
    EXPECT_EQ(current_image(), nullptr);
    CommitOverlayPlanes();
    PageFlipComplete();
    EXPECT_FALSE(displayed_image());
  }
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckCorrectBufferOrdering) {
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);
  const size_t kSwapCount = 5;

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  for (size_t i = 0; i < kSwapCount; ++i) {
    SwapBuffers();
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
    PageFlipComplete();
  }

  // Note: this must be three, not kSwapCount
  EXPECT_EQ(3, CountBuffers());

  for (size_t i = 0; i < kSwapCount; ++i) {
    auto* next_image = current_image();
    SwapBuffers();
    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(1U, swap_completion_callbacks().size());
    PageFlipComplete();
    EXPECT_EQ(displayed_image(), next_image);
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  }
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, ReshapeWithInFlightSurfaces) {
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);

  const size_t kSwapCount = 5;

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  for (size_t i = 0; i < kSwapCount; ++i) {
    SwapBuffers();
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
    PageFlipComplete();
  }

  SwapBuffers();

  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);

  // swap completion callbacks should not be cleared.
  EXPECT_EQ(1u, swap_completion_callbacks().size());

  PageFlipComplete();
  EXPECT_FALSE(displayed_image());

  // The dummy surfacess left should be discarded.
  EXPECT_EQ(3u, available_images().size());

  // Test swap after reshape
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  SwapBuffers();
  PageFlipComplete();
  EXPECT_NE(displayed_image(), nullptr);
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, BufferIsInOrder) {
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), kDefaultFormat,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_EQ(3u, available_images().size());

  int current_index = -1;
  int submitted_index = -1;
  int displayed_index = -1;

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  ++current_index;
  EXPECT_EQ(current_image(), images()[current_index % 3].get());
  EXPECT_EQ(submitted_image(), submitted_index < 0
                                   ? nullptr
                                   : images()[submitted_index % 3].get());
  EXPECT_EQ(displayed_image(), displayed_index < 0
                                   ? nullptr
                                   : images()[displayed_index % 3].get());

  SwapBuffers();
  ++submitted_index;
  EXPECT_EQ(current_image(), nullptr);
  EXPECT_EQ(submitted_image(), submitted_index < 0
                                   ? nullptr
                                   : images()[submitted_index % 3].get());
  EXPECT_EQ(displayed_image(), displayed_index < 0
                                   ? nullptr
                                   : images()[displayed_index % 3].get());

  const size_t kSwapCount = 10;
  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
    ++current_index;
    EXPECT_EQ(current_image(), images()[current_index % 3].get());
    EXPECT_EQ(submitted_image(), submitted_index < 0
                                     ? nullptr
                                     : images()[submitted_index % 3].get());
    EXPECT_EQ(displayed_image(), displayed_index < 0
                                     ? nullptr
                                     : images()[displayed_index % 3].get());

    SwapBuffers();
    ++submitted_index;
    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(submitted_image(), submitted_index < 0
                                     ? nullptr
                                     : images()[submitted_index % 3].get());
    EXPECT_EQ(displayed_image(), displayed_index < 0
                                     ? nullptr
                                     : images()[displayed_index % 3].get());

    PageFlipComplete();
    ++displayed_index;
    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(submitted_image(), submitted_index < 0
                                     ? nullptr
                                     : images()[submitted_index % 3].get());
    EXPECT_EQ(displayed_image(), displayed_index < 0
                                     ? nullptr
                                     : images()[displayed_index % 3].get());
  }

  PageFlipComplete();
  ++displayed_index;
  EXPECT_EQ(current_image(), nullptr);
  EXPECT_EQ(submitted_image(), submitted_index < 0
                                   ? nullptr
                                   : images()[submitted_index % 3].get());
  EXPECT_EQ(displayed_image(), displayed_index < 0
                                   ? nullptr
                                   : images()[displayed_index % 3].get());
}

}  // namespace
}  // namespace viz
