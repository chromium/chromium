// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/software_renderer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/run_loop.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "ui/gfx/skia_util.h"

namespace viz {
namespace {

class SoftwareRendererTest : public testing::Test {
 public:
  void InitializeRenderer(
      std::unique_ptr<SoftwareOutputDevice> software_output_device) {
    output_surface_ =
        FakeOutputSurface::CreateSoftware(std::move(software_output_device));
    output_surface_->BindToClient(&output_surface_client_);

    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        DisplayResourceProvider::kSoftware, nullptr,
        shared_bitmap_manager_.get());
    renderer_ = std::make_unique<SoftwareRenderer>(
        &settings_, output_surface_.get(), resource_provider());
    renderer_->Initialize();
    renderer_->SetVisible(true);

    child_resource_provider_ = std::make_unique<ClientResourceProvider>(true);
  }

  void TearDown() override {
    if (child_resource_provider_)
      child_resource_provider_->ShutdownAndReleaseAllResources();
    child_resource_provider_ = nullptr;
  }

  DisplayResourceProvider* resource_provider() const {
    return resource_provider_.get();
  }

  ClientResourceProvider* child_resource_provider() const {
    return child_resource_provider_.get();
  }

  SoftwareRenderer* renderer() const { return renderer_.get(); }

  ResourceId AllocateAndFillSoftwareResource(const gfx::Size& size,
                                             const SkBitmap& source) {
    base::MappedReadOnlyRegion shm =
        bitmap_allocation::AllocateSharedBitmap(size, RGBA_8888);
    SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
    source.readPixels(info, shm.mapping.memory(), info.minRowBytes(), 0, 0);

    // Registers the SharedBitmapId in the display compositor.
    SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
    shared_bitmap_manager_->ChildAllocatedSharedBitmap(shm.region.Map(),
                                                       shared_bitmap_id);

    // Makes a resource id that refers to the registered SharedBitmapId.
    return child_resource_provider_->ImportResource(
        TransferableResource::MakeSoftware(shared_bitmap_id, size, RGBA_8888),
        SingleReleaseCallback::Create(base::DoNothing()));
  }

  std::unique_ptr<SkBitmap> DrawAndCopyOutput(RenderPassList* list,
                                              float device_scale_factor,
                                              gfx::Size viewport_size) {
    std::unique_ptr<SkBitmap> bitmap_result;
    base::RunLoop loop;

    list->back()->copy_requests.push_back(std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA_BITMAP,
        base::BindOnce(&SoftwareRendererTest::SaveBitmapResult,
                       base::Unretained(&bitmap_result), loop.QuitClosure())));

    renderer()->DrawFrame(list, device_scale_factor, viewport_size);
    loop.Run();
    return bitmap_result;
  }

  static void SaveBitmapResult(std::unique_ptr<SkBitmap>* bitmap_result,
                               base::OnceClosure quit_closure,
                               std::unique_ptr<CopyOutputResult> result) {
    DCHECK(!result->IsEmpty());
    DCHECK_EQ(result->format(), CopyOutputResult::Format::RGBA_BITMAP);
    *bitmap_result = std::make_unique<SkBitmap>(result->AsSkBitmap());
    DCHECK((*bitmap_result)->readyToDraw());
    std::move(quit_closure).Run();
  }

 protected:
  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<FakeOutputSurface> output_surface_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<SoftwareRenderer> renderer_;
};

TEST_F(SoftwareRendererTest, SolidColorQuad) {
  gfx::Size outer_size(100, 100);
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  gfx::Rect visible_rect(gfx::Point(1, 2), gfx::Size(98, 97));

  InitializeRenderer(std::make_unique<SoftwareOutputDevice>());

  int root_render_pass_id = 1;
  std::unique_ptr<RenderPass> root_render_pass = RenderPass::Create();
  root_render_pass->SetNew(root_render_pass_id, outer_rect, outer_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), outer_rect, outer_rect,
                            gfx::RRectF(), outer_rect, false, true, 1.0,
                            SkBlendMode::kSrcOver, 0);
  auto* inner_quad =
      root_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  inner_quad->SetNew(shared_quad_state, inner_rect, inner_rect, SK_ColorCYAN,
                     false);
  inner_quad->visible_rect = visible_rect;
  auto* outer_quad =
      root_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  outer_quad->SetNew(shared_quad_state, outer_rect, outer_rect, SK_ColorYELLOW,
                     false);

  RenderPassList list;
  list.push_back(std::move(root_render_pass));

  float device_scale_factor = 1.f;
  std::unique_ptr<SkBitmap> output =
      DrawAndCopyOutput(&list, device_scale_factor, outer_size);
  EXPECT_EQ(outer_rect.width(), output->info().width());
  EXPECT_EQ(outer_rect.height(), output->info().height());

  EXPECT_EQ(SK_ColorYELLOW, output->getColor(0, 0));
  EXPECT_EQ(SK_ColorYELLOW,
            output->getColor(outer_size.width() - 1, outer_size.height() - 1));
  EXPECT_EQ(SK_ColorYELLOW, output->getColor(1, 1));
  EXPECT_EQ(SK_ColorCYAN, output->getColor(1, 2));
  EXPECT_EQ(SK_ColorCYAN,
            output->getColor(inner_size.width() - 1, inner_size.height() - 1));
}

TEST_F(SoftwareRendererTest, TileQuad) {
  gfx::Size outer_size(100, 100);
  gfx::Size inner_size(98, 98);
  gfx::Rect outer_rect(outer_size);
  gfx::Rect inner_rect(gfx::Point(1, 1), inner_size);
  bool needs_blending = false;
  InitializeRenderer(std::make_unique<SoftwareOutputDevice>());

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
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource_yellow, resource_cyan}, resource_provider(),
          child_resource_provider(), nullptr);
  ResourceId mapped_resource_yellow = resource_map[resource_yellow];
  ResourceId mapped_resource_cyan = resource_map[resource_cyan];

  gfx::Rect root_rect = outer_rect;

  int root_render_pass_id = 1;
  std::unique_ptr<RenderPass> root_render_pass = RenderPass::Create();
  root_render_pass->SetNew(root_render_pass_id, root_rect, root_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), outer_rect, outer_rect,
                            gfx::RRectF(), outer_rect, false, true, 1.0,
                            SkBlendMode::kSrcOver, 0);
  auto* inner_quad = root_render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  inner_quad->SetNew(shared_quad_state, inner_rect, inner_rect, needs_blending,
                     mapped_resource_cyan, gfx::RectF(gfx::SizeF(inner_size)),
                     inner_size, false, false, false);
  auto* outer_quad = root_render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  outer_quad->SetNew(shared_quad_state, outer_rect, outer_rect, needs_blending,
                     mapped_resource_yellow, gfx::RectF(gfx::SizeF(outer_size)),
                     outer_size, false, false, false);

  RenderPassList list;
  list.push_back(std::move(root_render_pass));

  float device_scale_factor = 1.f;
  std::unique_ptr<SkBitmap> output =
      DrawAndCopyOutput(&list, device_scale_factor, outer_size);
  EXPECT_EQ(outer_rect.width(), output->info().width());
  EXPECT_EQ(outer_rect.height(), output->info().height());

  EXPECT_EQ(SK_ColorYELLOW, output->getColor(0, 0));
  EXPECT_EQ(SK_ColorYELLOW,
            output->getColor(outer_size.width() - 1, outer_size.height() - 1));
  EXPECT_EQ(SK_ColorCYAN, output->getColor(1, 1));
  EXPECT_EQ(SK_ColorCYAN,
            output->getColor(inner_size.width() - 1, inner_size.height() - 1));
}

TEST_F(SoftwareRendererTest, TileQuadVisibleRect) {
  gfx::Size tile_size(100, 100);
  gfx::Rect tile_rect(tile_size);
  gfx::Rect visible_rect = tile_rect;
  bool needs_blending = false;
  visible_rect.Inset(1, 2, 3, 4);
  InitializeRenderer(std::make_unique<SoftwareOutputDevice>());

  SkBitmap cyan_tile;  // The lowest five rows are yellow.
  cyan_tile.allocN32Pixels(tile_size.width(), tile_size.height());
  cyan_tile.eraseColor(SK_ColorCYAN);
  cyan_tile.eraseArea(SkIRect::MakeLTRB(0, visible_rect.bottom() - 1,
                                        tile_rect.width(), tile_rect.bottom()),
                      SK_ColorYELLOW);

  ResourceId resource_cyan =
      AllocateAndFillSoftwareResource(tile_size, cyan_tile);

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource_cyan}, resource_provider(), child_resource_provider(),
          nullptr);
  ResourceId mapped_resource_cyan = resource_map[resource_cyan];

  gfx::Rect root_rect(tile_size);
  int root_render_pass_id = 1;
  std::unique_ptr<RenderPass> root_render_pass = RenderPass::Create();
  root_render_pass->SetNew(root_render_pass_id, root_rect, root_rect,
                           gfx::Transform());
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), tile_rect, tile_rect,
                            gfx::RRectF(), tile_rect, false, true, 1.0,
                            SkBlendMode::kSrcOver, 0);
  auto* quad = root_render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(shared_quad_state, tile_rect, tile_rect, needs_blending,
               mapped_resource_cyan, gfx::RectF(gfx::SizeF(tile_size)),
               tile_size, false, false, false);
  quad->visible_rect = visible_rect;

  RenderPassList list;
  list.push_back(std::move(root_render_pass));

  float device_scale_factor = 1.f;
  std::unique_ptr<SkBitmap> output =
      DrawAndCopyOutput(&list, device_scale_factor, tile_size);
  EXPECT_EQ(tile_rect.width(), output->info().width());
  EXPECT_EQ(tile_rect.height(), output->info().height());

  // Check portion of tile not in visible rect isn't drawn.
  const unsigned int kTransparent = SK_ColorTRANSPARENT;
  EXPECT_EQ(kTransparent, output->getColor(0, 0));
  EXPECT_EQ(kTransparent,
            output->getColor(tile_rect.width() - 1, tile_rect.height() - 1));
  EXPECT_EQ(kTransparent,
            output->getColor(visible_rect.x() - 1, visible_rect.y() - 1));
  EXPECT_EQ(kTransparent,
            output->getColor(visible_rect.right(), visible_rect.bottom()));
  // Ensure visible part is drawn correctly.
  EXPECT_EQ(SK_ColorCYAN, output->getColor(visible_rect.x(), visible_rect.y()));
  EXPECT_EQ(SK_ColorCYAN, output->getColor(visible_rect.right() - 2,
                                           visible_rect.bottom() - 2));
  // Ensure last visible line is correct.
  EXPECT_EQ(SK_ColorYELLOW, output->getColor(visible_rect.right() - 1,
                                             visible_rect.bottom() - 1));
}

TEST_F(SoftwareRendererTest, ShouldClearRootRenderPass) {
  float device_scale_factor = 1.f;
  gfx::Size viewport_size(100, 100);

  settings_.should_clear_root_render_pass = false;
  InitializeRenderer(std::make_unique<SoftwareOutputDevice>());

  RenderPassList list;

  // Draw a fullscreen green quad in a first frame.
  int root_clear_pass_id = 1;
  RenderPass* root_clear_pass =
      cc::AddRenderPass(&list, root_clear_pass_id, gfx::Rect(viewport_size),
                        gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_clear_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  renderer()->DecideRenderPassAllocationsForFrame(list);

  std::unique_ptr<SkBitmap> output =
      DrawAndCopyOutput(&list, device_scale_factor, viewport_size);
  EXPECT_EQ(viewport_size.width(), output->info().width());
  EXPECT_EQ(viewport_size.height(), output->info().height());

  EXPECT_EQ(SK_ColorGREEN, output->getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN, output->getColor(viewport_size.width() - 1,
                                            viewport_size.height() - 1));

  list.clear();

  // Draw a smaller magenta rect without filling the viewport in a separate
  // frame.
  gfx::Rect smaller_rect(20, 20, 60, 60);

  int root_smaller_pass_id = 2;
  RenderPass* root_smaller_pass =
      cc::AddRenderPass(&list, root_smaller_pass_id, gfx::Rect(viewport_size),
                        gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_smaller_pass, smaller_rect, SK_ColorMAGENTA);

  renderer()->DecideRenderPassAllocationsForFrame(list);

  output = DrawAndCopyOutput(&list, device_scale_factor, viewport_size);
  EXPECT_EQ(viewport_size.width(), output->info().width());
  EXPECT_EQ(viewport_size.height(), output->info().height());

  // If we didn't clear, the borders should still be green.
  EXPECT_EQ(SK_ColorGREEN, output->getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN, output->getColor(viewport_size.width() - 1,
                                            viewport_size.height() - 1));

  EXPECT_EQ(SK_ColorMAGENTA,
            output->getColor(smaller_rect.x(), smaller_rect.y()));
  EXPECT_EQ(SK_ColorMAGENTA, output->getColor(smaller_rect.right() - 1,
                                              smaller_rect.bottom() - 1));
}

TEST_F(SoftwareRendererTest, RenderPassVisibleRect) {
  float device_scale_factor = 1.f;
  gfx::Size viewport_size(100, 100);
  InitializeRenderer(std::make_unique<SoftwareOutputDevice>());

  RenderPassList list;

  // Pass drawn as inner quad is magenta.
  gfx::Rect smaller_rect(20, 20, 60, 60);
  int smaller_pass_id = 2;
  RenderPass* smaller_pass =
      cc::AddRenderPass(&list, smaller_pass_id, smaller_rect, gfx::Transform(),
                        cc::FilterOperations());
  cc::AddQuad(smaller_pass, smaller_rect, SK_ColorMAGENTA);

  // Root pass is green.
  int root_clear_pass_id = 1;
  RenderPass* root_clear_pass =
      AddRenderPass(&list, root_clear_pass_id, gfx::Rect(viewport_size),
                    gfx::Transform(), cc::FilterOperations());
  cc::AddRenderPassQuad(root_clear_pass, smaller_pass);
  cc::AddQuad(root_clear_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  // Interior pass quad has smaller visible rect.
  gfx::Rect interior_visible_rect(30, 30, 40, 40);
  root_clear_pass->quad_list.front()->visible_rect = interior_visible_rect;

  renderer()->DecideRenderPassAllocationsForFrame(list);

  std::unique_ptr<SkBitmap> output =
      DrawAndCopyOutput(&list, device_scale_factor, viewport_size);
  EXPECT_EQ(viewport_size.width(), output->info().width());
  EXPECT_EQ(viewport_size.height(), output->info().height());

  EXPECT_EQ(SK_ColorGREEN, output->getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN, output->getColor(viewport_size.width() - 1,
                                            viewport_size.height() - 1));

  // Part outside visible rect should remain green.
  EXPECT_EQ(SK_ColorGREEN,
            output->getColor(smaller_rect.x(), smaller_rect.y()));
  EXPECT_EQ(SK_ColorGREEN, output->getColor(smaller_rect.right() - 1,
                                            smaller_rect.bottom() - 1));

  EXPECT_EQ(SK_ColorMAGENTA, output->getColor(interior_visible_rect.x(),
                                              interior_visible_rect.y()));
  EXPECT_EQ(SK_ColorMAGENTA,
            output->getColor(interior_visible_rect.right() - 1,
                             interior_visible_rect.bottom() - 1));
}

class ClipTrackingCanvas : public SkNWayCanvas {
 public:
  ClipTrackingCanvas(int width, int height) : SkNWayCanvas(width, height) {}
  void onClipRect(const SkRect& rect,
                  SkClipOp op,
                  ClipEdgeStyle style) override {
    last_clip_rect_ = rect;
    SkNWayCanvas::onClipRect(rect, op, style);
  }

  SkRect last_clip_rect() const { return last_clip_rect_; }

 private:
  SkRect last_clip_rect_;
};

class PartialSwapSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  // SoftwareOutputDevice overrides.
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    damage_rect_at_start_ = damage_rect;
    canvas_.reset(new ClipTrackingCanvas(viewport_pixel_size_.width(),
                                         viewport_pixel_size_.height()));
    canvas_->addCanvas(SoftwareOutputDevice::BeginPaint(damage_rect));
    return canvas_.get();
  }

  void EndPaint() override {
    clip_rect_at_end_ = gfx::SkRectToRectF(canvas_->last_clip_rect());
    SoftwareOutputDevice::EndPaint();
  }

  gfx::Rect damage_rect_at_start() const { return damage_rect_at_start_; }
  gfx::RectF clip_rect_at_end() const { return clip_rect_at_end_; }

 private:
  std::unique_ptr<ClipTrackingCanvas> canvas_;
  gfx::Rect damage_rect_at_start_;
  gfx::RectF clip_rect_at_end_;
};

TEST_F(SoftwareRendererTest, PartialSwap) {
  float device_scale_factor = 1.f;
  gfx::Size viewport_size(100, 100);

  settings_.partial_swap_enabled = true;

  auto device_owned = std::make_unique<PartialSwapSoftwareOutputDevice>();
  auto* device = device_owned.get();
  InitializeRenderer(std::move(device_owned));

  RenderPassList list;

  int root_pass_id = 1;
  RenderPass* root_pass =
      AddRenderPass(&list, root_pass_id, gfx::Rect(viewport_size),
                    gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, gfx::Rect(viewport_size), SK_ColorGREEN);

  // Partial frame, we should pass this rect to the SoftwareOutputDevice.
  // partial swap is enabled.
  root_pass->damage_rect = gfx::Rect(2, 2, 3, 3);

  renderer()->DecideRenderPassAllocationsForFrame(list);
  renderer()->DrawFrame(&list, device_scale_factor, viewport_size);

  // The damage rect should be reported to the SoftwareOutputDevice.
  EXPECT_EQ(gfx::Rect(2, 2, 3, 3), device->damage_rect_at_start());
  // The SkCanvas should be clipped to the damage rect.
  EXPECT_EQ(gfx::RectF(2, 2, 3, 3), device->clip_rect_at_end());
}

}  // namespace
}  // namespace viz
