// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

#if !BUILDFLAG(SKIA_USE_METAL)
  bool on_return_release_fence_called = false;
  // This callback is unsupported when using Metal.
  return_release_fence_cb = base::BindOnce(
      [](bool* result, gfx::GpuFenceHandle handle) { *result = true; },
      &on_return_release_fence_called);
#endif  // !BUILDFLAG(SKIA_USE_METAL)

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
#if !BUILDFLAG(SKIA_USE_METAL)
  EXPECT_TRUE(on_return_release_fence_called);
#endif  // !BUILDFLAG(SKIA_USE_METAL)
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
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreatePiecewiseHDR(
      gfx::ColorSpace::PrimaryID::BT2020, 0.5, 1.5);
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

}  // namespace viz
