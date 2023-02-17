// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/surface_origin.h"
#include "ui/gl/dcomp_presenter.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/presenter.h"

namespace viz {

namespace {

class TestSharedImageBackingFactory : public gpu::SharedImageBackingFactory {
 public:
  TestSharedImageBackingFactory() : SharedImageBackingFactory(kUsageAll) {}

  MOCK_METHOD(std::unique_ptr<gpu::SharedImageBacking>,
              CreateSharedImage,
              (const gpu::Mailbox& mailbox,
               SharedImageFormat format,
               gpu::SurfaceHandle surface_handle,
               const gfx::Size& size,
               const gfx::ColorSpace& color_space,
               GrSurfaceOrigin surface_origin,
               SkAlphaType alpha_type,
               uint32_t usage,
               bool is_thread_safe),
              (override));

  void SetCreateSharedImageSuccessByDefault() {
    ON_CALL(*this, CreateSharedImage)
        .WillByDefault(
            [](const gpu::Mailbox& mailbox, SharedImageFormat format,
               gpu::SurfaceHandle surface_handle, const gfx::Size& size,
               const gfx::ColorSpace& color_space,
               GrSurfaceOrigin surface_origin, SkAlphaType alpha_type,
               uint32_t usage, bool is_thread_safe) {
              return std::make_unique<gpu::TestImageBacking>(
                  mailbox, format, size, color_space, surface_origin,
                  alpha_type, usage, 1);
            });
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
    NOTREACHED();
    return nullptr;
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
      SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      gfx::GpuMemoryBufferHandle handle) override {
    NOTREACHED();
    return nullptr;
  }
  std::unique_ptr<gpu::SharedImageBacking> CreateSharedImage(
      const gpu::Mailbox& mailbox,
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
};

// No-op surface compatible with SkiaOutputDeviceDCompPresenter
class NoopDCompPresenter : public gl::Presenter {
 public:
  NoopDCompPresenter()
      : gl::Presenter(gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size(1, 1)) {}

  bool SupportsDCLayers() const override { return true; }
  bool SupportsGpuVSync() const override { return true; }
  bool SupportsCommitOverlayPlanes() override { return false; }

  bool SetDrawRectangle(const gfx::Rect& rectangle) override { return true; }

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override {
    NOTREACHED();
  }

 protected:
  ~NoopDCompPresenter() override = default;
};

}  // namespace

class SkiaOutputDeviceDCompTest : public testing::Test {
 public:
  SkiaOutputDeviceDCompTest() {}

 protected:
  void SetUp() override {
    gpu::GpuDriverBugWorkarounds workarounds;
    gpu::GpuPreferences gpu_preferences;
    gpu_preferences.use_passthrough_cmd_decoder = true;
    gpu_preferences.enable_gpu_debugging = true;
    gpu::GpuFeatureInfo gpu_feature_info;

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    scoped_refptr<gl::GLSurface> surface = gl::init::CreateOffscreenGLSurface(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size());

    gpu::ContextCreationAttribs attribs_helper;
    attribs_helper.context_type = gpu::CONTEXT_TYPE_OPENGLES3;
    gl::GLContextAttribs attribs = gpu::gles2::GenerateGLContextAttribs(
        attribs_helper, gpu_preferences.use_passthrough_cmd_decoder);
    attribs.can_skip_validation = false;
    scoped_refptr<gl::GLContext> context =
        gl::init::CreateGLContext(share_group.get(), surface.get(), attribs);
    ASSERT_NE(nullptr, context);
    ASSERT_EQ(context->share_group(), share_group.get());
    gpu_feature_info.ApplyToGLContext(context.get());
    auto feature_info = base::MakeRefCounted<gpu::gles2::FeatureInfo>(
        workarounds, gpu_feature_info);
    ASSERT_TRUE(context->MakeCurrent(surface.get()));

    context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
        std::move(share_group), surface, std::move(context),
        false /* use_virtualized_gl_contexts */, base::DoNothing());
    ASSERT_TRUE(context_state_->MakeCurrent(surface.get()));
    ASSERT_TRUE(context_state_->InitializeGL(gpu_preferences, feature_info));
    ASSERT_TRUE(context_state_->InitializeGrContext(
        gpu_preferences, workarounds, /*cache=*/nullptr));
    EXPECT_EQ(gl::GetGLImplementation(), gl::kGLImplementationEGLANGLE);

    shared_image_factory_ = std::make_unique<gpu::SharedImageFactory>(
        gpu_preferences, workarounds, gpu_feature_info, context_state_.get(),
        &shared_image_manager_, nullptr,
        /*is_for_display_compositor=*/false);

    test_shared_image_factory_ =
        std::make_unique<TestSharedImageBackingFactory>();
    test_shared_image_factory_->SetCreateSharedImageSuccessByDefault();
    shared_image_factory_->RegisterSharedImageBackingFactoryForTesting(
        test_shared_image_factory_.get());

    shared_image_representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);

    surface_ = base::MakeRefCounted<NoopDCompPresenter>();

    output_device_ = base::WrapUnique(new SkiaOutputDeviceDCompPresenter(
        shared_image_factory_.get(), shared_image_representation_factory_.get(),
        context_state_.get(), surface_, feature_info, nullptr,
        base::DoNothing()));
  }

  void TearDown() override {
    output_device_.reset();
    surface_.reset();
    context_state_.reset();
    test_shared_image_factory_.reset();
    shared_image_factory_.reset();
    shared_image_representation_factory_.reset();
  }

  void Reshape(const gfx::Size size,
               SkColorType color_type,
               bool has_alpha,
               const gfx::ColorSpace& color_space) {
    sk_sp<GrContextThreadSafeProxy> gr_thread_safe_proxy =
        context_state_->gr_context()->threadSafeProxy();

    SkImageInfo image_info = SkImageInfo::Make(
        SkISize::Make(size.width(), size.height()), color_type,
        has_alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType);
    GrBackendFormat backend_format = gr_thread_safe_proxy->defaultBackendFormat(
        color_type, GrRenderable::kYes);

    SkSurfaceCharacterization characterization =
        gr_thread_safe_proxy->createCharacterization(
            context_state_->gr_context()->getResourceCacheLimit(), image_info,
            backend_format, 1, kTopLeft_GrSurfaceOrigin, SkSurfaceProps(),
            false);
    EXPECT_TRUE(
        output_device_->Reshape(characterization, color_space, 1.0,
                                gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE));

    last_reshape_size_ = size;
  }

  // Do a fake draw to force the root surface to allocate.
  void EnsureRootSurfaceAllocated() {
    EXPECT_TRUE(
        output_device_->SetDrawRectangle(gfx::Rect(last_reshape_size_)));
    std::vector<GrBackendSemaphore> end_semaphores;
    EXPECT_NE(nullptr, output_device_->BeginPaint(&end_semaphores));
    EXPECT_EQ(0u, end_semaphores.size());
    output_device_->EndPaint();
  }

  std::unique_ptr<SkiaOutputDeviceDCompPresenter> output_device_;

  std::unique_ptr<TestSharedImageBackingFactory> test_shared_image_factory_;

 private:
  // Store the last size passed to Reshape so that EnsureRootSurfaceAllocated
  // knows what update_rect to pass.
  gfx::Size last_reshape_size_;

  scoped_refptr<gpu::SharedContextState> context_state_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  gpu::SharedImageManager shared_image_manager_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  scoped_refptr<NoopDCompPresenter> surface_;
};

// Tests that switching using EnableDCLayers works.
TEST_F(SkiaOutputDeviceDCompTest, DXGIDCLayerSwitch) {
  Reshape(gfx::Size(100, 100), kRGBA_8888_SkColorType, true,
          gfx::ColorSpace::CreateSRGB());

  // Check we allocate a a DXGI swap chain when asked.
  output_device_->SetEnableDCLayers(false);
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());
  EXPECT_CALL(*test_shared_image_factory_,
              CreateSharedImage(testing::_, testing::_, testing::_, testing::_,
                                testing::_, testing::_, kPremul_SkAlphaType,
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                    gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                                    gpu::SHARED_IMAGE_USAGE_SCANOUT,
                                testing::_));
  EnsureRootSurfaceAllocated();
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());

  // Not changing the DC layer state should not affect anything. Note that since
  // CreateSharedImage is mocked, EnsureRootSurfaceAllocated will cause the test
  // to fail if it calls CreateSharedImage unexpectedly.
  output_device_->SetEnableDCLayers(false);
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());
  EnsureRootSurfaceAllocated();

  // Check that switching DC layer state releases the root surface and allocated
  // a DComp surface
  output_device_->SetEnableDCLayers(true);
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());
  EXPECT_CALL(*test_shared_image_factory_,
              CreateSharedImage(
                  testing::_, testing::_, testing::_, testing::_, testing::_,
                  testing::_, SkAlphaType::kPremul_SkAlphaType,
                  gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                      gpu::SHARED_IMAGE_USAGE_SCANOUT |
                      gpu::SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE,
                  testing::_));
  EnsureRootSurfaceAllocated();
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());

  // Not changing the DC layer state should not affect anything
  output_device_->SetEnableDCLayers(true);
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());
  EnsureRootSurfaceAllocated();

  // Check that we can switch back to a swap chain
  output_device_->SetEnableDCLayers(false);
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());
  EXPECT_CALL(*test_shared_image_factory_,
              CreateSharedImage(testing::_, testing::_, testing::_, testing::_,
                                testing::_, testing::_,
                                SkAlphaType::kPremul_SkAlphaType,
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                    gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                                    gpu::SHARED_IMAGE_USAGE_SCANOUT,
                                testing::_));
  EnsureRootSurfaceAllocated();
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());
}

// Check that Reshape only destroys the root surface when its parameters change.
TEST_F(SkiaOutputDeviceDCompTest, Reshape) {
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());

  Reshape(gfx::Size(100, 100), kRGBA_8888_SkColorType, true,
          gfx::ColorSpace::CreateSRGB());
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());

  EnsureRootSurfaceAllocated();
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());

  // No parameters changed, root surface should remain allocated
  Reshape(gfx::Size(100, 100), kRGBA_8888_SkColorType, true,
          gfx::ColorSpace::CreateSRGB());
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());

  // A change in parameters results in the root surface being deallocated
  Reshape(gfx::Size(100, 100), kRGBA_8888_SkColorType, false,
          gfx::ColorSpace::CreateSRGB());
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());
  EnsureRootSurfaceAllocated();
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());
}

// Check that we fallback to a swap chain when we want a DComp surface with an
// unsupported pixel format.
TEST_F(SkiaOutputDeviceDCompTest, HDR10ColorSpaceForcesSwapChain) {
  output_device_->SetEnableDCLayers(true);
  Reshape(gfx::Size(100, 100), kRGBA_1010102_SkColorType, true,
          gfx::ColorSpace::CreateHDR10());
  EXPECT_CALL(*test_shared_image_factory_,
              CreateSharedImage(testing::_, testing::_, testing::_, testing::_,
                                testing::_, testing::_,
                                SkAlphaType::kPremul_SkAlphaType,
                                gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                    gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                                    gpu::SHARED_IMAGE_USAGE_SCANOUT,
                                testing::_));
  EXPECT_FALSE(output_device_->IsRootSurfaceAllocatedForTesting());
  EnsureRootSurfaceAllocated();
  EXPECT_TRUE(output_device_->IsRootSurfaceAllocatedForTesting());
}

}  // namespace viz
