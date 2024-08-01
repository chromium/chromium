// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/render_pass_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace viz {
namespace {

class CopyOutputScalingPixelTest
    : public VizPixelTest,
      public testing::WithParamInterface<std::tuple<RendererType,
                                                    gfx::Vector2d,
                                                    gfx::Vector2d,
                                                    CopyOutputResult::Format>> {
 public:
  CopyOutputScalingPixelTest() : VizPixelTest(std::get<0>(GetParam())) {}

  DirectRenderer* renderer() { return renderer_.get(); }

  void SetUp() override {
    VizPixelTest::SetUp();
    scale_from_ = std::get<1>(GetParam());
    scale_to_ = std::get<2>(GetParam());
    result_format_ = std::get<3>(GetParam());
  }

  // This tests that copy requests requesting scaled results execute correctly.
  // The test procedure creates a scene similar to the wall art that can be
  // found in the stairwell of a certain Google office building: A white
  // background" (W=white) and four blocks of different colors (r=red, g=green,
  // b=blue, y=yellow).
  //
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWrrrrWWWWWWWWggggWWWW
  //   WWWWrrrrWWWWWWWWggggWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWbbbbWWWWWWWWyyyyWWWW
  //   WWWWbbbbWWWWWWWWyyyyWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //   WWWWWWWWWWWWWWWWWWWWWWWW
  //
  // The scene is drawn, which also causes the copy request to execute. Then,
  // the resulting bitmap is compared against an expected bitmap.
  void RunTest() {
    // TODO(b/297344089): Enable these tests once Skia Graphite supports stale
    // mipmap regeneration.
    if (is_skia_graphite()) {
      GTEST_SKIP();
    }

    const char* result_format_as_str = "<unknown>";

    // Tests only issue requests for system-memory destinations, no need to
    // take the destination into account:
    if (result_format_ == CopyOutputResult::Format::RGBA)
      result_format_as_str = "RGBA";
    else if (result_format_ == CopyOutputResult::Format::I420_PLANES)
      result_format_as_str = "I420_PLANES";
    else
      NOTIMPLEMENTED();
    SCOPED_TRACE(testing::Message()
                 << "scale_from=" << scale_from_.ToString()
                 << ", scale_to=" << scale_to_.ToString()
                 << ", result_format=" << result_format_as_str);

    // These need to be large enough to prevent odd-valued coordinates when
    // testing I420_PLANES requests. The requests would still work with
    // odd-valued coordinates, but the pixel comparisons at the end of the test
    // will fail due to off-by-one chroma reconstruction. That behavior is WAI,
    // though, since clients making CopyOutputRequests should always align to
    // even-valued coordinates!
    constexpr gfx::Size viewport_size = gfx::Size(48, 20);
    constexpr int x_block = 8;
    constexpr int y_block = 4;
    constexpr SkColor4f smaller_pass_colors[4] = {
        SkColors::kRed, SkColors::kGreen, SkColors::kBlue, SkColors::kYellow};
    constexpr SkColor4f root_pass_color = SkColors::kWhite;

    AggregatedRenderPassList list;

    // Create the render passes drawn on top of the root render pass.
    AggregatedRenderPass* smaller_passes[4];
    gfx::Rect smaller_pass_rects[4];
    AggregatedRenderPassId pass_id{5};
    for (int i = 0; i < 4;
         ++i, pass_id = AggregatedRenderPassId{pass_id.value() - 1}) {
      smaller_pass_rects[i] = gfx::Rect(
          i % 2 == 0 ? x_block : (viewport_size.width() - 2 * x_block),
          i / 2 == 0 ? y_block : (viewport_size.height() - 2 * y_block),
          x_block, y_block);
      smaller_passes[i] =
          AddRenderPass(&list, pass_id, smaller_pass_rects[i], gfx::Transform(),
                        cc::FilterOperations());
      cc::AddQuad(smaller_passes[i], smaller_pass_rects[i],
                  smaller_pass_colors[i]);
    }

    // Create the root render pass and add all the child passes to it.
    auto* root_pass =
        cc::AddRenderPass(&list, pass_id, gfx::Rect(viewport_size),
                          gfx::Transform(), cc::FilterOperations());
    for (int i = 0; i < 4; ++i)
      cc::AddRenderPassQuad(root_pass, smaller_passes[i]);
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), root_pass_color);

    // Make a copy request and execute it by drawing a frame. A subset of the
    // viewport is requested, to test that scaled offsets are being computed
    // correctly as well.
    const gfx::Rect copy_rect(x_block, y_block,
                              viewport_size.width() - 2 * x_block,
                              viewport_size.height() - 2 * y_block);
    std::unique_ptr<CopyOutputResult> result;
    {
      base::RunLoop loop;

      // Add a dummy copy request to be executed when the RED RenderPass is
      // drawn (before the root RenderPass). This is a regression test to
      // confirm GLRenderer state is consistent with the GL context after each
      // copy request executes, and before the next RenderPass is drawn.
      // http://crbug.com/792734
      bool dummy_ran = false;
      auto request = std::make_unique<CopyOutputRequest>(
          result_format_, CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              [](bool* dummy_ran, std::unique_ptr<CopyOutputResult> result) {
                EXPECT_TRUE(!result->IsEmpty());
                EXPECT_FALSE(*dummy_ran);
                *dummy_ran = true;
              },
              &dummy_ran));
      // Set a 10X zoom, which should be more than sufficient to disturb the
      // results of the main copy request (below) if the GL state is not
      // properly restored.
      request->SetUniformScaleRatio(1, 10);
      // Ensure the result callback is run on test main thread.
      request->set_result_task_runner(
          base::SequencedTaskRunner::GetCurrentDefault());
      list.front()->copy_requests.push_back(std::move(request));

      // Add a copy request to the root RenderPass, to capture the results of
      // drawing all passes for this frame.
      request = std::make_unique<CopyOutputRequest>(
          result_format_, CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              [](bool* dummy_ran,
                 std::unique_ptr<CopyOutputResult>* test_result,
                 const base::RepeatingClosure& quit_closure,
                 std::unique_ptr<CopyOutputResult> result_from_renderer) {
                EXPECT_TRUE(*dummy_ran);
                *test_result = std::move(result_from_renderer);
                quit_closure.Run();
              },
              &dummy_ran, &result, loop.QuitClosure()));
      request->set_result_selection(
          copy_output::ComputeResultRect(copy_rect, scale_from_, scale_to_));
      request->SetScaleRatio(scale_from_, scale_to_);
      // Ensure the result callback is run on test main thread.
      request->set_result_task_runner(
          base::SequencedTaskRunner::GetCurrentDefault());
      list.back()->copy_requests.push_back(std::move(request));

      SurfaceDamageRectList surface_damage_rect_list;
      renderer()->DrawFrame(&list, 1.0f, viewport_size,
                            gfx::DisplayColorSpaces(),
                            std::move(surface_damage_rect_list));
      // Call SwapBuffersSkipped(), so the renderer can release related
      // resources.
      renderer()->SwapBuffersSkipped();
      loop.Run();
    }

    // Check that the result succeeded and provides a bitmap of the expected
    // size.
    const gfx::Rect expected_result_rect =
        copy_output::ComputeResultRect(copy_rect, scale_from_, scale_to_);
    EXPECT_EQ(expected_result_rect, result->rect());
    EXPECT_EQ(result_format_, result->format());
    std::optional<CopyOutputResult::ScopedSkBitmap> scoped_bitmap;
    SkBitmap result_bitmap;
    if (result_format_ == CopyOutputResult::Format::I420_PLANES) {
      result_bitmap = ReadI420ResultToSkBitmap(*result);
    } else {
      scoped_bitmap = result->ScopedAccessSkBitmap();
      result_bitmap = scoped_bitmap->bitmap();
    }
    ASSERT_TRUE(result_bitmap.readyToDraw());
    ASSERT_EQ(expected_result_rect.width(), result_bitmap.width());
    ASSERT_EQ(expected_result_rect.height(), result_bitmap.height());

    // Create the "expected result" bitmap.
    SkBitmap expected_bitmap;
    expected_bitmap.allocN32Pixels(expected_result_rect.width(),
                                   expected_result_rect.height());
    expected_bitmap.eraseColor(root_pass_color);
    for (int i = 0; i < 4; ++i) {
      gfx::Rect rect = smaller_pass_rects[i] - copy_rect.OffsetFromOrigin();
      rect = copy_output::ComputeResultRect(rect, scale_from_, scale_to_);
      expected_bitmap.erase(
          smaller_pass_colors[i],
          SkIRect{rect.x(), rect.y(), rect.right(), rect.bottom()});
    }

    // Do an approximate comparison of the result bitmap to the expected one to
    // confirm the position and size of the color values in the result is
    // correct. Allow for pixel values to be a bit off for two reasons:
    //
    //   1. The scaler algorithms are not using a naïve box filter, and so will
    //      blend things together at edge boundaries.
    //   2. In the case of an I420 format request, the chroma is at half-
    //      resolution, and so there can be "fuzzy color blending" at the edges
    //      between the color rects.
    int num_bad_pixels = 0;
    gfx::Point first_failure_position;
    for (int y = 0; y < expected_bitmap.height(); ++y) {
      for (int x = 0; x < expected_bitmap.width(); ++x) {
        const SkColor4f expected = expected_bitmap.getColor4f(x, y);
        const SkColor4f actual = result_bitmap.getColor4f(x, y);
        const bool red_bad = (expected.fR < 0.5f) != (actual.fR < 0.5f);
        const bool green_bad = (expected.fG < 0.5f) != (actual.fG < 0.5f);
        const bool blue_bad = (expected.fB < 0.5f) != (actual.fB < 0.5f);
        const bool alpha_bad = (expected.fA < 0.5f) != (actual.fA < 0.5f);
        if (red_bad || green_bad || blue_bad || alpha_bad) {
          if (num_bad_pixels == 0)
            first_failure_position = gfx::Point(x, y);
          ++num_bad_pixels;
        }
      }
    }
    EXPECT_EQ(0, num_bad_pixels)
        << "First failure position at: " << first_failure_position.ToString()
        << "\nExpected bitmap: " << cc::GetPNGDataUrl(expected_bitmap)
        << "\nActual bitmap: " << cc::GetPNGDataUrl(result_bitmap);
  }

 private:
  // Calls result.ReadI420Planes() and then converts the I420 format back to a
  // SkBitmap for comparison with the expected bitmap.
  static SkBitmap ReadI420ResultToSkBitmap(const CopyOutputResult& result) {
    const int result_width = result.rect().width();
    const int result_height = result.rect().height();

    // Calculate width/height/stride for each plane and allocate temporary
    // buffers to hold the pixels. Note that the strides for each plane are
    // being set differently to test that the arguments are correctly plumbed-
    // through.
    const int y_width = result_width;
    const int y_stride = y_width + 7;
    auto y_data = base::HeapArray<uint8_t>::Uninit(y_stride * result_height);
    const int chroma_width = (result_width + 1) / 2;
    const int u_stride = chroma_width + 11;
    const int v_stride = chroma_width + 17;
    const int chroma_height = (result_height + 1) / 2;
    auto u_data = base::HeapArray<uint8_t>::Uninit(u_stride * chroma_height);
    auto v_data = base::HeapArray<uint8_t>::Uninit(v_stride * chroma_height);

    // Do the read.
    const bool success =
        result.ReadI420Planes(y_data.data(), y_stride, u_data.data(), u_stride,
                              v_data.data(), v_stride);
    CHECK(success);

    // Convert to an SkBitmap.
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(result_width, result_height,
                                         kBGRA_8888_SkColorType,
                                         kPremul_SkAlphaType));
    const int error_code = libyuv::I420ToARGB(
        y_data.data(), y_stride, u_data.data(), u_stride, v_data.data(),
        v_stride, static_cast<uint8_t*>(bitmap.getPixels()), bitmap.rowBytes(),
        result_width, result_height);
    CHECK_EQ(0, error_code);

    return bitmap;
  }

  gfx::Vector2d scale_from_;
  gfx::Vector2d scale_to_;
  CopyOutputResult::Format result_format_;
};

// Parameters common to all test instantiations. These are tuples consisting of
// {scale_from, scale_to, i420_format}.
const auto kParameters =
    testing::Combine(testing::ValuesIn(GetRendererTypes()),
                     testing::Values(gfx::Vector2d(1, 1),
                                     gfx::Vector2d(2, 1),
                                     gfx::Vector2d(1, 2),
                                     gfx::Vector2d(2, 2)),
                     testing::Values(gfx::Vector2d(1, 1),
                                     gfx::Vector2d(2, 1),
                                     gfx::Vector2d(1, 2)),
                     testing::Values(CopyOutputResult::Format::RGBA,
                                     CopyOutputResult::Format::I420_PLANES));

TEST_P(CopyOutputScalingPixelTest, ScaledCopyOfDrawnFrame) {
  RunTest();
}
INSTANTIATE_TEST_SUITE_P(, CopyOutputScalingPixelTest, kParameters);

}  // namespace
}  // namespace viz
