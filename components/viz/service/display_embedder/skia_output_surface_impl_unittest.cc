// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_implementation.h"

namespace viz {
namespace {

constexpr gfx::Rect kSurfaceRect(0, 0, 100, 100);
constexpr SkColor4f kOutputColor = SkColors::kRed;

}  // namespace

class SkiaOutputSurfaceImplTest : public testing::Test {
 public:
  SkiaOutputSurfaceImplTest();
  ~SkiaOutputSurfaceImplTest() override;

  GpuServiceImpl* GetGpuService() {
    return TestGpuServiceHolder::GetInstance()->gpu_service();
  }

  void SetUpSkiaOutputSurfaceImpl();

  // Paints and submits root RenderPass with a solid color rect of |size|.
  gpu::SyncToken PaintRootRenderPass(
      const gfx::Rect& output_rect,
      base::OnceClosure closure,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence);

  void CheckSyncTokenOnGpuThread(const gpu::SyncToken& sync_token);
  void CopyRequestCallbackOnGpuThread(const gfx::Rect& output_rect,
                                      const gfx::ColorSpace& color_space,
                                      std::unique_ptr<CopyOutputResult> result);
  void BlockMainThread();
  void UnblockMainThread();

 protected:
  DebugRendererSettings debug_settings_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  std::unique_ptr<DisplayCompositorMemoryAndTaskController> display_controller_;
  std::unique_ptr<SkiaOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  base::WaitableEvent wait_;
};

SkiaOutputSurfaceImplTest::SkiaOutputSurfaceImplTest()
    : wait_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
  SetUpSkiaOutputSurfaceImpl();
}

SkiaOutputSurfaceImplTest::~SkiaOutputSurfaceImplTest() {
  output_surface_.reset();
}

void SkiaOutputSurfaceImplTest::SetUpSkiaOutputSurfaceImpl() {
  auto skia_deps = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
      GetGpuService(), gpu::kNullSurfaceHandle);
  display_controller_ =
      std::make_unique<DisplayCompositorMemoryAndTaskController>(
          std::move(skia_deps));
  output_surface_ = SkiaOutputSurfaceImpl::Create(
      display_controller_.get(), RendererSettings(), &debug_settings_);
  output_surface_->BindToClient(&output_surface_client_);
}

gpu::SyncToken SkiaOutputSurfaceImplTest::PaintRootRenderPass(
    const gfx::Rect& output_rect,
    base::OnceClosure closure,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence) {
  SkPaint paint;
  paint.setColor(kOutputColor);
  SkCanvas* root_canvas = output_surface_->BeginPaintCurrentFrame();
  root_canvas->drawRect(gfx::RectToSkRect(output_rect), paint);
  output_surface_->EndPaint(std::move(closure), std::move(return_release_fence),
                            output_rect,
                            /*is_overlay=*/false);
  return output_surface_->Flush();
}

void SkiaOutputSurfaceImplTest::BlockMainThread() {
  wait_.Wait();
}

void SkiaOutputSurfaceImplTest::UnblockMainThread() {
  DCHECK(!wait_.IsSignaled());
  wait_.Signal();
}

void SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread(
    const gpu::SyncToken& sync_token) {
  EXPECT_TRUE(
      GetGpuService()->sync_point_manager()->IsSyncTokenReleased(sync_token));
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread(
    const gfx::Rect& output_rect,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputResult> result) {
  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  auto result_bitmap = scoped_bitmap.bitmap();
  EXPECT_EQ(result_bitmap.width(), output_rect.width());
  EXPECT_EQ(result_bitmap.height(), output_rect.height());

  SkBitmap expected;
  expected.allocPixels(SkImageInfo::MakeN32Premul(
      output_rect.width(), output_rect.height(), color_space.ToSkColorSpace()));
  expected.eraseColor(kOutputColor);

  EXPECT_TRUE(
      cc::MatchesBitmap(result_bitmap, expected, cc::ExactPixelComparator()));

  UnblockMainThread();
}

// TODO(crbug.com/40922049): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_EndPaint DISABLED_EndPaint
#else
#define MAYBE_EndPaint EndPaint
#endif
TEST_F(SkiaOutputSurfaceImplTest, MAYBE_EndPaint) {
  OutputSurface::ReshapeParams reshape_params;
  reshape_params.size = kSurfaceRect.size();
  output_surface_->Reshape(reshape_params);
  constexpr gfx::Rect output_rect(0, 0, 10, 10);

  bool on_finished_called = false;
  base::OnceClosure on_finished =
      base::BindOnce([](bool* result) { *result = true; }, &on_finished_called);
  base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb;

  gpu::SyncToken sync_token = PaintRootRenderPass(
      output_rect, std::move(on_finished), std::move(return_release_fence_cb));
  EXPECT_TRUE(sync_token.HasData());

  // Copy the output
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_rect, color_space));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_main_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  output_surface_->CopyOutput(geometry, color_space, std::move(request),
                              gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  BlockMainThread();

  // EndPaint draw is deferred until CopyOutput.
  base::OnceClosure closure =
      base::BindOnce(&SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread,
                     base::Unretained(this), sync_token);

  output_surface_->ScheduleGpuTaskForTesting(std::move(closure), {sync_token});
  BlockMainThread();

  // Let the cb to come back.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(on_finished_called);
}

// TODO(crbug.com/40922049): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_EndPaintReleaseFence DISABLED_EndPaintReleaseFence
#else
#define MAYBE_EndPaintReleaseFence EndPaintReleaseFence
#endif
TEST_F(SkiaOutputSurfaceImplTest, MAYBE_EndPaintReleaseFence) {
  // Skip test for Skia Graphite
  gpu::GrContextType gr_context_type =
      GetGpuService()->gpu_preferences().gr_context_type;
  if (gr_context_type == gpu::GrContextType::kGraphiteDawn) {
    GTEST_SKIP();
  }
  OutputSurface::ReshapeParams reshape_params;
  reshape_params.size = kSurfaceRect.size();
  output_surface_->Reshape(reshape_params);
  constexpr gfx::Rect output_rect(0, 0, 10, 10);

  bool on_finished_called = false;
  base::OnceClosure on_finished =
      base::BindOnce([](bool* result) { *result = true; }, &on_finished_called);
  base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb;

  bool on_return_release_fence_called = false;
  // This callback is unsupported when using Metal.
  return_release_fence_cb = base::BindOnce(
      [](bool* result, gfx::GpuFenceHandle handle) { *result = true; },
      &on_return_release_fence_called);

  gpu::SyncToken sync_token = PaintRootRenderPass(
      output_rect, std::move(on_finished), std::move(return_release_fence_cb));
  EXPECT_TRUE(sync_token.HasData());

  // Copy the output
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_rect, color_space));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_main_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  output_surface_->CopyOutput(geometry, color_space, std::move(request),
                              gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  BlockMainThread();

  // EndPaint draw is deferred until CopyOutput.
  base::OnceClosure closure =
      base::BindOnce(&SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread,
                     base::Unretained(this), sync_token);

  output_surface_->ScheduleGpuTaskForTesting(std::move(closure), {sync_token});
  BlockMainThread();

  // Let the cb to come back.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(on_finished_called);
  EXPECT_TRUE(on_return_release_fence_called);
}

// Draws two frames and calls Reshape() between the two frames changing the
// color space. Verifies draw after color space change is successful.
TEST_F(SkiaOutputSurfaceImplTest, SupportsColorSpaceChange) {
  for (auto& color_space : {gfx::ColorSpace(), gfx::ColorSpace::CreateSRGB()}) {
    OutputSurface::ReshapeParams reshape_params;
    reshape_params.size = kSurfaceRect.size();
    reshape_params.color_space = color_space;
    output_surface_->Reshape(reshape_params);

    // Draw something, it's not important what.
    base::RunLoop run_loop;
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb;
    PaintRootRenderPass(kSurfaceRect, run_loop.QuitClosure(),
                        std::move(return_release_fence_cb));

    OutputSurfaceFrame frame;
    frame.size = kSurfaceRect.size();
    output_surface_->SwapBuffers(std::move(frame));
    output_surface_->Flush();

    // TODO(crbug.com/40279197): We should not need to poll in this test.
    while (!run_loop.AnyQuitCalled()) {
      run_loop.RunUntilIdle();
      output_surface_->CheckAsyncWorkCompletionForTesting();
    }
  }
}

// TODO(crbug.com/40922049): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_CopyOutputBitmapSupportedColorSpace \
  DISABLED_CopyOutputBitmapSupportedColorSpace
#else
#define MAYBE_CopyOutputBitmapSupportedColorSpace \
  CopyOutputBitmapSupportedColorSpace
#endif
// Tests that the destination color space is preserved across a CopyOutput for
// ColorSpaces supported by SkColorSpace.
TEST_F(SkiaOutputSurfaceImplTest, MAYBE_CopyOutputBitmapSupportedColorSpace) {
  OutputSurface::ReshapeParams reshape_params;
  reshape_params.size = kSurfaceRect.size();
  output_surface_->Reshape(reshape_params);

  constexpr gfx::Rect output_rect(0, 0, 10, 10);
  const gfx::ColorSpace color_space = gfx::ColorSpace(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::LINEAR);
  base::RunLoop run_loop;
  std::unique_ptr<CopyOutputResult> result;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](std::unique_ptr<CopyOutputResult>* result_out,
             base::OnceClosure quit_closure,
             std::unique_ptr<CopyOutputResult> tmp_result) {
            *result_out = std::move(tmp_result);
            std::move(quit_closure).Run();
          },
          &result, run_loop.QuitClosure()));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_main_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb;
  PaintRootRenderPass(kSurfaceRect, base::DoNothing(),
                      std::move(return_release_fence_cb));
  output_surface_->CopyOutput(geometry, color_space, std::move(request),
                              gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  run_loop.Run();

  EXPECT_EQ(color_space, result->GetRGBAColorSpace());
}

// Tests that copying from a source with a color space that can't be converted
// to a SkColorSpace will fallback to a transform to sRGB.
TEST_F(SkiaOutputSurfaceImplTest, CopyOutputBitmapUnsupportedColorSpace) {
  OutputSurface::ReshapeParams reshape_params;
  reshape_params.size = kSurfaceRect.size();
  output_surface_->Reshape(reshape_params);

  constexpr gfx::Rect output_rect(0, 0, 10, 10);
  const gfx::ColorSpace color_space =
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::LOG_SQRT);
  base::RunLoop run_loop;
  std::unique_ptr<CopyOutputResult> result;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](std::unique_ptr<CopyOutputResult>* result_out,
             base::OnceClosure quit_closure,
             std::unique_ptr<CopyOutputResult> tmp_result) {
            *result_out = std::move(tmp_result);
            std::move(quit_closure).Run();
          },
          &result, run_loop.QuitClosure()));
  request->set_result_task_runner(
      TestGpuServiceHolder::GetInstance()->gpu_main_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = output_rect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = output_rect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb;
  PaintRootRenderPass(kSurfaceRect, base::DoNothing(),
                      std::move(return_release_fence_cb));
  output_surface_->CopyOutput(geometry, color_space, std::move(request),
                              gpu::Mailbox());
  output_surface_->SwapBuffersSkipped(kSurfaceRect);
  output_surface_->Flush();
  run_loop.Run();

  EXPECT_EQ(gfx::ColorSpace::CreateSRGB(), result->GetRGBAColorSpace());
}

struct PackedYuvTestParams {
  SharedImageFormat format;
  // The color type of the aliased view that Ganesh samples the texture as.
  SkColorType view_color_type;
};

class SkiaOutputSurfaceImplPackedYuvTest
    : public SkiaOutputSurfaceImplTest,
      public testing::WithParamInterface<PackedYuvTestParams> {
 protected:
  // Returns whether Ganesh can sample textures of `color_type`. SwiftShader,
  // for example, can't sample R16G16B16A16 (no EXT_texture_norm16).
  bool IsTexturable(SkColorType color_type) {
    bool texturable = false;
    TestGpuServiceHolder::GetInstance()->ScheduleGpuMainTask(
        base::BindLambdaForTesting([&]() {
          GrDirectContext* gr_context =
              GetGpuService()->GetContextState()->gr_context();
          texturable =
              gr_context &&
              gr_context->defaultBackendFormat(color_type, GrRenderable::kNo)
                  .isValid();
          UnblockMainThread();
        }));
    BlockMainThread();
    return texturable;
  }

  // Returns the promise image for a resource of the test format and `size`.
  sk_sp<SkImage> MakePromiseImage(const gfx::Size& size) {
    OutputSurface::ReshapeParams reshape_params;
    reshape_params.size = kSurfaceRect.size();
    output_surface_->Reshape(reshape_params);

    gpu::SharedImageMetadata metadata;
    metadata.format = GetParam().format;
    metadata.size = size;
    metadata.color_space = gfx::ColorSpace::CreateREC709();
    metadata.surface_origin = kTopLeft_GrSurfaceOrigin;
    metadata.alpha_type = kOpaque_SkAlphaType;
    metadata.usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    auto resource = TransferableResource::Make(
        gpu::ClientSharedImage::CreateForTesting(metadata),
        TransferableResource::ResourceSource::kTest, gpu::SyncToken());
    std::unique_ptr<ExternalUseClient::ImageContext> image_context =
        output_surface_->CreateImageContext(resource,
                                            /*maybe_concurrent_reads=*/false,
                                            /*client_id=*/0);

    output_surface_->BeginPaintCurrentFrame();
    output_surface_->MakePromiseSkImage(image_context.get(),
                                        /*force_rgbx=*/false);
    sk_sp<SkImage> image = image_context->image();
    output_surface_->EndPaint(base::DoNothing(),
                              base::OnceCallback<void(gfx::GpuFenceHandle)>(),
                              kSurfaceRect, /*is_overlay=*/false);
    output_surface_->SwapBuffersSkipped(kSurfaceRect);
    std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
        image_contexts;
    image_contexts.push_back(std::move(image_context));
    output_surface_->ReleaseImageContexts(std::move(image_contexts));
    return image;
  }
};

// Packed YUV formats are sampled through a YUVA promise image with a single
// plane. Graphite has no packed YUV path yet, so no image is created there and
// SkiaRenderer skips the quad.
TEST_P(SkiaOutputSurfaceImplPackedYuvTest, MakePromiseSkImage) {
  const gpu::GrContextType gr_context_type =
      GetGpuService()->gpu_preferences().gr_context_type;
  const bool is_graphite =
      gr_context_type == gpu::GrContextType::kGraphiteDawn ||
      gr_context_type == gpu::GrContextType::kGraphiteVulkan;
  if (gr_context_type != gpu::GrContextType::kGL && !is_graphite) {
    GTEST_SKIP() << "Packed YUV is only sampled with Ganesh GL";
  }
  const bool expect_image =
      !is_graphite && IsTexturable(GetParam().view_color_type);

  constexpr gfx::Size kImageSize(64, 48);
  sk_sp<SkImage> image = MakePromiseImage(kImageSize);
  if (!expect_image) {
    EXPECT_FALSE(image);
    return;
  }
  ASSERT_TRUE(image);
  EXPECT_TRUE(image->isTextureBacked());
  EXPECT_EQ(image->dimensions(), gfx::SizeToSkISize(kImageSize));
  // A YUVA image reports the RGBA color type that it's converted to, unlike a
  // single-plane promise image, which reports the plane's color type.
  EXPECT_EQ(image->colorType(), kRGBA_8888_SkColorType);
}

TEST_P(SkiaOutputSurfaceImplPackedYuvTest, MakePromiseSkImageOddWidth) {
  if (GetGpuService()->gpu_preferences().gr_context_type !=
      gpu::GrContextType::kGL) {
    GTEST_SKIP() << "Packed YUV is only sampled with Ganesh GL";
  }
  EXPECT_FALSE(MakePromiseImage(gfx::Size(63, 48)));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SkiaOutputSurfaceImplPackedYuvTest,
    testing::Values(PackedYuvTestParams{SinglePlaneFormat::kY210,
                                        kR16G16B16A16_unorm_SkColorType}),
    [](const testing::TestParamInfo<PackedYuvTestParams>& info) {
      return info.param.format.ToString();
    });

}  // namespace viz
