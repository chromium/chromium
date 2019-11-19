// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
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
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/vulkan/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/tests/native_window.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace viz {

const gfx::Rect kSurfaceRect(0, 0, 100, 100);

static void ExpectEquals(SkBitmap actual, SkBitmap expected) {
  EXPECT_EQ(actual.dimensions(), expected.dimensions());
  auto expected_url = cc::GetPNGDataUrl(expected);
  auto actual_url = cc::GetPNGDataUrl(actual);
  EXPECT_TRUE(actual_url == expected_url);
}

class SkiaOutputSurfaceImplTest : public testing::TestWithParam<bool> {
 public:
  void CheckSyncTokenOnGpuThread(const gpu::SyncToken& sync_token);
  void CopyRequestCallbackOnGpuThread(const SkColor output_color,
                                      const gfx::Rect& output_rect,
                                      const gfx::ColorSpace& color_space,
                                      std::unique_ptr<CopyOutputResult> result);

 protected:
  SkiaOutputSurfaceImplTest()
      : output_surface_client_(std::make_unique<cc::FakeOutputSurfaceClient>()),
        wait_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED),
        on_screen_(GetParam()) {}
  inline void SetUp() override { SetUpSkiaOutputSurfaceImpl(); }
  void TearDown() override;
  void BlockMainThread();
  void UnblockMainThread();

  GpuServiceImpl* gpu_service() { return gpu_service_holder_->gpu_service(); }

  TestGpuServiceHolder* gpu_service_holder_;
  std::unique_ptr<SkiaOutputSurface> output_surface_;

 private:
  void SetUpSkiaOutputSurfaceImpl();

  std::unique_ptr<cc::FakeOutputSurfaceClient> output_surface_client_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  base::WaitableEvent wait_;
  const bool on_screen_;
};

void SkiaOutputSurfaceImplTest::BlockMainThread() {
  wait_.Wait();
}

void SkiaOutputSurfaceImplTest::UnblockMainThread() {
  DCHECK(!wait_.IsSignaled());
  wait_.Signal();
}

void SkiaOutputSurfaceImplTest::TearDown() {
  output_surface_.reset();
  scoped_feature_list_.reset();
}

void SkiaOutputSurfaceImplTest::SetUpSkiaOutputSurfaceImpl() {
  // SkiaOutputSurfaceImplOnGpu requires UseSkiaRenderer.
  const char enable_features[] = "VizDisplayCompositor,UseSkiaRenderer";
  const char disable_features[] = "";
  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitFromCommandLine(enable_features, disable_features);
  gpu_service_holder_ = TestGpuServiceHolder::GetInstance();

  // Set up the SkiaOutputSurfaceImpl.
  gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
  if (on_screen_) {
#if BUILDFLAG(ENABLE_VULKAN) && defined(USE_X11)
    surface_handle_ = gpu::CreateNativeWindow(kSurfaceRect);
#else
    // TODO(backer): Support other platforms.
    NOTREACHED();
#endif
  }
  output_surface_ = SkiaOutputSurfaceImpl::Create(
      std::make_unique<SkiaOutputSurfaceDependencyImpl>(gpu_service(),
                                                        surface_handle_),
      RendererSettings());
  output_surface_->BindToClient(output_surface_client_.get());
}

void SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread(
    const gpu::SyncToken& sync_token) {
  EXPECT_TRUE(
      gpu_service()->sync_point_manager()->IsSyncTokenReleased(sync_token));
  UnblockMainThread();
}

void SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread(
    const SkColor output_color,
    const gfx::Rect& output_rect,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputResult> result) {
  std::unique_ptr<SkBitmap> result_bitmap;
  result_bitmap = std::make_unique<SkBitmap>(result->AsSkBitmap());
  EXPECT_EQ(result_bitmap->width(), output_rect.width());
  EXPECT_EQ(result_bitmap->height(), output_rect.height());

  std::vector<SkPMColor> expected_pixels(
      output_rect.width() * output_rect.height(),
      SkPreMultiplyColor(output_color));
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(output_rect.width(), output_rect.height(),
                                 color_space.ToSkColorSpace()),
      expected_pixels.data(), output_rect.width() * sizeof(SkColor));
  ExpectEquals(*result_bitmap.get(), expected);

  UnblockMainThread();
}

INSTANTIATE_TEST_SUITE_P(SkiaOutputSurfaceImplTest,
                         SkiaOutputSurfaceImplTest,
#if BUILDFLAG(ENABLE_VULKAN) && defined(USE_X11)
                         ::testing::Values(false, true)
#else
                         ::testing::Values(false)
#endif
);

TEST_P(SkiaOutputSurfaceImplTest, SubmitPaint) {
  output_surface_->Reshape(kSurfaceRect.size(), 1, gfx::ColorSpace(),
                           /*has_alpha=*/false, /*use_stencil=*/false);
  SkCanvas* root_canvas = output_surface_->BeginPaintCurrentFrame();
  SkPaint paint;
  const SkColor output_color = SK_ColorRED;
  const gfx::Rect output_rect(0, 0, 10, 10);
  paint.setColor(output_color);
  SkRect rect = SkRect::MakeWH(output_rect.width(), output_rect.height());
  root_canvas->drawRect(rect, paint);

  bool on_finished_called = false;
  base::OnceClosure on_finished =
      base::BindOnce([](bool* result) { *result = true; }, &on_finished_called);

  gpu::SyncToken sync_token =
      output_surface_->SubmitPaint(std::move(on_finished));
  EXPECT_TRUE(sync_token.HasData());

  // Copy the output
  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&SkiaOutputSurfaceImplTest::CopyRequestCallbackOnGpuThread,
                     base::Unretained(this), output_color, output_rect,
                     color_space));
  request->set_result_task_runner(
      gpu_service_holder_->gpu_thread_task_runner());
  copy_output::RenderPassGeometry geometry;
  geometry.result_bounds = kSurfaceRect;
  geometry.result_selection = output_rect;
  geometry.sampling_bounds = kSurfaceRect;
  geometry.readback_offset = gfx::Vector2d(0, 0);

  output_surface_->CopyOutput(0, geometry, color_space, std::move(request));
  BlockMainThread();

  // SubmitPaint draw is deferred until CopyOutput.
  base::OnceClosure closure =
      base::BindOnce(&SkiaOutputSurfaceImplTest::CheckSyncTokenOnGpuThread,
                     base::Unretained(this), sync_token);

  output_surface_->ScheduleGpuTaskForTesting(std::move(closure), {sync_token});
  BlockMainThread();
  EXPECT_TRUE(on_finished_called);
}

// Draws two frames and calls Reshape() between the two frames changing the
// color space. Verifies draw after color space change is successful.
TEST_P(SkiaOutputSurfaceImplTest, SupportsColorSpaceChange) {
  for (auto& color_space : {gfx::ColorSpace(), gfx::ColorSpace::CreateSRGB()}) {
    output_surface_->Reshape(kSurfaceRect.size(), 1, color_space,
                             /*has_alpha=*/false, /*use_stencil=*/false);

    // Draw something, it's not important what.
    SkCanvas* root_canvas = output_surface_->BeginPaintCurrentFrame();
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    root_canvas->drawRect(SkRect::MakeWH(10, 10), paint);

    base::RunLoop run_loop;
    output_surface_->SubmitPaint(run_loop.QuitClosure());

    OutputSurfaceFrame frame;
    frame.size = kSurfaceRect.size();
    output_surface_->SkiaSwapBuffers(std::move(frame),
                                     /*wants_sync_token=*/false);

    run_loop.Run();
  }
}

}  // namespace viz
