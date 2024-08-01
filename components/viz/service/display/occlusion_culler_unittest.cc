// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display/occlusion_culler.h"

#include <memory>
#include <optional>
#include <utility>

#include "cc/base/math_util.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

constexpr float kDefaultDeviceScaleFactor = 1.0f;

size_t NumVisibleRects(const QuadList& quads) {
  size_t visible_rects = 0;
  for (const auto* q : quads) {
    if (!q->visible_rect.size().IsEmpty()) {
      visible_rects++;
    }
  }
  return visible_rects;
}

class OcclusionCullerTest : public testing::Test {
 public:
  OcclusionCullerTest() = default;

  OcclusionCullerTest(const OcclusionCullerTest&) = delete;
  OcclusionCullerTest& operator=(const OcclusionCullerTest&) = delete;

  ~OcclusionCullerTest() override = default;

 protected:
  void InitOcclusionCuller() { InitOcclusionCuller({}); }

  void InitOcclusionCuller(RendererSettings::OcclusionCullerSettings settings) {
    CHECK(!occlusion_culler_);
    occlusion_culler_ =
        std::make_unique<OcclusionCuller>(overlay_processor_.get(), settings);
  }

  OcclusionCuller* occlusion_culler() { return occlusion_culler_.get(); }

  // testing::Test:
  void SetUp() override {
    overlay_processor_ = std::make_unique<OverlayProcessorStub>();
  }

  std::unique_ptr<OverlayProcessorInterface> overlay_processor_;
  std::unique_ptr<OcclusionCuller> occlusion_culler_;
};

// Quads that require blending should not be treated as occluders
// regardless of full opacity.
TEST_F(OcclusionCullerTest, OcclusionCullingWithBlending) {
  RendererSettings::OcclusionCullerSettings settings;
  settings.minimum_fragments_reduced = 0;

  InitOcclusionCuller(settings);
  AggregatedFrame frame = MakeDefaultAggregatedFrame(/*num_render_passes=*/2);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  auto src_rect = gfx::Rect(0, 0, 100, 100);
  auto dest_rect = gfx::Rect(25, 25, 25, 25);

  for (auto& render_pass : frame.render_pass_list) {
    bool is_root_render_pass = render_pass == frame.render_pass_list.back();

    auto* src_sqs = render_pass->CreateAndAppendSharedQuadState();
    src_sqs->SetAll(
        gfx::Transform(), src_rect, src_rect, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        is_root_render_pass ? SkBlendMode::kSrcOver : SkBlendMode::kSrcIn,
        /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    auto* dest_sqs = render_pass->CreateAndAppendSharedQuadState();
    dest_sqs->SetAll(
        gfx::Transform(), dest_rect, dest_rect, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        is_root_render_pass ? SkBlendMode::kSrcOver : SkBlendMode::kDstIn,
        /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    auto* src_quad =
        render_pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    src_quad->SetNew(src_sqs, src_rect, src_rect, SkColors::kBlack, false);

    auto* dest_quad =
        render_pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    dest_quad->SetNew(dest_sqs, dest_rect, dest_rect, SkColors::kRed, false);
  }

  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.back()->quad_list));

  occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.back()->quad_list));
}

// Quads that intersect backdrop filter render pass quads should not be
// split because splitting may affect how the filter applies to an
// underlying quad.
TEST_F(OcclusionCullerTest, OcclusionCullingWithIntersectingBackdropFilter) {
  RendererSettings::OcclusionCullerSettings settings;
  settings.minimum_fragments_reduced = 0;

  InitOcclusionCuller(settings);
  AggregatedFrame frame = MakeDefaultAggregatedFrame(/*num_render_passes=*/2);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  // Rects, shared quad states and quads map 1:1:1
  gfx::Rect rects[3] = {
      gfx::Rect(75, 0, 50, 100),
      gfx::Rect(0, 0, 50, 50),
      gfx::Rect(0, 0, 100, 100),
  };

  SharedQuadState* shared_quad_states[3];
  DrawQuad* quads[3];

  // Set up the backdrop filter render pass
  auto& bd_render_pass = frame.render_pass_list.at(0);
  auto& root_render_pass = frame.render_pass_list.at(1);
  auto bd_filter_rect = rects[0];

  cc::FilterOperations backdrop_filters;
  backdrop_filters.Append(cc::FilterOperation::CreateBlurFilter(5.0));
  bd_render_pass->SetAll(
      AggregatedRenderPassId{2}, bd_filter_rect, gfx::Rect(), gfx::Transform(),
      cc::FilterOperations(), backdrop_filters,
      gfx::RRectF(gfx::RectF(bd_filter_rect), 0), gfx::ContentColorUsage::kSRGB,
      false, false, false, false);

  // Add quads to root render pass
  for (int i = 0; i < 3; i++) {
    shared_quad_states[i] = root_render_pass->CreateAndAppendSharedQuadState();
    shared_quad_states[i]->SetAll(
        gfx::Transform(), rects[i], rects[i], gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    if (i == 0) {  // Backdrop filter quad
      auto* new_quad =
          root_render_pass->quad_list
              .AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
      new_quad->SetNew(shared_quad_states[i], rects[i], rects[i],
                       bd_render_pass->id, ResourceId(2), gfx::RectF(),
                       gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(),
                       gfx::RectF(), false, 1.f);
      quads[i] = new_quad;
    } else {
      auto* new_quad = root_render_pass->quad_list
                           .AllocateAndConstruct<SolidColorDrawQuad>();
      new_quad->SetNew(shared_quad_states[i], rects[i], rects[i],
                       SkColors::kBlack, false);
      quads[i] = new_quad;
    }
  }

  // +---+-+-+-+
  // | 1 | | . |
  // +---+ | 0 |
  // | 2   | . |
  // +-----+---+
  EXPECT_EQ(std::size(rects), root_render_pass->quad_list.size());
  occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);
  ASSERT_EQ(std::size(rects), root_render_pass->quad_list.size());

  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(rects[i], root_render_pass->quad_list.ElementAt(i)->visible_rect);
  }
}

// Check if occlusion culling does not remove any DrawQuads when no quad is
// being covered completely.
TEST_F(OcclusionCullerTest, OcclusionCullingWithNonCoveringDrawQuad) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 100, 100);
  gfx::Rect rect3(25, 25, 50, 100);
  gfx::Rect rect4(150, 0, 50, 50);
  gfx::Rect rect5(0, 0, 120, 120);
  gfx::Rect rect6(25, 0, 50, 160);
  gfx::Rect rect7(0, 20, 100, 100);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // |    |
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // This is a base case, the compositor frame contains only one
    // DrawQuad, so the size of quad_list remains unchanged after calling
    // RemoveOverdrawQuads.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // | +--|-+
  // +----+ |
  //   +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect2 (50, 50, 100x100)), the |quad_list| size remains the
    // same after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect2 - rect1 U rect2 = (100, 50, 50x50 U 50, 100, 100x50),
    // which cannot be represented by a smaller rect (its visible_rect stays
    // the same).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  // +------+                                +------+
  // |      |                                |      |
  // | +--+ |          show on screen        |      |
  // +------+                =>              +------+
  //   |  |                                    |  |
  //   +--+                                    +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect3 (25, 25, 50x100)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect3 - rect1 U rect3 = (25, 100, 50x25), which updates its
    // visible_rect accordingly.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(25, 100, 50, 25),
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  //  +--+                                        +--+
  // +----+                                      +----+
  // ||  ||             shown on screen          |    |
  // +----+                                      +----+
  //  +--+                                        +--+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect7, rect7,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect6, rect6,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect7, rect7, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since |quad| (defined by rect7 (0, 20, 100x100)) cannot cover |quad2|
    // (define by rect6 (25, 0, 50x160)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect6 - rect7 = (25, 0, 50x20 U 25, 120, 50x40), which
    // cannot be represented by a smaller rect (its visible_rect stays the
    // same).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect7,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect6,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  // +----+   +--+
  // |    |   +--+
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect4 (150, 0, 50x50)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect4 (150, 0, 50x50), its visible_rect stays the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect4,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect5, rect5,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since |quad| (defined by rect1 (0, 0, 100x100)) cannot cover |quad2|
    // (define by rect5 (0, 0, 120x120)), the |quad_list| size remains the same
    // after calling RemoveOverdrawQuads. The visible region of |quad2| on
    // screen is rect5 - rect1 = (100, 0, 20x100 U 0, 100, 100x20),
    // which cannot be represented by a smaller rect (its visible_rect stays the
    // same).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

// Check if the occlusion culling removes a DrawQuad that is hidden behind
// a smaller disjointed DrawQuad.
// NOTE: this test will fail if RendererSettings.kMaximumOccluderComplexity is
// reduced to 1, since |rects[1]| will become the only occluder, and the quad
// defined by |rects[2]| will not be occluded (removed).
TEST_F(OcclusionCullerTest,
       OcclusionCullingWithSingleOverlapBehindDisjointedDrawQuads) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  std::vector<gfx::Rect> rects;
  rects.emplace_back(0, 0, 100, 100);
  rects.emplace_back(150, 0, 150, 150);
  rects.emplace_back(25, 25, 50, 50);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  for (const gfx::Rect& rect : rects) {
    SharedQuadState* shared_quad_state =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    auto* quad = frame.render_pass_list.front()
                     ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
  }

  //              +-------+
  //  +-----+     |       |
  //  | +-+ |     |       |
  //  | +-+ |     |       |
  //  +-----+     +-------+
  {
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The third quad (defined by rects[2](25, 25, 50x50)) is completely
    // occluded by the first quad (defined by rects[0](0, 0, 100x100)), so the
    // third quad is removed from the |quad_list|, leaving the first and second
    // (defined by rects[1](150, 0, 150x150); the largest) quads intact.
    ASSERT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rects[0],
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rects[1],
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

// Check if the occlusion culling removes DrawQuads that are hidden behind
// two different sized disjointed DrawQuads.
// NOTE: this test will fail if RendererSettings.kMaximumOccluderComplexity is
// reduced to 1, since |rects[1]| will become the only occluder, and the quad
// defined by |rects[2]| will not be occluded (removed).
TEST_F(OcclusionCullerTest,
       OcclusionCullingWithMultipleOverlapBehindDisjointedDrawQuads) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  std::vector<gfx::Rect> rects;
  rects.emplace_back(0, 0, 100, 100);
  rects.emplace_back(150, 0, 150, 150);
  rects.emplace_back(25, 25, 50, 50);
  rects.emplace_back(150, 0, 100, 100);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  for (const gfx::Rect& rect : rects) {
    SharedQuadState* shared_quad_state =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    auto* quad = frame.render_pass_list.front()
                     ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
  }

  //              +-------+
  //  +-----+     +-----+ |
  //  | +-+ |     |     | |
  //  | +-+ |     |     | |
  //  +-----+     +-----+-+
  {
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The third (defined by rects[2](25, 25, 50x50)) and fourth (defined by
    // rects[3](150, 0, 100x100)) quads are completely occluded by the first
    // (defined by rects[0](0, 0, 100x100)) and second (defined by rects[1](150,
    // 0, 150x150)) quads, respectively, so both are removed from the
    // |quad_list|, leaving the first and and second quads intact.
    ASSERT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rects[0],
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rects[1],
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

// Check if occlusion culling removes DrawQuads that are not shown on screen.
TEST_F(OcclusionCullerTest, CompositorFrameWithOverlapDrawQuad) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 50, 50);
  gfx::Rect rect3(50, 50, 50, 25);
  gfx::Rect rect4(0, 0, 50, 50);

  bool are_contents_opaque = true;
  float opacity = 1.f;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // completely overlapping: +-----+
  //                         |     |
  //                         +-----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1,
                               gfx::MaskFilterInfo(), std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| overlaps |quad1|, so |quad2| is removed from the |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
  //  +-----+
  //  | +-+ |
  //  | +-+ |
  //  +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| is hiding behind |quad1|, so |quad2| is removed from the
    // |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  // +-----+
  // |  +--|
  // |  +--|
  // +-----+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| is behind |quad1| and aligns with the edge of |quad1|, so |quad2|
    // is removed from the |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  // +-----++
  // |     ||
  // +-----+|
  // +------+
  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect4, rect4,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| is covered by |quad 1|, so |quad2| is removed from the
    // |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if occlusion occlusion works well with scale change transformer.
TEST_F(OcclusionCullerTest, CompositorFrameWithTransformer) {
  InitOcclusionCuller();

  // Rect 2, 3, 4 are contained in rect 1 only after applying the half scale
  // matrix. They are repetition of CompositorFrameWithOverlapDrawQuad.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 100, 100);
  gfx::Rect rect3(100, 100, 100, 50);
  gfx::Rect rect4(0, 0, 120, 120);

  // Rect 5, 6, 7, 8, 9, 10 are not contained by rect 1 after applying the
  // double scale matrix. They are repetition of
  // OcclusionCullingWithNonCoveringDrawQuad.
  gfx::Rect rect5(25, 25, 60, 60);
  gfx::Rect rect6(12, 12, 25, 50);
  gfx::Rect rect7(75, 0, 25, 25);
  gfx::Rect rect8(0, 0, 60, 60);
  gfx::Rect rect9(12, 0, 25, 80);
  gfx::Rect rect10(0, 10, 50, 50);

  gfx::Transform half_scale;
  half_scale.Scale3d(0.5, 0.5, 0.5);
  gfx::Transform double_scale;
  double_scale.Scale(2, 2);
  bool are_contents_opaque = true;
  float opacity = 1.f;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(half_scale, rect2, rect2, gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |rect2| becomes (12, 12, 50x50) after applying half scale transform,
    // |quad2| is now covered by |quad|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(half_scale, rect3, rect3, gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |rect3| becomes (25, 25, 50x25) after applying half scale transform,
    // |quad2| is now covered by |quad|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(half_scale, rect4, rect4, gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect4, rect4, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |rect4| becomes (0, 0, 60x60) after applying half scale transform,
    // |quad2| is now covered by |quad1|. So the size of |quad_list| is reduced
    // by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    shared_quad_state->SetAll(double_scale, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/std::nullopt, are_contents_opaque,
                              opacity, SkBlendMode::kSrcOver,
                              /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The compositor frame contains only one quad, so |quad_list| remains 1
    // after calling RemoveOverdrawQuads.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect5, rect5,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect5, rect5, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| (defined by |rect5|) becomes (50, 50, 120x120) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect5| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect6, rect6,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect6, rect6, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| (defined by |rect6|) becomes (24, 24, 50x100) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect5| is (12, 50, 25x12), quad2::visible_rect
    // updates accordingly.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(12, 50, 25, 12),
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect7, rect7,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect7, rect7, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| (defined by |rect7|) becomes (150, 0, 50x50) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect7| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect7,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect8, rect8,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect8, rect8, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| (defined by |rect8|) becomes (0, 0, 120x120) after
    // applying double scale transform, it is not covered by |quad1| (defined by
    // |rect1| (0, 0, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect8| is not a rect, quad2::visible_rect stays
    // the same.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect8,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }

  {
    shared_quad_state->SetAll(double_scale, rect10, rect10,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(double_scale, rect9, rect9,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect10, rect10, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect9, rect9, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| (defined by |rect9|) becomes (24, 0, 50x160) after
    // applying double scale transform, it is not covered by |quad| (defined by
    // |rect10| (0, 20, 100x100)). So the size of |quad_list| is the same.
    // Since visible region of |rect9| is not a rect, quad2::visible_rect stays
    // the same
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect10,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect9,
        frame.render_pass_list.front()->quad_list.ElementAt(4)->visible_rect);
  }
}

// Check if occlusion culling works with transform at epsilon scale.
TEST_F(OcclusionCullerTest, CompositorFrameWithEpsilonScaleTransform) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect(0, 0, 100, 100);

  SkScalar epsilon = 0.000000001f;
  SkScalar larger_than_epsilon = 0.00000001f;
  gfx::Transform zero_scale;
  zero_scale.Scale(0, 0);
  gfx::Transform epsilon_scale;
  epsilon_scale.Scale(epsilon, epsilon);
  gfx::Transform larger_epsilon_scale;
  larger_epsilon_scale.Scale(larger_than_epsilon, larger_than_epsilon);
  bool are_contents_opaque = true;
  float opacity = 1.f;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  gfx::Transform inverted;

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(zero_scale, rect, rect, gfx::MaskFilterInfo(),
                               std::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // zero matrix transform is non-invertible, so |quad2| is not removed from
    // occlusion culling algorithm.
    EXPECT_FALSE(zero_scale.GetInverse(&inverted));
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(epsilon_scale, rect, rect, gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, are_contents_opaque,
                               opacity, SkBlendMode::kSrcOver,
                               /*sorting_context=*/1, /*layer_id=*/0u,
                               /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // This test verifies that the occlusion culling algorithm does not break
    // when the scale of the transform is very close to zero. |epsilon_scale|
    // transform has the scale set to 10^-8. the quad is considering to be empty
    // after the transform, so it fails to intersect the occlusion rect.
    // |quad2| is not removed from occlusion culling.
    EXPECT_TRUE(epsilon_scale.GetInverse(&inverted));
    EXPECT_TRUE(cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                    epsilon_scale, rect)
                    .IsEmpty());
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(larger_epsilon_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // This test verifies that the occlusion culling algorithm works well with
    // small scales that is just larger than the epsilon scale in the previous
    // case. |larger_epsilon_scale| transform has the scale set to 10^-7.
    // |quad2| will be transformed to a tiny rect that is covered by the
    // occlusion rect, so |quad2| is removed.
    EXPECT_TRUE(larger_epsilon_scale.GetInverse(&inverted));
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if occlusion culling works with transform at negative scale.
TEST_F(OcclusionCullerTest, CompositorFrameWithNegativeScaleTransform) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect(0, 0, 100, 100);

  gfx::Transform negative_scale;
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    negative_scale.Scale3d(-1, 1, 1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(negative_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the x-axis is negated, |quad2| after applying transform does not
    // intersect with |quad| any more, so no quad is removed.
    // In target space:
    //          |
    //  q2 +----|----+ occlusion rect
    //     |    |    |
    // ---------+----------
    //          |
    //          |
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    negative_scale.MakeIdentity();
    negative_scale.Scale3d(1, -1, 1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(negative_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the y-axis is negated, |quad2| after applying transform does not
    // intersect with |quad| any more, so no quad is removed.
    // In target space:
    //          |
    //          |----+ occlusion rect
    //          |    |
    // ---------+----------
    //          |    |
    //          |----+
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    negative_scale.MakeIdentity();
    negative_scale.Scale3d(1, 1, -1);
    shared_quad_state->SetAll(gfx::Transform(), rect, rect,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(negative_scale, rect, rect,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect, rect, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since z-axis is missing in a 2d plane, negating the z-axis does not cause
    // |q2| to move at all. So |quad2| overlaps with |quad| in target space.
    // In target space:
    //          |
    //          |----+ occlusion rect
    //          |    |   q2
    // ---------+----------
    //          |
    //          |
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if occlusion culling works well with rotation transform.
//
//  +-----+                                  +----+
//  |     |   rotation (by 45 on y-axis) ->  |    |     same height
//  +-----+                                  +----+     reduced weight
TEST_F(OcclusionCullerTest, CompositorFrameWithRotation) {
  InitOcclusionCuller();

  // rect 2 is inside rect 1 initially.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(75, 75, 10, 10);

  // rect 3 intersects with rect 1 initially
  gfx::Rect rect3(50, 50, 25, 100);

  gfx::Transform rotate;
  rotate.RotateAboutYAxis(45);
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // Apply rotation transform on |rect1| only.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/std::nullopt, are_contents_opaque,
                              opacity, SkBlendMode::kSrcOver,
                              /*sorting_context=*/0, /*layer_id=*/0u,
                              /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect2, rect2,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // In target space, |quad| becomes (0, 0, 71x100) (after applying rotation
    // transform) and |quad2| becomes (75, 75 10x10). So |quad2| does not
    // intersect with |quad|. No changes in quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    // Apply rotation transform on |rect1| and |rect2|.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              std::nullopt, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(rotate, rect2, rect2, gfx::MaskFilterInfo(),
                               std::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // In target space, |quad| becomes (0, 0, 70x100) and |quad2| becomes
    // (53, 75 8x10) (after applying rotation transform). So |quad2| is behind
    // |quad|. |quad2| is removed from |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/std::nullopt, are_contents_opaque,
                              opacity, SkBlendMode::kSrcOver,
                              /*sorting_context=*/0, /*layer_id=*/0u,
                              /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect3, rect3,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // In target space, |quad| becomes (0, 0, 71x100) (after applying rotation
    // transform) and |quad2| becomes (50, 50, 25x100). So |quad2| does not
    // intersect with |quad|. No changes in quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }

  {
    // Since we only support updating |visible_rect| of DrawQuad with scale
    // or translation transform and rotation transform applies to quads,
    // |visible_rect| of |quad2| should not be changed.
    shared_quad_state->SetAll(rotate, rect1, rect1, gfx::MaskFilterInfo(),
                              std::nullopt, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(rotate, rect3, rect3, gfx::MaskFilterInfo(),
                               std::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since both |quad| and |quad2| went through the same transform and |rect1|
    // does not cover |rect3| initially, |quad| does not cover |quad2| in target
    // space.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

// Check if occlusion culling is handled correctly if the transform does not
// preserves 2d axis alignment.
TEST_F(OcclusionCullerTest, CompositorFrameWithPerspective) {
  InitOcclusionCuller();

  // rect 2 is inside rect 1 initially.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 1, 1);

  gfx::Transform perspective;
  perspective.ApplyPerspectiveDepth(100);
  perspective.RotateAboutYAxis(45);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state->SetAll(perspective, rect1, rect1, gfx::MaskFilterInfo(),
                              std::nullopt, are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The transform used on |quad| is a combination of rotation and
    // perspective matrix, so it does not preserve 2d axis. Since it takes too
    // long to define a enclosed rect to describe the occlusion region,
    // occlusion region is not defined and no changes in quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(perspective, rect2, rect2, gfx::MaskFilterInfo(),
                               std::nullopt, are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The transform used on |quad2| is a combination of rotation and
    // perspective matrix, so it does not preserve 2d axis. it's easy to find
    // an enclosing rect to describe |quad2|. |quad2| is hiding behind |quad|,
    // so it's removed from |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if occlusion culling works with transparent DrawQuads.
TEST_F(OcclusionCullerTest, CompositorFrameWithOpacityChange) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool are_contents_opaque = true;
  float opacity1 = 1.f;
  float opacityLess1 = 0.5f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), std::nullopt,
        are_contents_opaque, opacityLess1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(), std::nullopt,
        are_contents_opaque, opacity1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the opacity of |rect2| is less than 1, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), std::nullopt,
        are_contents_opaque, opacity1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(), std::nullopt,
        are_contents_opaque, opacity1, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Repeat the above test and set the opacity of |rect1| to 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithOpaquenessChange) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(25, 25, 10, 10);

  bool opaque_content = true;
  bool transparent_content = false;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), std::nullopt,
        transparent_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the opaqueness of |rect2| is false, |rect1| cannot occlude
    // |rect2| even though |rect2| is inside |rect1|.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Repeat the above test and set the opaqueness of |rect2| to true.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Test if occlusion culling skips 3d objects. https://crbug.com/833748
TEST_F(OcclusionCullerTest, CompositorFrameZTranslate) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(0, 0, 200, 100);

  gfx::Transform translate_back;
  translate_back.Translate3d(0, 0, 100);
  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // 2 rects inside of 3d object is completely overlapping.
  //                         +-----+
  //                         |     |
  //                         +-----+
  {
    shared_quad_state->SetAll(translate_back, rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/1,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(gfx::Transform(), rect1, rect1,
                               gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                               are_contents_opaque, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/1,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect1, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since both |quad| and |quad2| are inside of a 3d object, OcclusionCulling
    // will not be applied to them.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(rect1,
              frame.render_pass_list.front()->quad_list.ElementAt(0)->rect);
    EXPECT_EQ(rect2,
              frame.render_pass_list.front()->quad_list.ElementAt(1)->rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithTranslateTransformer) {
  InitOcclusionCuller();

  // rect 2 and 3 are outside rect 1 initially.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(120, 120, 10, 10);
  gfx::Rect rect3(100, 100, 100, 20);

  bool opaque_content = true;
  bool transparent_content = false;
  float opacity = 1.f;
  gfx::Transform translate_up;
  translate_up.Translate(50, 50);
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //
    //   +----+
    //   |    |
    //   |    |
    //   +----+
    //           +-+
    //           +-+
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), std::nullopt,
        transparent_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |rect2| and |rect1| are disjoined as show in the first image. The size of
    // |quad_list| remains 2.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    //   quad content space:                                      target space:
    //   +----+
    //   |    |               translation transform
    //   |    |     (move the bigger rect (0, 0) -> (50, 50))         +-----+
    //   +----+                       =>                              | +-+ |
    //           +-+                                                  | +-+ |
    //           +-+                                                  +-----+
    shared_quad_state->SetAll(translate_up, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/std::nullopt, opaque_content, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Move |quad| defined by |rect1| over |quad2| defined by |rect2| by
    // applying translation transform. |quad2| will be covered by |quad|, so
    // |quad_list| size is reduced by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    // After applying translation transform on rect1:
    //   before                                                        after
    //   +----+
    //   |    |
    //   |    |     (move the bigger rect (0, 0) -> (50, 50))          +----+
    //   +----+                       =>                               |  +---+
    //           +---+                                                 |  +---+
    //           +---+                                                 +----+
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(translate_up, rect1, rect1, gfx::MaskFilterInfo(),
                              /*clip=*/std::nullopt, opaque_content, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Move |quad| defined by |rect1| over |quad2| defined by |rect3| by
    // applying translation transform. In target space, |quad| is (50, 50,
    // 100x100) and |quad2| is (100, 100, 100x20). So the visible region of
    // |quad2| is (150, 100, 50x20).
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(150, 100, 50, 20),
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithCombinedSharedQuadState) {
  InitOcclusionCuller();

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);
  gfx::Rect rect3(10, 10, 120, 30);

  // rect 4 and 5 intersect with the combined rect of 1 and 2.
  gfx::Rect rect4(10, 10, 180, 30);
  gfx::Rect rect5(10, 10, 120, 100);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //  rect1 & rect2                      rect 3 added
    //   +----+----+                       +----+----+
    //   |    |    |                       |____|___||
    //   |    |----+             =>        |    |----+
    //   +----+                            +----+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by both |rect1| and |rect2|, so |rect3| is
    // removed from |quad_list|.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }

  {
    //  rect1 & rect2                      rect 4 added
    //   +----+----+                       +----+----+-+
    //   |    |    |                       |____|____|_|
    //   |    |----+           =>          |    |----+
    //   +----+                            +----+
    //
    quad3 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state3->SetAll(
        gfx::Transform(), rect4, rect4, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad3->SetNew(shared_quad_state3, rect4, rect4, SkColors::kBlack, false);
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The occlusion rect, which is enlarged horizontally after visiting |rect1|
    // and |rect2|, is (0, 0, 160x60). Since visible region of rect 4 is
    // (160, 10, 30x30), |visible_rect| of |quad3| is updated.
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(160, 10, 30, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    //  rect1 & rect2                      rect 5 added
    //   +----+----+                       +----+----+
    //   |    |    |                       | +--|--+ |
    //   |    |----+           =>          | |  |--|-+
    //   +----+                            +-|--+  |
    //                                       +-----+
    shared_quad_state3->SetAll(
        gfx::Transform(), rect5, rect5, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad3->SetNew(shared_quad_state3, rect5, rect5, SkColors::kBlack, false);
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The occlusion rect, which is enlarged horizontally after visiting |rect1|
    // and |rect2|, is (0, 0, 160x60). Since visible region of rect 5 is
    // (10, 60, 120x50), |visible_rect| of |quad3| is updated.
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(10, 60, 120, 50),
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }
}

// Remove overlapping quads in non-root render passes.
TEST_F(OcclusionCullerTest, OcclusionCullingWithMultipleRenderPass) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame(/*num_render_passes=*/2);

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  // rect 4 is identical to rect 3, but in a separate render pass.
  gfx::Rect rects[4] = {
      gfx::Rect(0, 0, 100, 100),
      gfx::Rect(100, 0, 60, 60),
      gfx::Rect(10, 10, 120, 30),
      gfx::Rect(10, 10, 120, 30),
  };

  SharedQuadState* shared_quad_states[4];
  SolidColorDrawQuad* quads[4];
  for (int i = 0; i < 4; i++) {
    // add all but quad 4 into non-root render pass.
    auto& render_pass =
        i == 3 ? frame.render_pass_list.back() : frame.render_pass_list.front();
    shared_quad_states[i] = render_pass->CreateAndAppendSharedQuadState();
    quads[i] =
        render_pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_states[i]->SetAll(
        gfx::Transform(), rects[i], rects[i], gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, /*contents_opaque=*/true,
        /*opacity_f=*/1.f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quads[i]->SetNew(shared_quad_states[i], rects[i], rects[i],
                     SkColors::kBlack, false /*force_anti_aliasing_off*/);
  }

  auto& render_pass = frame.render_pass_list.front();
  auto& root_render_pass = frame.render_pass_list.back();
  EXPECT_EQ(3u, NumVisibleRects(render_pass->quad_list));
  EXPECT_EQ(1u, NumVisibleRects(root_render_pass->quad_list));

  occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

  EXPECT_EQ(2u, NumVisibleRects(render_pass->quad_list));
  EXPECT_EQ(1u, NumVisibleRects(root_render_pass->quad_list));
  EXPECT_EQ(rects[0], render_pass->quad_list.ElementAt(0)->visible_rect);
  EXPECT_EQ(rects[1], render_pass->quad_list.ElementAt(1)->visible_rect);
  EXPECT_EQ(rects[3], root_render_pass->quad_list.ElementAt(0)->visible_rect);
}

// Occlusion tracking should not persist across render passes.
TEST_F(OcclusionCullerTest, CompositorFrameWithMultipleRenderPass) {
  InitOcclusionCuller();

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(100, 0, 60, 60);

  auto render_pass2 = std::make_unique<AggregatedRenderPass>();
  render_pass2->SetNew(AggregatedRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                       gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));
  gfx::Rect rect3(10, 10, 120, 30);

  bool opaque_content = true;
  float opacity = 1.f;

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.at(1)
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // rect1 and rect2 are from first RenderPass and rect 3 is from the second
    // RenderPass.
    //  rect1 & rect2                      rect 3 added
    //   +----+----+                       +----+----+
    //   |    |    |                       |____|___||
    //   |    |----+             =>        |    |----+
    //   +----+                            +----+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // The occlusion rect is enlarged horizontally after visiting |rect1| and
    // |rect2|. |rect3| is covered by the unioned region of |rect1| and |rect2|.
    // But |rect3| so |rect3| is to be removed from |quad_list|.
    EXPECT_EQ(2u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.at(1)->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.at(1)->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithCoveredRenderPass) {
  InitOcclusionCuller();

  // rect 3 is inside of combined rect of rect 1 and rect 2.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);

  auto render_pass2 = std::make_unique<AggregatedRenderPass>();
  render_pass2->SetNew(AggregatedRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                       gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass2));

  bool opaque_content = true;
  float opacity = 1.f;
  AggregatedRenderPassId render_pass_id{1};
  ResourceId mask_resource_id(2);

  SharedQuadState* shared_quad_state =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.at(1)
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.at(1)->CreateAndAppendSharedQuadState();
  auto* quad1 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();

  {
    // rect1 is a DrawQuad from SQS1 and which is also the RenderPass rect
    // from SQS2. The AggregatedRenderPassDrawQuad should not be occluded.
    //  rect1
    //   +----+
    //   |    |
    //   |    |
    //   +----+
    //

    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad1->SetNew(shared_quad_state2, rect1, rect1, render_pass_id,
                  mask_resource_id, gfx::RectF(), gfx::Size(),
                  gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                  1.0f);

    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |rect1| and |rect2| shares the same region where |rect1| is a draw
    // quad and |rect2| RenderPass. |rect2| will be not removed from the
    // |quad_list|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(1u, frame.render_pass_list.at(1)->quad_list.size());
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.at(1)->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithClip) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);
  gfx::Rect clip_rect(0, 0, 60, 60);
  gfx::Rect rect3(50, 50, 20, 10);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    //  rect1 & rect2
    //   +------+
    //   |      |
    //   |   +-+|
    //   |   | ||
    //   +------+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |rect1| covers |rect2| as shown in the figure above, So the size of
    // |quad_list| is reduced by 1.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }

  {
    //  rect1 & rect2                             clip_rect & rect2
    //   +------+                                     +----+
    //   |      |                                     |    |
    //   |   +-+|             =>                      +----+ +-+
    //   +------+                                            +-+
    //
    quad2 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), clip_rect,
        opaque_content, opacity, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // In the target space, a clip is applied on |quad| (defined by |clip_rect|,
    // (0, 0, 60x60) |quad| and |quad2| (50, 50, 25x25) don't intersect in the
    // target space. So no change is applied to quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }

  {
    //  rect1(non-clip) & rect2                rect1(clip) & rect3
    //   +------+                                     +---+
    //   |   +-+|                                     |  +|+
    //   |   +-+|             =>                      +--+++
    //   +------+
    //
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(), clip_rect,
        opaque_content, opacity, SkBlendMode::kSrcOver, /*sorting_context=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect3, rect3, SkColors::kBlack, false);
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // In the target space, a clip is applied on |quad| (defined by |rect3|,
    // (50, 50, 20x10)). |quad| intersects with |quad2| in the target space. The
    // visible region of |quad2| is (60, 50, 10x10). So |quad2| is updated
    // accordingly.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(60, 50, 10, 10),
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

// Check if occlusion culling works with copy requests in root RenderPass only.
TEST_F(OcclusionCullerTest, CompositorFrameWithCopyRequest) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 50, 25, 25);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    frame.render_pass_list.front()->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // root RenderPass contains |rect1|, |rect2| and copy_request (where
    // |rect2| is in |rect1|). Since our current implementation only supports
    // occlusion with copy_request on root RenderPass, |quad_list| reduces its
    // size by 1 after calling remove overdraw.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithRenderPass) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(50, 0, 100, 100);
  gfx::Rect rect3(0, 0, 25, 25);
  gfx::Rect rect4(100, 0, 25, 25);
  gfx::Rect rect5(0, 0, 50, 50);
  gfx::Rect rect6(0, 75, 25, 25);
  gfx::Rect rect7(0, 0, 10, 10);

  bool opaque_content = true;
  AggregatedRenderPassId render_pass_id{1};
  ResourceId mask_resource_id(2);
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R1 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* R2 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* D1 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  SharedQuadState* shared_quad_state4 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* D2 = frame.render_pass_list.front()
                 ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  {
    // RenderPass r1 and r2 are intersecting to each other; however, the opaque
    // regions D1 and D2 on R1 and R2 are not intersecting.
    // +-------+---+--------+
    // |_D1_|  |   |_D2_|   |
    // |       |   |        |
    // |   R1  |   |    R2  |
    // +-------+---+--------+
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state4->SetAll(
        gfx::Transform(), rect4, rect4, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    R1->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect2, rect2, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    D2->SetNew(shared_quad_state4, rect4, rect4, SkColors::kBlack, false);
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since AggregatedRenderPassDrawQuad |r1| and |r2|
    // cannot be removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    // RenderPass R2 is contained in R1, but the opaque region of the two
    // RenderPasses are separated.
    // +-------+-----------+
    // |_D2_|  |      |_D1_|
    // |       |           |
    // |   R2  |       R1  |
    // +-------+-----------+
    shared_quad_state->SetAll(
        gfx::Transform(), rect5, rect5, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state4->SetAll(
        gfx::Transform(), rect6, rect6, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    D2->SetNew(shared_quad_state4, rect6, rect6, SkColors::kBlack, false);
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // As shown in the image above, the opaque region |d1| and |d2| does not
    // occlude each other. Since AggregatedRenderPassDrawQuad |r1| and |r2|
    // cannot be removed to reduce overdraw, |quad_list| remains unchanged.
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect6,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    // RenderPass R2 is contained in R1, and opaque region of R2 in R1 as well.
    // +-+---------+-------+
    // |-+   |     |       |
    // |-----+     |       |
    // |   R2      |   R1  |
    // +-----------+-------+
    shared_quad_state->SetAll(
        gfx::Transform(), rect5, rect5, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state4->SetAll(
        gfx::Transform(), rect7, rect7, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    R1->SetNew(shared_quad_state, rect5, rect5, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    R2->SetNew(shared_quad_state, rect1, rect1, render_pass_id,
               mask_resource_id, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
    D1->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);
    D2->SetNew(shared_quad_state4, rect7, rect7, SkColors::kBlack, false);
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // As shown in the image above, the opaque region |d2| is contained in |d1|
    // Since AggregatedRenderPassDrawQuad |r1| and |r2| cannot be removed to
    // reduce overdraw, |quad_list| is reduced by 1.
    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect5,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest,
       CompositorFrameWithMultipleDrawQuadInSharedQuadState) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect1_1(0, 0, 50, 50);
  gfx::Rect rect1_2(50, 0, 50, 50);
  gfx::Rect rect1_3(0, 50, 50, 50);
  gfx::Rect rect1_4(50, 50, 50, 50);
  gfx::Rect rect_in_rect1(0, 0, 60, 40);
  gfx::Rect rect_intersects_rect1(80, 0, 50, 30);

  gfx::Rect rect2(20, 0, 100, 100);
  gfx::Rect rect2_1(20, 0, 50, 50);
  gfx::Rect rect2_2(70, 0, 50, 50);
  gfx::Rect rect2_3(20, 50, 50, 50);
  gfx::Rect rect2_4(70, 50, 50, 50);
  gfx::Rect rect3(0, 0, 140, 60);
  gfx::Rect rect3_1(0, 0, 70, 30);
  gfx::Rect rect3_2(70, 0, 70, 30);

  bool opaque_content = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad4 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad5 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    // A Shared quad states contains 4 draw quads and it covers another draw
    // quad from different shared quad state.
    // +--+--+
    // +--|+ |
    // +--+--+
    // |  |  |
    // +--+--+
    shared_quad_state->SetAll(
        gfx::Transform(), rect1, rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect_in_rect1, rect_in_rect1, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad1->SetNew(shared_quad_state, rect1_1, rect1_1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state, rect1_2, rect1_2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state, rect1_3, rect1_3, SkColors::kBlack, false);
    quad4->SetNew(shared_quad_state, rect1_4, rect1_4, SkColors::kBlack, false);
    quad5->SetNew(shared_quad_state2, rect_in_rect1, rect_in_rect1,
                  SkColors::kBlack, false);
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // covers the visible region of |shared_quad_state2|.
    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1_1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1_2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect1_3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect1_4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
  }

  {
    // A Shared quad states that contains 4 drawquads that intersect with
    // another shared quad state that contains 1 drawquad.
    // +--+-++--+
    // |  | +|--+
    // +--+--+
    // |  |  |
    // +--+--+
    quad5 = frame.render_pass_list.front()
                ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state2->SetAll(gfx::Transform(), rect_intersects_rect1,
                               rect_intersects_rect1, gfx::MaskFilterInfo(),
                               /*clip=*/std::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad5->SetNew(shared_quad_state2, rect_intersects_rect1,
                  rect_intersects_rect1, SkColors::kBlack, false);
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // partially covers the visible region of |shared_quad_state2|. The
    // |visible_rect| of |quad5| is updated.
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1_1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect1_2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect1_3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect1_4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(100, 0, 30, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(5)->visible_rect);
  }

  {
    // A Shared quad states that contains 4 DrawQuads that intersects with
    // another shared quad state that contains 2 DrawQuads.
    // +-+--+--+-+
    // +-|--|--|-+
    //   +--+--+
    //   |  |  |
    //   +--+--+

    auto* quad6 = frame.render_pass_list.front()
                      ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    shared_quad_state->SetAll(
        gfx::Transform(), rect2, rect2, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(
        gfx::Transform(), rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad1->SetNew(shared_quad_state, rect2_1, rect2_1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state, rect2_2, rect2_2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state, rect2_3, rect2_3, SkColors::kBlack, false);
    quad4->SetNew(shared_quad_state, rect2_4, rect2_4, SkColors::kBlack, false);
    quad5->SetNew(shared_quad_state2, rect3_1, rect3_1, SkColors::kBlack,
                  false);
    quad6->SetNew(shared_quad_state2, rect3_2, rect3_2, SkColors::kBlack,
                  false);
    EXPECT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |visible_rect| of |shared_quad_state| is formed by 4 DrawQuads and it
    // partially covers the visible region of |shared_quad_state2|. So the
    // |visible_rect| of DrawQuads in |share_quad_state2| are updated to the
    // region shown on screen.
    EXPECT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect2_1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect2_2,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(
        rect2_3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(
        rect2_4,
        frame.render_pass_list.front()->quad_list.ElementAt(3)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(0, 0, 20, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(5)->visible_rect);
    EXPECT_EQ(
        gfx::Rect(120, 0, 20, 30),
        frame.render_pass_list.front()->quad_list.ElementAt(6)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, CompositorFrameWithNonInvertibleTransform) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect rect1(0, 0, 100, 100);
  gfx::Rect rect2(10, 10, 50, 50);
  gfx::Rect rect3(0, 0, 10, 10);

  gfx::Transform invertible;
  auto non_invertible = gfx::Transform::RowMajor(10, 10, 0, 0,  // row 1
                                                 10, 10, 0, 0,  // row 2
                                                 0, 0, 1, 0,    // row 3
                                                 0, 0, 0, 1);   // row 4
  gfx::Transform non_invertible_miss_z;
  non_invertible_miss_z.Scale3d(1, 1, 0);
  bool opaque_content = true;
  float opacity = 1.f;
  auto* quad1 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad2 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* quad3 = frame.render_pass_list.front()
                    ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  SharedQuadState* shared_quad_state1 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state2 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state3 =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  {
    // in quad content space:        in target space:
    // +-+---------+                 +-----------+----+
    // +-+   q1    |                 |        q1 | q3 |
    // | +----+    |                 | +----+    |    |
    // | | q2 |    |                 | | q2 |    |    |
    // | +----+    |                 | +----+    |    |
    // |           |                 |           |    |
    // +-----------+                 +-----------+    |
    //                               |                |
    //                               +----------------+
    // |quad1| forms an occlusion rect; |quad2| follows a invertible transform
    // and is hiding behind quad1; |quad3| follows a non-invertible transform
    // and it is not covered by the occlusion rect.
    shared_quad_state1->SetAll(invertible, rect1, rect1, gfx::MaskFilterInfo(),
                               std::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state2->SetAll(invertible, rect2, rect2, gfx::MaskFilterInfo(),
                               std::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        non_invertible, rect3, rect3, gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad1->SetNew(shared_quad_state1, rect1, rect1, SkColors::kBlack, false);
    quad2->SetNew(shared_quad_state2, rect2, rect2, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);

    EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad2| is removed because it is not shown on screen in the target space.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        rect3,
        frame.render_pass_list.front()->quad_list.ElementAt(2)->visible_rect);
  }

  {
    // in quad content space:     in target space:
    // +--------+                 +--------+
    // | |      |                 | |      |
    // |-+      |                 |-+      |
    // |        |                 |        |
    // +--------+                 +--------+
    // Verify if occlusion culling can occlude quad with non-invertible
    // transform.
    shared_quad_state1->SetAll(invertible, rect1, rect1, gfx::MaskFilterInfo(),
                               std::nullopt, opaque_content, opacity,
                               SkBlendMode::kSrcOver, /*sorting_context=*/0,
                               /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    shared_quad_state3->SetAll(
        non_invertible_miss_z, rect3, rect3, gfx::MaskFilterInfo(),
        std::nullopt, opaque_content, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quad1->SetNew(shared_quad_state1, rect1, rect1, SkColors::kBlack, false);
    quad3->SetNew(shared_quad_state3, rect3, rect3, SkColors::kBlack, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // |quad3| follows an non-invertible transform and it's covered by the
    // occlusion rect. So |quad3| is removed from the |frame|.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

// Check if occlusion culling works with very large DrawQuad. crbug.com/824528.
TEST_F(OcclusionCullerTest, OcclusionCullingWithLargeDrawQuad) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  // The size of this DrawQuad will be 237790x237790 > 2^32 (uint32_t.max())
  // which caused the integer overflow in the bug.
  gfx::Rect rect1(237790, 237790);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  auto* quad = frame.render_pass_list.front()
                   ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  // +----+
  // |    |
  // +----+
  {
    shared_quad_state->SetAll(gfx::Transform(), rect1, rect1,
                              gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                              are_contents_opaque, opacity,
                              SkBlendMode::kSrcOver, /*sorting_context=*/0,
                              /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    quad->SetNew(shared_quad_state, rect1, rect1, SkColors::kBlack, false);
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // This is a base case, the compositor frame contains only one
    // DrawQuad, so the size of quad_list remains unchanged after calling
    // RemoveOverdrawQuads.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        rect1,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingWithRoundedCornerDoesNotOcclude) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  // The quad with rounded corner does not completely cover the quad below it.
  // The corners of the below quad are visible through the clipped corners.
  gfx::Rect quad_rect(10, 10, 100, 100);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), quad_rect, quad_rect, gfx::MaskFilterInfo(),
        std::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, quad_rect, quad_rect,
                          SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since none of the quads are culled, there should be 2 quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingWithRoundedCornerDoesNotOccludeY) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  // The quad with distinct rounded corner does not completely cover the quad
  // below it. The corners of the below quad are visible through the clipped
  // corners.
  gfx::Rect quad_rect(10, 10, 100, 100);
  gfx::Rect occluded_quad_rect(13, 13, 994, 994);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f, 20.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_quad_rect, occluded_quad_rect,
        gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_quad_rect,
                          occluded_quad_rect, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since none of the quads are culled, there should be 2 quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        occluded_quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingWithRoundedCornerDoesNotOccludeX) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  // The quad with distinct rounded corner does not completely cover the quad
  // below it. The corners of the below quad are visible through the clipped
  // corners.
  gfx::Rect quad_rect(10, 10, 100, 100);
  gfx::Rect occluded_quad_rect(13, 13, 994, 994);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 20.f, 10.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_quad_rect, occluded_quad_rect,
        gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_quad_rect,
                          occluded_quad_rect, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since none of the quads are culled, there should be 2 quads.
    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(
        occluded_quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(1)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingWithRoundedCornerDoesOcclude) {
  InitOcclusionCuller();

  // The quad with rounded corner completely covers the quad below it.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::Rect occluded_quad_rect(13, 13, 994, 994);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_quad_rect, occluded_quad_rect,
        gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_quad_rect,
                          occluded_quad_rect, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingWithRoundedCornerDoesOccludeXY) {
  InitOcclusionCuller();

  // The quad with distinct rounded corners completely covers the quad below it.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::Rect occluded_quad_rect(13, 16, 994, 988);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f, 20.f));

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_quad_rect, occluded_quad_rect,
        gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_quad_rect,
                          occluded_quad_rect, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(1u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingSplit) {
  InitOcclusionCuller();

  // The two partially occluded quads will be split into two additional quads,
  // preserving only the visible regions.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  //  +--------------------------------+
  //  |***+----------------------+ <- Large occluding Rect
  //  +---|-  -   -  - +  -  -  -|-----+
  //  |***|            .         |*****|
  //  |***+----------------------+*****|
  //  |****************|***************|
  //  +----------------+---------------+
  //
  // * -> Visible rect for the quads.

  const gfx::Rect occluding_rect(10, 10, 1000, 490);
  const gfx::Rect quad_rects[3] = {
      gfx::Rect(0, 0, 1200, 20),
      gfx::Rect(0, 20, 600, 490),
      gfx::Rect(600, 20, 600, 490),
  };
  gfx::Rect occluded_sqs_rect(0, 0, 1200, 510);

  const bool are_contents_opaque = true;
  const float opacity = 1.f;
  SharedQuadState* shared_quad_state_occluder =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  SolidColorDrawQuad* quads[4];
  for (auto*& quad : quads) {
    quad = frame.render_pass_list.front()
               ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  }

  {
    shared_quad_state_occluder->SetAll(
        gfx::Transform(), occluding_rect, occluding_rect, gfx::MaskFilterInfo(),
        std::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    quads[0]->SetNew(shared_quad_state_occluder, occluding_rect, occluding_rect,
                     SkColors::kRed, false);

    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_sqs_rect, occluded_sqs_rect,
        gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    for (int i = 1; i < 4; i++) {
      quads[i]->SetNew(shared_quad_state_occluded, quad_rects[i - 1],
                       quad_rects[i - 1], SkColors::kRed, false);
    }

    EXPECT_EQ(4u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    ASSERT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        occluding_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);

    // Computed the expected quads
    //  +--------------------------------+
    //  |                1               |
    //  +---+----------------------+-----+
    //  | 2 |                      |  3  |
    //  +---+------------+---------+-----+
    //  |        4       |        5      |
    //  +----------------+---------------+
    const gfx::Rect expected_visible_rects[5]{
        // The occluded region of rest one is small, so we do not split the
        // quad.
        quad_rects[0],
        gfx::Rect(0, 20, 10, 480),
        gfx::Rect(0, 500, 600, 10),
        gfx::Rect(1010, 20, 190, 480),
        gfx::Rect(600, 500, 600, 10),
    };

    const QuadList& quad_list = frame.render_pass_list.front()->quad_list;
    for (int i = 0; i < 5; i++) {
      EXPECT_EQ(expected_visible_rects[i],
                quad_list.ElementAt(i + 1)->visible_rect);
    }
  }
}

// Tests cases in which occlusion culling splits are performed due to first pass
// complexity reduction in visible regions. For more details, see:
// https://tinyurl.com/RegionComplexityReduction#heading=h.fg95k5w5t791
TEST_F(OcclusionCullerTest, FirstPassVisibleComplexityReduction) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  const bool are_contents_opaque = true;
  const float opacity = 1.f;

  //  +---------+-------+--------------+
  //  |*********|       |**************|
  //  |*********|       +------+*******|
  //  |*********|       |      |*******|
  //  |*********|       +------+*******|
  //  |*********|       |**************|
  //  +---------+-------+--------------+
  //
  //  *--> occluded quad
  //
  // This configuration will produce the following visible region for the
  // occluded quad.
  //  +---------+       +--------------+
  //  |    1    |       |      2       |
  //  |---------+       +------+-------|
  //  |    3    |              |   4   |
  //  |---------+       +------+-------|
  //  |    5    |       |      6       |
  //  +---------+       +--------------+
  //
  // The above split is unnecessarily complex. Rectangles 1, 3, and 5 should be
  // merged:
  //  +---------+       +--------------+
  //  |         |       |      2       |
  //  |         |       +------+-------|
  //  |    1    |              |   3   |
  //  |         |       +------+-------|
  //  |         |       |      4       |
  //  +---------+       +--------------+
  //
  // If the merge is not done, this visible region will be discarded and the
  // quad will not be split.

  const gfx::Rect occluding_rects[2] = {
      gfx::Rect(300, 0, 550, 270),
      gfx::Rect(850, 50, 150, 150),
  };
  for (const auto& r : occluding_rects) {
    SharedQuadState* shared_quad_state_occluder =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    shared_quad_state_occluder->SetAll(
        gfx::Transform(), r, r, gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
        are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    SolidColorDrawQuad* quad =
        frame.render_pass_list.front()
            ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state_occluder, r, r, SkColors::kRed, false);
  }

  const gfx::Rect occluded_rect(0, 0, 1350, 270);
  {
    SharedQuadState* shared_quad_state_occluded =
        frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_rect, occluded_rect, gfx::MaskFilterInfo(),
        std::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
        /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    SolidColorDrawQuad* occluded_quad =
        frame.render_pass_list.front()
            ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
    occluded_quad->SetNew(shared_quad_state_occluded, occluded_rect,
                          occluded_rect, SkColors::kRed, false);
  }

  EXPECT_EQ(3u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
  occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

  ASSERT_EQ(6u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

  //  Expected visible quads:
  //  +---------+-------+--------------+
  //  |*********|       |******4*******|
  //  |*********|       +------+-------|
  //  |****3****|   1   |   2  |***5***|
  //  |*********|       +------+-------|
  //  |*********|       |******6*******|
  //  +---------+-------+--------------+
  //
  // * -> Visible rect for the quads.

  const gfx::Rect expected_visible_rects[6] = {
      occluding_rects[0],
      occluding_rects[1],
      gfx::Rect(0, 0, 300, 270),
      gfx::Rect(850, 0, 500, 50),
      gfx::Rect(1000, 50, 350, 150),
      gfx::Rect(850, 200, 500, 70),
  };

  for (size_t i = 0; i < std::size(expected_visible_rects); ++i) {
    EXPECT_EQ(
        expected_visible_rects[i],
        frame.render_pass_list.front()->quad_list.ElementAt(i)->visible_rect);
  }
}

TEST_F(OcclusionCullerTest, OcclusionCullingWithRoundedCornerPartialOcclude) {
  InitOcclusionCuller();

  // The quad with rounded corner completely covers the quad below it.
  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  //      +----------------------+
  //      |                      | <- Large occluding Rect
  //  +---|-  -  -  -  +  -  -  -|-------+
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  +---|-  -  -  -  +  -  -  -|-------+
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  |***|            .         |*******|
  //  +---|-  -  -  -  +  -  -  -|-------+
  //      |                      |
  //      +----------------------+
  //
  // * -> Visible rect for the quads.
  gfx::Rect quad_rect(10, 10, 1000, 1000);
  gfx::MaskFilterInfo mask_filter_info(
      gfx::RRectF(gfx::RectF(quad_rect), 10.f));
  gfx::Rect occluded_quad_rect_1(0, 20, 600, 490);
  gfx::Rect occluded_quad_rect_2(600, 20, 600, 490);
  gfx::Rect occluded_quad_rect_3(0, 510, 600, 490);
  gfx::Rect occluded_quad_rect_4(600, 510, 600, 490);
  gfx::Rect occluded_sqs_rect;
  occluded_sqs_rect.Union(occluded_quad_rect_1);
  occluded_sqs_rect.Union(occluded_quad_rect_2);
  occluded_sqs_rect.Union(occluded_quad_rect_3);
  occluded_sqs_rect.Union(occluded_quad_rect_4);

  bool are_contents_opaque = true;
  float opacity = 1.f;
  SharedQuadState* shared_quad_state_with_rrect =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();

  auto* rounded_corner_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_1 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_2 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_3 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  auto* occluded_quad_4 =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();

  {
    shared_quad_state_occluded->SetAll(
        gfx::Transform(), occluded_sqs_rect, occluded_sqs_rect,
        gfx::MaskFilterInfo(), std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    occluded_quad_1->SetNew(shared_quad_state_occluded, occluded_quad_rect_1,
                            occluded_quad_rect_1, SkColors::kRed, false);
    occluded_quad_2->SetNew(shared_quad_state_occluded, occluded_quad_rect_2,
                            occluded_quad_rect_2, SkColors::kRed, false);
    occluded_quad_3->SetNew(shared_quad_state_occluded, occluded_quad_rect_3,
                            occluded_quad_rect_3, SkColors::kRed, false);
    occluded_quad_4->SetNew(shared_quad_state_occluded, occluded_quad_rect_4,
                            occluded_quad_rect_4, SkColors::kRed, false);

    shared_quad_state_with_rrect->SetAll(
        gfx::Transform(), quad_rect, quad_rect, mask_filter_info,
        /*clip=*/std::nullopt, are_contents_opaque, opacity,
        SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);
    rounded_corner_quad->SetNew(shared_quad_state_with_rrect, quad_rect,
                                quad_rect, SkColors::kBlue, false);

    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    occlusion_culler()->RemoveOverdrawQuads(&frame, kDefaultDeviceScaleFactor);

    // Since the quad with rounded corner completely covers the quad with
    // no rounded corner, the later quad is culled. We should only have 1 quad
    // in the final list now.
    EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
    EXPECT_EQ(
        quad_rect,
        frame.render_pass_list.front()->quad_list.ElementAt(0)->visible_rect);

    // For rounded rect of bounds (10, 10, 1000, 1000) and corner radius of 10,
    // the occluding rect for it would be (13, 13, 994, 994).
    const gfx::Rect occluding_rect(13, 13, 994, 994);

    // Computed the expe
    gfx::Rect expected_visible_rect_1 = occluded_quad_rect_1;
    expected_visible_rect_1.Subtract(occluding_rect);
    gfx::Rect expected_visible_rect_2 = occluded_quad_rect_2;
    expected_visible_rect_2.Subtract(occluding_rect);
    gfx::Rect expected_visible_rect_3 = occluded_quad_rect_3;
    expected_visible_rect_3.Subtract(occluding_rect);
    gfx::Rect expected_visible_rect_4 = occluded_quad_rect_4;
    expected_visible_rect_4.Subtract(occluding_rect);

    const QuadList& quad_list = frame.render_pass_list.front()->quad_list;

    EXPECT_EQ(expected_visible_rect_1, quad_list.ElementAt(1)->visible_rect);
    EXPECT_EQ(expected_visible_rect_2, quad_list.ElementAt(2)->visible_rect);
    EXPECT_EQ(expected_visible_rect_3, quad_list.ElementAt(3)->visible_rect);
    EXPECT_EQ(expected_visible_rect_4, quad_list.ElementAt(4)->visible_rect);
  }
}

// Test that the threshold we use to determine if it's worth splitting a quad or
// not takes into account the device scale factor. In particular, this test
// would not pass if we had a display scale factor equal to 1.f instead of 1.5f
// since the number of saved fragments would only be 100x100 which is lower than
// our threshold 128x128.
TEST_F(OcclusionCullerTest, OcclusionCullingSplitDeviceScaleFactorFractional) {
  InitOcclusionCuller();

  AggregatedFrame frame = MakeDefaultAggregatedFrame();

  const bool are_contents_opaque = true;
  const float opacity = 1.f;

  // Occluder quad.
  const gfx::Rect occluding_rect(10, 10, 100, 100);
  SharedQuadState* shared_quad_state_occluding =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SolidColorDrawQuad* occluding_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  shared_quad_state_occluding->SetAll(
      gfx::Transform(), occluding_rect, occluding_rect, gfx::MaskFilterInfo(),
      std::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
      /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  occluding_quad->SetNew(shared_quad_state_occluding, occluding_rect,
                         occluding_rect, SkColors::kRed, false);

  // Occluded quad.
  const gfx::Rect occluded_rect = gfx::Rect(0, 0, 1000, 1000);
  SharedQuadState* shared_quad_state_occluded =
      frame.render_pass_list.front()->CreateAndAppendSharedQuadState();
  SolidColorDrawQuad* occluded_quad =
      frame.render_pass_list.front()
          ->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  shared_quad_state_occluded->SetAll(
      gfx::Transform(), occluded_rect, occluded_rect, gfx::MaskFilterInfo(),
      std::nullopt, are_contents_opaque, opacity, SkBlendMode::kSrcOver,
      /*sorting_context=*/0, /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  occluded_quad->SetNew(shared_quad_state_occluded, occluded_rect,
                        occluded_rect, SkColors::kRed, false);

  EXPECT_EQ(2u, NumVisibleRects(frame.render_pass_list.front()->quad_list));

  occlusion_culler()->RemoveOverdrawQuads(&frame, 1.5f);
  EXPECT_EQ(5u, NumVisibleRects(frame.render_pass_list.front()->quad_list));
}

}  // namespace
}  // namespace viz
