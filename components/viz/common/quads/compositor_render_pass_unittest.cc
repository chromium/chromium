// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_render_pass.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

static void CompareRenderPassLists(
    const CompositorRenderPassList& expected_list,
    const CompositorRenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    CompositorRenderPass* expected = expected_list[i].get();
    CompositorRenderPass* actual = actual_list[i].get();

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->filters, actual->filters);
    EXPECT_EQ(expected->backdrop_filters, actual->backdrop_filters);
    EXPECT_EQ(expected->backdrop_filter_bounds, actual->backdrop_filter_bounds);
    EXPECT_EQ(expected->subtree_capture_id, actual->subtree_capture_id);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);
    EXPECT_EQ(expected->generate_mipmap, actual->generate_mipmap);
    EXPECT_EQ(expected->has_per_quad_damage, actual->has_per_quad_damage);
    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (auto exp_iter = expected->quad_list.cbegin(),
              act_iter = actual->quad_list.cbegin();
         exp_iter != expected->quad_list.cend(); ++exp_iter, ++act_iter) {
      EXPECT_EQ(exp_iter->rect.ToString(), act_iter->rect.ToString());
      EXPECT_EQ(exp_iter->shared_quad_state->quad_layer_rect.ToString(),
                act_iter->shared_quad_state->quad_layer_rect.ToString());
    }
  }
}

TEST(CompositorRenderPassTest,
     AggregatedCopyShouldBeIdenticalExceptIdAndQuads) {
  AggregatedRenderPassId render_pass_id{3u};
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform::Affine(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateOpacityFilter(0.5));
  cc::FilterOperations backdrop_filters;
  backdrop_filters.Append(cc::FilterOperation::CreateInvertFilter(1.0));
  std::optional<gfx::RRectF> backdrop_filter_bounds(
      {10, 20, 130, 140, 1, 2, 3, 4, 5, 6, 7, 8});
  gfx::ContentColorUsage content_color_usage = gfx::ContentColorUsage::kHDR;
  bool has_transparent_background = true;
  bool cache_render_pass = false;
  bool has_damage_from_contributing_content = false;
  bool generate_mipmap = false;

  auto pass = std::make_unique<AggregatedRenderPass>();
  pass->SetAll(render_pass_id, output_rect, damage_rect, transform_to_root,
               filters, backdrop_filters, backdrop_filter_bounds,
               content_color_usage, has_transparent_background,
               cache_render_pass, has_damage_from_contributing_content,
               generate_mipmap);
  pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

  // Stick a quad in the pass, this should not get copied.
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(), gfx::Rect(),
                       gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                       /*contents_opaque=*/false, /*opacity_f=*/1,
                       SkBlendMode::kSrcOver, /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(pass->shared_quad_state_list.back(), gfx::Rect(),
                     gfx::Rect(), SkColor4f(), false);

  AggregatedRenderPassId new_render_pass_id{63u};

  auto copy = pass->Copy(new_render_pass_id);
  EXPECT_EQ(new_render_pass_id, copy->id);
  EXPECT_EQ(pass->output_rect, copy->output_rect);
  EXPECT_EQ(pass->transform_to_root_target, copy->transform_to_root_target);
  EXPECT_EQ(pass->damage_rect, copy->damage_rect);
  EXPECT_EQ(pass->filters, copy->filters);
  EXPECT_EQ(pass->backdrop_filters, copy->backdrop_filters);
  EXPECT_TRUE(pass->backdrop_filter_bounds->ApproximatelyEqual(
      copy->backdrop_filter_bounds.value(), 0.001));
  EXPECT_EQ(pass->content_color_usage, copy->content_color_usage);
  EXPECT_EQ(pass->has_transparent_background, copy->has_transparent_background);
  EXPECT_EQ(pass->cache_render_pass, copy->cache_render_pass);
  EXPECT_EQ(pass->has_damage_from_contributing_content,
            copy->has_damage_from_contributing_content);
  EXPECT_EQ(pass->generate_mipmap, copy->generate_mipmap);
  EXPECT_EQ(0u, copy->quad_list.size());

  // The copy request should not be copied/duplicated.
  EXPECT_EQ(1u, pass->copy_requests.size());
  EXPECT_EQ(0u, copy->copy_requests.size());
}

TEST(CompositorRenderPassTest, CopyAllShouldBeIdentical) {
  CompositorRenderPassList pass_list;

  CompositorRenderPassId id{3};
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform::Affine(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateOpacityFilter(0.5));
  cc::FilterOperations backdrop_filters;
  backdrop_filters.Append(cc::FilterOperation::CreateInvertFilter(1.0));
  std::optional<gfx::RRectF> backdrop_filter_bounds(
      {10, 20, 130, 140, 1, 2, 3, 4, 5, 6, 7, 8});
  bool has_transparent_background = true;
  bool cache_render_pass = false;
  bool has_damage_from_contributing_content = false;
  bool generate_mipmap = false;
  bool has_per_quad_damage = false;

  auto pass = CompositorRenderPass::Create();
  pass->SetAll(id, output_rect, damage_rect, transform_to_root, filters,
               backdrop_filters, backdrop_filter_bounds,
               SubtreeCaptureId(base::Token(0u, 1u)), output_rect.size(),
               ViewTransitionElementResourceId(), has_transparent_background,
               cache_render_pass, has_damage_from_contributing_content,
               generate_mipmap, has_per_quad_damage);

  // Two quads using one shared state.
  SharedQuadState* shared_state1 = pass->CreateAndAppendSharedQuadState();
  shared_state1->SetAll(gfx::Transform(), gfx::Rect(0, 0, 1, 1), gfx::Rect(),
                        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                        /*contents_opaque=*/false, /*opacity_f=*/1,
                        SkBlendMode::kSrcOver, /*sorting_context=*/0,
                        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* color_quad1 = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad1->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 1, 1, 1), SkColor4f(),
                      false);

  auto* color_quad2 = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad2->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(2, 2, 2, 2), gfx::Rect(2, 2, 2, 2), SkColor4f(),
                      false);

  // And two quads using another shared state.
  SharedQuadState* shared_state2 = pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                        /*contents_opaque=*/false, /*opacity_f=*/1,
                        SkBlendMode::kSrcOver, /*sorting_context=*/0,
                        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* color_quad3 = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad3->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(3, 3, 3, 3), gfx::Rect(3, 3, 3, 3), SkColor4f(),
                      false);

  auto* color_quad4 = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad4->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(4, 4, 4, 4), gfx::Rect(4, 4, 4, 4), SkColor4f(),
                      false);

  // A second render pass with a quad.
  CompositorRenderPassId contrib_id{4};
  gfx::Rect contrib_output_rect(10, 15, 12, 17);
  gfx::Transform contrib_transform_to_root =
      gfx::Transform::Affine(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect contrib_damage_rect(11, 16, 10, 15);
  cc::FilterOperations contrib_filters;
  contrib_filters.Append(cc::FilterOperation::CreateSepiaFilter(0.5));
  cc::FilterOperations contrib_backdrop_filters;
  contrib_backdrop_filters.Append(cc::FilterOperation::CreateSaturateFilter(1));
  std::optional<gfx::RRectF> contrib_backdrop_filter_bounds(
      {20, 30, 140, 150, 1, 2, 3, 4, 5, 6, 7, 8});
  bool contrib_has_transparent_background = true;
  bool contrib_cache_render_pass = false;
  bool contrib_has_damage_from_contributing_content = false;
  bool contrib_generate_mipmap = false;
  bool contrib_has_per_quad_damage = false;

  auto contrib = CompositorRenderPass::Create();
  contrib->SetAll(contrib_id, contrib_output_rect, contrib_damage_rect,
                  contrib_transform_to_root, contrib_filters,
                  contrib_backdrop_filters, contrib_backdrop_filter_bounds,
                  SubtreeCaptureId(base::Token(0u, 2u)),
                  contrib_output_rect.size(), ViewTransitionElementResourceId(),
                  contrib_has_transparent_background, contrib_cache_render_pass,
                  contrib_has_damage_from_contributing_content,
                  contrib_generate_mipmap, contrib_has_per_quad_damage);

  SharedQuadState* contrib_shared_state =
      contrib->CreateAndAppendSharedQuadState();
  contrib_shared_state->SetAll(
      gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
      gfx::MaskFilterInfo(), /*clip=*/std::nullopt, /*contents_opaque=*/false,
      /*opacity_f=*/1, SkBlendMode::kSrcOver, /*sorting_context=*/0,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* contrib_quad = contrib->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  contrib_quad->SetNew(contrib->shared_quad_state_list.back(),
                       gfx::Rect(3, 3, 3, 3), gfx::Rect(3, 3, 3, 3),
                       SkColor4f(), false);

  // And a CompositorRenderPassDrawQuad for the contributing pass.
  auto pass_quad = std::make_unique<CompositorRenderPassDrawQuad>();
  pass_quad->SetNew(pass->shared_quad_state_list.back(), contrib_output_rect,
                    contrib_output_rect, contrib_id, ResourceId(1u),
                    gfx::RectF(), gfx::Size(), gfx::Vector2dF(1.0f, 1.0f),
                    gfx::PointF(), gfx::RectF(), false, 1.0f);

  pass_list.push_back(std::move(pass));
  pass_list.push_back(std::move(contrib));

  // Make a copy with CopyAll().
  CompositorRenderPassList copy_list;
  CompositorRenderPass::CopyAllForTest(pass_list, &copy_list);

  CompareRenderPassLists(pass_list, copy_list);
}

TEST(CompositorRenderPassTest, CopyAllWithCulledQuads) {
  CompositorRenderPassList pass_list;

  CompositorRenderPassId id{3};
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform::Affine(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateOpacityFilter(0.5));
  cc::FilterOperations backdrop_filters;
  backdrop_filters.Append(cc::FilterOperation::CreateInvertFilter(1.0));
  std::optional<gfx::RRectF> backdrop_filter_bounds(
      {10, 20, 130, 140, 1, 2, 3, 4, 5, 6, 7, 8});
  bool has_transparent_background = true;
  bool cache_render_pass = false;
  bool has_damage_from_contributing_content = false;
  bool generate_mipmap = false;
  bool has_per_quad_damage = false;
  auto pass = CompositorRenderPass::Create();
  pass->SetAll(id, output_rect, damage_rect, transform_to_root, filters,
               backdrop_filters, backdrop_filter_bounds, SubtreeCaptureId(),
               output_rect.size(), ViewTransitionElementResourceId(),
               has_transparent_background, cache_render_pass,
               has_damage_from_contributing_content, generate_mipmap,
               has_per_quad_damage);

  // A shared state with a quad.
  SharedQuadState* shared_state1 = pass->CreateAndAppendSharedQuadState();
  shared_state1->SetAll(gfx::Transform(), gfx::Rect(0, 0, 1, 1), gfx::Rect(),
                        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                        /*contents_opaque=*/false, /*opacity_f=*/1,
                        SkBlendMode::kSrcOver, /*sorting_context=*/0,
                        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* color_quad1 = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad1->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 1, 1, 1), SkColor4f(),
                      false);

  // A shared state with no quads, they were culled.
  SharedQuadState* shared_state2 = pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                        /*contents_opaque=*/false, /*opacity_f=*/1,
                        SkBlendMode::kSrcOver, /*sorting_context=*/0,
                        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  // A second shared state with no quads.
  SharedQuadState* shared_state3 = pass->CreateAndAppendSharedQuadState();
  shared_state3->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                        /*contents_opaque=*/false, /*opacity_f=*/1,
                        SkBlendMode::kSrcOver, /*sorting_context=*/0,
                        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  // A last shared state with a quad again.
  SharedQuadState* shared_state4 = pass->CreateAndAppendSharedQuadState();
  shared_state4->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::MaskFilterInfo(), /*clip=*/std::nullopt,
                        /*contents_opaque=*/false, /*opacity_f=*/1,
                        SkBlendMode::kSrcOver, /*sorting_context=*/0,
                        /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  auto* color_quad2 = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad2->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(3, 3, 3, 3), gfx::Rect(3, 3, 3, 3), SkColor4f(),
                      false);

  pass_list.push_back(std::move(pass));

  // Make a copy with CopyAll().
  CompositorRenderPassList copy_list;
  CompositorRenderPass::CopyAllForTest(pass_list, &copy_list);

  CompareRenderPassLists(pass_list, copy_list);
}

TEST(CompositorRenderPassTest, ReplacedQuadsShouldntMove) {
  auto pass = CompositorRenderPass::Create();
  SharedQuadState* quad_state = pass->CreateAndAppendSharedQuadState();
  auto* quad = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  gfx::Rect quad_rect(1, 2, 3, 4);
  quad->SetNew(quad_state, quad_rect, quad_rect, SkColor4f(), false);
  pass->ReplaceExistingQuadWithSolidColor(pass->quad_list.begin(), SkColor4f(),
                                          SkBlendMode::kSrcOver);
  EXPECT_EQ(pass->quad_list.begin()->rect, quad_rect);
}

TEST(CompositorRenderPassTest, ReplacedQuadsShouldntBeOpaque) {
  auto pass = CompositorRenderPass::Create();
  SharedQuadState* quad_state = pass->CreateAndAppendSharedQuadState();
  auto* quad = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  gfx::Rect quad_rect(1, 2, 3, 4);
  quad->SetNew(quad_state, quad_rect, quad_rect, SkColor4f(), false);
  pass->ReplaceExistingQuadWithSolidColor(pass->quad_list.begin(), SkColor4f(),
                                          SkBlendMode::kSrcOver);
  EXPECT_FALSE(pass->quad_list.begin()->shared_quad_state->are_contents_opaque);
}

TEST(CompositorRenderPassTest, ReplacedQuadsGetColor) {
  auto pass = CompositorRenderPass::Create();
  const SharedQuadState* quad_state = pass->CreateAndAppendSharedQuadState();
  auto* quad = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  const gfx::Rect quad_rect(1, 2, 3, 4);
  quad->SetNew(quad_state, quad_rect, quad_rect, SkColors::kRed, false);
  pass->ReplaceExistingQuadWithSolidColor(
      pass->quad_list.begin(), SkColors::kGreen, SkBlendMode::kSrcOver);
  EXPECT_EQ(SkColors::kGreen, quad->color);
}

TEST(CompositorRenderPassTest, ReplacedQuadsGetBlendMode) {
  auto pass = CompositorRenderPass::Create();
  SharedQuadState* quad_state = pass->CreateAndAppendSharedQuadState();
  // Make |are_contents_opaque| already false, to test that the blend mode is
  // recognized as a reason for needing a new |SharedQuadState|.
  quad_state->are_contents_opaque = false;
  auto* quad = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  const gfx::Rect quad_rect(1, 2, 3, 4);
  quad->SetNew(quad_state, quad_rect, quad_rect, SkColor4f(), false);
  pass->ReplaceExistingQuadWithSolidColor(pass->quad_list.begin(), SkColor4f(),
                                          SkBlendMode::kDstOut);
  EXPECT_EQ(SkBlendMode::kDstOut, quad->shared_quad_state->blend_mode);
}

TEST(CompositorRenderPassTest,
     ReplacedQuadsKeepOldSharedQuadStateWhenPossible) {
  auto pass = CompositorRenderPass::Create();
  SharedQuadState* quad_state = pass->CreateAndAppendSharedQuadState();
  quad_state->are_contents_opaque = false;
  quad_state->blend_mode = SkBlendMode::kSoftLight;
  auto* quad = pass->quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  const gfx::Rect quad_rect(1, 2, 3, 4);
  quad->SetNew(quad_state, quad_rect, quad_rect, SkColors::kRed, false);
  pass->ReplaceExistingQuadWithSolidColor(
      pass->quad_list.begin(), SkColors::kGreen, SkBlendMode::kSoftLight);
  EXPECT_EQ(quad_state, quad->shared_quad_state);
}

}  // namespace
}  // namespace viz
