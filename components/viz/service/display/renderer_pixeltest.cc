// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <tuple>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "cc/test/test_types.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display/delegated_ink_point_pixel_test_helper.h"
#include "components/viz/service/display/renderer_pixeltest_utils.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "components/viz/test/buildflags.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "components/viz/test/test_types.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/renderers/video_resource_updater.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "third_party/skia/include/private/chromium/SkPMColor.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_implementation.h"

namespace viz {
namespace {

using RendererPixelTest = VizPixelTestWithParam;
INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTest,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

using GPURendererPixelTest = VizPixelTestWithParam;
INSTANTIATE_TEST_SUITE_P(,
                         GPURendererPixelTest,
                         // TODO(crbug.com/40106226): Enable these tests for
                         // SkiaRenderer Dawn once video is supported.
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GPURendererPixelTest);

TEST_P(RendererPixelTest, SimpleGreenRect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

// Check that RendererPixelTest can run tests that verify incremental damage.
TEST_P(RendererPixelTest, SimpleDamageRect) {
  const gfx::Rect rect(this->device_viewport_size_);
  const gfx::Rect damage_rect = gfx::Rect(20, 30, 40, 50);

  const SkColor4f background_color = SkColors::kGreen;
  const SkColor4f foreground_color = SkColors::kBlue;

  std::vector<SkColor> expected_output_colors(rect.width() * rect.height());
  for (int y = 0; y < rect.height(); y++) {
    for (int x = 0; x < rect.width(); x++) {
      expected_output_colors[y * rect.width() + x] =
          damage_rect.Contains(x, y) ? foreground_color.toSkColor()
                                     : background_color.toSkColor();
    }
  }

  // Draw two frames with semi-transparent content. Both frames should result in
  // the same image.
  for (size_t i = 0; i < 2; i++) {
    SCOPED_TRACE(base::StringPrintf("Frame %zu", i));

    auto pass = CreateTestRootRenderPass(AggregatedRenderPassId{1}, rect);

    if (i != 0) {
      pass->damage_rect = damage_rect;
    }

    SharedQuadState* shared_state = CreateTestSharedQuadState(
        gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

    auto* foreground_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    foreground_quad->SetNew(shared_state, damage_rect, damage_rect,
                            foreground_color, false);

    // Only add the background in the first frame. If the renderer forces full
    // damage for all frames, the second frame will not contain the background
    // color from the first frame.
    if (i == 0) {
      auto* background_quad =
          pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      background_quad->SetNew(shared_state, rect, rect, background_color,
                              false);
    }

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    EXPECT_TRUE(this->RunPixelTest(&pass_list, &expected_output_colors,
                                   cc::AlphaDiscardingExactPixelComparator()));
  }
}

TEST_P(RendererPixelTest, OutputSurfaceClipRect) {
  gfx::Rect rect(device_viewport_size_);

  auto draw_frame = [&](base::FilePath::StringViewType path, SkColor4f color) {
    AggregatedRenderPassId id{1};
    auto pass = CreateTestRootRenderPass(id, rect);

    SharedQuadState* shared_state = CreateTestSharedQuadState(
        gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(shared_state, rect, rect, color, false);

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    EXPECT_TRUE(RunPixelTest(&pass_list, base::FilePath(path),
                             cc::AlphaDiscardingExactPixelComparator()));
  };

  draw_frame(FILE_PATH_LITERAL("green.png"), SkColors::kGreen);

  renderer_->SetOutputSurfaceClipRect(gfx::Rect(150, 150, 50, 50));

  draw_frame(FILE_PATH_LITERAL("green_with_blue_corner.png"), SkColors::kBlue);
}

TEST_P(RendererPixelTest, SimpleGreenRectNonRootRenderPass) {
  gfx::Rect rect(this->device_viewport_size_);
  gfx::Rect small_rect(100, 100);

  AggregatedRenderPassId child_id{2};
  auto child_pass =
      CreateTestRenderPass(child_id, small_rect, gfx::Transform());

  SharedQuadState* child_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), small_rect, child_pass.get(),
                                gfx::MaskFilterInfo());

  auto* color_quad = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(child_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassId root_id{1};
  auto root_pass = CreateTestRenderPass(root_id, rect, gfx::Transform());

  SharedQuadState* root_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  CreateTestRenderPassDrawQuad(root_shared_state, small_rect, child_id,
                               root_pass.get());

  auto* child_pass_ptr = child_pass.get();

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTestWithCopyOutputRequest(
      &pass_list, child_pass_ptr,
      base::FilePath(FILE_PATH_LITERAL("green_small.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, PremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTextureDrawQuad(!is_software_renderer(),
                            gfx::Rect(this->device_viewport_size_),
                            {0.0f, 1.0f, 0.0f, 0.5f},  // Texel color.
                            SkColors::kTransparent,    // Background color.
                            true,                      // Premultiplied alpha.
                            shared_state, this->resource_provider_.get(),
                            this->child_resource_provider_.get(),
                            this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, PremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  texture_quad_state->opacity = 0.8f;

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 255, 120)),  // Texel color.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      pass.get());

  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, TextureDrawQuadVisibleRectInsetTopLeft) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(0, 120, 255, 255)),  // Texel color 1.
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 0, 255)),  // Texel color 2.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      false,             // flipped_texture_quad.
      false,             // Half and half.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      pass.get());
  pass->quad_list.front()->visible_rect.Inset(gfx::Insets::TLBR(50, 30, 0, 0));
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("inset_top_left.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// This tests drawing a TextureDrawQuad with a visible_rect strictly included in
// rect, custom UVs, and rect.origin() that is not in the origin.
TEST_P(RendererPixelTest,
       TextureDrawQuadTranslatedAndVisibleRectInsetTopLeftAndCustomUV) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(0, 120, 255, 255)),  // Texel color 1.
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 0, 255)),  // Texel color 2.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      false,             // flipped_texture_quad.
      false,             // Half and half.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      pass.get());
  auto* quad = static_cast<TextureDrawQuad*>(pass->quad_list.front());
  quad->rect.Offset(10, 10);
  quad->visible_rect.Offset(10, 10);
  quad->visible_rect.Inset(gfx::Insets::TLBR(50, 30, 12, 12));
  quad->SetNormalizedTexCoordsForTesting(
      gfx::BoundingRect(gfx::PointF(0.2f, 0.3f), gfx::PointF(0.4f, 0.7f)),
      this->device_viewport_size_);
  quad->nearest_neighbor = true;  // To avoid bilinear filter differences.
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("offset_inset_top_left.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, BypassableTextureQuad_ClipRect) {
  gfx::Rect root_pass_rect(device_viewport_size_);
  gfx::Rect child_pass_rect(180, 180);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPassId child_pass_id{2};

  gfx::Transform transform_root_to_child_pass;
  transform_root_to_child_pass.Translate(10, 10);

  AggregatedRenderPassList pass_list;
  {
    // The child render pass has a single TextureDrawQuad so it will be bypassed
    // by SkiaRenderer. There is a clip_rect that clips the rightmost 10 pixels
    // in the x-axis only.
    auto child_pass = std::make_unique<AggregatedRenderPass>();
    child_pass->SetNew(child_pass_id, child_pass_rect, child_pass_rect,
                       transform_root_to_child_pass.GetCheckedInverse());

    auto* sqs =
        CreateTestSharedQuadState(gfx::Transform(), child_pass_rect,
                                  child_pass.get(), gfx::MaskFilterInfo());
    sqs->clip_rect = gfx::Rect(170, 200);

    CreateTestTwoColoredTextureDrawQuad(
        !is_software_renderer(), child_pass_rect,
        /*texel_color_one=*/SkColors::kYellow,
        /*texel_color_two=*/SkColors::kMagenta,
        /*background_color=*/SkColors::kGreen,
        /*premultiplied_alpha=*/true,
        /*flipped_texture_quad=*/false,
        /*half_and_half=*/false, sqs, resource_provider_.get(),
        child_resource_provider_.get(), child_context_provider_,
        child_pass.get());
    pass_list.push_back(std::move(child_pass));
  }

  {
    // The root render pass has a blue background and draws the (bypassed)
    // render pass into center 180x180 of the root render pass.
    auto root_pass = CreateTestRootRenderPass(root_pass_id, root_pass_rect);
    {
      auto* sqs = CreateTestSharedQuadState(transform_root_to_child_pass,
                                            child_pass_rect, root_pass.get(),
                                            gfx::MaskFilterInfo());
      auto* pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      pass_quad->SetNew(sqs, child_pass_rect, child_pass_rect, child_pass_id,
                        kInvalidResourceId, gfx::RectF(), gfx::Size(), false);
    }
    {
      auto* sqs =
          CreateTestSharedQuadState(gfx::Transform(), root_pass_rect,
                                    root_pass.get(), gfx::MaskFilterInfo());
      auto* blue_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      blue_quad->SetNew(sqs, root_pass_rect, root_pass_rect, SkColors::kBlue,
                        false);
    }
    pass_list.push_back(std::move(root_pass));
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("bypass_texture.png")),
      cc::ExactPixelComparator()));
}

TEST_P(RendererPixelTest, BypassableTextureQuad_Rotation_ClipRect) {
  gfx::Rect root_pass_rect(device_viewport_size_);
  gfx::Rect child_pass_rect(120, 120);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPassId child_pass_id{2};

  gfx::Transform transform_root_to_child_pass;
  transform_root_to_child_pass.RotateAboutZAxis(45.0);
  transform_root_to_child_pass.Translate(-child_pass_rect.width() / 2,
                                         -child_pass_rect.height() / 2);
  transform_root_to_child_pass.PostTranslate(root_pass_rect.width() / 2,
                                             root_pass_rect.height() / 2);

  AggregatedRenderPassList pass_list;
  {
    // The child render pass has a single TextureDrawQuad so it will be bypassed
    // by SkiaRenderer. The texture is drawn rotated 45° so all four corners are
    // clipped by the render pass output_rect. There is a clip_rect that clips
    // the rightmost 10 pixels in the x-axis only.
    auto child_pass = std::make_unique<AggregatedRenderPass>();
    child_pass->SetNew(child_pass_id, child_pass_rect, child_pass_rect,
                       transform_root_to_child_pass.GetCheckedInverse());

    gfx::Transform transform_texture_quad;
    {
      int half_length = child_pass_rect.width() / 2;
      transform_texture_quad.RotateAboutZAxis(45.0);
      transform_texture_quad.Translate(-half_length, -half_length);
      transform_texture_quad.PostTranslate(half_length, half_length);
    }

    auto* sqs =
        CreateTestSharedQuadState(transform_texture_quad, child_pass_rect,
                                  child_pass.get(), gfx::MaskFilterInfo());
    sqs->clip_rect = gfx::Rect(110, 140);

    CreateTestTwoColoredTextureDrawQuad(
        !is_software_renderer(), child_pass_rect,
        /*texel_color_one=*/SkColors::kYellow,
        /*texel_color_two=*/SkColors::kMagenta,
        /*background_color=*/SkColors::kGreen,
        /*premultiplied_alpha=*/true,
        /*flipped_texture_quad=*/false,
        /*half_and_half=*/false, sqs, resource_provider_.get(),
        child_resource_provider_.get(), child_context_provider_,
        child_pass.get());
    pass_list.push_back(std::move(child_pass));
  }

  {
    // The root render pass has a blue background and draws the (bypassed)
    // render pass rotated another 45°.
    auto root_pass = CreateTestRootRenderPass(root_pass_id, root_pass_rect);
    {
      auto* sqs = CreateTestSharedQuadState(transform_root_to_child_pass,
                                            child_pass_rect, root_pass.get(),
                                            gfx::MaskFilterInfo());
      auto* pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      pass_quad->SetNew(sqs, child_pass_rect, child_pass_rect, child_pass_id,
                        kInvalidResourceId, gfx::RectF(), gfx::Size(), false);
    }
    {
      auto* sqs =
          CreateTestSharedQuadState(gfx::Transform(), root_pass_rect,
                                    root_pass.get(), gfx::MaskFilterInfo());
      auto* blue_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      blue_quad->SetNew(sqs, root_pass_rect, root_pass_rect, SkColors::kBlue,
                        false);
    }
    pass_list.push_back(std::move(root_pass));
  }

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("bypass_texture_rotated.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }

  EXPECT_TRUE(this->RunPixelTest(&pass_list, expected_result,
                                 cc::FuzzyPixelComparator()
                                     .SetErrorPixelsPercentageLimit(3.5f)
                                     .SetAbsErrorLimit(127)
                                     .SetAvgAbsErrorLimit(40)));
}

TEST_P(RendererPixelTest, BypassableRenderPassQuad) {
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId grand_child_pass_id{3};

  gfx::Rect root_pass_rect(device_viewport_size_);
  gfx::Rect child_pass_rect(180, 180);
  gfx::Rect grand_child_pass_rect(320, 320);

  gfx::Transform transform_root_to_child_pass;
  transform_root_to_child_pass.Translate(10, 10);

  gfx::Transform transform_child_to_grand_child_pass;
  transform_child_to_grand_child_pass.Translate(15, 15);
  transform_child_to_grand_child_pass.Scale(0.5f, 0.5f);

  AggregatedRenderPassList pass_list;

  {
    // This render pass has two quads so it can't be bypassed. The quads are
    // bigger than the render pass so they are clipped by the render pass
    // output_rect.
    gfx::Transform transform_root_to_grand_child_pass =
        transform_root_to_child_pass * transform_child_to_grand_child_pass;
    auto grand_child_pass = std::make_unique<AggregatedRenderPass>();
    grand_child_pass->SetNew(
        grand_child_pass_id, grand_child_pass_rect, grand_child_pass_rect,
        transform_root_to_grand_child_pass.GetCheckedInverse());

    gfx::Rect quad_rect(360, 360);
    auto* sqs = CreateTestSharedQuadState(gfx::Transform(), quad_rect,
                                          grand_child_pass.get(),
                                          gfx::MaskFilterInfo());

    gfx::Rect magenta_rect(quad_rect.width() / 4, quad_rect.height() / 4,
                           quad_rect.width() / 2, quad_rect.height() / 2);
    auto* magenta_quad =
        grand_child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    magenta_quad->SetNew(sqs, magenta_rect, magenta_rect, SkColors::kMagenta,
                         false);

    auto* yellow_quad =
        grand_child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    yellow_quad->SetNew(sqs, quad_rect, quad_rect, SkColors::kYellow, false);

    pass_list.push_back(std::move(grand_child_pass));
  }

  {
    // This render pass can be bypassed by SkiaRenderer. There is a clip_rect
    // that clips the rightmost 20 pixels in the x-axis only.
    auto child_pass = std::make_unique<AggregatedRenderPass>();
    child_pass->SetNew(child_pass_id, child_pass_rect, child_pass_rect,
                       transform_root_to_child_pass.GetCheckedInverse());

    auto* sqs = CreateTestSharedQuadState(
        transform_child_to_grand_child_pass, grand_child_pass_rect,
        child_pass.get(), gfx::MaskFilterInfo());
    sqs->clip_rect = gfx::Rect(160, 200);

    auto* pass_quad =
        child_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    pass_quad->SetNew(sqs, grand_child_pass_rect, grand_child_pass_rect,
                      grand_child_pass_id, kInvalidResourceId, gfx::RectF(),
                      gfx::Size(), false);
    pass_list.push_back(std::move(child_pass));
  }

  {
    // The root render pass has a blue background and draws the (bypassed)
    // render pass into center 180x180 of the root render pass.
    auto root_pass = CreateTestRootRenderPass(root_pass_id, root_pass_rect);
    {
      auto* sqs = CreateTestSharedQuadState(transform_root_to_child_pass,
                                            child_pass_rect, root_pass.get(),
                                            gfx::MaskFilterInfo());
      auto* pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      pass_quad->SetNew(sqs, child_pass_rect, child_pass_rect, child_pass_id,
                        kInvalidResourceId, gfx::RectF(), gfx::Size(), false);
    }
    {
      auto* sqs =
          CreateTestSharedQuadState(gfx::Transform(), root_pass_rect,
                                    root_pass.get(), gfx::MaskFilterInfo());
      auto* blue_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      blue_quad->SetNew(sqs, root_pass_rect, root_pass_rect, SkColors::kBlue,
                        false);
    }
    pass_list.push_back(std::move(root_pass));
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("bypass_render_pass.png")),
      cc::ExactPixelComparator()));
}

TEST_P(RendererPixelTest, BypassableRenderPassQuad_DoubleBypass) {
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId grand_child_pass_id{3};

  gfx::Rect root_pass_rect(device_viewport_size_);
  gfx::Rect child_pass_rect(180, 180);
  gfx::Rect grand_child_pass_rect(160, 160);

  gfx::Transform transform_root_to_child_pass;
  transform_root_to_child_pass.Translate(10, 10);

  gfx::Transform transform_child_to_grand_child_pass;
  transform_child_to_grand_child_pass.Translate(15, 15);

  AggregatedRenderPassList pass_list;

  {
    // This render pass contains a single TextureDrawQuad so SkiaRenderer can
    // bypass it. The quad is bigger than the render pass so it is clipped by
    // render pass output_rect.
    gfx::Transform transform_root_to_grand_child_pass =
        transform_root_to_child_pass * transform_child_to_grand_child_pass;
    auto grand_child_pass = std::make_unique<AggregatedRenderPass>();
    grand_child_pass->SetNew(
        grand_child_pass_id, grand_child_pass_rect, grand_child_pass_rect,
        transform_root_to_grand_child_pass.GetCheckedInverse());

    auto* sqs = CreateTestSharedQuadState(gfx::Transform(), child_pass_rect,
                                          grand_child_pass.get(),
                                          gfx::MaskFilterInfo());

    CreateTestTwoColoredTextureDrawQuad(
        !is_software_renderer(), child_pass_rect,
        /*texel_color_one=*/SkColors::kYellow,
        /*texel_color_two=*/SkColors::kMagenta,
        /*background_color=*/SkColors::kGreen,
        /*premultiplied_alpha=*/true,
        /*flipped_texture_quad=*/false,
        /*half_and_half=*/false, sqs, resource_provider_.get(),
        child_resource_provider_.get(), child_context_provider_,
        grand_child_pass.get());

    pass_list.push_back(std::move(grand_child_pass));
  }

  {
    // This render pass contains a single RenderPassDrawQuad so SkiaRenderer can
    // also bypass it. There is a clip_rect that clips the rightmost 20 pixels
    // in the x-axis only.
    auto child_pass = std::make_unique<AggregatedRenderPass>();
    child_pass->SetNew(child_pass_id, child_pass_rect, child_pass_rect,
                       transform_root_to_child_pass.GetCheckedInverse());

    auto* sqs = CreateTestSharedQuadState(
        transform_child_to_grand_child_pass, grand_child_pass_rect,
        child_pass.get(), gfx::MaskFilterInfo());
    sqs->clip_rect = gfx::Rect(160, 200);
    auto* pass_quad =
        child_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    pass_quad->SetNew(sqs, grand_child_pass_rect, grand_child_pass_rect,
                      grand_child_pass_id, kInvalidResourceId, gfx::RectF(),
                      gfx::Size(), false);
    pass_list.push_back(std::move(child_pass));
  }

  {
    // The root render pass has a blue background and draws the (bypassed)
    // render pass into center 180x180 of the root render pass.
    auto root_pass = CreateTestRootRenderPass(root_pass_id, root_pass_rect);
    {
      auto* sqs = CreateTestSharedQuadState(transform_root_to_child_pass,
                                            child_pass_rect, root_pass.get(),
                                            gfx::MaskFilterInfo());
      auto* pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      pass_quad->SetNew(sqs, child_pass_rect, child_pass_rect, child_pass_id,
                        kInvalidResourceId, gfx::RectF(), gfx::Size(), false);
    }
    {
      auto* sqs =
          CreateTestSharedQuadState(gfx::Transform(), root_pass_rect,
                                    root_pass.get(), gfx::MaskFilterInfo());
      auto* blue_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      blue_quad->SetNew(sqs, root_pass_rect, root_pass_rect, SkColors::kBlue,
                        false);
    }
    pass_list.push_back(std::move(root_pass));
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("bypass_render_pass.png")),
      cc::ExactPixelComparator()));
}

TEST_P(RendererPixelTest, BypassableRenderPassQuad_DoubleBypass_ScaledClip) {
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPassId grand_child_pass_id{3};

  gfx::Rect root_pass_rect(device_viewport_size_);
  gfx::Rect child_pass_rect(180, 180);
  gfx::Rect grand_child_pass_rect(360, 360);

  gfx::Transform transform_root_to_child_pass;
  transform_root_to_child_pass.Translate(10, 10);

  gfx::Transform transform_child_to_grand_child_pass;
  transform_child_to_grand_child_pass.Translate(15, 15);
  transform_child_to_grand_child_pass.Scale(0.5f, 0.5f);

  AggregatedRenderPassList pass_list;

  {
    // This render pass contains a single TextureDrawQuad so SkiaRenderer can
    // bypass it. The quad has a clip_rect which clips 40px on the right and
    // bottom.
    gfx::Transform transform_root_to_grand_child_pass =
        transform_root_to_child_pass * transform_child_to_grand_child_pass;
    auto grand_child_pass = std::make_unique<AggregatedRenderPass>();
    grand_child_pass->SetNew(
        grand_child_pass_id, grand_child_pass_rect, grand_child_pass_rect,
        transform_root_to_grand_child_pass.GetCheckedInverse());

    auto* sqs = CreateTestSharedQuadState(
        gfx::Transform(), grand_child_pass_rect, grand_child_pass.get(),
        gfx::MaskFilterInfo());
    sqs->clip_rect = gfx::Rect(320, 320);

    CreateTestTwoColoredTextureDrawQuad(
        !is_software_renderer(), grand_child_pass_rect,
        /*texel_color_one=*/SkColors::kYellow,
        /*texel_color_two=*/SkColors::kMagenta,
        /*background_color=*/SkColors::kGreen,
        /*premultiplied_alpha=*/true,
        /*flipped_texture_quad=*/false,
        /*half_and_half=*/false, sqs, resource_provider_.get(),
        child_resource_provider_.get(), child_context_provider_,
        grand_child_pass.get());

    pass_list.push_back(std::move(grand_child_pass));
  }

  {
    // This render pass contains a single RenderPassDrawQuad so SkiaRenderer can
    // also bypass it. There is a clip_rect that clips the rightmost 20 pixels
    // in the x-axis only.
    auto child_pass = std::make_unique<AggregatedRenderPass>();
    child_pass->SetNew(child_pass_id, child_pass_rect, child_pass_rect,
                       transform_root_to_child_pass.GetCheckedInverse());

    auto* sqs = CreateTestSharedQuadState(
        transform_child_to_grand_child_pass, grand_child_pass_rect,
        child_pass.get(), gfx::MaskFilterInfo());
    sqs->clip_rect = gfx::Rect(160, 200);
    auto* pass_quad =
        child_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    pass_quad->SetNew(sqs, grand_child_pass_rect, grand_child_pass_rect,
                      grand_child_pass_id, kInvalidResourceId, gfx::RectF(),
                      gfx::Size(), false);
    pass_list.push_back(std::move(child_pass));
  }

  {
    // The root render pass has a blue background and draws the (bypassed)
    // render pass into center 180x180 of the root render pass.
    auto root_pass = CreateTestRootRenderPass(root_pass_id, root_pass_rect);
    {
      auto* sqs = CreateTestSharedQuadState(transform_root_to_child_pass,
                                            child_pass_rect, root_pass.get(),
                                            gfx::MaskFilterInfo());
      auto* pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      pass_quad->SetNew(sqs, child_pass_rect, child_pass_rect, child_pass_id,
                        kInvalidResourceId, gfx::RectF(), gfx::Size(), false);
    }
    {
      auto* sqs =
          CreateTestSharedQuadState(gfx::Transform(), root_pass_rect,
                                    root_pass.get(), gfx::MaskFilterInfo());
      auto* blue_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      blue_quad->SetNew(sqs, root_pass_rect, root_pass_rect, SkColors::kBlue,
                        false);
    }
    pass_list.push_back(std::move(root_pass));
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("bypass_render_pass.png")),
      cc::ExactPixelComparator()));
}

TEST_P(RendererPixelTest, BypassableRenderPassQuad_BackdropFilter_Extents) {
  // This tests that a bypassable render pass with a backdrop filter applies
  // the backdrop filter to the RPDQ's entire visible_rect, even if the
  // child of the render pass has smaller content that is being drawn directly
  // because of the bypass.
  gfx::Rect root_pass_rect(device_viewport_size_);
  gfx::Rect backdrop_pass_rect(device_viewport_size_.width() - 20,
                               device_viewport_size_.height() - 20);
  gfx::Rect child_content_rect(90, 90);

  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPassId backdrop_pass_id{2};

  gfx::Transform transform_root_to_backdrop_pass;
  transform_root_to_backdrop_pass.Translate(10, 10);

  AggregatedRenderPassList pass_list;
  {
    // The child render pass has a single TextureDrawQuad so it will be bypassed
    // by SkiaRenderer. The TextureDrawQuad is smaller than the visible rect
    // of the child render pass, which has a backdrop filter that should cover
    // the root pass up to a 10px inset.
    auto backdrop_pass = std::make_unique<AggregatedRenderPass>();
    backdrop_pass->SetNew(backdrop_pass_id, backdrop_pass_rect,
                          backdrop_pass_rect,
                          transform_root_to_backdrop_pass.GetCheckedInverse());

    gfx::Transform transform_child_to_backdrop_pass;
    transform_child_to_backdrop_pass.Translate(
        backdrop_pass_rect.CenterPoint().x() -
            0.5f * child_content_rect.width(),
        backdrop_pass_rect.CenterPoint().y() -
            0.5f * child_content_rect.height());

    auto* sqs = CreateTestSharedQuadState(
        transform_child_to_backdrop_pass, child_content_rect,
        backdrop_pass.get(), gfx::MaskFilterInfo());
    sqs->clip_rect = cc::MathUtil::MapEnclosingClippedRect(
        transform_child_to_backdrop_pass, child_content_rect);

    // NOTE: From https://g-issues.chromium.org/issues/355981041, the backdrop
    // filter of a bypassed render pass was being restricted to the visible rect
    // of the child. Use kTransparent for the outer color and background color
    // to allow backdrop filtered content to be visible under part of this
    // texture quad to highlight that the filter is being processed, but was
    // incorrectly clipped during bypassing.
    CreateTestTwoColoredTextureDrawQuad(
        !is_software_renderer(), child_content_rect,
        /*texel_color_one=*/SkColors::kTransparent,
        /*texel_color_two=*/SkColors::kMagenta,
        /*background_color=*/SkColors::kTransparent,
        /*premultiplied_alpha=*/true,
        /*flipped_texture_quad=*/false,
        /*half_and_half=*/false, sqs, resource_provider_.get(),
        child_resource_provider_.get(), child_context_provider_,
        backdrop_pass.get());
    pass_list.push_back(std::move(backdrop_pass));
  }

  {
    // The root render pass has a blue and yellow checkerboard background and
    // draws the (bypassed) render pass inset in the root by 10px.
    auto root_pass = CreateTestRootRenderPass(root_pass_id, root_pass_rect);
    {
      auto* sqs = CreateTestSharedQuadState(transform_root_to_backdrop_pass,
                                            backdrop_pass_rect, root_pass.get(),
                                            gfx::MaskFilterInfo());
      auto* pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      pass_quad->SetNew(sqs, backdrop_pass_rect, backdrop_pass_rect,
                        backdrop_pass_id, kInvalidResourceId, gfx::RectF(),
                        gfx::Size(), false);
      pass_quad->SetFilters(
          /*filters=*/{}, /*backdrop_filters=*/
          cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(
              8.f, SkTileMode::kMirror)}),
          /*backdrop_filter_bounds=*/std::nullopt,
          /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
          /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
    }
    {
      auto* sqs =
          CreateTestSharedQuadState(gfx::Transform(), root_pass_rect,
                                    root_pass.get(), gfx::MaskFilterInfo());
      static constexpr int checker_size = 16;

      for (int y = root_pass_rect.y(); y < root_pass_rect.bottom();
           y += checker_size) {
        for (int x = root_pass_rect.x(); x < root_pass_rect.right();
             x += checker_size) {
          gfx::Rect box{x, y, checker_size, checker_size};
          bool firstColor =
              ((x / checker_size) + ((y / checker_size) % 2)) % 2 == 0;
          auto* quad = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
          quad->SetNew(sqs, box, box,
                       firstColor ? SkColors::kBlue : SkColors::kYellow, false);
        }
      }
    }
    pass_list.push_back(std::move(root_pass));
  }

  // Use a fairly fuzz comparison to allow for deviations in how the renderer
  // types implement the actual blur. In particular, the SW renderer does not
  // support the mirror tile mode so its blurs deviate more from GPU renderers.
  const bool blur_fully_supported = !is_software_renderer();
  auto comparator =
      cc::FuzzyPixelComparator()
          .SetErrorPixelsPercentageLimit(blur_fully_supported ? 0.32f : 53.f)
          .SetAvgAbsErrorLimit(blur_fully_supported ? 1 : 2)
          .SetAbsErrorLimit(blur_fully_supported ? 1 : 8);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("bypass_texture_backdrop_extent.png")),
      comparator));
}

TEST_P(RendererPixelTest, TextureDrawQuadVisibleRectInsetBottomRight) {
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
  // Test is flaking with failed large allocations under TSAN when using
  // SkiaRenderer with GL backend. See https://crbug.com/1320955.
  if (renderer_type() == RendererType::kSkiaGL)
    return;
#endif

  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(0, 120, 255, 255)),  // Texel color 1.
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 0, 255)),  // Texel color 2.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      false,             // flipped_texture_quad.
      false,             // Half and half.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      pass.get());
  pass->quad_list.front()->visible_rect.Inset(gfx::Insets::TLBR(0, 0, 60, 40));
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("inset_bottom_right.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, SolidColorBlend) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  pass->has_transparent_background = false;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 1 - 16.0f / 255;
  shared_state->blend_mode = SkBlendMode::kDstOut;

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kRed, false);

  SharedQuadState* shared_state_background = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  SkColor4f background_color =
      SkColor4f::FromColor(SkColorSetRGB(0xff, 0xff * 14 / 16, 0xff));
  auto* color_quad_background =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad_background->SetNew(shared_state_background, rect, rect,
                                background_color, false);
  // Result should be r=16, g=14, b=16.

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, SolidColorWithTemperature) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kYellow, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  SkM44 color_matrix;
  color_matrix.setRC(0, 0, 0.7f);
  color_matrix.setRC(1, 1, 0.4f);
  color_matrix.setRC(2, 2, 0.5f);
  this->output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("temperature_brown.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, SolidColorWithTemperatureNonRootRenderPass) {
  // Create a root and a child passes with two different solid color quads.
  AggregatedRenderPassList render_passes_in_draw_order;
  gfx::Rect viewport_rect(this->device_viewport_size_);
  gfx::Rect root_rect(0, 0, viewport_rect.width(), viewport_rect.height() / 2);
  gfx::Rect child_rect(0, root_rect.bottom(), viewport_rect.width(),
                       root_rect.height());

  // Child pass.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass =
      cc::AddRenderPass(&render_passes_in_draw_order, child_pass_id,
                        viewport_rect, gfx::Transform());
  cc::AddQuad(child_pass, child_rect, SkColors::kGreen);

  // Root pass.
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass =
      cc::AddRenderPass(&render_passes_in_draw_order, root_pass_id,
                        viewport_rect, gfx::Transform());
  cc::AddQuad(root_pass, root_rect, SkColors::kYellow);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), viewport_rect, root_pass,
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, viewport_rect, child_pass_id,
                               root_pass);

  // Set a non-identity output color matrix on the output surface, and expect
  // that the colors will be transformed.
  SkM44 color_matrix;
  color_matrix.setRC(0, 0, 0.7f);
  color_matrix.setRC(1, 1, 0.4f);
  color_matrix.setRC(2, 2, 0.5f);
  this->output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &render_passes_in_draw_order,
      base::FilePath(FILE_PATH_LITERAL("temperature_brown_non_root.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Check that the renderer draws a fallback quad for quads that require overlay.
TEST_P(GPURendererPixelTest, OverlayHintRequiredFallback) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // Add a texture quad with the overlay priority of "required". Most properties
  // shouldn't matter since the renderer shouldn't attempt to draw this quad.
  TextureDrawQuad* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(texture_quad_state, gfx::Rect(this->device_viewport_size_),
               gfx::Rect(this->device_viewport_size_), false, ResourceId{1},
               gfx::PointF(), gfx::PointF(), SkColors::kTransparent, false,
               false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);
  quad->overlay_priority_hint = OverlayPriority::kRequired;

  // Add a background that's not the expected fallback color.
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

#if DCHECK_IS_ON()
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("magenta.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
#else
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("black.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
#endif
}

// Check that the renderer draws a fallback quad for quads that require overlay,
// but are processed by the RPDQ bypass case.
TEST_P(GPURendererPixelTest, OverlayHintRequiredFallbackRPDQBypassCase) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassList pass_list;

  // Inner pass with just a video quad. This is intended to trigger the RPDQ
  // bypass case in DirectRenderer.
  AggregatedRenderPassId inner_id{2};
  {
    auto pass = CreateTestRenderPass(inner_id, rect, gfx::Transform());

    SharedQuadState* sqs = CreateTestSharedQuadState(
        gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

    // Add a texture quad with the overlay priority of "required". Most
    // properties shouldn't matter since the renderer shouldn't attempt to draw
    // this quad.
    TextureDrawQuad* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    quad->SetNew(sqs, gfx::Rect(this->device_viewport_size_),
                 gfx::Rect(this->device_viewport_size_), false, ResourceId{1},
                 gfx::PointF(), gfx::PointF(), SkColors::kTransparent, false,
                 false, gfx::ProtectedVideoType::kClear,
                 /*is_tex_coords_normalized=*/false);
    quad->overlay_priority_hint = OverlayPriority::kRequired;

    pass_list.push_back(std::move(pass));
  }

  // Root pass with a RPDQ
  {
    AggregatedRenderPassId id{1};
    auto pass = CreateTestRootRenderPass(id, rect);

    SharedQuadState* sqs = CreateTestSharedQuadState(
        gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

    CreateTestRenderPassDrawQuad(sqs, rect, inner_id, pass.get());

    // Add a background that's not the expected fallback color.
    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(sqs, rect, rect, SkColors::kWhite, false);

    pass_list.push_back(std::move(pass));
  }

  const size_t num_passes = pass_list.size();

  base::HistogramTester histogram;

#if DCHECK_IS_ON()
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("magenta.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
#else
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("black.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
#endif

  // Check that we have two render passes, but one of them hit the RPDQ bypass
  // case.
  EXPECT_EQ(num_passes, 2u);
  histogram.ExpectTotalCount("Compositing.Display.FlattenedRenderPassCount", 1);
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_P(GPURendererPixelTest, NonPremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTextureDrawQuad(!is_software_renderer(),
                            gfx::Rect(this->device_viewport_size_),
                            {0.0f, 1.0f, 0.0f, 0.5f},  // Texel color.
                            SkColors::kTransparent,    // Background color.
                            false,                     // Premultiplied alpha.
                            shared_state, this->resource_provider_.get(),
                            this->child_resource_provider_.get(),
                            this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_P(GPURendererPixelTest, NonPremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  texture_quad_state->opacity = 0.8f;

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 255, 120)),  // Texel color.
      SkColors::kGreen,  // Background color.
      false,             // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      pass.get());

  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassColorFilterAlpha) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  float matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(
          cc::ColorFilter::MakeMatrix(matrix), nullptr)));

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), false);
  render_pass_quad->SetFilters(filters, /*backdrop_filters=*/{},
                               /*backdrop_filter_bounds=*/std::nullopt,
                               /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
                               /*filters_origin=*/gfx::PointF(),
                               /*backdrop_filter_quality=*/1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassSaturateFilter) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), false);
  render_pass_quad->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateSaturateFilter(0.5f)}),
      /*backdrop_filters=*/{},
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test blends slightly differently with the software renderer vs. the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassFilterChain) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateGrayscaleFilter(1.f));
  filters.Append(cc::FilterOperation::CreateBrightnessFilter(0.5f));

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), false);
  render_pass_quad->SetFilters(filters, /*backdrop_filters=*/{},
                               /*backdrop_filter_bounds=*/std::nullopt,
                               /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
                               /*filters_origin=*/gfx::PointF(),
                               /*backdrop_filter_quality=*/1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test blends slightly differently with the software renderer vs. the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassColorFilterAlphaTranslation) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  float matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = 0;
  matrix[4] = 20.f / 255;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = 0;
  matrix[9] = 200.f / 255;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = 0;
  matrix[14] = 1.5f / 255;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(
          cc::ColorFilter::MakeMatrix(matrix), nullptr)));
  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), false);
  render_pass_quad->SetFilters(filters, /*backdrop_filters=*/{},
                               /*backdrop_filter_bounds=*/std::nullopt,
                               /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
                               /*filters_origin=*/gfx::PointF(),
                               /*backdrop_filter_quality=*/1.0f);

  AggregatedRenderPassList pass_list;

  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha_translate.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

// Test that an RPDQ that has a filter that samples outside the output rect of
// the render pass can draw the filter contents even if the embedding RPDQ has
// an empty size.
TEST_P(RendererPixelTest, NonEmptyFilterClipRectOnEmptyRenderPassQuad) {
  const gfx::Rect viewport_rect(this->device_viewport_size_);

  // Add a non-zero offset to the RPDQ to avoid depending on a zero offset in
  // the implementation.
  const gfx::Vector2d rpdq_offset(20, 10);
  const gfx::Rect empty_rpdq_rect;

  AggregatedRenderPassList pass_list;

  // Zero-sized render pass with a reference filter that fills an area green.
  AggregatedRenderPassId child_pass_id{2};
  {
    auto child_pass =
        CreateTestRenderPass(child_pass_id, empty_rpdq_rect,
                             /*transform_to_root_target=*/
                             gfx::Transform::MakeTranslation(rpdq_offset));

    pass_list.push_back(std::move(child_pass));
  }

  // Root pass that embeds the child pass as a zero-sized RPDQ.
  {
    AggregatedRenderPassId root_pass_id{1};
    auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

    cc::FilterOperations filters;
    const SkRect filter_clip_rect = gfx::RectToSkRect(
        gfx::Rect(gfx::Point() - rpdq_offset, this->device_viewport_size_));
    filters.Append(cc::FilterOperation::CreateReferenceFilter(
        sk_make_sp<cc::ColorFilterPaintFilter>(
            cc::ColorFilter::MakeBlend(SkColors::kGreen, SkBlendMode::kSrc),
            nullptr, &filter_clip_rect)));

    CreateTestRenderPassDrawQuad(
        CreateTestSharedQuadState(
            /*quad_to_target_transform=*/gfx::Transform::MakeTranslation(
                rpdq_offset),
            gfx::Rect(gfx::Point() - rpdq_offset, this->device_viewport_size_),
            root_pass.get(), gfx::MaskFilterInfo()),
        empty_rpdq_rect, child_pass_id, root_pass.get(), filters);

    // Add a red background to make it clear when the test is failing.
    auto* color_quad = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(
        CreateTestSharedQuadState(gfx::Transform(), viewport_rect,
                                  root_pass.get(), gfx::MaskFilterInfo()),
        viewport_rect, viewport_rect, SkColors::kRed, false);

    pass_list.push_back(std::move(root_pass));
  }

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, EnlargedRenderPassTexture) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Size(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, EnlargedRenderPassTextureWithAntiAliasing) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  gfx::Transform aa_transform;
  aa_transform.Translate(0.5, 0.0);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(aa_transform, pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  SharedQuadState* root_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  auto* background = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  background->SetNew(root_shared_state, gfx::Rect(this->device_viewport_size_),
                     gfx::Rect(this->device_viewport_size_), SkColors::kWhite,
                     false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Size(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_anti_aliasing.png")),
      cc::FuzzyPixelComparator()
          .DiscardAlpha()
          .SetErrorPixelsPercentageLimit(100.f)
          .SetAvgAbsErrorLimit(5.f)
          .SetAbsErrorLimit(7)));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TEST_P(RendererPixelTest, RenderPassAndMaskWithPartialQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is just a green box.
  static const SkColor4f kCSSGreen = SkColor4f::FromColor(0xff008000);
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(child_pass_shared_state, viewport_rect, viewport_rect,
                kCSSGreen, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(SkIntToScalar(4));
  flags.setColor(SkColors::kWhite);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        flags);
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
  }

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
        gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    mask_resource_id = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // This AggregatedRenderPassDrawQuad does not include the full |viewport_rect|
  // which is the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 50, 200, 100);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, sub_rect, sub_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(sub_rect), 2.f / mask_rect.width(),
                     2.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      false);                                    // force_anti_aliasing_off
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("mask_bottom_right.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TEST_P(RendererPixelTest, RenderPassAndMaskWithPartialQuad2) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is just a green box.
  static const SkColor4f kCSSGreen = SkColor4f::FromColor(0xff008000);
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(child_pass_shared_state, viewport_rect, viewport_rect,
                kCSSGreen, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(SkIntToScalar(4));
  flags.setColor(SkColors::kWhite);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        flags);
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
  }

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
        gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    mask_resource_id = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // This AggregatedRenderPassDrawQuad does not include the full |viewport_rect|
  // which is the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 20, 200, 60);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, sub_rect, sub_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(sub_rect), 2.f / mask_rect.width(),
                     2.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      false);                                    // force_anti_aliasing_off
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("mask_middle.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, RenderPassAndMaskForRoundedCorner) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is just a blue box.
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(child_pass_shared_state, viewport_rect, viewport_rect,
               SkColors::kBlue, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColors::kWhite);
  flags.setAntiAlias(true);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rounded_corner_rect = mask_rect;
  rounded_corner_rect.Inset(kInset);
  SkRRect rounded_corner = SkRRect::MakeRectXY(
      gfx::RectToSkRect(rounded_corner_rect), kCornerRadius, kCornerRadius);
  canvas.drawRRect(rounded_corner, flags);

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
        gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    mask_resource_id = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, viewport_rect, viewport_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(viewport_rect), 1.f / mask_rect.width(),
                     1.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      false);                                    // force_anti_aliasing_off
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // The rounded corners generated by masks should be very close to the rounded
  // corners generated by the fragment shader approach. The percentage of pixel
  // mismatch is around 0.52%.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_simple.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

TEST_P(RendererPixelTest, RenderPassAndMaskForRoundedCornerMultiRadii) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  const SkVector kCornerRadii[4] = {
      SkVector::Make(5.0, 5.0),
      SkVector::Make(15.0, 15.0),
      SkVector::Make(25.0, 25.0),
      SkVector::Make(35.0, 35.0),
  };

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is half a blue box and other half yellow box.
  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(child_pass_shared_state, blue_rect, blue_rect, SkColors::kBlue,
               false);

  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(child_pass_shared_state, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColors::kWhite);
  flags.setAntiAlias(true);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rounded_corner_rect = mask_rect;
  rounded_corner_rect.Inset(kInset);
  SkRRect rounded_corner =
      SkRRect::MakeRect(gfx::RectToSkRect(rounded_corner_rect));
  rounded_corner.setRectRadii(rounded_corner.rect(), kCornerRadii);
  canvas.drawRRect(rounded_corner, flags);

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
        gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    mask_resource_id = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, viewport_rect, viewport_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(viewport_rect), 1.f / mask_rect.width(),
                     1.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      false);                                    // force_anti_aliasing_off
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_multi_radii.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

class RendererPixelTestWithBackdropFilter : public VizPixelTestWithParam {
 protected:
  void SetUp() override {
    VizPixelTestWithParam::SetUp();
    filter_pass_layer_rect_ = gfx::Rect(device_viewport_size_);
    filter_pass_layer_rect_.Inset(gfx::Insets::TLBR(14, 12, 18, 16));
    backdrop_filter_bounds_ =
        SkPath::Rect(gfx::RectToSkRect(filter_pass_layer_rect_));
  }

  void SetUpRenderPassList() {
    gfx::Rect device_viewport_rect(this->device_viewport_size_);

    AggregatedRenderPassId root_id{1};
    auto root_pass = CreateTestRootRenderPass(root_id, device_viewport_rect);
    root_pass->has_transparent_background = false;

    gfx::Transform identity_quad_to_target_transform;

    AggregatedRenderPassId filter_pass_id{2};
    gfx::Transform transform_to_root;
    auto filter_pass = CreateTestRenderPass(
        filter_pass_id, filter_pass_layer_rect_, transform_to_root);

    // A non-visible quad in the filtering render pass.
    {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, filter_pass_layer_rect_,
          filter_pass.get(), gfx::MaskFilterInfo());
      auto* color_quad =
          filter_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, filter_pass_layer_rect_,
                         filter_pass_layer_rect_, SkColors::kTransparent,
                         false);
    }

    ResourceId mapped_mask_resource_id(0);
    gfx::RectF mask_uv_rect;
    gfx::Size mask_texture_size;
    if (include_backdrop_mask_) {
      // Make a mask.
      gfx::Rect viewport_rect(this->device_viewport_size_);
      constexpr int kInset = 20;
      const SkVector kCornerRadii[4] = {
          SkVector::Make(5.0, 5.0),
          SkVector::Make(15.0, 15.0),
          SkVector::Make(25.0, 25.0),
          SkVector::Make(35.0, 35.0),
      };
      gfx::Rect mask_rect = viewport_rect;
      SkBitmap bitmap;
      bitmap.allocPixels(
          SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
      cc::SkiaPaintCanvas canvas(bitmap);
      cc::PaintFlags flags;
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(SkColors::kWhite);
      flags.setAntiAlias(true);
      canvas.clear(SkColors::kTransparent);
      gfx::Rect rounded_corner_rect = mask_rect;
      rounded_corner_rect.Inset(kInset);
      SkRRect rounded_corner =
          SkRRect::MakeRect(gfx::RectToSkRect(rounded_corner_rect));
      rounded_corner.setRectRadii(rounded_corner.rect(), kCornerRadii);
      canvas.drawRRect(rounded_corner, flags);

      ResourceId mask_resource_id;
      if (!is_software_renderer()) {
        mask_resource_id = CreateGpuResource(
            this->child_context_provider_, this->child_resource_provider_.get(),
            mask_rect.size(), SinglePlaneFormat::kRGBA_8888,
            kPremul_SkAlphaType, gfx::ColorSpace(), MakePixelSpan(bitmap));
      } else {
        mask_resource_id = this->AllocateAndFillSoftwareResource(
            this->child_context_provider_, mask_rect.size(), bitmap);
      }

      // Return the mapped resource id.
      std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>
          resource_map = cc::SendResourceAndGetChildToParentMap(
              {mask_resource_id}, this->resource_provider_.get(),
              this->child_resource_provider_.get(),
              this->child_context_provider_->SharedImageInterface());
      mapped_mask_resource_id = resource_map[mask_resource_id];

      mask_uv_rect =
          gfx::ScaleRect(gfx::RectF(viewport_rect), 1.f / mask_rect.width(),
                         1.f / mask_rect.height()),  // mask_uv_rect
          mask_texture_size = gfx::Size(mask_rect.size());
    }

    {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          filter_pass_to_target_transform_, filter_pass_layer_rect_,
          root_pass.get(), gfx::MaskFilterInfo());
      auto* filter_pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      filter_pass_quad->SetNew(shared_state, filter_pass_layer_rect_,
                               filter_pass_layer_rect_, filter_pass_id,
                               mapped_mask_resource_id, mask_uv_rect,
                               mask_texture_size,
                               false);        // force_anti_aliasing_off
      filter_pass_quad->SetFilters(
          /*filters=*/{}, this->backdrop_filters_,
          this->backdrop_filter_bounds_,
          /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
          /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
    }

    const int kColumnWidth = device_viewport_rect.width() / 3;

    gfx::Rect left_rect = gfx::Rect(0, 0, kColumnWidth, 20);
    while (left_rect.y() < device_viewport_rect.height()) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, left_rect, root_pass.get(),
          gfx::MaskFilterInfo());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, left_rect, left_rect, SkColors::kGreen,
                         false);
      left_rect += gfx::Vector2d(0, left_rect.height() + 1);
    }

    gfx::Rect middle_rect = gfx::Rect(kColumnWidth + 1, 0, kColumnWidth, 20);
    while (middle_rect.y() < device_viewport_rect.height()) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, middle_rect, root_pass.get(),
          gfx::MaskFilterInfo());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, middle_rect, middle_rect, SkColors::kRed,
                         false);
      middle_rect += gfx::Vector2d(0, middle_rect.height() + 1);
    }

    gfx::Rect right_rect =
        gfx::Rect((kColumnWidth + 1) * 2, 0, kColumnWidth, 20);
    while (right_rect.y() < device_viewport_rect.height()) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, right_rect, root_pass.get(),
          gfx::MaskFilterInfo());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, right_rect, right_rect, SkColors::kBlue,
                         false);
      right_rect += gfx::Vector2d(0, right_rect.height() + 1);
    }

    SharedQuadState* shared_state = CreateTestSharedQuadState(
        identity_quad_to_target_transform, device_viewport_rect,
        root_pass.get(), gfx::MaskFilterInfo());
    auto* background_quad =
        root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    background_quad->SetNew(shared_state, device_viewport_rect,
                            device_viewport_rect, SkColors::kWhite, false);

    pass_list_.push_back(std::move(filter_pass));
    pass_list_.push_back(std::move(root_pass));
  }

  AggregatedRenderPassList pass_list_;
  cc::FilterOperations backdrop_filters_;
  std::optional<SkPath> backdrop_filter_bounds_;
  bool include_backdrop_mask_ = false;
  gfx::Transform filter_pass_to_target_transform_;
  gfx::Rect filter_pass_layer_rect_;
};

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestWithBackdropFilter,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

TEST_P(RendererPixelTestWithBackdropFilter, ZoomFilter) {
  backdrop_filters_.Append(cc::FilterOperation::CreateZoomFilter(2.0f, 20));
  SetUpRenderPassList();
  EXPECT_TRUE(RunPixelTest(
      &pass_list_,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter_zoom.png")),
      cc::ExactPixelComparator()));
}

TEST_P(RendererPixelTestWithBackdropFilter, OffsetFilter) {
  backdrop_filters_.Append(
      cc::FilterOperation::CreateOffsetFilter(gfx::Point(5, 5)));
  SetUpRenderPassList();

  base::FilePath expected_path(FILE_PATH_LITERAL("backdrop_filter_offset.png"));

  EXPECT_TRUE(
      RunPixelTest(&pass_list_, expected_path, cc::ExactPixelComparator()));
}

TEST_P(RendererPixelTestWithBackdropFilter, InvertFilter) {
  backdrop_filters_.Append(cc::FilterOperation::CreateInvertFilter(1.f));
  SetUpRenderPassList();
  EXPECT_TRUE(RunPixelTest(
      &pass_list_, base::FilePath(FILE_PATH_LITERAL("backdrop_filter.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTestWithBackdropFilter, InvertFilterWithMask) {
  backdrop_filters_.Append(cc::FilterOperation::CreateInvertFilter(1.f));
  include_backdrop_mask_ = true;
  SetUpRenderPassList();

  base::FilePath expected_path(
      is_software_renderer()
          ? FILE_PATH_LITERAL("backdrop_filter_masked_sw.png")
          : FILE_PATH_LITERAL("backdrop_filter_masked.png"));

  EXPECT_TRUE(RunPixelTest(&pass_list_, expected_path,
                           cc::FuzzyPixelOffByOneComparator()));
}

// Software renderer does not support anti-aliased edges.
TEST_P(GPURendererPixelTest, AntiAliasing) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  gfx::Transform red_quad_to_target_transform;
  red_quad_to_target_transform.Rotate(10);
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(red_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, rect, rect, SkColors::kRed, false);

  gfx::Transform yellow_quad_to_target_transform;
  yellow_quad_to_target_transform.Rotate(5);
  SharedQuadState* yellow_shared_state = CreateTestSharedQuadState(
      yellow_quad_to_target_transform, rect, pass.get(), gfx::MaskFilterInfo());

  auto* yellow = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(yellow_shared_state, rect, rect, SkColors::kYellow, false);

  gfx::Transform blue_quad_to_target_transform;
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(blue_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SkColors::kBlue, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath baseline =
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing_.png"))
          .InsertBeforeExtensionASCII(this->renderer_str());

  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, baseline, cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Software renderer does not support anti-aliased edges.
TEST_P(GPURendererPixelTest, AntiAliasingPerspective) {
  gfx::Rect rect(this->device_viewport_size_);

  auto pass = CreateTestRootRenderPass(AggregatedRenderPassId{1}, rect);

  gfx::Rect red_rect(0, 0, 180, 500);
  auto red_quad_to_target_transform = gfx::Transform::RowMajor(
      1.0f, 2.4520f, 10.6206f, 19.0f, 0.0f, 0.3528f, 5.9737f, 9.5f, 0.0f,
      -0.2250f, -0.9744f, 0.0f, 0.0f, 0.0225f, 0.0974f, 1.0f);
  SharedQuadState* red_shared_state = CreateTestSharedQuadState(
      red_quad_to_target_transform, red_rect, pass.get(), gfx::MaskFilterInfo());
  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, red_rect, red_rect, SkColors::kRed, false);

  gfx::Rect green_rect(19, 7, 180, 10);
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), green_rect, pass.get(),
                                gfx::MaskFilterInfo());
  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, green_rect, green_rect, SkColors::kGreen,
                false);

  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SkColors::kBlue, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath baseline =
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing_perspective_.png"))
          .InsertBeforeExtensionASCII(this->renderer_str());

  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, baseline, cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// This test tests that anti-aliasing works for axis aligned quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, AxisAligned) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);

  CreateTestAxisAlignedQuads(rect, SkColors::kRed, SkColors::kYellow, false,
                             false, pass.get());

  gfx::Transform blue_quad_to_target_transform;
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(blue_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SkColors::kBlue, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("axis_aligned.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// This test tests that forcing anti-aliasing off works as expected for
// solid color draw quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, SolidColorDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);
  pass->has_transparent_background = false;

  gfx::Transform hole_quad_to_target_transform;
  hole_quad_to_target_transform.Translate(50, 50);
  hole_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state =
      CreateTestSharedQuadState(hole_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* hole = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  hole->SetAll(hole_shared_state, rect, rect, false, SkColors::kTransparent,
               true);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, pass.get(), gfx::MaskFilterInfo());

  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(this->RunPixelTest(&pass_list, expected_result,
                                 cc::AlphaDiscardingExactPixelComparator()));
}

// This test tests that forcing anti-aliasing off works as expected for
// render pass draw quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, RenderPassDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  gfx::Transform transform_to_root;
  auto root_pass = CreateTestRenderPass(root_pass_id, rect, transform_to_root);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform child_pass_transform;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, rect, child_pass_transform);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* hole_shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, rect, child_pass.get(), gfx::MaskFilterInfo());
  SolidColorDrawQuad* hole =
      child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  hole->SetAll(hole_shared_state, rect, rect, false, SkColors::kTransparent,
               false);

  bool needs_blending = false;
  bool force_anti_aliasing_off = true;
  float backdrop_filter_quality = 1.0f;
  bool intersects_damage_under = true;
  gfx::Transform hole_pass_to_target_transform;
  hole_pass_to_target_transform.Translate(50, 50);
  hole_pass_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      hole_pass_to_target_transform, rect, root_pass.get(), gfx::MaskFilterInfo());
  AggregatedRenderPassDrawQuad* pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  pass_quad->SetAll(pass_shared_state, rect, rect, needs_blending,
                    child_pass_id, kInvalidResourceId, gfx::RectF(),
                    gfx::Size(), gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(),
                    force_anti_aliasing_off, backdrop_filter_quality,
                    intersects_damage_under,
                    /*filters=*/cc::FilterOperations(),
                    /*backdrop_filters=*/cc::FilterOperations(),
                    /*backdrop_filter_bounds=*/std::nullopt);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, root_pass.get(), gfx::MaskFilterInfo());

  SolidColorDrawQuad* green =
      root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(this->RunPixelTest(&pass_list, expected_result,
                                 cc::AlphaDiscardingExactPixelComparator()));
}

// This test tests that forcing anti-aliasing off works as expected for
// tile draw quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, TileDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(SkColors::kTransparent);

  gfx::Size tile_size(32, 32);
  ResourceId resource;
  if (!is_software_renderer()) {
    resource = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        tile_size, SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
        gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    resource = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, tile_size, bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);
  pass->has_transparent_background = false;

  bool needs_blending = false;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = true;
  gfx::Transform hole_quad_to_target_transform;
  hole_quad_to_target_transform.Translate(50, 50);
  hole_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state =
      CreateTestSharedQuadState(hole_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());
  TileDrawQuad* hole = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  hole->SetNew(hole_shared_state, rect, rect, needs_blending, mapped_resource,
               gfx::RectF(gfx::Rect(tile_size)), nearest_neighbor,
               force_anti_aliasing_off);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, pass.get(), gfx::MaskFilterInfo());

  SolidColorDrawQuad* green =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(this->RunPixelTest(&pass_list, expected_result,
                                 cc::AlphaDiscardingExactPixelComparator()));
}

// This test tests that forcing anti-aliasing off works as expected while
// blending is still enabled.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, BlendingWithoutAntiAliasing) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);
  pass->has_transparent_background = false;

  CreateTestAxisAlignedQuads(rect, SkColor4f{0.0f, 0.0f, 1.0f, 0.5},
                             SkColor4f{0.0f, 1.0f, 0.0f, 0.5f}, true, true,
                             pass.get());

  SharedQuadState* background_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* background_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  background_quad->SetNew(background_quad_state, rect, rect, SkColors::kBlack,
                          false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("translucent_quads_no_aa.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(this->RunPixelTest(&pass_list, expected_result,
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(GPURendererPixelTest, TrilinearFiltering) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  root_pass->has_transparent_background = false;

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  gfx::Rect child_pass_rect(
      ScaleToCeiledSize(this->device_viewport_size_, 4.0f));
  bool generate_mipmap = true;
  auto child_pass = std::make_unique<AggregatedRenderPass>();
  child_pass->SetAll(
      child_pass_id, child_pass_rect, child_pass_rect, transform_to_root,
      gfx::ContentColorUsage::kSRGB, false, false, false, generate_mipmap);

  gfx::Rect red_rect(child_pass_rect);
  // Small enough red rect that linear filtering will miss it but large enough
  // that it makes a meaningful contribution when using trilinear filtering.
  red_rect.ClampToCenteredSize(gfx::Size(2, child_pass_rect.height()));
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), red_rect, child_pass.get(),
                                gfx::MaskFilterInfo());
  auto* red = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, red_rect, red_rect, SkColors::kRed, false);

  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), child_pass_rect, child_pass.get(), gfx::MaskFilterInfo());
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, child_pass_rect, child_pass_rect,
               SkColors::kBlue, false);

  auto child_to_root_transform = gfx::TransformBetweenRects(
      gfx::RectF(child_pass_rect), gfx::RectF(viewport_rect));
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      child_to_root_transform, child_pass_rect, root_pass.get(), gfx::MaskFilterInfo());
  auto* child_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  child_pass_quad->SetNew(child_pass_shared_state, child_pass_rect,
                          child_pass_rect, child_pass_id, kInvalidResourceId,
                          gfx::RectF(), gfx::Size(), false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  if (is_skia_graphite()) {
    // Rendering with Graphite results in a imperceptible, one-unit difference
    // from the result when rendering with Skia's previous GPU backend.
    EXPECT_TRUE(this->RunPixelTest(
        &pass_list,
        base::FilePath(
            FILE_PATH_LITERAL("trilinear_filtering_skia_graphite.png")),
        cc::AlphaDiscardingExactPixelComparator()));
  } else {
    base::FilePath baseline =
        base::FilePath(FILE_PATH_LITERAL("trilinear_filtering.png"));

    if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
      baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
    }

    EXPECT_TRUE(this->RunPixelTest(&pass_list, baseline,
                                   cc::AlphaDiscardingExactPixelComparator()));
  }
}

class SoftwareRendererPixelTest : public VizPixelTest {
 public:
  SoftwareRendererPixelTest() : VizPixelTest(RendererType::kSoftware) {}
};

TEST_F(SoftwareRendererPixelTest, PictureDrawQuadIdentityScale) {
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // One clipped blue quad in the lower right corner.  Outside the clip
  // is red, which should not appear.
  gfx::Rect blue_rect(gfx::Size(100, 100));
  gfx::Rect blue_clip_rect(gfx::Point(50, 50), gfx::Size(50, 50));

  cc::FakeRecordingSource blue_recording(blue_rect.size());
  cc::PaintFlags red_flags;
  red_flags.setColor(SkColors::kRed);
  blue_recording.add_draw_rect_with_flags(blue_rect, red_flags);
  cc::PaintFlags blue_flags;
  blue_flags.setColor(SkColors::kBlue);
  blue_recording.add_draw_rect_with_flags(blue_clip_rect, blue_flags);
  blue_recording.Rerecord();

  scoped_refptr<cc::RasterSource> blue_raster_source =
      blue_recording.CreateRasterSource();

  gfx::Vector2d offset(viewport.bottom_right() - blue_rect.bottom_right());
  bool needs_blending = true;
  gfx::Transform blue_quad_to_target_transform;
  blue_quad_to_target_transform.Translate(offset.x(), offset.y());
  gfx::Rect blue_target_clip_rect = cc::MathUtil::MapEnclosingClippedRect(
      blue_quad_to_target_transform, blue_clip_rect);
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadStateClipped(blue_quad_to_target_transform, blue_rect,
                                       blue_target_clip_rect, pass.get());

  auto* blue_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();

  blue_quad->SetNew(blue_shared_state,
                    viewport,  // Intentionally bigger than clip.
                    viewport, needs_blending, gfx::RectF(viewport),
                    nearest_neighbor, viewport, 1.f, {},
                    blue_raster_source->GetDisplayItemList(),
                    cc::ScrollOffsetMap());

  // One viewport-filling green quad.
  cc::FakeRecordingSource green_recording(viewport.size());
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  green_recording.add_draw_rect_with_flags(viewport, green_flags);
  green_recording.Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording.CreateRasterSource();

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(green_quad_to_target_transform, viewport,
                                pass.get(), gfx::MaskFilterInfo());

  auto* green_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(green_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0.f, 0.f, 1.f, 1.f), nearest_neighbor, viewport,
                     1.f, {}, green_raster_source->GetDisplayItemList(),
                     cc::ScrollOffsetMap());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// Not WithSkiaGPUBackend since that path currently requires tiles for opacity.
TEST_F(SoftwareRendererPixelTest, PictureDrawQuadOpacity) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // One viewport-filling 0.5-opacity green quad.
  cc::FakeRecordingSource green_recording(viewport.size());
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  green_recording.add_draw_rect_with_flags(viewport, green_flags);
  green_recording.Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording.CreateRasterSource();

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(green_quad_to_target_transform, viewport,
                                pass.get(), gfx::MaskFilterInfo());
  green_shared_state->opacity = 0.5f;

  auto* green_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(green_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), nearest_neighbor, viewport, 1.f,
                     {}, green_raster_source->GetDisplayItemList(),
                     cc::ScrollOffsetMap());

  // One viewport-filling white quad.
  cc::FakeRecordingSource white_recording(viewport.size());
  cc::PaintFlags white_flags;
  white_flags.setColor(SkColors::kWhite);
  white_recording.add_draw_rect_with_flags(viewport, white_flags);
  white_recording.Rerecord();
  scoped_refptr<cc::RasterSource> white_raster_source =
      white_recording.CreateRasterSource();

  gfx::Transform white_quad_to_target_transform;
  SharedQuadState* white_shared_state = CreateTestSharedQuadState(
      white_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());

  auto* white_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  white_quad->SetNew(white_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), nearest_neighbor, viewport, 1.f,
                     {}, white_raster_source->GetDisplayItemList(),
                     cc::ScrollOffsetMap());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_F(SoftwareRendererPixelTest, PictureDrawQuadOpacityWithAlpha) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // One viewport-filling 0.5-opacity transparent quad.
  cc::FakeRecordingSource transparent_recording(viewport.size());
  cc::PaintFlags transparent_flags;
  transparent_flags.setColor(SkColors::kTransparent);
  transparent_recording.add_draw_rect_with_flags(viewport, transparent_flags);
  transparent_recording.Rerecord();
  scoped_refptr<cc::RasterSource> transparent_raster_source =
      transparent_recording.CreateRasterSource();

  gfx::Transform transparent_quad_to_target_transform;
  SharedQuadState* transparent_shared_state = CreateTestSharedQuadState(
      transparent_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());
  transparent_shared_state->opacity = 0.5f;

  auto* transparent_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  transparent_quad->SetNew(
      transparent_shared_state, viewport, viewport, needs_blending,
      gfx::RectF(0, 0, 1, 1), nearest_neighbor, viewport, 1.f, {},
      transparent_raster_source->GetDisplayItemList(), cc::ScrollOffsetMap());

  // One viewport-filling white quad.
  cc::FakeRecordingSource white_recording(viewport.size());
  cc::PaintFlags white_flags;
  white_flags.setColor(SkColors::kWhite);
  white_recording.add_draw_rect_with_flags(viewport, white_flags);
  white_recording.Rerecord();
  scoped_refptr<cc::RasterSource> white_raster_source =
      white_recording.CreateRasterSource();

  gfx::Transform white_quad_to_target_transform;
  SharedQuadState* white_shared_state = CreateTestSharedQuadState(
      white_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());

  auto* white_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  white_quad->SetNew(white_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), nearest_neighbor, viewport, 1.f,
                     {}, white_raster_source->GetDisplayItemList(),
                     cc::ScrollOffsetMap());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("white.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

void draw_point_color(SkCanvas* canvas,
                      SkScalar x,
                      SkScalar y,
                      SkColor4f color) {
  SkPaint paint;
  paint.setColor(color, nullptr /* SkColorSpace* colorSpace */);
  canvas->drawPoint(x, y, paint);
}

// This disables filtering by setting |nearest_neighbor| on the
// PictureDrawQuad.
TEST_F(SoftwareRendererPixelTest, PictureDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = true;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  ASSERT_NE(surface, nullptr);
  SkCanvas* canvas = surface->getCanvas();
  draw_point_color(canvas, 0, 0, SkColors::kGreen);
  draw_point_color(canvas, 0, 1, SkColors::kBlue);
  draw_point_color(canvas, 1, 0, SkColors::kBlue);
  draw_point_color(canvas, 1, 1, SkColors::kGreen);

  cc::FakeRecordingSource recording(viewport.size());
  recording.add_draw_image_with_flags(
      surface->makeImageSnapshot(), gfx::Point(),
      SkSamplingOptions(SkFilterMode::kLinear), cc::PaintFlags());
  recording.Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording.CreateRasterSource();

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               gfx::RectF(0, 0, 2, 2), nearest_neighbor, viewport, 1.f, {},
               raster_source->GetDisplayItemList(), cc::ScrollOffsetMap());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, PictureDrawQuadRasterInducingScroll) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  cc::PaintFlags red_flags;
  red_flags.setColor(SkColors::kRed);
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  cc::PaintFlags blue_flags;
  blue_flags.setColor(SkColors::kBlue);

  gfx::PointF blue_offset1(123, 456);
  auto scroll_list1 = base::MakeRefCounted<cc::DisplayItemList>();
  scroll_list1->StartPaint();
  scroll_list1->push<cc::DrawRectOp>(SkRect::MakeWH(1000, 1000), red_flags);
  scroll_list1->push<cc::DrawRectOp>(
      SkRect::MakeXYWH(blue_offset1.x(), blue_offset1.y(), 150, 100),
      blue_flags);
  scroll_list1->EndPaintOfUnpaired(gfx::Rect(1000, 1000));
  scroll_list1->Finalize();

  gfx::PointF blue_offset2(234, 789);
  auto scroll_list2 = base::MakeRefCounted<cc::DisplayItemList>();
  scroll_list2->StartPaint();
  scroll_list2->push<cc::DrawRectOp>(SkRect::MakeWH(1000, 1000), red_flags);
  scroll_list2->push<cc::DrawRectOp>(
      SkRect::MakeXYWH(blue_offset2.x(), blue_offset2.y(), 100, 100),
      blue_flags);
  scroll_list2->EndPaintOfUnpaired(gfx::Rect(1000, 1000));
  scroll_list2->Finalize();

  cc::ElementId scroll_element_id1(123);
  cc::ElementId scroll_element_id2(456);
  auto display_list = base::MakeRefCounted<cc::DisplayItemList>();
  display_list->StartPaint();
  display_list->push<cc::DrawRectOp>(SkRect::MakeWH(200, 200), green_flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(200, 200));

  // Draw scrolling contents op 1 under a clip.
  display_list->StartPaint();
  display_list->push<cc::SaveOp>();
  display_list->push<cc::TranslateOp>(100.f, 0.f);
  display_list->push<cc::ClipRectOp>(SkRect::MakeXYWH(0, 0, 100, 100),
                                     SkClipOp::kIntersect, false);
  display_list->EndPaintOfPairedBegin();
  display_list->PushDrawScrollingContentsOp(
      scroll_element_id1, std::move(scroll_list1), gfx::Rect(100, 0, 100, 100));
  display_list->StartPaint();
  display_list->push<cc::RestoreOp>();
  display_list->EndPaintOfPairedEnd();

  // Draw another scrolling contents op 2 under a translate and a clip.
  display_list->StartPaint();
  display_list->push<cc::SaveOp>();
  display_list->push<cc::TranslateOp>(0.f, 100.f);
  display_list->push<cc::ClipRectOp>(SkRect::MakeWH(100, 100),
                                     SkClipOp::kIntersect, false);
  display_list->EndPaintOfPairedBegin();
  display_list->PushDrawScrollingContentsOp(
      scroll_element_id2, std::move(scroll_list2), gfx::Rect(0, 100, 100, 100));
  display_list->StartPaint();
  display_list->push<cc::RestoreOp>();
  display_list->EndPaintOfPairedEnd();
  display_list->Finalize();

  EXPECT_EQ(2u, display_list->raster_inducing_scrolls().size());

  cc::FakeContentLayerClient client;
  client.set_display_item_list(std::move(display_list));
  cc::RecordingSource recording;
  cc::Region invalidation;
  recording.Update(gfx::Size(200, 200), 1, client, invalidation);
  scoped_refptr<cc::RasterSource> raster_source =
      recording.CreateRasterSource();

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());

  cc::ScrollOffsetMap raster_inducing_scroll_offsets = {
      {scroll_element_id1, blue_offset1},
      {scroll_element_id2, blue_offset2},
  };
  auto* quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               gfx::RectF(viewport), nearest_neighbor, viewport, 1.f, {},
               raster_source->GetDisplayItemList(),
               raster_inducing_scroll_offsets);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This disables filtering by setting |nearest_neighbor| on the
// TileDrawQuad.
TEST_P(RendererPixelTest, TileDrawQuadNearestNeighbor) {
  constexpr bool needs_blending = true;
  constexpr bool nearest_neighbor = true;
  constexpr bool force_anti_aliasing_off = false;
  const SharedImageFormat format = is_software_renderer()
                                       ? SinglePlaneFormat::kBGRA_8888
                                       : SinglePlaneFormat::kRGBA_8888;
  gfx::Rect viewport(this->device_viewport_size_);

  SkColorType ct = ToClosestSkColorType(format);
  SkImageInfo info = SkImageInfo::Make(2, 2, ct, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  draw_point_color(&canvas, 0, 0, SkColors::kGreen);
  draw_point_color(&canvas, 0, 1, SkColors::kBlue);
  draw_point_color(&canvas, 1, 0, SkColors::kBlue);
  draw_point_color(&canvas, 1, 1, SkColors::kGreen);

  gfx::Size tile_size(2, 2);
  ResourceId resource;
  if (!is_software_renderer()) {
    resource = CreateGpuResource(this->child_context_provider_,
                                 this->child_resource_provider_.get(),
                                 tile_size, format, kPremul_SkAlphaType,
                                 gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    resource = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, tile_size, bitmap);
  }
  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, gfx::RectF(gfx::Rect(tile_size)),
               nearest_neighbor, force_anti_aliasing_off);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This disables filtering by setting |nearest_neighbor| to true on the
// TextureDrawQuad.
TEST_F(SoftwareRendererPixelTest, TextureDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = true;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  draw_point_color(&canvas, 0, 0, SkColors::kGreen);
  draw_point_color(&canvas, 0, 1, SkColors::kBlue);
  draw_point_color(&canvas, 1, 0, SkColors::kBlue);
  draw_point_color(&canvas, 1, 1, SkColors::kGreen);

  gfx::Size tile_size(2, 2);
  ResourceId resource = this->AllocateAndFillSoftwareResource(
      this->child_context_provider_, tile_size, bitmap);

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, gfx::PointF(0, 0), gfx::PointF(2, 2),
               SkColors::kBlack, nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f)));
}

// This ensures filtering is enabled by setting |nearest_neighbor| to false on
// the TextureDrawQuad.
TEST_F(SoftwareRendererPixelTest, TextureDrawQuadLinear) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  {
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    draw_point_color(&canvas, 0, 0, SkColors::kGreen);
    draw_point_color(&canvas, 0, 1, SkColors::kBlue);
    draw_point_color(&canvas, 1, 0, SkColors::kBlue);
    draw_point_color(&canvas, 1, 1, SkColors::kGreen);
  }

  gfx::Size tile_size(2, 2);
  ResourceId resource = this->AllocateAndFillSoftwareResource(
      this->child_context_provider_, tile_size, bitmap);

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, gfx::PointF(0, 0), gfx::PointF(2, 2),
               SkColors::kBlack, nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Allow for a small amount of error as the blending alogrithm used by Skia is
  // affected by the offset in the expanded rect.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers_linear.png")),
      cc::FuzzyPixelComparator()
          .SetErrorPixelsPercentageLimit(100.f)
          .SetAbsErrorLimit(16)));
}

TEST_F(SoftwareRendererPixelTest, PictureDrawQuadNonIdentityScale) {
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  bool needs_blending = true;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // As scaling up the blue checkerboards will cause sampling on the GPU,
  // a few extra "cleanup rects" need to be added to clobber the blending
  // to make the output image more clean.  This will also test subrects
  // of the layer.
  gfx::Transform green_quad_to_target_transform;
  gfx::Rect green_rect1(gfx::Point(80, 0), gfx::Size(20, 100));
  gfx::Rect green_rect2(gfx::Point(0, 80), gfx::Size(100, 20));

  cc::FakeRecordingSource green_recording(viewport.size());

  cc::PaintFlags red_flags;
  red_flags.setColor(SkColors::kRed);
  green_recording.add_draw_rect_with_flags(viewport, red_flags);
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  green_recording.add_draw_rect_with_flags(green_rect1, green_flags);
  green_recording.add_draw_rect_with_flags(green_rect2, green_flags);
  green_recording.Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording.CreateRasterSource();

  SharedQuadState* top_right_green_shared_quad_state =
      CreateTestSharedQuadState(green_quad_to_target_transform, viewport,
                                pass.get(), gfx::MaskFilterInfo());

  auto* green_quad1 = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad1->SetNew(
      top_right_green_shared_quad_state, green_rect1, green_rect1,
      needs_blending, gfx::RectF(gfx::SizeF(green_rect1.size())),
      nearest_neighbor, green_rect1, 1.f, {},
      green_raster_source->GetDisplayItemList(), cc::ScrollOffsetMap());

  auto* green_quad2 = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad2->SetNew(
      top_right_green_shared_quad_state, green_rect2, green_rect2,
      needs_blending, gfx::RectF(gfx::SizeF(green_rect2.size())),
      nearest_neighbor, green_rect2, 1.f, {},
      green_raster_source->GetDisplayItemList(), cc::ScrollOffsetMap());

  // Add a green clipped checkerboard in the bottom right to help test
  // interleaving picture quad content and solid color content.
  gfx::Rect bottom_right_rect(
      gfx::Point(viewport.width() / 2, viewport.height() / 2),
      gfx::Size(viewport.width() / 2, viewport.height() / 2));
  SharedQuadState* bottom_right_green_shared_state =
      CreateTestSharedQuadStateClipped(green_quad_to_target_transform, viewport,
                                       bottom_right_rect, pass.get());
  auto* bottom_right_color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bottom_right_color_quad->SetNew(bottom_right_green_shared_state, viewport,
                                  viewport, SkColors::kGreen, false);

  // Add two blue checkerboards taking up the bottom left and top right,
  // but use content scales as content rects to make this happen.
  // The content is at a 4x content scale.
  gfx::Rect layer_rect(gfx::Size(20, 30));
  float contents_scale = 4.f;
  // Two rects that touch at their corners, arbitrarily placed in the layer.
  gfx::RectF blue_layer_rect1(gfx::PointF(5.5f, 9.0f), gfx::SizeF(2.5f, 2.5f));
  gfx::RectF blue_layer_rect2(gfx::PointF(8.0f, 6.5f), gfx::SizeF(2.5f, 2.5f));
  gfx::RectF union_layer_rect = blue_layer_rect1;
  union_layer_rect.Union(blue_layer_rect2);

  // Because scaling up will cause sampling outside the rects, add one extra
  // pixel of buffer at the final content scale.
  float inset = -1.f / contents_scale;
  blue_layer_rect1.Inset(inset);
  blue_layer_rect2.Inset(inset);

  cc::FakeRecordingSource recording(layer_rect.size());

  cc::Region outside(layer_rect);
  outside.Subtract(gfx::ToEnclosingRect(union_layer_rect));
  for (gfx::Rect rect : outside) {
    recording.add_draw_rect_with_flags(rect, red_flags);
  }

  cc::PaintFlags blue_flags;
  blue_flags.setColor(SkColors::kBlue);
  recording.add_draw_rectf_with_flags(blue_layer_rect1, blue_flags);
  recording.add_draw_rectf_with_flags(blue_layer_rect2, blue_flags);
  recording.Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording.CreateRasterSource();

  gfx::Rect content_union_rect(
      gfx::ToEnclosingRect(gfx::ScaleRect(union_layer_rect, contents_scale)));

  // At a scale of 4x the rectangles with a width of 2.5 will take up 10 pixels,
  // so scale an additional 10x to make them 100x100.
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Scale(10.0, 10.0);
  gfx::Rect quad_content_rect(gfx::Size(20, 20));
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, quad_content_rect, pass.get(), gfx::MaskFilterInfo());

  auto* blue_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  blue_quad->SetNew(blue_shared_state, quad_content_rect, quad_content_rect,
                    needs_blending, gfx::RectF(quad_content_rect),
                    nearest_neighbor, content_union_rect, contents_scale, {},
                    raster_source->GetDisplayItemList(), cc::ScrollOffsetMap());

  // Fill left half of viewport with green.
  gfx::Transform half_green_quad_to_target_transform;
  gfx::Rect half_green_rect(gfx::Size(viewport.width() / 2, viewport.height()));
  SharedQuadState* half_green_shared_state = CreateTestSharedQuadState(
      half_green_quad_to_target_transform, half_green_rect, pass.get(),
      gfx::MaskFilterInfo());
  auto* half_color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  half_color_quad->SetNew(half_green_shared_state, half_green_rect,
                          half_green_rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

class RendererPixelTestWithFlippedOutputSurface : public VizPixelTestWithParam {
 protected:
  gfx::SurfaceOrigin GetSurfaceOrigin() const override {
    return gfx::SurfaceOrigin::kTopLeft;
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestWithFlippedOutputSurface,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    RendererPixelTestWithFlippedOutputSurface);

TEST_P(RendererPixelTestWithFlippedOutputSurface, ExplicitFlipTest) {
  // This draws a blue rect above a yellow rect with an inverted output surface.
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Note: RunPixelTest() will issue a CopyOutputRequest on the root pass. The
  // implementation should realize the output surface is flipped, and return a
  // right-side up result regardless (i.e., NOT blue_yellow_flipped.png).
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTestWithFlippedOutputSurface, CheckChildPassUnflipped) {
  // This draws a blue rect above a yellow rect with an inverted output surface.
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Check that the child pass remains unflipped.
  EXPECT_TRUE(this->RunPixelTestWithCopyOutputRequest(
      &pass_list, pass_list.front().get(),
      base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(GPURendererPixelTest, CheckReadbackSubset) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // Draw a green quad full-size with a blue quad in the lower-right corner.
  gfx::Rect blue_rect(this->device_viewport_size_.width() * 3 / 4,
                      this->device_viewport_size_.height() * 3 / 4,
                      this->device_viewport_size_.width() * 3 / 4,
                      this->device_viewport_size_.height() * 3 / 4);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect green_rect(0, 0, this->device_viewport_size_.width(),
                       this->device_viewport_size_.height());
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(shared_state, green_rect, green_rect, SkColors::kGreen, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Check that the child pass remains unflipped.
  gfx::Rect capture_rect(this->device_viewport_size_.width() / 2,
                         this->device_viewport_size_.height() / 2,
                         this->device_viewport_size_.width() / 2,
                         this->device_viewport_size_.height() / 2);
  EXPECT_TRUE(this->RunPixelTestWithCopyOutputRequestAndArea(
      &pass_list, pass_list.front().get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")),
      cc::AlphaDiscardingExactPixelComparator(), &capture_rect));
}

TEST_P(GPURendererPixelTest, TextureQuadBatching) {
  // This test verifies that multiple texture quads using the same resource
  // get drawn correctly.  It implicitly is trying to test that the
  // renderer does the right thing with its draw quad cache.

  gfx::Rect rect(this->device_viewport_size_);
  bool needs_blending = false;

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // Make a mask.
  gfx::Rect mask_rect = rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(SkIntToScalar(4));
  paint.setColor(SkColors::kGreen);
  canvas.clear(SkColors::kWhite);
  gfx::Rect inset_rect = rect;
  while (!inset_rect.IsEmpty()) {
    inset_rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
    canvas.drawRect(SkRect::MakeXYWH(inset_rect.x(), inset_rect.y(),
                                     inset_rect.width(), inset_rect.height()),
                    paint);
    inset_rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
  }

  ResourceId resource = CreateGpuResource(
      this->child_context_provider_, this->child_resource_provider_.get(),
      mask_rect.size(), SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
      gfx::ColorSpace(), MakePixelSpan(bitmap));

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  // Arbitrary dividing lengths to divide up the resource into 16 quads.
  auto widths = std::to_array<int>({
      0,
      60,
      50,
      40,
  });
  auto heights = std::to_array<int>({
      0,
      10,
      80,
      50,
  });
  size_t num_quads = 4;
  for (size_t i = 0; i < num_quads; ++i) {
    int x_start = widths[i];
    int x_end = i == num_quads - 1 ? rect.width() : widths[i + 1];
    DCHECK_LE(x_end, rect.width());
    for (size_t j = 0; j < num_quads; ++j) {
      int y_start = heights[j];
      int y_end = j == num_quads - 1 ? rect.height() : heights[j + 1];
      DCHECK_LE(y_end, rect.height());

      gfx::Rect layer_rect(x_start, y_start, x_end - x_start, y_end - y_start);
      gfx::RectF tex_coord_rect = gfx::RectF(layer_rect);

      auto* texture_quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_quad->SetNew(
          shared_state, layer_rect, layer_rect, needs_blending, mapped_resource,
          tex_coord_rect.origin(), tex_coord_rect.bottom_right(),
          SkColors::kWhite, false,
          /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
          /*is_tex_coords_normalized=*/false);
    }
  }

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("spiral.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, TileQuadClamping) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;
  bool use_aa = false;

  gfx::Size layer_size(4, 4);
  gfx::Size tile_size(20, 20);
  gfx::Rect quad_rect(layer_size);
  gfx::RectF tex_coord_rect(quad_rect);

  // tile sized bitmap, with valid contents green and contents outside the
  // layer rect red.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(tile_size.width(), tile_size.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint red;
  red.setColor(SkColors::kRed);
  canvas.drawRect(SkRect::MakeWH(tile_size.width(), tile_size.height()), red);
  SkPaint green;
  green.setColor(SkColors::kGreen);
  canvas.drawRect(SkRect::MakeWH(layer_size.width(), layer_size.height()),
                  green);

  ResourceId resource;
  if (!is_software_renderer()) {
    resource = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        tile_size, SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
        gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    resource = this->AllocateAndFillSoftwareResource(
        this->child_context_provider_, tile_size, bitmap);
  }
  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // Green quad that should not show any red pixels from outside the
  // tex coord rect.
  gfx::Transform transform;
  transform.Scale(40, 40);
  SharedQuadState* quad_shared =
      CreateTestSharedQuadState(transform, gfx::Rect(layer_size), pass.get(),
                                gfx::MaskFilterInfo());
  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(quad_shared, gfx::Rect(layer_size), gfx::Rect(layer_size),
               needs_blending, mapped_resource, tex_coord_rect,
               nearest_neighbor, use_aa);

  // Green background.
  SharedQuadState* background_shared =
      CreateTestSharedQuadState(gfx::Transform(), viewport, pass.get(),
                                gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(background_shared, viewport, viewport, SkColors::kGreen,
                     false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, RoundedCornerSimpleSolidDrawQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Transform quad_to_target_transform;
  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height());
  gfx::Rect red_rect = blue_rect;
  blue_rect.Inset(kInset);

  gfx::RRectF rounded_corner_rrect(gfx::RectF(blue_rect), kCornerRadius);
  SharedQuadState* shared_state_rounded = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_rrect));

  auto* blue = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_rounded, blue_rect, blue_rect, SkColors::kBlue,
               false);

  SharedQuadState* shared_state_normal = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_normal, red_rect, red_rect, SkColors::kWhite,
                false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_simple.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.55f)));
}

TEST_P(GPURendererPixelTest, RoundedCornerSimpleTextureDrawQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Transform quad_to_target_transform;
  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height());
  gfx::Rect red_rect = blue_rect;
  blue_rect.Inset(kInset);

  gfx::RRectF rounded_corner_rrect(gfx::RectF(blue_rect), kCornerRadius);
  SharedQuadState* shared_state_rounded = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_rrect));

  const uint8_t colors[] = {0, 0, 255, 255, 0, 0, 255, 255,
                            0, 0, 255, 255, 0, 0, 255, 255};
  ResourceId resource = CreateGpuResource(
      this->child_context_provider_, this->child_resource_provider_.get(),
      gfx::Size(2, 2), SinglePlaneFormat::kRGBA_8888, kPremul_SkAlphaType,
      gfx::ColorSpace(), colors);

  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];
  bool needs_blending = true;
  const gfx::PointF tex_coord_top_left(0.0f, 0.0f);
  const gfx::PointF tex_coord_bottom_right(2.0f, 2.0f);
  const bool nearest_neighbor = false;
  auto* blue = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  blue->SetNew(shared_state_rounded, blue_rect, blue_rect, needs_blending,
               mapped_resource, tex_coord_top_left, tex_coord_bottom_right,
               SkColors::kBlack, nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);

  SharedQuadState* shared_state_normal = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_normal, red_rect, red_rect, SkColors::kWhite,
                false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_simple.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

TEST_P(RendererPixelTest, RoundedCornerOnRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;
  constexpr int kBlueCornerRadius = 10;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  gfx::Rect blue_rect = child_pass_local_rect;
  gfx::Vector2dF blue_offset_from_target(-30, 40);
  gfx::RRectF blue_rrect(gfx::RectF(blue_rect), kBlueCornerRadius);
  blue_rrect.Offset(blue_offset_from_target);
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(blue_offset_from_target);
  SharedQuadState* shared_state_with_rrect = CreateTestSharedQuadState(
      quad_to_target_transform, child_pass_local_rect, child_pass.get(),
      gfx::MaskFilterInfo(blue_rrect));
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_with_rrect, blue_rect, blue_rect, SkColors::kBlue,
               false);

  SharedQuadState* shared_state_without_rrect = CreateTestSharedQuadState(
      gfx::Transform(), child_pass_local_rect, child_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect yellow_rect = child_pass_local_rect;
  yellow_rect.Offset(30, -60);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state_without_rrect, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  gfx::Rect white_rect = child_pass_local_rect;
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadius);
  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds));
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  base::FilePath path(FILE_PATH_LITERAL("rounded_corner_render_pass_.png"));
  path = path.InsertBeforeExtensionASCII(this->renderer_str());
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, path, cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/40259140): currently failing on iOS.
#define MAYBE_LinearGradientOnRenderPass DISABLED_LinearGradientOnRenderPass
#else
#define MAYBE_LinearGradientOnRenderPass LinearGradientOnRenderPass
#endif  // BUILDFLAG(IS_IOS)
TEST_P(GPURendererPixelTest, MAYBE_LinearGradientOnRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  gfx::Rect white_rect = child_pass_local_rect;
  SharedQuadState* shared_state_without_rrect =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_local_rect,
                                child_pass.get(), gfx::MaskFilterInfo());
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadius);
  gfx::LinearGradient gradient_mask(330);
  gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
  gradient_mask.AddStep(.5, 255);
  gradient_mask.AddStep(1, 255);
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), pass_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_bounds, gradient_mask));
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("linear_gradient_render_pass.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/40259140): currently failing on iOS.
#define MAYBE_MultiLinearGradientOnRenderPass \
  DISABLED_MultiLinearGradientOnRenderPass
#else
#define MAYBE_MultiLinearGradientOnRenderPass MultiLinearGradientOnRenderPass
#endif  // BUILDFLAG(IS_IOS)
TEST_P(GPURendererPixelTest, MAYBE_MultiLinearGradientOnRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kCornerRadius = 20;
  constexpr int kInset = 20;
  constexpr int kBlueCornerRadius = 10;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  gfx::Rect blue_rect = child_pass_local_rect;
  gfx::Vector2dF blue_offset_from_target(-30, 40);
  gfx::RRectF blue_rrect(gfx::RectF(blue_rect), kBlueCornerRadius);
  blue_rrect.Offset(blue_offset_from_target);
  gfx::LinearGradient blue_gradient(0);
  blue_gradient.AddStep(/*fraction=*/0, /*alpha=*/255);
  blue_gradient.AddStep(1, 0);

  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(blue_offset_from_target);
  SharedQuadState* shared_state_with_rrect = CreateTestSharedQuadState(
      quad_to_target_transform, child_pass_local_rect, child_pass.get(),
      gfx::MaskFilterInfo(blue_rrect, blue_gradient));
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_with_rrect, blue_rect, blue_rect, SkColors::kBlue,
               false);

  gfx::Rect white_rect = child_pass_local_rect;
  SharedQuadState* shared_state_without_rrect =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_local_rect,
                                child_pass.get(), gfx::MaskFilterInfo());
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadius);
  gfx::LinearGradient gradient_mask(-30);
  gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
  gradient_mask.AddStep(.5, 255);
  gradient_mask.AddStep(1, 255);
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), pass_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_bounds, gradient_mask));
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(
          FILE_PATH_LITERAL("multi_linear_gradient_render_pass.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

TEST_P(RendererPixelTest, RoundedCornerMultiRadii) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr gfx::RoundedCornersF kCornerRadii(5, 15, 25, 35);
  constexpr int kInset = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadii);
  gfx::Rect blue_rect = pass_rect;
  blue_rect.set_height(blue_rect.height() / 2);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state_normal = CreateTestSharedQuadState(
      quad_to_target_transform, pass_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_bounds));
  auto* blue = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_normal, blue_rect, blue_rect, SkColors::kBlue,
               false);

  gfx::Rect yellow_rect = blue_rect;
  yellow_rect.Offset(0, blue_rect.height());

  auto* yellow = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state_normal, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  SharedQuadState* sqs_white = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect white_rect = gfx::Rect(this->device_viewport_size_);
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(sqs_white, white_rect, white_rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  // Software/skia renderer uses skia rrect to create rounded corner clip.
  // This results in a different corner path due to a different anti aliasing
  // approach than the fragment shader in gl renderer.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_multi_radii.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.55f)));
}

TEST_P(RendererPixelTest, RoundedCornerMultipleQads) {
  const gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr gfx::RoundedCornersF kCornerRadiiUL(5, 0, 0, 0);
  constexpr gfx::RoundedCornersF kCornerRadiiUR(0, 15, 0, 0);
  constexpr gfx::RoundedCornersF kCornerRadiiLR(0, 0, 25, 0);
  constexpr gfx::RoundedCornersF kCornerRadiiLL(0, 0, 0, 35);
  constexpr int kInset = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::RRectF rounded_corner_bounds_ul(gfx::RectF(pass_rect), kCornerRadiiUL);
  gfx::RRectF rounded_corner_bounds_ur(gfx::RectF(pass_rect), kCornerRadiiUR);
  gfx::RRectF rounded_corner_bounds_lr(gfx::RectF(pass_rect), kCornerRadiiLR);
  gfx::RRectF rounded_corner_bounds_ll(gfx::RectF(pass_rect), kCornerRadiiLL);

  gfx::Rect ul_rect = pass_rect;
  ul_rect.set_height(ul_rect.height() / 2);
  ul_rect.set_width(ul_rect.width() / 2);

  gfx::Rect ur_rect = pass_rect;
  ur_rect.set_x(ul_rect.right());
  ur_rect.set_width(pass_rect.right() - ur_rect.x());
  ur_rect.set_height(ul_rect.height());

  gfx::Rect lr_rect = pass_rect;
  lr_rect.set_y(ur_rect.bottom());
  lr_rect.set_x(ur_rect.x());
  lr_rect.set_width(ur_rect.width());
  lr_rect.set_height(pass_rect.bottom() - lr_rect.y());

  gfx::Rect ll_rect = pass_rect;
  ll_rect.set_y(lr_rect.y());
  ll_rect.set_width(ul_rect.width());
  ll_rect.set_height(lr_rect.height());

  SharedQuadState* shared_state_normal_ul =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_ul));

  SharedQuadState* shared_state_normal_ur =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_ur));

  SharedQuadState* shared_state_normal_lr =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_lr));

  SharedQuadState* shared_state_normal_ll =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_ll));

  auto* ul = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* ur = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* lr = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* ll = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

  ul->SetNew(shared_state_normal_ul, ul_rect, ul_rect, SkColors::kRed, false);
  ur->SetNew(shared_state_normal_ur, ur_rect, ur_rect, SkColors::kGreen, false);
  lr->SetNew(shared_state_normal_lr, lr_rect, lr_rect, SkColors::kBlue, false);
  ll->SetNew(shared_state_normal_ll, ll_rect, ll_rect, SkColors::kYellow,
             false);

  SharedQuadState* sqs_white = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect white_rect = gfx::Rect(this->device_viewport_size_);
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(sqs_white, white_rect, white_rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  auto comparator =
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.55f);
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_multi_quad.png")),
      comparator));
}

TEST_P(RendererPixelTest, BlurExpandsBounds) {
#if defined(MEMORY_SANITIZER)
  // TODO(crbug.com/40266622): Re-enable this test.
  // Skia Vulkan renderer had problems with this test when MSAN was enabled.
  if (renderer_type() == RendererType::kSkiaVk) {
    GTEST_SKIP();
  }
#endif  // defined(MEMORY_SANITIZER)

  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, gfx::Transform());

  // Add blue and yellow rect to child render pass.
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect blue_rect(0, 0, viewport_rect.width(), viewport_rect.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, viewport_rect.height() / 2, viewport_rect.width(),
                        viewport_rect.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  // Transform child pass off the screen, but within the blur size.
  gfx::Transform child_transform;
  child_transform.Translate(viewport_rect.width() + 5, 0);
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      child_transform, pass_rect, root_pass.get(), gfx::MaskFilterInfo());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), false);
  // Add 60px blur to draw quad.
  render_pass_quad->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateBlurFilter(20.0f)}),
      /*backdrop_filters=*/{},
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);

  // White background underneath
  SharedQuadState* blank_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  SolidColorDrawQuad* color_quad =
      root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(blank_state, viewport_rect, viewport_rect,
                     SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("blur_expands_bounds.png"));
  if (is_software_renderer()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_sw");
  } else if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, expected_result,
      // Allow 55/200 ~= 28% of pixels to be off by a small amount in each
      // channel to permit some small difference between renderers.
      cc::FuzzyPixelComparator()
          .SetAbsErrorLimit(2.0f)
          .SetErrorPixelsPercentageLimit(28.f)));
}

class RendererPixelTestWithOverdrawFeedback : public VizPixelTestWithParam {
 protected:
  void SetUp() override {
    this->debug_settings_.show_overdraw_feedback = true;
    VizPixelTestWithParam::SetUp();
  }
};

TEST_P(RendererPixelTestWithOverdrawFeedback, TranslucentRectangles) {
  // TODO(crbug.com/40279711): Enable this test once issue is fixed for
  // Graphite.
  if (is_skia_graphite()) {
    GTEST_SKIP();
  }
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);

  CreateTestAxisAlignedQuads(rect, SkColor4f{0.267f, 0.267f, 0.267f, 0.063f},
                             SkColor4f{0.8f, 0.8f, 0.8f, 0.063f}, true, false,
                             pass.get());

  gfx::Transform bg_quad_to_target_transform;
  SharedQuadState* bg_shared_state =
      CreateTestSharedQuadState(bg_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* bg = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bg->SetNew(bg_shared_state, rect, rect, SkColors::kBlack, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // TODO(xing.xu): investigate why overdraw feedback has small difference
  // (http://crbug.com/909971)
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("translucent_rectangles.png")),
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f)));
}

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestWithOverdrawFeedback,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    RendererPixelTestWithOverdrawFeedback);

class RendererPixelTestColorConversion : public VizPixelTestWithParam {
 public:
  RendererPixelTestColorConversion() {
    // Set a color space that is not suitable for blending to ensure we go
    // through the color conversion code paths.
    this->display_color_spaces_ =
        gfx::DisplayColorSpaces(gfx::ColorSpace::CreateSCRGBLinear80Nits());
    this->display_color_spaces_.SetSDRMaxLuminanceNits(80.f);
    this->display_color_spaces_.SetOutputFormats(SinglePlaneFormat::kRGBA_F16,
                                                 SinglePlaneFormat::kRGBA_F16);
  }
};

// Check that render pass updates do not blend with previous frames.
TEST_P(RendererPixelTestColorConversion,
       RenderPassClearsUpdatesWithHdrContent) {
  gfx::Rect rect(this->device_viewport_size_);

  SkColor4f semi_transparent_white = SkColors::kWhite;
  semi_transparent_white.fA = 0.5;

  const int value = 255 * semi_transparent_white.fA;
  std::vector<SkColor> expected_output_colors(
      rect.width() * rect.height(), SkColorSetARGB(255, value, value, value));

  // Draw two frames with semi-transparent content. Both frames should result in
  // the same image.
  for (int i = 0; i < 2; i++) {
    SCOPED_TRACE(base::StringPrintf("Frame %d", i));

    AggregatedRenderPassId id{1};
    auto pass = CreateTestRootRenderPass(id, rect);
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    SharedQuadState* shared_state = CreateTestSharedQuadState(
        gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(shared_state, rect, rect, semi_transparent_white, false);

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    EXPECT_TRUE(this->RunPixelTest(&pass_list, &expected_output_colors,
                                   cc::AlphaDiscardingExactPixelComparator()));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestColorConversion,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(RendererPixelTestColorConversion);

TEST_P(RendererPixelTest, CopyOutputRequestTrackedElements) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  // Create a child pass at (20, 20) with size (150, 150).
  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect child_pass_rect(0, 0, 150, 150);
  gfx::Transform child_transform_to_root;
  child_transform_to_root.Translate(20, 20);
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_rect,
                                         child_transform_to_root);

  // Add a green quad to the child pass.
  SharedQuadState* child_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_rect,
                                child_pass.get(), gfx::MaskFilterInfo());
  auto* child_color_quad =
      child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  child_color_quad->SetNew(child_shared_state, child_pass_rect, child_pass_rect,
                           SkColors::kGreen, false);

  // Add the child pass to the root pass.
  SharedQuadState* root_shared_state =
      CreateTestSharedQuadState(child_transform_to_root, child_pass_rect,
                                root_pass.get(), gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(root_shared_state, child_pass_rect,
                               child_pass_id, root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  TrackedElementId id1 = base::Token::CreateRandom();
  TrackedElementId id2 = base::Token::CreateRandom();
  TrackedElementId id3 = base::Token::CreateRandom();

  // Define the initial tracked element rects in root space.
  this->initial_tracked_element_rects_
      [TrackedElementFeature::kTrackedElementFeatureMax] = {
      {id1, gfx::Rect(40, 40, 20, 20)},  // Fully inside capture area.
      {id2, gfx::Rect(20, 20, 20, 20)},  // Partially inside capture area.
      {id3, gfx::Rect(20, 20, 5, 5)},    // Fully outside capture area.
  };

  // The capture rect will be relative to the child pass.
  gfx::Rect capture_rect(10, 10, 100, 100);

  // Run with copy request on the child pass.
  EXPECT_TRUE(this->RunPixelTestWithCopyOutputRequestAndArea(
      &pass_list, pass_list.front().get(),
      base::FilePath(FILE_PATH_LITERAL("green_small.png")),
      cc::AlphaDiscardingExactPixelComparator(), &capture_rect));

  // Verify results.
  const auto& results = this->result_tracked_element_rects_;
  ASSERT_TRUE(
      results.contains(TrackedElementFeature::kTrackedElementFeatureMax));
  const auto& rect_list =
      results.at(TrackedElementFeature::kTrackedElementFeatureMax);

  // Root pass is (0, 0, 200, 200)
  // Child pass is at (20, 20, 150, 150) - relative to root pass.
  // Capture area is at (10, 10, 100, 100) - relative to child pass.

  // id1
  // Root space: (40, 40, 20, 20)
  // Child pass space: (20, 20, 20, 20)
  // Capture space: (10, 10, 20, 20)

  // id2
  // Root space: (20, 20, 20, 20)
  // Child pass space: (0, 0, 20, 20)
  // Capture space: (0, 0, 10, 10)

  // id3
  // Root space: (20, 20, 5, 5)
  // Child pass space: (0, 0, 5, 5)
  // Does not intersect with capture area, should be dropped.

  ASSERT_EQ(rect_list.size(), 2u);

  EXPECT_EQ(rect_list[0].id, id1);
  EXPECT_EQ(rect_list[0].visible_bounds, gfx::Rect(10, 10, 20, 20));

  EXPECT_EQ(rect_list[1].id, id2);
  EXPECT_EQ(rect_list[1].visible_bounds, gfx::Rect(0, 0, 10, 10));
}

}  // namespace
}  // namespace viz
