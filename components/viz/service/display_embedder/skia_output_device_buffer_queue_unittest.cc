// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/service/display_embedder/output_presenter_gl.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/presenter.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

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

const gfx::Size kScreenSize = gfx::Size(30, 30);
const SkColorType kDefaultColorType = kRGBA_8888_SkColorType;

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
    gpu_service_holder_->ScheduleCompositorGpuTask(std::move(wrap));
    wait_.Wait();
  }

  virtual void SetUpOnMain() {}
  virtual void SetUpOnGpu() {}
  virtual void TearDownOnMain() {}
  virtual void TearDownOnGpu() {}
  virtual void TestBodyOnGpu() {}

  raw_ptr<TestGpuServiceHolder> gpu_service_holder_;
  base::WaitableEvent wait_;
};

// Here starts SkiaOutputDeviceBufferQueue test related code

class TestImageBackingFactory : public gpu::SharedImageBackingFactory {
 public:
  TestImageBackingFactory() = default;
  ~TestImageBackingFactory() override = default;

  MOCK_METHOD1(OnSharedImageSetPurgeable, void(const gpu::Mailbox& mailbox));
  MOCK_METHOD1(OnSharedImageSetNotPurgeable, void(const gpu::Mailbox& mailbox));

  // gpu::SharedImageBackingFactory implementation.
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      SharedImageFormat format,
      gpu::SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override {
    size_t estimated_size =
        ResourceSizes::CheckedSizeInBytes<size_t>(size, format);
    auto result = std::make_unique<gpu::TestImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        estimated_size);
    SetPurgeableCallbacks(result.get());
    return result;
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    auto result = std::make_unique<gpu::TestImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        pixel_data.size());
    SetPurgeableCallbacks(result.get());
    return result;
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override {
    NOTREACHED();
    return nullptr;
  }
  bool IsSupported(uint32_t usage,
                   SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   gpu::GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override {
    return true;
  }

  void SetPurgeableCallbacks(gpu::TestImageBacking* backing) {
    if (enable_purge_mocks_) {
      backing->SetPurgeableCallbacks(
          base::BindRepeating(
              &TestImageBackingFactory::OnSharedImageSetPurgeable,
              base::Unretained(this)),
          base::BindRepeating(
              &TestImageBackingFactory::OnSharedImageSetNotPurgeable,
              base::Unretained(this)));
    }
  }
  bool enable_purge_mocks_ = false;
};

class MockPresenter : public gl::Presenter {
 public:
  explicit MockPresenter(gl::GLDisplayEGL* display)
      : gl::Presenter(display, gfx::Size()) {}

  bool SupportsAsyncSwap() override { return true; }

  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback,
                        gl::FrameData data) override {
    swap_completion_callbacks_.push_back(std::move(completion_callback));
    presentation_callbacks_.push_back(std::move(presentation_callback));
  }

  void CommitOverlayPlanesAsync(SwapCompletionCallback completion_callback,
                                PresentationCallback presentation_callback,
                                gl::FrameData data) override {
    swap_completion_callbacks_.push_back(std::move(completion_callback));
    presentation_callbacks_.push_back(std::move(presentation_callback));
  }

  bool ScheduleOverlayPlane(
      gl::OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data) override {
    return true;
  }

  bool ScheduleCALayer(const ui::CARendererLayerParams& params) override {
    return true;
  }

  gfx::SurfaceOrigin GetOrigin() const override {
    return gfx::SurfaceOrigin::kTopLeft;
  }

  void SwapComplete() {
    DCHECK(!swap_completion_callbacks_.empty());
    std::move(swap_completion_callbacks_.front())
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
    swap_completion_callbacks_.pop_front();

    DCHECK(!presentation_callbacks_.empty());
    std::move(presentation_callbacks_.front()).Run({});
    presentation_callbacks_.pop_front();
  }

 protected:
  ~MockPresenter() override = default;
  base::circular_deque<SwapCompletionCallback> swap_completion_callbacks_;
  base::circular_deque<PresentationCallback> presentation_callbacks_;
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

using DidSwapBufferCompleteCallback =
    base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                 const gfx::Size& pixel_size,
                                 gfx::GpuFenceHandle release_fence)>;
using BufferPresentedCallback =
    base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;

class SkiaOutputDeviceBufferQueueTest : public TestOnGpu {
 public:
  SkiaOutputDeviceBufferQueueTest() = default;

  void SetUpOnMain() override {
    gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
    dependency_ = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
        gpu_service_holder_->gpu_service(), surface_handle_);
  }

  virtual DidSwapBufferCompleteCallback GetDidSwapBuffersCompleteCallback() {
    return base::DoNothing();
  }

  void SetUpOnGpu() override {
    // TODO(vasilyt): Remove this once presenter doesn't need display.
    display_ = gl::GetDefaultDisplayEGL();
    presenter_ = base::MakeRefCounted<MockPresenter>(display_);
    memory_tracker_ = std::make_unique<MemoryTrackerStub>();
    shared_image_factory_ = std::make_unique<gpu::SharedImageFactory>(
        dependency_->GetGpuPreferences(),
        dependency_->GetGpuDriverBugWorkarounds(),
        dependency_->GetGpuFeatureInfo(),
        dependency_->GetSharedContextState().get(),
        dependency_->GetSharedImageManager(), dependency_->GetGpuImageFactory(),
        memory_tracker_.get(),
        /*is_for_display_compositor=*/true),
    shared_image_factory_->RegisterSharedImageBackingFactoryForTesting(
        &test_backing_factory_);
    shared_image_representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(
            dependency_->GetSharedImageManager(), memory_tracker_.get());

    auto present_callback = GetDidSwapBuffersCompleteCallback();

    output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
        std::make_unique<OutputPresenterGL>(
            presenter_, dependency_.get(), shared_image_factory_.get(),
            shared_image_representation_factory_.get()),
        dependency_.get(), shared_image_representation_factory_.get(),
        memory_tracker_.get(), present_callback);
  }

  void TearDownOnGpu() override {
    output_device_.reset();
    shared_image_representation_factory_.reset();
    shared_image_factory_->DestroyAllSharedImages(true);
    shared_image_factory_.reset();
    memory_tracker_.reset();
    presenter_.reset();
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

  std::vector<gpu::Mailbox> pending_overlay_mailboxes() {
    return output_device_->pending_overlay_mailboxes_;
  }

  std::vector<gpu::Mailbox> committed_overlay_mailboxes() {
    return output_device_->committed_overlay_mailboxes_;
  }

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
    absl::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
        no_plane;
    output_device_->SchedulePrimaryPlane(no_plane);
  }

  virtual void SwapBuffers() {
    output_device_->SwapBuffers(base::DoNothing(), OutputSurfaceFrame());
  }

  void CommitOverlayPlanes() {
    output_device_->CommitOverlayPlanes(base::DoNothing(),
                                        OutputSurfaceFrame());
  }

  void PageFlipComplete() { presenter_->SwapComplete(); }

  SkSurfaceCharacterization CreateSkSurfaceCharacterization(
      const gfx::Size size = kScreenSize) {
    auto* gr_context = dependency_->GetSharedContextState()->gr_context();
    auto gr_context_thread_safe_proxy = gr_context->threadSafeProxy();

    auto image_info =
        SkImageInfo::Make(size.width(), size.height(), kDefaultColorType,
                          kPremul_SkAlphaType, nullptr);
    const auto backend_format =
        gr_context_thread_safe_proxy->defaultBackendFormat(kDefaultColorType,
                                                           GrRenderable::kYes);
    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
    auto cache_max_resource_bytes = gr_context->getResourceCacheLimit();
    return gr_context_thread_safe_proxy->createCharacterization(
        cache_max_resource_bytes, image_info, backend_format,
        /*sampleCount=*/1, kTopLeft_GrSurfaceOrigin, surface_props,
        /*isMipMapped=*/false,
        /*willUseGLFBO0=*/false, /*isTextureable=*/true);
  }

  void FirstReshape() {
    // If the renderer allocates images we shouldn't call
    // EnsureMinNumberOfBuffers.
    if (output_device_->capabilities()
            .supports_dynamic_frame_buffer_allocation &&
        !output_device_->capabilities().renderer_allocates_images) {
      output_device_->EnsureMinNumberOfBuffers(
          output_device_->capabilities().number_of_buffers);
    }
    output_device_->Reshape(CreateSkSurfaceCharacterization(), {}, 1.0f,
                            gfx::OVERLAY_TRANSFORM_NONE);
  }

  gpu::Mailbox MakeOverlayMailbox() {
    gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();
    SharedImageFormat si_format =
        SharedImageFormat::SinglePlane(ResourceFormat::RGBA_8888);
    bool success = shared_image_factory_->CreateSharedImage(
        mailbox, si_format, gfx::Size(1000, 1000),
        gfx::ColorSpace::CreateSRGB(),
        GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
        SkAlphaType::kPremul_SkAlphaType, gpu::kNullSurfaceHandle,
        gpu::SHARED_IMAGE_USAGE_SCANOUT);
    CHECK(success);

    shared_image_representation_factory_->ProduceOverlay(mailbox)->SetCleared();
    return mailbox;
  }

 protected:
  std::unique_ptr<SkiaOutputSurfaceDependency> dependency_;
  raw_ptr<gl::GLDisplayEGL> display_;
  scoped_refptr<MockPresenter> presenter_;
  std::unique_ptr<MemoryTrackerStub> memory_tracker_;
  TestImageBackingFactory test_backing_factory_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<SkiaOutputDeviceBufferQueue> output_device_;
};

class SkiaOutputDeviceBufferQueuePurgeableTest
    : public SkiaOutputDeviceBufferQueueTest {
 public:
  SkiaOutputDeviceBufferQueuePurgeableTest() {
    feature_list_.InitAndEnableFeature(features::kBufferQueueImageSetPurgeable);
  }

  base::test::ScopedFeatureList feature_list_;
};

namespace {

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, MultipleGetCurrentBufferCalls) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check that multiple bind calls do not create or change surfaces.

  FirstReshape();
  const int kNumBuffers = CountBuffers();
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_NE(PaintPrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
  auto* fb = current_image();
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
  EXPECT_EQ(fb, current_image());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckDoubleBuffering) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check buffer flow through double buffering path.
  FirstReshape();
  const int kNumBuffers = CountBuffers();
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
  EXPECT_NE(current_image(), nullptr);
  EXPECT_FALSE(displayed_image());
  SwapBuffers();
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  PageFlipComplete();
  EXPECT_EQ(0U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
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
  EXPECT_EQ(static_cast<size_t>(kNumBuffers - 1), available_images().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
  CheckUnique();
  EXPECT_EQ(static_cast<size_t>(kNumBuffers - 2), available_images().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckTripleBuffering) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check buffer flow through triple buffering path.
  FirstReshape();
  const int kNumBuffers = CountBuffers();
  EXPECT_NE(0U, memory_tracker().GetSize());

  // This bit is the same sequence tested in the doublebuffering case.
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_FALSE(displayed_image());
  SwapBuffers();
  PageFlipComplete();
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  SwapBuffers();

  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(1U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  PageFlipComplete();
  EXPECT_EQ(kNumBuffers, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(0U, swap_completion_callbacks().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_EQ(static_cast<size_t>(kNumBuffers - 2), available_images().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckEmptySwap) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  FirstReshape();
  const int kNumBuffers = CountBuffers();

  EXPECT_NE(0U, memory_tracker().GetSize());
  auto* image = PaintAndSchedulePrimaryPlane();
  EXPECT_NE(image, nullptr);
  EXPECT_NE(0U, memory_tracker().GetSize());
  EXPECT_EQ(kNumBuffers, CountBuffers());
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
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  FirstReshape();

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
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  FirstReshape();
  const int kNumBuffers = CountBuffers();
  const int kSwapCount = kNumBuffers * 2;

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  for (int i = 0; i < kSwapCount; ++i) {
    SwapBuffers();
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
    PageFlipComplete();
  }

  // Note: this must not be kSwapCount
  EXPECT_EQ(kNumBuffers, CountBuffers());

  for (int i = 0; i < kSwapCount; ++i) {
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
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  FirstReshape();
  const size_t kNumBuffers = available_images().size();

  const size_t kSwapCount = 5;

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  for (size_t i = 0; i < kSwapCount; ++i) {
    SwapBuffers();
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
    PageFlipComplete();
  }

  SwapBuffers();

  output_device_->Reshape(
      CreateSkSurfaceCharacterization(
          gfx::Size(kScreenSize.width() - 1, kScreenSize.height() - 1)),
      {}, 1.0f, gfx::OVERLAY_TRANSFORM_NONE);

  // swap completion callbacks should not be cleared.
  EXPECT_EQ(1u, swap_completion_callbacks().size());

  PageFlipComplete();
  EXPECT_FALSE(displayed_image());

  // The dummy surfacess left should be discarded.
  EXPECT_EQ(kNumBuffers, available_images().size());

  // Test swap after reshape
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  SwapBuffers();
  PageFlipComplete();
  EXPECT_NE(displayed_image(), nullptr);
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, BufferIsInOrder) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  FirstReshape();
  int kNumBuffers = available_images().size();

  int current_index = -1;
  int submitted_index = -1;
  int displayed_index = -1;

  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  ++current_index;
  EXPECT_EQ(current_image(), images()[current_index % kNumBuffers].get());
  EXPECT_EQ(submitted_image(),
            submitted_index < 0
                ? nullptr
                : images()[submitted_index % kNumBuffers].get());
  EXPECT_EQ(displayed_image(),
            displayed_index < 0
                ? nullptr
                : images()[displayed_index % kNumBuffers].get());

  SwapBuffers();
  ++submitted_index;
  EXPECT_EQ(current_image(), nullptr);
  EXPECT_EQ(submitted_image(),
            submitted_index < 0
                ? nullptr
                : images()[submitted_index % kNumBuffers].get());
  EXPECT_EQ(displayed_image(),
            displayed_index < 0
                ? nullptr
                : images()[displayed_index % kNumBuffers].get());

  const size_t kSwapCount = 10;
  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
    ++current_index;
    EXPECT_EQ(current_image(), images()[current_index % kNumBuffers].get());
    EXPECT_EQ(submitted_image(),
              submitted_index < 0
                  ? nullptr
                  : images()[submitted_index % kNumBuffers].get());
    EXPECT_EQ(displayed_image(),
              displayed_index < 0
                  ? nullptr
                  : images()[displayed_index % kNumBuffers].get());

    SwapBuffers();
    ++submitted_index;
    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(submitted_image(),
              submitted_index < 0
                  ? nullptr
                  : images()[submitted_index % kNumBuffers].get());
    EXPECT_EQ(displayed_image(),
              displayed_index < 0
                  ? nullptr
                  : images()[displayed_index % kNumBuffers].get());

    PageFlipComplete();
    ++displayed_index;
    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(submitted_image(),
              submitted_index < 0
                  ? nullptr
                  : images()[submitted_index % kNumBuffers].get());
    EXPECT_EQ(displayed_image(),
              displayed_index < 0
                  ? nullptr
                  : images()[displayed_index % kNumBuffers].get());
  }

  PageFlipComplete();
  ++displayed_index;
  EXPECT_EQ(current_image(), nullptr);
  EXPECT_EQ(submitted_image(),
            submitted_index < 0
                ? nullptr
                : images()[submitted_index % kNumBuffers].get());
  EXPECT_EQ(displayed_image(),
            displayed_index < 0
                ? nullptr
                : images()[displayed_index % kNumBuffers].get());
}

SkiaOutputSurface::OverlayList MakeOverlayList(
    std::vector<gpu::Mailbox> mailboxes) {
  SkiaOutputSurface::OverlayList overlay_list;
  for (auto& mailbox : mailboxes) {
    OutputPresenter::OverlayPlaneCandidate overlay;
    overlay.mailbox = mailbox;
#if BUILDFLAG(IS_APPLE)
    overlay.shared_state = base::MakeRefCounted<CALayerOverlaySharedState>();
#endif  // BUILDFLAG(IS_APPLE)
    overlay_list.push_back(overlay);
  }
  return overlay_list;
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, ScheduleOverlaysNoPrimaryPlane) {
  FirstReshape();

  // Make 3 primary plane buffers
  std::vector<gpu::Mailbox> mailboxes;
  for (int i = 0; i < 3; ++i) {
    gpu::Mailbox mailbox = MakeOverlayMailbox();
    mailboxes.push_back(mailbox);
  }

  // Do a swap and commit overlay planes with no primary plane.
  for (size_t i = 0; i < 6; ++i) {
    // Repeat each mailbox for 2 frames.
    auto mailbox = mailboxes[i / 2];

    ScheduleNoPrimaryPlane();
    output_device_->ScheduleOverlays(MakeOverlayList({mailbox}));

    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(displayed_image(), nullptr);
    EXPECT_THAT(pending_overlay_mailboxes(), testing::ElementsAre(mailbox));

    // Do a swap then a commit for each overlay mailbox.
    if ((i % 2) == 0) {
      SwapBuffers();
    } else if ((i % 2) == 1) {
      CommitOverlayPlanes();
    }

    EXPECT_EQ(current_image(), nullptr);
    EXPECT_EQ(displayed_image(), nullptr);
    EXPECT_THAT(pending_overlay_mailboxes(), testing::IsEmpty());
    EXPECT_THAT(committed_overlay_mailboxes(), testing::ElementsAre(mailbox));

    PageFlipComplete();
  }
}

TEST_F_GPU(SkiaOutputDeviceBufferQueuePurgeableTest, ToggleNoPrimaryPlane) {
  test_backing_factory_.enable_purge_mocks_ = true;
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  FirstReshape();
  constexpr size_t kBufferQueueSize = 3;

  // Swap three frames, saving the mailboxes of the buffers.
  EXPECT_CALL(test_backing_factory_, OnSharedImageSetPurgeable(_)).Times(0);
  EXPECT_CALL(test_backing_factory_, OnSharedImageSetNotPurgeable(_)).Times(0);
  std::vector<gpu::Mailbox> mailboxes;
  for (size_t i = 0; i < kBufferQueueSize; ++i) {
    PaintAndSchedulePrimaryPlane();
    mailboxes.push_back(current_image()->mailbox());
    SwapBuffers();
    PageFlipComplete();
  }
  EXPECT_NE(mailboxes[0], mailboxes[1]);
  EXPECT_NE(mailboxes[0], mailboxes[2]);

  // Each swap with no primary plane will cause the plane to purged, until all
  // are purged.
  for (size_t i = 0; i < 2 * kBufferQueueSize; ++i) {
    ScheduleNoPrimaryPlane();
    SwapBuffers();
    if (i < kBufferQueueSize) {
      EXPECT_CALL(test_backing_factory_,
                  OnSharedImageSetPurgeable(mailboxes[i]));
    }
    PageFlipComplete();
  }

  // The first swap will un-purge an image before paint.
  {
    EXPECT_CALL(test_backing_factory_,
                OnSharedImageSetNotPurgeable(mailboxes[0]));
    PaintAndSchedulePrimaryPlane();
    SwapBuffers();
    PageFlipComplete();
  }
  // The next swap will un-purge an image before paint, and then un-purge the
  // final image in PageFlipComplete.
  {
    EXPECT_CALL(test_backing_factory_,
                OnSharedImageSetNotPurgeable(mailboxes[1]));
    PaintAndSchedulePrimaryPlane();
    SwapBuffers();
    EXPECT_CALL(test_backing_factory_,
                OnSharedImageSetNotPurgeable(mailboxes[2]));
    PageFlipComplete();
  }
}

}  // namespace

class SkiaOutputDeviceSwapSkippedTest : public SkiaOutputDeviceBufferQueueTest {
 public:
  SkiaOutputDeviceSwapSkippedTest() = default;

  DidSwapBufferCompleteCallback GetDidSwapBuffersCompleteCallback() override {
    return swap_buffers_complete_cb.Get();
  }

  void SwapBuffers() override {
    output_device_->SwapBuffers(buffer_presented_cb.Get(),
                                OutputSurfaceFrame());
  }

  void SwapBuffersSkipped() {
    output_device_->SwapBuffersSkipped(buffer_presented_cb.Get(),
                                       OutputSurfaceFrame());
  }

  base::MockCallback<DidSwapBufferCompleteCallback> swap_buffers_complete_cb;
  base::MockCallback<BufferPresentedCallback> buffer_presented_cb;
};

namespace {

MATCHER_P2(CheckSwapResponse, expected_swap_id, expected_result, "") {
  return arg.swap_response.swap_id == expected_swap_id &&
         arg.swap_response.result == expected_result;
}

MATCHER_P(CheckPresentationFeedback, expected_fail, "") {
  return expected_fail == arg.failed();
}

TEST_F_GPU(SkiaOutputDeviceSwapSkippedTest, SkipWithoutPending) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check that skipping a SwapBuffers without any pending swaps immediately
  // invokes the complete/presented callbacks.
  FirstReshape();
  EXPECT_CALL(swap_buffers_complete_cb,
              Run(CheckSwapResponse(1U, gfx::SwapResult::SWAP_SKIPPED), _, _));
  EXPECT_CALL(buffer_presented_cb,
              Run(CheckPresentationFeedback(true /* failed */)));

  SwapBuffersSkipped();
}

TEST_F_GPU(SkiaOutputDeviceSwapSkippedTest, SkipWithPending) {
  if (output_device_->capabilities().renderer_allocates_images) {
    GTEST_SKIP_(
        "Tests behaviour not exercised when the renderer allocates the "
        "images.");
  }

  // Check that skipping a SwapBuffers with existing pending swaps waits for
  // the pending swaps to complete before invoking the complete/presented
  // callbacks.
  FirstReshape();
  EXPECT_NE(PaintAndSchedulePrimaryPlane(), nullptr);
  EXPECT_NE(current_image(), nullptr);

  SwapBuffers();
  SwapBuffersSkipped();

  EXPECT_CALL(swap_buffers_complete_cb,
              Run(CheckSwapResponse(1U, gfx::SwapResult::SWAP_ACK), _, _));
  EXPECT_CALL(buffer_presented_cb,
              Run(CheckPresentationFeedback(false /* failed */)));
  EXPECT_CALL(swap_buffers_complete_cb,
              Run(CheckSwapResponse(2U, gfx::SwapResult::SWAP_SKIPPED), _, _));
  EXPECT_CALL(buffer_presented_cb,
              Run(CheckPresentationFeedback(true /* failed */)));

  PageFlipComplete();
}

}  // namespace
}  // namespace viz
