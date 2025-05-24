// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/software_renderer.h"

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace viz {
namespace {
void DeleteSharedImage(scoped_refptr<gpu::ClientSharedImage> shared_image,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  shared_image->UpdateDestructionSyncToken(sync_token);
}

// A single rect drawn into `GenerateExpectedImage`.
struct ExpectedImageRect {
  gfx::Rect rect;
  SkColor4f color;
  bool debug_border = false;
};

SkBitmap GenerateExpectedImage(
    const gfx::Size& size,
    std::vector<ExpectedImageRect> back_to_front_rects) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  bitmap.eraseColor(SkColors::kTransparent);

  SkCanvas canvas(bitmap);

  for (const auto& filled_rect : back_to_front_rects) {
    SkPaint paint;
    paint.setColor(filled_rect.color);

    if (filled_rect.debug_border) {
      // `SoftwareRenderer` draws debug borders as a path with a miter join.
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeJoin(SkPaint::kMiter_Join);
      SkPath path;
      path.addRect(gfx::RectToSkRect(filled_rect.rect));
      canvas.drawPath(path, paint);
    } else {
      canvas.drawRect(gfx::RectToSkRect(filled_rect.rect), paint);
    }
  }

  return bitmap;
}

class SoftwareRendererTest : public VizPixelTest {
 public:
  SoftwareRendererTest() : VizPixelTest(RendererType::kSoftware) {}

  void SetUp() override {
    this->device_viewport_size_ = gfx::Size(100, 100);
    VizPixelTest::SetUp();
  }

  DisplayResourceProvider* resource_provider() const {
    return resource_provider_.get();
  }

  ClientResourceProvider* child_resource_provider() const {
    return child_resource_provider_.get();
  }

  ResourceId AllocateAndFillSoftwareResource(const gfx::Size& size,
                                             const SkBitmap& source) {
    auto* shared_image_interface =
        child_context_provider_->SharedImageInterface();
    auto shared_image =
        shared_image_interface->CreateSharedImageForSoftwareCompositor(
            {SinglePlaneFormat::kBGRA_8888, size, gfx::ColorSpace(),
             gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
             "SoftwareRendererTestSharedBitmap"});
    auto mapping = shared_image->Map();

    SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
    source.readPixels(info, mapping->GetMemoryForPlane(0).data(),
                      info.minRowBytes(), 0, 0);

    auto transferable_resource = TransferableResource::MakeSoftwareSharedImage(
        shared_image, shared_image_interface->GenVerifiedSyncToken(), size,
        SinglePlaneFormat::kBGRA_8888,
        TransferableResource::ResourceSource::kTileRasterTask);
    auto release_callback =
        base::BindOnce(&DeleteSharedImage, std::move(shared_image));

    return child_resource_provider_->ImportResource(
        std::move(transferable_resource), std::move(release_callback));
  }
};

TEST_F(SoftwareRendererTest, SolidColorQuad) {
  gfx::Size outer_size(100, 100);
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  gfx::Rect visible_rect(gfx::Point(1, 2), gfx::Size(98, 97));

  AggregatedRenderPassId root_render_pass_id{1};
  auto root_render_pass = std::make_unique<AggregatedRenderPass>();
  root_render_pass->SetNew(root_render_pass_id, outer_rect, outer_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), outer_rect, outer_rect,
                            gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                            /*contents_opaque=*/true, /*opacity_f=*/1.0,
                            SkBlendMode::kSrcOver, /*sorting_context=*/0,
                            /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* inner_quad =
      root_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  inner_quad->SetNew(shared_quad_state, inner_rect, inner_rect, SkColors::kCyan,
                     false);
  inner_quad->visible_rect = visible_rect;
  auto* outer_quad =
      root_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  outer_quad->SetNew(shared_quad_state, outer_rect, outer_rect,
                     SkColors::kYellow, false);

  AggregatedRenderPassList list;
  list.push_back(std::move(root_render_pass));

  EXPECT_TRUE(
      RunPixelTest(&list,
                   GenerateExpectedImage(
                       outer_size,
                       {
                           {.rect = outer_rect, .color = SkColors::kYellow},
                           {.rect = visible_rect, .color = SkColors::kCyan},
                       }),
                   cc::ExactPixelComparator()));
}

TEST_F(SoftwareRendererTest, DebugBorderDrawQuad) {
  gfx::Size rect_size(10, 10);
  gfx::Size full_size(100, 100);
  gfx::Rect screen_rect(full_size);
  gfx::Rect rect_1(rect_size);
  gfx::Rect rect_2(gfx::Point(1, 1), rect_size);
  gfx::Rect rect_3(gfx::Point(2, 2), rect_size);
  gfx::Rect rect_4(gfx::Point(3, 3), rect_size);

  AggregatedRenderPassId root_render_pass_id{1};
  auto root_render_pass = std::make_unique<AggregatedRenderPass>();
  root_render_pass->SetNew(root_render_pass_id, screen_rect, screen_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), screen_rect, screen_rect,
                            gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                            /*contents_opaque=*/true, /*opacity_f=*/1.0,
                            SkBlendMode::kSrcOver, /*sorting_context=*/0,
                            /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* quad_1 =
      root_render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  quad_1->SetNew(shared_quad_state, rect_1, rect_1, SkColors::kCyan, false);
  auto* quad_2 =
      root_render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  quad_2->SetNew(shared_quad_state, rect_2, rect_2, SkColors::kMagenta, false);

  auto* quad_3 =
      root_render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  quad_3->SetNew(shared_quad_state, rect_3, rect_3, SkColors::kYellow, false);

  // Test one non-opaque color.
  // TODO(crbug.com/40219248): Colors clearly get transformed into ints at some
  // point in the pipeline, so we need to use values n/255 for now.
  SkColor4f semi_transparent_white =
      SkColor4f{1.0f, 1.0f, 1.0f, 128.0 / 255.0f};
  auto* quad_4 =
      root_render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  quad_4->SetNew(shared_quad_state, rect_4, rect_4, semi_transparent_white,
                 false);

  AggregatedRenderPassList list;
  list.push_back(std::move(root_render_pass));

  EXPECT_TRUE(RunPixelTest(
      &list,
      GenerateExpectedImage(
          full_size,
          {
              {.rect = rect_4,
               .color = semi_transparent_white,
               .debug_border = true},
              {.rect = rect_3,
               .color = SkColors::kYellow,
               .debug_border = true},
              {.rect = rect_2,
               .color = SkColors::kMagenta,
               .debug_border = true},
              {.rect = rect_1, .color = SkColors::kCyan, .debug_border = true},
          }),
      cc::ExactPixelComparator()));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(SoftwareRendererTest, TileQuad) {
  gfx::Size outer_size(100, 100);
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  bool needs_blending = false;

  SkBitmap yellow_tile;
  yellow_tile.allocN32Pixels(outer_size.width(), outer_size.height());
  yellow_tile.eraseColor(SK_ColorYELLOW);

  SkBitmap cyan_tile;
  cyan_tile.allocN32Pixels(inner_size.width(), inner_size.height());
  cyan_tile.eraseColor(SK_ColorCYAN);

  ResourceId resource_yellow =
      this->AllocateAndFillSoftwareResource(outer_size, yellow_tile);
  ResourceId resource_cyan =
      this->AllocateAndFillSoftwareResource(inner_size, cyan_tile);

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource_yellow, resource_cyan}, resource_provider(),
          child_resource_provider(), nullptr);
  ResourceId mapped_resource_yellow = resource_map[resource_yellow];
  ResourceId mapped_resource_cyan = resource_map[resource_cyan];

  gfx::Rect root_rect = outer_rect;

  AggregatedRenderPassId root_render_pass_id{1};
  auto root_render_pass = std::make_unique<AggregatedRenderPass>();
  root_render_pass->SetNew(root_render_pass_id, root_rect, root_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), outer_rect, outer_rect,
                            gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                            /*contents_opaque=*/true, /*opacity_f=*/1.0,
                            SkBlendMode::kSrcOver, /*sorting_context=*/0,
                            /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* inner_quad = root_render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  inner_quad->SetNew(shared_quad_state, inner_rect, inner_rect, needs_blending,
                     mapped_resource_cyan, gfx::RectF(gfx::SizeF(inner_size)),
                     inner_size, false, false);
  auto* outer_quad = root_render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  outer_quad->SetNew(shared_quad_state, outer_rect, outer_rect, needs_blending,
                     mapped_resource_yellow, gfx::RectF(gfx::SizeF(outer_size)),
                     outer_size, false, false);

  AggregatedRenderPassList list;
  list.push_back(std::move(root_render_pass));

  EXPECT_TRUE(
      RunPixelTest(&list,
                   GenerateExpectedImage(
                       outer_size,
                       {
                           {.rect = outer_rect, .color = SkColors::kYellow},
                           {.rect = inner_rect, .color = SkColors::kCyan},
                       }),
                   cc::ExactPixelComparator()));
}

TEST_F(SoftwareRendererTest, TileQuadVisibleRect) {
  gfx::Size tile_size(100, 100);
  gfx::Rect tile_rect(tile_size);
  gfx::Rect visible_rect = tile_rect;
  bool needs_blending = false;
  visible_rect.Inset(gfx::Insets::TLBR(2, 1, 4, 3));

  SkBitmap cyan_tile;  // The lowest five rows are yellow.
  cyan_tile.allocN32Pixels(tile_size.width(), tile_size.height());
  cyan_tile.eraseColor(SK_ColorCYAN);
  cyan_tile.eraseArea(SkIRect::MakeLTRB(0, visible_rect.bottom() - 1,
                                        tile_rect.width(), tile_rect.bottom()),
                      SK_ColorYELLOW);

  ResourceId resource_cyan =
      AllocateAndFillSoftwareResource(tile_size, cyan_tile);

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource_cyan}, resource_provider(), child_resource_provider(),
          nullptr);
  ResourceId mapped_resource_cyan = resource_map[resource_cyan];

  gfx::Rect root_rect(tile_size);
  AggregatedRenderPassId root_render_pass_id{1};
  std::unique_ptr<AggregatedRenderPass> root_render_pass =
      std::make_unique<AggregatedRenderPass>();
  root_render_pass->SetNew(root_render_pass_id, root_rect, root_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), tile_rect, tile_rect,
                            gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                            /*contents_opaque=*/true, /*opacity_f=*/1.0,
                            SkBlendMode::kSrcOver, /*sorting_context=*/0,
                            /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  auto* quad = root_render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(shared_quad_state, tile_rect, tile_rect, needs_blending,
               mapped_resource_cyan, gfx::RectF(gfx::SizeF(tile_size)),
               tile_size, false, false);
  quad->visible_rect = visible_rect;

  AggregatedRenderPassList list;
  list.push_back(std::move(root_render_pass));

  EXPECT_TRUE(RunPixelTest(
      &list,
      GenerateExpectedImage(
          root_rect.size(),
          {
              {.rect = visible_rect, .color = SkColors::kCyan},
              // Ensure last visible line is correct.
              {.rect = gfx::Rect(visible_rect.x(), visible_rect.bottom() - 1,
                                 visible_rect.width(), 1),
               .color = SkColors::kYellow},
          }),
      cc::ExactPixelComparator()));
}
#endif  // BUILDFLAG(IS_ANDROID)

class SoftwareRendererTestShouldClearRootRenderPass
    : public SoftwareRendererTest {
 public:
  void SetUp() override {
    renderer_settings_.should_clear_root_render_pass = false;
    SoftwareRendererTest::SetUp();
  }
};

TEST_F(SoftwareRendererTestShouldClearRootRenderPass,
       ShouldClearRootRenderPass) {
  gfx::Size viewport_size(100, 100);

  AggregatedRenderPassList list;

  // Draw a fullscreen green quad in a first frame.
  AggregatedRenderPassId root_clear_pass_id{1};
  AggregatedRenderPass* root_clear_pass =
      cc::AddRenderPass(&list, root_clear_pass_id, gfx::Rect(viewport_size),
                        gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_clear_pass, gfx::Rect(viewport_size), SkColors::kGreen);

  EXPECT_TRUE(RunPixelTest(
      &list,
      GenerateExpectedImage(
          viewport_size,
          {
              {.rect = gfx::Rect(viewport_size), .color = SkColors::kGreen},
          }),
      cc::ExactPixelComparator()));

  // Draw a smaller magenta rect without filling the viewport in a separate
  // frame.
  gfx::Rect smaller_rect(20, 20, 60, 60);

  AggregatedRenderPassId root_smaller_pass_id{2};
  AggregatedRenderPass* root_smaller_pass =
      cc::AddRenderPass(&list, root_smaller_pass_id, gfx::Rect(viewport_size),
                        gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_smaller_pass, smaller_rect, SkColors::kMagenta);

  EXPECT_TRUE(RunPixelTest(
      &list,
      GenerateExpectedImage(
          viewport_size,
          {
              // If we didn't clear, the borders should still be green.
              {.rect = gfx::Rect(viewport_size), .color = SkColors::kGreen},
              {.rect = smaller_rect, .color = SkColors::kMagenta},
          }),
      cc::ExactPixelComparator()));
}

TEST_F(SoftwareRendererTest, RenderPassVisibleRect) {
  gfx::Size viewport_size(100, 100);

  AggregatedRenderPassList list;

  // Pass drawn as inner quad is magenta.
  gfx::Rect smaller_rect(20, 20, 60, 60);
  AggregatedRenderPassId smaller_pass_id{2};
  auto* smaller_pass =
      cc::AddRenderPass(&list, smaller_pass_id, smaller_rect, gfx::Transform(),
                        cc::FilterOperations());
  cc::AddQuad(smaller_pass, smaller_rect, SkColors::kMagenta);

  // Root pass is green.
  AggregatedRenderPassId root_clear_pass_id{1};
  AggregatedRenderPass* root_clear_pass =
      AddRenderPass(&list, root_clear_pass_id, gfx::Rect(viewport_size),
                    gfx::Transform(), cc::FilterOperations());
  cc::AddRenderPassQuad(root_clear_pass, smaller_pass);
  cc::AddQuad(root_clear_pass, gfx::Rect(viewport_size), SkColors::kGreen);

  // Interior pass quad has smaller visible rect.
  gfx::Rect interior_visible_rect(30, 30, 40, 40);
  root_clear_pass->quad_list.front()->visible_rect = interior_visible_rect;

  EXPECT_TRUE(RunPixelTest(
      &list,
      GenerateExpectedImage(
          viewport_size,
          {
              {.rect = gfx::Rect(viewport_size), .color = SkColors::kGreen},
              {.rect = interior_visible_rect, .color = SkColors::kMagenta},
          }),
      cc::ExactPixelComparator()));
}

TEST_F(SoftwareRendererTest, ClipRoundRect) {
  gfx::Size viewport_size(100, 100);
  gfx::Rect clip_rect = gfx::Rect(1, 1, 30, 30);

  AggregatedRenderPassList list;
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass =
      AddRenderPass(&list, root_pass_id, gfx::Rect(viewport_size),
                    gfx::Transform(), cc::FilterOperations());

  // Draw outer rect with clipping.
  {
    gfx::Size outer_size(50, 50);
    gfx::Rect outer_rect(outer_size);

    SharedQuadState* shared_quad_state =
        root_pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(gfx::Transform(), outer_rect, outer_rect,
                              gfx::MaskFilterInfo(), clip_rect,
                              /*contents_opaque=*/true, /*opacity_f=*/1.0,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* outer_quad = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    outer_quad->SetNew(shared_quad_state, outer_rect, outer_rect,
                       SkColors::kGreen, false);
  }

  // Draw inner round rect.
  {
    gfx::Size inner_size(20, 20);
    gfx::Rect inner_rect(inner_size);

    SharedQuadState* shared_quad_state =
        root_pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(
        gfx::Transform(), inner_rect, inner_rect,
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(5, 5, 10, 10), 2)),
        /*clip=*/std::nullopt, /*contents_opaque=*/true, /*opacity_f=*/1.0,
        SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* inner_quad = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    inner_quad->SetNew(shared_quad_state, inner_rect, inner_rect,
                       SkColors::kRed, false);
  }

  EXPECT_TRUE(RunPixelTest(
      &list,
      GenerateExpectedImage(viewport_size,
                            {
                                {.rect = clip_rect, .color = SkColors::kGreen},
                            }),
      cc::ExactPixelComparator()));
}

class SoftwareRendererTestPartialSwap : public SoftwareRendererTest {
  void SetUp() override {
    renderer_settings_.partial_swap_enabled = true;
    SoftwareRendererTest::SetUp();
  }
};

TEST_F(SoftwareRendererTestPartialSwap, PartialSwap) {
  gfx::Size viewport_size(100, 100);

  {
    // Draw one black frame to make sure output surface is reshaped before
    // tests.
    AggregatedRenderPassList list;
    AggregatedRenderPassId root_pass_id{1};
    SurfaceDamageRectList surface_damage_rect_list;
    auto* root_pass =
        AddRenderPass(&list, root_pass_id, gfx::Rect(viewport_size),
                      gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SkColors::kBlack);

    // Partial frame, we should pass this rect to the SoftwareOutputDevice.
    // partial swap is enabled.
    root_pass->damage_rect = gfx::Rect(viewport_size);

    EXPECT_TRUE(RunPixelTest(
        &list,
        GenerateExpectedImage(
            viewport_size,
            {
                {.rect = gfx::Rect(viewport_size), .color = SkColors::kBlack},
            }),
        cc::ExactPixelComparator()));
  }
  {
    AggregatedRenderPassList list;
    AggregatedRenderPassId root_pass_id{1};
    SurfaceDamageRectList surface_damage_rect_list;
    auto* root_pass =
        AddRenderPass(&list, root_pass_id, gfx::Rect(viewport_size),
                      gfx::Transform(), cc::FilterOperations());
    cc::AddQuad(root_pass, gfx::Rect(viewport_size), SkColors::kGreen);

    // Partial frame, only this region will draw, even though the quad covers
    // the whole frame.
    gfx::Rect damage_rect = gfx::Rect(2, 2, 3, 3);
    root_pass->damage_rect = damage_rect;

    EXPECT_TRUE(RunPixelTest(
        &list,
        GenerateExpectedImage(
            viewport_size,
            {
                {.rect = gfx::Rect(viewport_size), .color = SkColors::kBlack},
                {.rect = damage_rect, .color = SkColors::kGreen},
            }),
        cc::ExactPixelComparator()));
  }
}

}  // namespace
}  // namespace viz
