// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
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
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                        \
    (const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete;    \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) & operator=(           \
        const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete; \
                                                                              \
   private:                                                                   \
    virtual void TestBodyOnGpu();                                             \
    GTEST_INTERNAL_ATTRIBUTE_MAYBE_UNUSED                                     \
        static ::testing::TestInfo* const test_info_;                         \
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
  TestImageBackingFactory() : SharedImageBackingFactory(kUsageAll) {}
  ~TestImageBackingFactory() override = default;

  // gpu::SharedImageBackingFactory implementation.
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      SharedImageFormat format,
      gpu::SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe) override {
    size_t estimated_size = format.EstimatedSizeInBytes(size);
    return std::make_unique<gpu::TestImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        estimated_size);
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      bool is_thread_safe,
      base::span<const uint8_t> pixel_data) override {
    return std::make_unique<gpu::TestImageBacking>(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        pixel_data.size());
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      gfx::GpuMemoryBufferHandle handle) override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  bool IsSupported(gpu::SharedImageUsageSet usage,
                   SharedImageFormat format,
                   const gfx::Size& size,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   gpu::GrContextType gr_context_type,
                   base::span<const uint8_t> pixel_data) override {
    return true;
  }
  gpu::SharedImageBackingType GetBackingType() override {
    return gpu::SharedImageBackingType::kTest;
  }
};

class MockPresenter : public gl::Presenter {
 public:
  MockPresenter() = default;

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override {
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
    return base::BindRepeating(
        &SkiaOutputDeviceBufferQueueTest::DidSwapBuffersComplete,
        base::Unretained(this));
  }

  virtual SkiaOutputDevice::ReleaseOverlaysCallback
  GetReleaseOverlaysCallback() {
    return base::BindRepeating(
        &SkiaOutputDeviceBufferQueueTest::ReleaseOverlays,
        base::Unretained(this));
  }

  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params,
                              const gfx::Size& pixel_size,
                              gfx::GpuFenceHandle release_fence) {
    params_.push_back(params);
  }

  void ReleaseOverlays(std::vector<gpu::Mailbox> overlays) {
    released_overlays_params_.push_back(overlays);
  }

  void SetUpOnGpu() override {
    presenter_ = base::MakeRefCounted<MockPresenter>();
    memory_tracker_ = std::make_unique<MemoryTrackerStub>();
    shared_image_factory_ = std::make_unique<gpu::SharedImageFactory>(
        dependency_->GetGpuPreferences(),
        dependency_->GetGpuDriverBugWorkarounds(),
        dependency_->GetGpuFeatureInfo(),
        dependency_->GetSharedContextState().get(),
        dependency_->GetSharedImageManager(), memory_tracker_.get(),
        /*is_for_display_compositor=*/true),
    shared_image_factory_->RegisterSharedImageBackingFactoryForTesting(
        &test_backing_factory_);
    shared_image_representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(
            dependency_->GetSharedImageManager(), memory_tracker_.get());

    auto present_callback = GetDidSwapBuffersCompleteCallback();
    auto release_callback = GetReleaseOverlaysCallback();

    output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
        std::make_unique<OutputPresenterGL>(presenter_, dependency_.get()),
        dependency_.get(), shared_image_representation_factory_.get(),
        memory_tracker_.get(), present_callback, release_callback);
  }

  void TearDownOnGpu() override {
    output_device_.reset();
    shared_image_representation_factory_.reset();
    shared_image_factory_->DestroyAllSharedImages(true);
    shared_image_factory_.reset();
    memory_tracker_.reset();
    presenter_.reset();
  }

  std::vector<gpu::Mailbox> pending_overlay_mailboxes() {
    return output_device_->pending_overlay_mailboxes_;
  }

  std::vector<gpu::Mailbox> committed_overlay_mailboxes() {
    return output_device_->committed_overlay_mailboxes_;
  }

  const gpu::MemoryTracker& memory_tracker() { return *memory_tracker_; }

  virtual void Present() {
    // SkiaOutputDeviceBuffer queue doesn't care about rect, so we can pass
    // empty one.
    output_device_->Present(gfx::Rect(), base::DoNothing(),
                            OutputSurfaceFrame());
  }

  void PageFlipComplete() { presenter_->SwapComplete(); }

  SkImageInfo CreateSkImageInfo(const gfx::Size size = kScreenSize) {
    return SkImageInfo::Make(size.width(), size.height(), kDefaultColorType,
                             kPremul_SkAlphaType, nullptr);
  }

  void FirstReshape() {
    SkiaOutputDevice::ReshapeParams reshape_params = {.image_info =
                                                          CreateSkImageInfo()};
    output_device_->Reshape(reshape_params);
  }

  std::unique_ptr<gpu::OverlayImageRepresentation> MakeOverlay() {
    gpu::Mailbox mailbox = gpu::Mailbox::Generate();
    bool success = shared_image_factory_->CreateSharedImage(
        mailbox, SinglePlaneFormat::kRGBA_8888, gfx::Size(1000, 1000),
        gfx::ColorSpace::CreateSRGB(),
        GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
        SkAlphaType::kPremul_SkAlphaType, gpu::kNullSurfaceHandle,
        gpu::SHARED_IMAGE_USAGE_SCANOUT, "TestLabel");
    CHECK(success);

    auto overlay =
        shared_image_representation_factory_->ProduceOverlay(mailbox);
    overlay->SetCleared();
    return overlay;
  }

  gpu::Mailbox MakeOverlayMailbox() { return MakeOverlay()->mailbox(); }

 protected:
  std::unique_ptr<SkiaOutputSurfaceDependency> dependency_;
  scoped_refptr<MockPresenter> presenter_;
  std::unique_ptr<MemoryTrackerStub> memory_tracker_;
  TestImageBackingFactory test_backing_factory_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<SkiaOutputDeviceBufferQueue> output_device_;
  std::vector<gpu::SwapBuffersCompleteParams> params_;
  std::vector<std::vector<gpu::Mailbox>> released_overlays_params_;
  base::test::ScopedFeatureList feature_list{
      ::features::kDeferredOverlaysRelease};
  base::SimpleTestTickClock test_tick_clock_;
};

namespace {

SkiaOutputSurface::OverlayList MakeOverlayList(
    std::vector<gpu::Mailbox> mailboxes) {
  SkiaOutputSurface::OverlayList overlay_list;
  for (auto& mailbox : mailboxes) {
    OutputPresenter::OverlayPlaneCandidate overlay;
    overlay.mailbox = mailbox;
#if BUILDFLAG(IS_APPLE)
    overlay.transform = gfx::Transform();
#endif
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

    output_device_->ScheduleOverlays(MakeOverlayList({mailbox}));

    EXPECT_THAT(pending_overlay_mailboxes(), testing::ElementsAre(mailbox));

    // Do a swap then a commit for each overlay mailbox.
    Present();

    EXPECT_THAT(pending_overlay_mailboxes(), testing::IsEmpty());
    EXPECT_THAT(committed_overlay_mailboxes(), testing::ElementsAre(mailbox));

    PageFlipComplete();
  }
}

#if BUILDFLAG(IS_APPLE)
TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, ScheduleOverlaysStillInUse) {
  FirstReshape();

  std::unique_ptr<gpu::OverlayImageRepresentation> overlay_1 = MakeOverlay();
  std::unique_ptr<gpu::OverlayImageRepresentation> overlay_2 = MakeOverlay();

  output_device_->ScheduleOverlays(MakeOverlayList({overlay_1->mailbox()}));

  EXPECT_THAT(pending_overlay_mailboxes(),
              testing::ElementsAre(overlay_1->mailbox()));

  Present();
  EXPECT_THAT(pending_overlay_mailboxes(), testing::IsEmpty());
  EXPECT_THAT(committed_overlay_mailboxes(),
              testing::ElementsAre(overlay_1->mailbox()));

  PageFlipComplete();
  EXPECT_EQ(1u, params_.size());
  EXPECT_EQ(0u, params_[0].released_overlays.size());

  auto* overlay2 =
      static_cast<gpu::TestOverlayImageRepresentation*>(overlay_2.get());
  overlay2->MarkBackingInUse(true);

  output_device_->ScheduleOverlays(MakeOverlayList({overlay_2->mailbox()}));
  Present();
  PageFlipComplete();
  EXPECT_EQ(2u, params_.size());
  EXPECT_THAT(params_[1].released_overlays,
              testing::ElementsAre(overlay_1->mailbox()));

  // The overlay is still in use, cannot release it.
  output_device_->ScheduleOverlays(MakeOverlayList({overlay_1->mailbox()}));
  Present();
  PageFlipComplete();
  EXPECT_EQ(3u, params_.size());
  EXPECT_TRUE(params_[2].released_overlays.empty());

  // Now that the overlay is no longer in use, the next frame will release it.
  overlay2->MarkBackingInUse(false);
  output_device_->ScheduleOverlays(MakeOverlayList({overlay_1->mailbox()}));
  Present();
  PageFlipComplete();
  EXPECT_EQ(4u, params_.size());
  EXPECT_THAT(params_[3].released_overlays,
              testing::ElementsAre(overlay_2->mailbox()));
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, InUseOverlaysAreCollected) {
  output_device_->SetSwapTimeClockForTesting(&test_tick_clock_);
  FirstReshape();

  std::unique_ptr<gpu::OverlayImageRepresentation> overlay_1 = MakeOverlay();
  std::unique_ptr<gpu::OverlayImageRepresentation> overlay_2 = MakeOverlay();

  output_device_->ScheduleOverlays(MakeOverlayList({overlay_1->mailbox()}));

  EXPECT_THAT(pending_overlay_mailboxes(),
              testing::ElementsAre(overlay_1->mailbox()));

  Present();
  EXPECT_THAT(pending_overlay_mailboxes(), testing::IsEmpty());
  EXPECT_THAT(committed_overlay_mailboxes(),
              testing::ElementsAre(overlay_1->mailbox()));

  PageFlipComplete();
  EXPECT_EQ(1u, params_.size());
  EXPECT_EQ(0u, params_[0].released_overlays.size());

  auto* overlay2 =
      static_cast<gpu::TestOverlayImageRepresentation*>(overlay_2.get());
  overlay2->MarkBackingInUse(true);

  output_device_->ScheduleOverlays(MakeOverlayList({overlay_2->mailbox()}));
  Present();
  PageFlipComplete();
  EXPECT_EQ(2u, params_.size());
  EXPECT_THAT(params_[1].released_overlays,
              testing::ElementsAre(overlay_1->mailbox()));
  EXPECT_FALSE(output_device_->OverlaysReclaimTimerForTesting().IsRunning());

  // The overlay is still in use, cannot release it.
  output_device_->ScheduleOverlays(MakeOverlayList({overlay_1->mailbox()}));
  Present();
  PageFlipComplete();
  EXPECT_EQ(3u, params_.size());
  EXPECT_TRUE(params_[2].released_overlays.empty());
  EXPECT_TRUE(output_device_->OverlaysReclaimTimerForTesting().IsRunning());

  overlay2->MarkBackingInUse(false);

  // Not enough time since last commit, reschedule.
  test_tick_clock_.Advance(base::Milliseconds(1));
  output_device_->OverlaysReclaimTimerForTesting().FireNow();
  EXPECT_TRUE(output_device_->OverlaysReclaimTimerForTesting().IsRunning());

  // Now we can release it.
  test_tick_clock_.Advance(base::Seconds(1));
  output_device_->OverlaysReclaimTimerForTesting().FireNow();
  EXPECT_FALSE(output_device_->OverlaysReclaimTimerForTesting().IsRunning());
  EXPECT_EQ(1u, released_overlays_params_.size());
  EXPECT_THAT(released_overlays_params_[0],
              testing::ElementsAre(overlay_2->mailbox()));
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace
}  // namespace viz
