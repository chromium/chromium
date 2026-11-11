// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/delegated_ink_point_pixel_test_helper.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)

namespace viz {
namespace {

constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
constexpr FrameSinkId kArbitraryChildFrameSinkId(2, 2);
constexpr FrameSinkId kArbitraryLeftFrameSinkId(3, 3);
constexpr FrameSinkId kArbitraryRightFrameSinkId(4, 4);
constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;

class SurfaceAggregatorPixelTest : public VizPixelTestWithParam {
 public:
  SurfaceAggregatorPixelTest()
      : manager_(FrameSinkManagerImpl::InitParams()),
        support_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &manager_,
            kArbitraryRootFrameSinkId,
            kIsRoot)) {}
  ~SurfaceAggregatorPixelTest() override = default;

  base::TimeTicks GetNextDisplayTime() {
    base::TimeTicks display_time = next_display_time_;
    next_display_time_ += BeginFrameArgs::DefaultInterval();
    return display_time;
  }

 protected:
  FrameSinkManagerImpl manager_;
  ParentLocalSurfaceIdAllocator root_allocator_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  base::TimeTicks next_display_time_ = base::TimeTicks() + base::Seconds(1);
};

class SurfaceAggregatorBackingScalePixelTest
    : public SurfaceAggregatorPixelTest {
 public:
  SurfaceAggregatorBackingScalePixelTest() {
    feature_list_.InitAndEnableFeature(
        features::kRenderNonMergedSurfaceAtBackingScale);
    // Use production backing allocation and prevent renderer-side pass
    // bypassing so tests exercise the allocated render-pass texture.
    renderer_settings_.dont_round_texture_sizes_for_pixel_tests = false;
    renderer_settings_.disable_render_pass_bypassing = true;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SurfaceAggregatorPixelTest,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SurfaceAggregatorPixelTest);

INSTANTIATE_TEST_SUITE_P(,
                         SurfaceAggregatorBackingScalePixelTest,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

SharedQuadState* CreateAndAppendTestSharedQuadState(
    CompositorRenderPass* render_pass,
    const gfx::Transform& transform,
    const gfx::Size& size) {
  const gfx::Rect layer_rect = gfx::Rect(size);
  const gfx::Rect visible_layer_rect = gfx::Rect(size);
  const gfx::MaskFilterInfo mask_filter_info;
  bool are_contents_opaque = false;
  float opacity = 1.f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  auto* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, layer_rect, visible_layer_rect,
                       mask_filter_info, /*clip=*/std::nullopt,
                       are_contents_opaque, opacity, blend_mode,
                       /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  return shared_state;
}

// Draws a very simple frame with no surface references.
TEST_P(SurfaceAggregatorPixelTest, DrawSimpleFrame) {
  gfx::Rect rect(this->device_viewport_size_);
  CompositorRenderPassId id{1};
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());

  CreateAndAppendTestSharedQuadState(pass.get(), gfx::Transform(),
                                     this->device_viewport_size_);

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bool force_anti_aliasing_off = false;
  color_quad->SetNew(pass->shared_quad_state_list.back(), rect, rect,
                     SkColors::kGreen, force_anti_aliasing_off);

  auto root_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

  this->root_allocator_.GenerateId();
  SurfaceId root_surface_id(this->support_->frame_sink_id(),
                            this->root_allocator_.GetCurrentLocalSurfaceId());
  this->support_->SubmitCompositorFrame(
      this->root_allocator_.GetCurrentLocalSurfaceId(), std::move(root_frame));

  SurfaceAggregator aggregator(this->manager_.surface_manager(),
                               this->resource_provider_.get(), false);
  auto aggregated_frame = aggregator.Aggregate(
      root_surface_id, this->GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE);

  cc::ExactPixelComparator pixel_comparator;
  auto* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(this->RunPixelTest(pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 pixel_comparator));
}

// Draws a frame with simple surface embedding.
TEST_P(SurfaceAggregatorPixelTest, DrawSimpleAggregatedFrame) {
  gfx::Size child_size(200, 100);
  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &this->manager_, kArbitraryChildFrameSinkId, kIsChildRoot);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_support->frame_sink_id(),
                             child_local_surface_id);
  this->root_allocator_.GenerateId();
  LocalSurfaceId root_local_surface_id =
      this->root_allocator_.GetCurrentLocalSurfaceId();
  SurfaceId root_surface_id(this->support_->frame_sink_id(),
                            root_local_surface_id);

  {
    gfx::Rect rect(this->device_viewport_size_);
    CompositorRenderPassId id{1};
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(pass.get(), gfx::Transform(),
                                       this->device_viewport_size_);

    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(
        pass->shared_quad_state_list.back(), gfx::Rect(child_size),
        gfx::Rect(child_size), SurfaceRange(std::nullopt, child_surface_id),
        SkColors::kWhite, /*stretch_content_to_fill_bounds=*/false);

    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    color_quad->SetNew(pass->shared_quad_state_list.back(), rect, rect,
                       SkColors::kYellow, force_anti_aliasing_off);

    auto root_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    this->support_->SubmitCompositorFrame(root_local_surface_id,
                                          std::move(root_frame));
  }

  {
    gfx::Rect rect(child_size);
    CompositorRenderPassId id{1};
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(pass.get(), gfx::Transform(),
                                       child_size);

    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    color_quad->SetNew(pass->shared_quad_state_list.back(), rect, rect,
                       SkColors::kBlue, force_anti_aliasing_off);

    auto child_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    child_support->SubmitCompositorFrame(child_local_surface_id,
                                         std::move(child_frame));
  }

  SurfaceAggregator aggregator(this->manager_.surface_manager(),
                               this->resource_provider_.get(), false);
  auto aggregated_frame = aggregator.Aggregate(
      root_surface_id, this->GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE);

  cc::ExactPixelComparator pixel_comparator;
  auto* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(this->RunPixelTest(
      pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      pixel_comparator));
}

// Verifies aggregation and rendering when a very small backing scale produces
// a logical render-pass size below one pixel before enclosing.
TEST_P(SurfaceAggregatorBackingScalePixelTest, DrawAtVerySmallBackingScale) {
  // Embeds a retained blue 100x100 child over red with backing and embedding
  // scales of 0.009. Verifies:
  // - Aggregation encloses the 0.9x0.9 output to 1x1.
  // - Renderer coordinate conversion and sampling use the non-zero logical
  //   output size to produce one blue pixel over red.
  constexpr float kBackingScale = 0.009f;
  constexpr float kEmbeddingScale = 0.009f;
  const gfx::Size child_size(100, 100);
  const gfx::Rect child_rect(child_size);
  const gfx::Rect viewport_rect(device_viewport_size_);

  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryChildFrameSinkId, kIsChildRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  const LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  const SurfaceId child_surface_id(child_support->frame_sink_id(),
                                   child_local_surface_id);

  root_allocator_.GenerateId();
  const LocalSurfaceId root_local_surface_id =
      root_allocator_.GetCurrentLocalSurfaceId();
  const SurfaceId root_surface_id(support_->frame_sink_id(),
                                  root_local_surface_id);

  // Submit the child at its original 100x100 dimensions. Aggregation applies
  // the backing scale from the frame metadata:
  //
  //   100 * kBackingScale
  //     = 100 * 0.009
  //     = 0.9
  //
  // ScaleToEnclosingRect() rounds the non-empty 0.9x0.9 output up to 1x1.
  {
    auto child_pass = CompositorRenderPass::Create();
    child_pass->SetNew(CompositorRenderPassId{1}, child_rect, child_rect,
                       gfx::Transform());
    SharedQuadState* child_shared_state = CreateAndAppendTestSharedQuadState(
        child_pass.get(), gfx::Transform(), child_size);
    auto* blue_quad = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    blue_quad->SetNew(child_shared_state, child_rect, child_rect,
                      SkColors::kBlue, /*anti_aliasing_off=*/true);

    CompositorFrame child_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(child_pass)).Build();
    child_frame.metadata.surface_backing_size_scale_factor = kBackingScale;
    child_support->SubmitCompositorFrame(child_local_surface_id,
                                         std::move(child_frame));
  }

  // Embed the original 100x100 child at 0.009x and retain its root pass. During
  // aggregation, the embedding transform is combined with compensation for the
  // backing scale:
  //
  //   kEmbeddingScale * (1 / kBackingScale)
  //     = 0.009 * (1 / 0.009)
  //     = identity
  //
  // The aggregated 1x1 child pass therefore occupies one pixel in the root.
  {
    auto root_pass = CompositorRenderPass::Create();
    root_pass->SetNew(CompositorRenderPassId{1}, viewport_rect, viewport_rect,
                      gfx::Transform());

    gfx::Transform embedding_transform;
    embedding_transform.Scale(kEmbeddingScale);
    SharedQuadState* surface_shared_state = CreateAndAppendTestSharedQuadState(
        root_pass.get(), embedding_transform, child_size);
    auto* surface_quad = root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(surface_shared_state, child_rect, child_rect,
                         /*needs_blending=*/false,
                         SurfaceRange(std::nullopt, child_surface_id),
                         SkColors::kWhite,
                         /*stretch_content=*/false, /*reflection=*/false,
                         /*merge=*/false);

    // Draw red behind the child so a missing or incorrectly sampled child pass
    // is visible in the output.
    SharedQuadState* background_shared_state =
        CreateAndAppendTestSharedQuadState(root_pass.get(), gfx::Transform(),
                                           device_viewport_size_);
    auto* red_background =
        root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    red_background->SetNew(background_shared_state, viewport_rect,
                           viewport_rect, SkColors::kRed,
                           /*anti_aliasing_off=*/false);

    support_->SubmitCompositorFrame(
        root_local_surface_id,
        CompositorFrameBuilder().AddRenderPass(std::move(root_pass)).Build());
  }

  SurfaceAggregator aggregator(manager_.surface_manager(),
                               resource_provider_.get(), false);
  auto aggregated_frame = aggregator.Aggregate(
      root_surface_id, GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE);

  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  // The retained child's 0.9x0.9 output must enclose to 1x1 to avoid zero
  // divisors.
  EXPECT_EQ(gfx::Rect(1, 1),
            aggregated_frame.render_pass_list.front()->output_rect);

  // The 1x1 logical child pass gets a 64x64 backing. Its scaled blue quad
  // covers [0, 0.9) on each axis, including the texel center at (0.5, 0.5).
  // Antialiasing is disabled, so that texel is fully blue. The child pass's
  // single logical pixel maps to the root's top-left pixel over a red
  // background.
  std::vector<SkColor> expected_output_colors(
      viewport_rect.width() * viewport_rect.height(),
      SkColors::kRed.toSkColor());
  expected_output_colors.front() = SkColors::kBlue.toSkColor();
  EXPECT_TRUE(RunPixelTest(&aggregated_frame.render_pass_list,
                           &expected_output_colors,
                           cc::AlphaDiscardingExactPixelComparator()));
}

// Tests a surface quad that has a non-identity transform into its pass.
TEST_P(SurfaceAggregatorPixelTest, DrawAggregatedFrameWithSurfaceTransforms) {
  gfx::Size child_size(100, 200);
  gfx::Size quad_size(100, 100);
  // Structure:
  // root (200x200) -> left_child (100x200 @ 0x0,
  //                   right_child (100x200 @ 0x100)
  //   left_child -> top_green_quad (100x100 @ 0x0),
  //                 bottom_blue_quad (100x100 @ 0x100)
  //   right_child -> top_blue_quad (100x100 @ 0x0),
  //                  bottom_green_quad (100x100 @ 0x100)
  auto left_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &this->manager_, kArbitraryLeftFrameSinkId, kIsChildRoot);
  auto right_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &this->manager_, kArbitraryRightFrameSinkId, kIsChildRoot);
  ParentLocalSurfaceIdAllocator left_child_allocator;
  left_child_allocator.GenerateId();
  LocalSurfaceId left_child_local_id =
      left_child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId left_child_id(left_support->frame_sink_id(), left_child_local_id);
  ParentLocalSurfaceIdAllocator right_child_allocator;
  right_child_allocator.GenerateId();
  LocalSurfaceId right_child_local_id =
      right_child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId right_child_id(right_support->frame_sink_id(),
                           right_child_local_id);
  this->root_allocator_.GenerateId();
  LocalSurfaceId root_local_surface_id =
      this->root_allocator_.GetCurrentLocalSurfaceId();
  SurfaceId root_surface_id(this->support_->frame_sink_id(),
                            root_local_surface_id);

  {
    gfx::Rect rect(this->device_viewport_size_);
    CompositorRenderPassId id{1};
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    gfx::Transform surface_transform;
    CreateAndAppendTestSharedQuadState(pass.get(), surface_transform,
                                       this->device_viewport_size_);

    auto* left_surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    left_surface_quad->SetNew(
        pass->shared_quad_state_list.back(), gfx::Rect(child_size),
        gfx::Rect(child_size), SurfaceRange(std::nullopt, left_child_id),
        SkColors::kWhite, /*stretch_content_to_fill_bounds=*/false);

    surface_transform.Translate(100, 0);
    CreateAndAppendTestSharedQuadState(pass.get(), surface_transform,
                                       this->device_viewport_size_);

    auto* right_surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    right_surface_quad->SetNew(
        pass->shared_quad_state_list.back(), gfx::Rect(child_size),
        gfx::Rect(child_size), SurfaceRange(std::nullopt, right_child_id),
        SkColors::kWhite, /*stretch_content_to_fill_bounds=*/false);

    auto root_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    this->support_->SubmitCompositorFrame(root_local_surface_id,
                                          std::move(root_frame));
  }

  {
    gfx::Rect rect(child_size);
    CompositorRenderPassId id{1};
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(pass.get(), gfx::Transform(),
                                       child_size);

    auto* top_color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    top_color_quad->SetNew(pass->shared_quad_state_list.back(),
                           gfx::Rect(quad_size), gfx::Rect(quad_size),
                           SkColors::kGreen, force_anti_aliasing_off);

    auto* bottom_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_color_quad->SetNew(
        pass->shared_quad_state_list.back(), gfx::Rect(0, 100, 100, 100),
        gfx::Rect(0, 100, 100, 100), SkColors::kBlue, force_anti_aliasing_off);

    auto child_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    left_support->SubmitCompositorFrame(left_child_local_id,
                                        std::move(child_frame));
  }

  {
    gfx::Rect rect(child_size);
    CompositorRenderPassId id{1};
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(pass.get(), gfx::Transform(),
                                       child_size);

    auto* top_color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    top_color_quad->SetNew(pass->shared_quad_state_list.back(),
                           gfx::Rect(quad_size), gfx::Rect(quad_size),
                           SkColors::kBlue, force_anti_aliasing_off);

    auto* bottom_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_color_quad->SetNew(
        pass->shared_quad_state_list.back(), gfx::Rect(0, 100, 100, 100),
        gfx::Rect(0, 100, 100, 100), SkColors::kGreen, force_anti_aliasing_off);

    auto child_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    right_support->SubmitCompositorFrame(right_child_local_id,
                                         std::move(child_frame));
  }

  SurfaceAggregator aggregator(this->manager_.surface_manager(),
                               this->resource_provider_.get(), false);
  auto aggregated_frame = aggregator.Aggregate(
      root_surface_id, this->GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE);

  cc::ExactPixelComparator pixel_comparator;
  auto* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(this->RunPixelTest(
      pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      pixel_comparator));
}

// Draw a simple frame with a delegated ink trail on top of it, then confirm
// that it is erased by the next aggregation.
TEST_P(SurfaceAggregatorPixelTest, DrawAndEraseDelegatedInkTrail) {
  DelegatedInkPointPixelTestHelper delegated_ink_helper(renderer_.get());

  // Create and send metadata and points to the renderer that will be drawn.
  // Points and timestamps are chosen arbitrarily.
  const gfx::PointF kFirstPoint(10, 10);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  delegated_ink_helper.CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  delegated_ink_helper.CreateAndSendPointFromLastPoint(gfx::PointF(26, 37));
  delegated_ink_helper.CreateAndSendPointFromLastPoint(gfx::PointF(45, 87));

  delegated_ink_helper.CreateAndSendMetadata(kFirstPoint, 7.7f,
                                             SkColors::kWhite, kFirstTimestamp,
                                             gfx::RectF(0, 0, 200, 200),
                                             /*render_pass_id=*/1);

  gfx::Rect rect(this->device_viewport_size_);
  CompositorRenderPassId id{1};
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());

  CreateAndAppendTestSharedQuadState(pass.get(), gfx::Transform(),
                                     this->device_viewport_size_);

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bool force_anti_aliasing_off = false;
  color_quad->SetNew(pass->shared_quad_state_list.back(), rect, rect,
                     SkColors::kGreen, force_anti_aliasing_off);

  auto root_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

  this->root_allocator_.GenerateId();
  SurfaceId root_surface_id(this->support_->frame_sink_id(),
                            this->root_allocator_.GetCurrentLocalSurfaceId());
  this->support_->SubmitCompositorFrame(
      this->root_allocator_.GetCurrentLocalSurfaceId(), std::move(root_frame));

  SurfaceAggregator aggregator(this->manager_.surface_manager(),
                               this->resource_provider_.get(), false);
  auto aggregated_frame = aggregator.Aggregate(
      root_surface_id, this->GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE);

  cc::FuzzyPixelOffByOneComparator pixel_comparator;
  auto* pass_list = &aggregated_frame.render_pass_list;
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_trail.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_graphite");
  }
  EXPECT_TRUE(this->RunPixelTest(pass_list, expected_result, pixel_comparator));

  // Providing the damage rect as the target damage ensures that aggregation
  // occurs and DrawFrame() has something new to draw. If this doesn't cause
  // anything to be aggregated, a black square is drawn. If it does, the result
  // should just erase the previously drawn trail completely.
  aggregated_frame = aggregator.Aggregate(
      root_surface_id, this->GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE,
      delegated_ink_helper.GetDelegatedInkDamageRect());
  pass_list = &aggregated_frame.render_pass_list;

  EXPECT_TRUE(this->RunPixelTest(pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 pixel_comparator));
}

// Exercises a surface with two render passes being embedded multiple times in
// the root surface, causing the same render passes to be copied into the
// AggregatedFrame multiple times and drawn more than once (write -> read ->
// write -> read) during a single DrawFrame.
TEST_P(SurfaceAggregatorPixelTest, DrawDuplicatedSurfaceRenderPasses) {
  gfx::Rect viewport_rect(device_viewport_size_);
  gfx::Rect child_root_rect(100, 100);
  gfx::Rect child_pass_rect(80, 80);

  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryChildFrameSinkId, kIsChildRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_support->frame_sink_id(),
                             child_local_surface_id);

  root_allocator_.GenerateId();
  LocalSurfaceId root_local_surface_id =
      root_allocator_.GetCurrentLocalSurfaceId();
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id);

  {
    CompositorRenderPassId child_non_root_id{1};
    CompositorRenderPassId child_root_id{2};

    auto child_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(child_non_root_id, child_pass_rect)
                    .AddSolidColorQuad(child_pass_rect, SkColors::kGreen))
            .AddRenderPass(
                RenderPassBuilder(child_root_id, child_root_rect)
                    .AddRenderPassQuad(child_pass_rect, child_non_root_id)
                    .SetQuadToTargetTranslation(10, 10)
                    .AddSolidColorQuad(child_root_rect, SkColors::kMagenta))
            .Build();
    child_support->SubmitCompositorFrame(child_local_surface_id,
                                         std::move(child_frame));
  }

  {
    auto root_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, viewport_rect)
                    .AddSurfaceQuad(child_root_rect,
                                    SurfaceRange(child_surface_id),
                                    {.allow_merge = false})
                    .AddSurfaceQuad(child_root_rect,
                                    SurfaceRange(child_surface_id),
                                    {.allow_merge = false})
                    .SetQuadToTargetTranslation(100, 100)
                    .AddSolidColorQuad(viewport_rect, SkColors::kYellow))
            .Build();
    support_->SubmitCompositorFrame(root_local_surface_id,
                                    std::move(root_frame));
  }

  SurfaceAggregator aggregator(manager_.surface_manager(),
                               resource_provider_.get(), false);
  auto aggregated_frame = aggregator.Aggregate(
      root_surface_id, GetNextDisplayTime(), gfx::OVERLAY_TRANSFORM_NONE);

  EXPECT_TRUE(RunPixelTest(
      &aggregated_frame.render_pass_list,
      base::FilePath(FILE_PATH_LITERAL("multiple_surface_embedding.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

}  // namespace
}  // namespace viz

#endif  // !BUILDFLAG(IS_ANDROID)
