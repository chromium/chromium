// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_factory.h"

#include <unordered_map>
#include <vector>

#include "base/dcheck_is_on.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/display_resource_provider_null.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gfx/video_types.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

using RoundedDisplayMasksInfo = TextureDrawQuad::RoundedDisplayMasksInfo;

OverlayCandidateFactory::OverlayContext GetOverlayContextForTesting() {
  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.supports_clip_rect = true;
  context.supports_out_of_window_clip_rect = true;
  context.supports_arbitrary_transform = true;
  context.supports_rounded_display_masks = false;
  return context;
}

// TODO(zoraiznaeem): Move resource creation code into OverlayTestBase class.
class OverlayCandidateFactoryTestBase : public testing::Test {
 public:
  OverlayCandidateFactoryTestBase() = default;

  OverlayCandidateFactoryTestBase(const OverlayCandidateFactoryTestBase&) =
      delete;
  OverlayCandidateFactoryTestBase& operator=(
      const OverlayCandidateFactoryTestBase&) = delete;

  ~OverlayCandidateFactoryTestBase() override = default;

 protected:
  void TearDown() override {
    child_resource_provider_.ReleaseAllExportedResources(true);
  }

  ResourceId CreateResource(bool is_overlay_candidate) {
    scoped_refptr<RasterContextProvider> child_context_provider =
        TestContextProvider::Create();

    child_context_provider->BindToCurrentSequence();

    auto resource = TransferableResource::MakeGpu(
        gpu::Mailbox::Generate(), GL_TEXTURE_2D, gpu::SyncToken(),
        gfx::Size(1, 1), SinglePlaneFormat::kRGBA_8888, is_overlay_candidate);

    ResourceId resource_id =
        child_resource_provider_.ImportResource(resource, base::DoNothing());

    int child_id =
        resource_provider_.CreateChild(base::DoNothing(), SurfaceId());

    // Transfer resource to the parent.
    std::vector<ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource_id);
    std::vector<TransferableResource> list;
    child_resource_provider_.PrepareSendToParent(
        resource_ids_to_transfer, &list, child_context_provider.get());
    resource_provider_.ReceiveFromChild(child_id, list);

    // Delete it in the child so it won't be leaked, and will be released once
    // returned from the parent.
    child_resource_provider_.RemoveImportedResource(resource_id);

    // In DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        resource_provider_.GetChildToParentMap(child_id);

    return resource_map[list[0].id];
  }

  OverlayCandidateFactory CreateCandidateFactory(
      const AggregatedRenderPass& render_pass,
      const gfx::RectF& primary_rect,
      const OverlayCandidateFactory::OverlayContext& context) {
    return OverlayCandidateFactory(
        &render_pass, &resource_provider_, &surface_damage_list_, &identity_,
        primary_rect, &render_pass_filters_, context);
  }

  void RunRoundedCornerTest(bool disable_wire_size_optimization) {
    AggregatedRenderPass render_pass;
    render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                       gfx::Rect(0, 0, 100, 100), gfx::Rect(),
                       gfx::Transform());

    // We're creating a tile layer, 3 columns by 2 rows, with a rounded corner
    // affecting the outer tiles. This is a case similar to the open omnibox.
    gfx::Rect layer_rect = gfx::Rect(0, 0, 90, 60);
    gfx::RRectF rounded_corners = gfx::RRectF(gfx::RectF(layer_rect), 10);
    gfx::Rect tile_rects[] = {
        // Row 1
        gfx::Rect(0, 0, 30, 30),
        gfx::Rect(30, 0, 30, 30),
        gfx::Rect(60, 0, 30, 30),
        // Row 2
        gfx::Rect(0, 30, 30, 30),
        gfx::Rect(30, 30, 30, 30),
        gfx::Rect(60, 30, 30, 30),
    };

    SharedQuadState* sqs = render_pass.CreateAndAppendSharedQuadState();
    sqs->SetAll(
        /*transform=*/gfx::Transform(), /*layer_rect=*/layer_rect,
        /*visible_layer_rect=*/layer_rect,
        /*filter_info=*/gfx::MaskFilterInfo(rounded_corners),
        /*clip=*/std::nullopt,
        /*contents_opaque=*/true,
        /*opacity_f=*/1.f,
        /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);

    for (const auto& tile_rect : tile_rects) {
      SolidColorDrawQuad* solid_quad =
          render_pass.CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      solid_quad->SetNew(sqs, /*rect=*/tile_rect, /*visible_rect=*/tile_rect,
                         SkColors::kBlack,
                         /*anti_aliasing_off=*/false);
    }

    OverlayCandidateFactory::OverlayContext context;
    context.is_delegated_context = true;
    context.disable_wire_size_optimization = disable_wire_size_optimization;
    context.supports_clip_rect = true;
    context.supports_arbitrary_transform = true;
    context.supports_mask_filter = true;
    OverlayCandidateFactory factory = CreateCandidateFactory(
        render_pass, gfx::RectF(render_pass.output_rect), context);

    OverlayCandidateList candidates;
    for (const auto* quad : render_pass.quad_list) {
      OverlayCandidate candidate;
      OverlayCandidate::CandidateStatus status =
          factory.FromDrawQuad(quad, candidate);
      ASSERT_EQ(status, OverlayCandidate::CandidateStatus::kSuccess);
      candidates.push_back(candidate);
    }

    // We expect the outer quads that intersect the rounded corner mask will
    // always have rounded corners.
    EXPECT_FALSE(candidates[0].rounded_corners.IsEmpty());
    EXPECT_FALSE(candidates[2].rounded_corners.IsEmpty());
    EXPECT_FALSE(candidates[3].rounded_corners.IsEmpty());
    EXPECT_FALSE(candidates[5].rounded_corners.IsEmpty());

    // We expect the inner quads that do not intersect with the rounded corner
    // mask to optionally have it set.
    const bool expect_rounded_corner_on_inner_quads =
        disable_wire_size_optimization;
    EXPECT_EQ(!candidates[1].rounded_corners.IsEmpty(),
              expect_rounded_corner_on_inner_quads);
    EXPECT_EQ(!candidates[4].rounded_corners.IsEmpty(),
              expect_rounded_corner_on_inner_quads);
  }

  ClientResourceProvider child_resource_provider_;
  DisplayResourceProviderNull resource_provider_;
  SurfaceDamageRectList surface_damage_list_;
  SkM44 identity_;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters_;
};

SolidColorDrawQuad* AddQuad(const gfx::Rect quad_rect,
                            const gfx::Transform& quad_to_target_transform,
                            AggregatedRenderPass* render_pass,
                            const std::optional<gfx::Rect> clip_rect,
                            const gfx::Rect visible_rect) {
  SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

  quad_state->SetAll(
      /*transform=*/quad_to_target_transform, quad_rect,
      /*visible_layer_rect=*/quad_rect,
      /*filter_info=*/gfx::MaskFilterInfo(), clip_rect,
      /*are contents opaque=*/true,
      /*opacity_f=*/1.f,
      /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
      /*fast_rounded_corner=*/false);

  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, quad_rect, visible_rect, SkColors::kBlack,
                     false /* force_anti_aliasing_off */);
  return solid_quad;
}

SolidColorDrawQuad* AddQuad(const gfx::Rect quad_rect,
                            const gfx::Transform& quad_to_target_transform,
                            AggregatedRenderPass* render_pass) {
  return AddQuad(quad_rect, quad_to_target_transform, render_pass, std::nullopt,
                 quad_rect);
}

AggregatedRenderPassDrawQuad* AddRenderPassQuad(
    gfx::Rect quad_rect,
    gfx::Transform transform,
    std::optional<gfx::Rect> clip_rect,
    AggregatedRenderPassId rpid,
    AggregatedRenderPass* render_pass) {
  SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

  quad_state->SetAll(
      /*transform=*/transform, quad_rect,
      /*visible_layer_rect=*/quad_rect,
      /*filter_info=*/gfx::MaskFilterInfo(),
      /*clip=*/clip_rect,
      /*are contents opaque=*/true,
      /*opacity_f=*/1.f,
      /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
      /*fast_rounded_corner=*/false);

  auto* rpdq =
      render_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  rpdq->SetNew(quad_state, quad_rect, quad_rect, rpid, kInvalidResourceId,
               gfx::RectF(), gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(),
               gfx::RectF(), false, 1.0f);
  return rpdq;
}

OverlayCandidate CreateCandidate(float left,
                                 float top,
                                 float right,
                                 float bottom) {
  OverlayCandidate candidate;
  candidate.display_rect.SetRect(left, top, right - left, bottom - top);
  return candidate;
}

using OverlayCandidateFactoryTest = OverlayCandidateFactoryTestBase;

TEST_F(OverlayCandidateFactoryTest, IsOccluded) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());
  gfx::Transform identity;
  identity.MakeIdentity();

  // Create overlapping quads around 1,1 - 10,10.
  AddQuad(gfx::Rect(0, 0, 1, 10), identity, &render_pass);
  AddQuad(gfx::Rect(0, 0, 10, 1), identity, &render_pass);
  AddQuad(gfx::Rect(10, 0, 1, 10), identity, &render_pass);
  AddQuad(gfx::Rect(0, 10, 10, 1), identity, &render_pass);

  EXPECT_FALSE(factory.IsOccluded(CreateCandidate(0.5f, 0.5f, 10.49f, 10.49f),
                                  render_pass.quad_list.begin(),
                                  render_pass.quad_list.end()));

  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(0.49f, 0.5f, 10.49f, 10.49f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));

  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(0.5f, 0.49f, 10.50f, 10.5f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));
  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(0.5f, 0.5f, 10.5f, 10.49f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));

  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(0.5f, 0.5f, 10.49f, 10.5f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));
}

TEST_F(OverlayCandidateFactoryTest, IsOccludedScaled) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Scale(1.6, 1.6);

  // Create overlapping quads around 1.6,2.4 - 14.4,17.6.
  AddQuad(gfx::Rect(0, 0, 1, 10), quad_to_target_transform, &render_pass);
  AddQuad(gfx::Rect(0, 0, 10, 2), quad_to_target_transform, &render_pass);
  AddQuad(gfx::Rect(9, 0, 1, 10), quad_to_target_transform, &render_pass);
  AddQuad(gfx::Rect(0, 11, 10, 1), quad_to_target_transform, &render_pass);

  EXPECT_FALSE(factory.IsOccluded(CreateCandidate(2.f, 3.f, 14.f, 17.f),
                                  render_pass.quad_list.begin(),
                                  render_pass.quad_list.end()));
  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(1.f, 3.f, 14.f, 17.f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));
  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(2.f, 2.f, 14.f, 17.f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));
  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(2.f, 3.f, 15.f, 17.f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));
  EXPECT_TRUE(factory.IsOccluded(CreateCandidate(2.f, 3.f, 15.f, 18.f),
                                 render_pass.quad_list.begin(),
                                 render_pass.quad_list.end()));
}

TEST_F(OverlayCandidateFactoryTest, RoundedCorner) {
  RunRoundedCornerTest(/*disable_wire_size_optimization=*/true);
}

TEST_F(OverlayCandidateFactoryTest, RoundedCornerOptimizedwireSize) {
  RunRoundedCornerTest(/*disable_wire_size_optimization=*/false);
}

class OverlayCandidateFactoryArbitraryTransformTest
    : public OverlayCandidateFactoryTestBase {
 protected:
  TextureDrawQuad CreateUnclippedDrawQuad(
      AggregatedRenderPass& render_pass,
      const gfx::Rect& quad_rect,
      const gfx::Transform& quad_to_target_transform) {
    SharedQuadState* sqs = render_pass.CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform = quad_to_target_transform;
    TextureDrawQuad quad;
    quad.SetNew(sqs, quad_rect, quad_rect, false,
                CreateResource(/*is_overlay_candidate=*/true), false,
                gfx::PointF(), gfx::PointF(1, 1), SkColors::kTransparent, false,
                false, false, gfx::ProtectedVideoType::kClear);

    return quad;
  }
};

// Check that axis-aligned transforms are stored as OverlayTransforms when
// possible.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       AxisAlignedNotBakedIntoDisplayRect) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 10, 10), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());

  gfx::Transform transform;
  transform.Translate(1, 2);
  transform.Scale(3, 4);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
  ASSERT_TRUE(
      absl::holds_alternative<gfx::OverlayTransform>(candidate.transform));
  EXPECT_EQ(absl::get<gfx::OverlayTransform>(candidate.transform),
            gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE);
  EXPECT_EQ(candidate.display_rect, gfx::RectF(1, 2, 3, 4));
}

// Check that even arbitrary transforms are preserved on the overlay
// candidate.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest, SupportsNonAxisAligned) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());

  gfx::Transform transform;
  transform.Rotate(1);
  transform.Skew(2, 3);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
  ASSERT_TRUE(absl::holds_alternative<gfx::Transform>(candidate.transform));
  EXPECT_EQ(absl::get<gfx::Transform>(candidate.transform), transform);
  EXPECT_EQ(candidate.display_rect, gfx::RectF(0, 0, 1, 1));
}

// Check that we include the Y-flip state with our arbitrary transform since
// we don't include it on the gfx::OverlayTransform in this case.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest, TransformIncludesYFlip) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());

  gfx::Transform transform;
  // Use a non-axis aligned transform so it can't be converted to an
  // OverlayTransform.
  transform.SkewX(45.0);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);
  quad.y_flipped = true;

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);

  gfx::Transform transform_y_flipped;
  transform_y_flipped.SkewX(45.0);
  transform_y_flipped.Translate(0, 1);
  transform_y_flipped.Scale(1, -1);
  ASSERT_TRUE(absl::holds_alternative<gfx::Transform>(candidate.transform));
  EXPECT_EQ(absl::get<gfx::Transform>(candidate.transform),
            transform_y_flipped);
  gfx::PointF display_rect_origin =
      absl::get<gfx::Transform>(candidate.transform)
          .MapPoint(candidate.display_rect.origin());
  // Flip moves the origin to 0,1. The skew slides it out to 1,1.
  EXPECT_EQ(display_rect_origin, gfx::PointF(1, 1));
  EXPECT_EQ(candidate.display_rect, gfx::RectF(0, 0, 1, 1));
}

TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       UseArbitraryTransformWhenSupported) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.disable_wire_size_optimization = true;
  context.supports_clip_rect = true;
  context.supports_arbitrary_transform = true;
  context.supports_mask_filter = true;
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  gfx::Transform transform = gfx::Transform::MakeTranslation(1, 1);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);

  EXPECT_EQ(candidate.display_rect, gfx::RectF(0, 0, 1, 1));
  ASSERT_TRUE(absl::holds_alternative<gfx::Transform>(candidate.transform));
  EXPECT_EQ(absl::get<gfx::Transform>(candidate.transform), transform);
}

TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       UseOverlayTransformWhenPossibleForWireSize) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 2, 2), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.disable_wire_size_optimization = false;
  context.supports_clip_rect = true;
  context.supports_arbitrary_transform = true;
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  gfx::Transform transform = gfx::Transform::MakeTranslation(0.5, 0.5);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);

  EXPECT_EQ(candidate.display_rect, gfx::RectF(0.5, 0.5, 1, 1));
  ASSERT_TRUE(
      absl::holds_alternative<gfx::OverlayTransform>(candidate.transform));
  EXPECT_EQ(absl::get<gfx::OverlayTransform>(candidate.transform),
            gfx::OVERLAY_TRANSFORM_NONE);
}

TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       Allow3DTransformNoPerspective) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 10, 10), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());

  gfx::Transform transform;
  transform.RotateAboutXAxis(5);
  transform.RotateAboutYAxis(5);
  transform.RotateAboutZAxis(5);

  EXPECT_TRUE(!transform.HasPerspective());

  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
}

// Checks that a transform that preserve the flatness of quads on the XY-plane,
// but not necessarily without perspective are still allowed to be promoted.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       TechnicallyFlatTransform) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 10, 10), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());

  gfx::Transform transform = gfx::Transform::ColMajor(1, 0.1, 0, 0,      //
                                                      0.1, 1, 0, 0,      //
                                                      0.1, 0.1, 2, 0.1,  //
                                                      0.1, 0.1, 0, 1);

  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
}

#if DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST)
class OverlayCandidateFactoryInvalidContextTest
    : public OverlayCandidateFactoryTestBase {
 protected:
  void CheckContext(const OverlayCandidateFactory::OverlayContext& context,
                    const char* expected_assertion) {
    AggregatedRenderPass render_pass;
    render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                       gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());
    EXPECT_DEATH(CreateCandidateFactory(
                     render_pass, gfx::RectF(render_pass.output_rect), context),
                 expected_assertion);
  }
};

// Check that OverlayCandidateFactory isn't changed to allow for arbitrary
// transform support when clip support is not available. Such a configuration
// would likely be incorrect since clip rects are generally provided in target
// space and cannot be baked into the display rect when there is an arbitrary
// transform in between.
TEST_F(OverlayCandidateFactoryInvalidContextTest, NoClipSupport) {
  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.supports_clip_rect = false;
  context.supports_out_of_window_clip_rect = false;
  context.supports_arbitrary_transform = true;
  CheckContext(context,
               "context_.supports_clip_rect \\|\\| "
               "!context_.supports_arbitrary_transform");
}

// All quads have a transform on their |sqs|, we need to support some way to
// store it on our OverlayCandidates. This test checks that an
// OverlayCandidateFactory without transform support is invalid.
TEST_F(OverlayCandidateFactoryInvalidContextTest, NoTransformSupport) {
  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.disable_wire_size_optimization = true;
  context.supports_arbitrary_transform = false;
  CheckContext(context,
               "!context_.disable_wire_size_optimization \\|\\| "
               "context_.supports_arbitrary_transform");
}
#endif

// Check that a factory fails to promote a quad with a non-axis-aligned
// transform when it doesn't support arbitrary transforms.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       NoArbitraryTransformSupportFails) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.supports_clip_rect = true;
  context.supports_arbitrary_transform = false;

  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  gfx::Transform transform;
  transform.Rotate(1);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);
  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result,
            OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dRotation);
}

TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       OccludedByFilteredQuadWorksInTargetSpace) {
  AggregatedRenderPassId render_pass_id =
      AggregatedRenderPassId::FromUnsafeValue(1);
  AggregatedRenderPass render_pass;
  render_pass.SetNew(render_pass_id, gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                     gfx::Transform());

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.supports_clip_rect = true;
  context.supports_arbitrary_transform = false;

  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  QuadList quad_list;
  AggregatedRenderPassDrawQuad* rpdq =
      quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
  rpdq->SetNew(render_pass.CreateAndAppendSharedQuadState(),
               gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 1, 1, 1), render_pass_id,
               kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(0, 0), gfx::RectF(), false,
               1.0);

  base::flat_map<AggregatedRenderPassId,
                 raw_ptr<cc::FilterOperations, CtnExperimental>>
      filter_map;
  // The actual filter operation doesn't matter in this case.
  cc::FilterOperations filter_op;
  filter_map.insert({render_pass_id, &filter_op});

  // Check that an untransformed 1x1 quad doesn't intersect with the filtered
  // RPDQ.
  {
    gfx::Transform transform;
    auto quad =
        CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

    OverlayCandidate candidate;
    OverlayCandidate::CandidateStatus result =
        factory.FromDrawQuad(&quad, candidate);
    ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
    EXPECT_FALSE(factory.IsOccludedByFilteredQuad(candidate, quad_list.begin(),
                                                  quad_list.end(), filter_map));
  }

  // Check that a transformed 1x1 quad intersects with the filtered RPDQ.
  {
    gfx::Transform transform;
    transform.Translate(0.5, 0.5);
    auto quad =
        CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

    OverlayCandidate candidate;
    OverlayCandidate::CandidateStatus result =
        factory.FromDrawQuad(&quad, candidate);
    ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
    EXPECT_TRUE(factory.IsOccludedByFilteredQuad(candidate, quad_list.begin(),
                                                 quad_list.end(), filter_map));
  }
}

TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       UnassignedDamageWithArbitraryTransforms) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 2, 2), gfx::Rect(), gfx::Transform());

  // Add damage so that the factory has unassigned surface damage internally.
  surface_damage_list_.emplace_back(1, 1, 1, 1);

  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             GetOverlayContextForTesting());

  // Make a rotated quad which doesn't intersect with the damage, but the
  // axis-aligned bounding box of its target space rect does. This rect should
  // not get any damage.
  {
    gfx::Transform transform;
    transform.Translate(0, -1);
    transform.Rotate(-45);
    auto quad =
        CreateUnclippedDrawQuad(render_pass, gfx::Rect(2, 2), transform);

    OverlayCandidate candidate;
    OverlayCandidate::CandidateStatus result =
        factory.FromDrawQuad(&quad, candidate);
    ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
    EXPECT_TRUE(candidate.damage_rect.IsEmpty());
    QuadList quad_list;
    EXPECT_EQ(factory.EstimateVisibleDamage(&quad, candidate, quad_list.begin(),
                                            quad_list.end()),
              0);
  }

  // Ensure when that same rect does intersect with the damage picks up damage.
  {
    gfx::Transform transform;
    transform.Rotate(-45);
    auto quad =
        CreateUnclippedDrawQuad(render_pass, gfx::Rect(2, 2), transform);

    OverlayCandidate candidate;
    OverlayCandidate::CandidateStatus result =
        factory.FromDrawQuad(&quad, candidate);
    ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
    // Damage should not be assigned to transformed quads.
    EXPECT_TRUE(candidate.damage_rect.IsEmpty());
    QuadList quad_list;
    // But we can still estimate damage for sorting purposes.
    EXPECT_GT(factory.EstimateVisibleDamage(&quad, candidate, quad_list.begin(),
                                            quad_list.end()),
              0);
  }
}

constexpr float kEpsilon = 0.001f;

// Check that uv clips are applied correctly when the candidate is transformed.
class TransformedOverlayClipRectTest : public OverlayCandidateFactoryTestBase {
 protected:
  TextureDrawQuad CreateClippedDrawQuad(
      AggregatedRenderPass& render_pass,
      const gfx::Rect& quad_rect,
      const gfx::Transform& quad_to_target_transform,
      const gfx::Rect& clip_rect,
      const gfx::RectF& quad_uv_rect) {
    SharedQuadState* sqs = render_pass.CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform = quad_to_target_transform;
    sqs->clip_rect = clip_rect;
    TextureDrawQuad quad;
    quad.SetNew(sqs, quad_rect, quad_rect, false,
                CreateResource(/*is_overlay_candidate=*/true), false,
                quad_uv_rect.origin(), quad_uv_rect.bottom_right(),
                SkColors::kTransparent, false, false, false,
                gfx::ProtectedVideoType::kClear);

    return quad;
  }

  void RunClipToTopLeftCornerTest(gfx::OverlayTransform overlay_transform,
                                  gfx::RectF quad_uvs,
                                  gfx::RectF expected_uvs,
                                  bool needs_detiling = false) {
    AggregatedRenderPass render_pass;
    gfx::Rect bounds(100, 100);
    render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1), bounds,
                       gfx::Rect(), gfx::Transform());

    // Create a factory without clip rect or arbitrary transform delegation, so
    // that any clips will be baked into the candidate.
    OverlayCandidateFactory::OverlayContext context;
    context.is_delegated_context = true;
    OverlayCandidateFactory factory = CreateCandidateFactory(
        render_pass, gfx::RectF(render_pass.output_rect), context);

    // |transform| maps the rect (0,0 1x1) to (50,50 100x100).
    gfx::Transform transform =
        gfx::OverlayTransformToTransform(overlay_transform, gfx::SizeF(1, 1));
    transform.PostScale(100, 100);
    transform.PostTranslate(50, 50);
    auto quad = CreateClippedDrawQuad(render_pass, gfx::Rect(1, 1), transform,
                                      bounds, quad_uvs);

    OverlayCandidate candidate;
    candidate.needs_detiling = needs_detiling;
    OverlayCandidate::CandidateStatus result =
        factory.FromDrawQuad(&quad, candidate);
    ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
    ASSERT_TRUE(
        absl::holds_alternative<gfx::OverlayTransform>(candidate.transform));
    EXPECT_EQ(absl::get<gfx::OverlayTransform>(candidate.transform),
              overlay_transform);
    EXPECT_EQ(candidate.display_rect, gfx::RectF(50, 50, 50, 50));
    EXPECT_TRUE(
        candidate.uv_rect.ApproximatelyEqual(expected_uvs, kEpsilon, kEpsilon));
  }
};

TEST_F(TransformedOverlayClipRectTest, NoTransform) {
  RunClipToTopLeftCornerTest(gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
                             gfx::RectF(1, 1),
                             gfx::RectF(0.0f, 0.0f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, Rotate90) {
  // If the candidate is rotated by 90 degrees, the top-left corner of the quad
  // corresponds to the bottom-left corner in UV space.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
      gfx::RectF(1, 1), gfx::RectF(0.0f, 0.5f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, Rotate180) {
  // If the candidate is rotated by 180 degrees, the top-left corner of the quad
  // corresponds to the bottom-right corner in UV space.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
      gfx::RectF(1, 1), gfx::RectF(0.5f, 0.5f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, Rotate270) {
  // If the candidate is rotated by 270 degrees, the top-left corner of the quad
  // corresponds to the top-right corner in UV space.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
      gfx::RectF(1, 1), gfx::RectF(0.5f, 0.0f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, ClippedUvs) {
  // Check that the clip is calculated correctly if the candidate's |uv_rect| is
  // not full size, and offset from the origin.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
      gfx::RectF(0.1f, 0.2f, 0.4f, 0.4f), gfx::RectF(0.3f, 0.4f, 0.2f, 0.2f));
}

// Test to make sure we handle overlays that need detiling and have a rotation
// correctly. The UV rect of these overlays assumes no rotation, so we have to
// rotate them before applying the clip.
TEST_F(TransformedOverlayClipRectTest, NoTransformNeedsDetiling) {
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, gfx::RectF(0.7f, 0.6f),
      gfx::RectF(0.0f, 0.0f, 0.35f, 0.3f), /*needs_detiling=*/true);
}

TEST_F(TransformedOverlayClipRectTest, Rotate90NeedsDetiling) {
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
      gfx::RectF(0.7f, 0.6f), gfx::RectF(0.0f, 0.3f, 0.35f, 0.3f),
      /*needs_detiling=*/true);
}

TEST_F(TransformedOverlayClipRectTest, Rotate180NeedsDetiling) {
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
      gfx::RectF(0.7f, 0.6f), gfx::RectF(0.35f, 0.3f, 0.35f, 0.3f),
      /*needs_detiling=*/true);
}

TEST_F(TransformedOverlayClipRectTest, Rotate270NeedsDetiling) {
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
      gfx::RectF(0.7f, 0.6f), gfx::RectF(0.35f, 0.0f, 0.35f, 0.3f),
      /*needs_detiling=*/true);
}

TEST_F(OverlayCandidateFactoryTest, RenderPassClipped) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 100, 100), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  // Entirely clipped
  gfx::Rect clip_rect(0, 0);
  AggregatedRenderPassId rpid(2);
  auto* rpdq = AddRenderPassQuad(gfx::Rect(100, 100), gfx::Transform(),
                                 clip_rect, rpid, &render_pass);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(rpdq, candidate);

  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kFailVisible);
}

TEST_F(OverlayCandidateFactoryTest, RenderPassOffscreen) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 100, 100), gfx::Rect(), gfx::Transform());

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  AggregatedRenderPassId rpid(2);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(0, 101));
  auto* rpdq = AddRenderPassQuad(gfx::Rect(100, 100), transform, std::nullopt,
                                 rpid, &render_pass);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(rpdq, candidate);

  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kFailVisible);
}

TEST_F(OverlayCandidateFactoryTest, RenderPassOffscreenBeforeFilter) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 100, 100), gfx::Rect(), gfx::Transform());

  // Add a blur to this render pass that expands it's bounds into the viewport.
  auto blur = cc::FilterOperation::CreateBlurFilter(10.0f);
  cc::FilterOperations filter_ops;
  filter_ops.Append(blur);
  AggregatedRenderPassId rpid(2);
  render_pass_filters_[rpid] = &filter_ops;

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(0, 101));
  auto* rpdq = AddRenderPassQuad(gfx::Rect(100, 100), transform, std::nullopt,
                                 rpid, &render_pass);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(rpdq, candidate);

  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
}

TEST_F(OverlayCandidateFactoryTest, ClipDelegation_Success) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 100, 100), gfx::Rect(), gfx::Transform());
  gfx::Rect rect(0, 0, 75, 75);
  gfx::Rect clip(0, 0, 50, 50);
  gfx::Transform identity;
  auto* quad = AddQuad(rect, identity, &render_pass, clip, rect);

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  OverlayCandidateFactory noclip_factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);
  context.supports_clip_rect = true;
  OverlayCandidateFactory clip_factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  OverlayCandidate no_clip_cand;
  OverlayCandidate clip_cand;
  ASSERT_EQ(noclip_factory.FromDrawQuad(quad, no_clip_cand),
            OverlayCandidate::CandidateStatus::kSuccess);
  ASSERT_EQ(clip_factory.FromDrawQuad(quad, clip_cand),
            OverlayCandidate::CandidateStatus::kSuccess);

  // Clip rect can be delegated if supported.
  EXPECT_RECTF_EQ(no_clip_cand.display_rect, gfx::RectF(clip));
  EXPECT_FALSE(no_clip_cand.clip_rect.has_value());
  EXPECT_RECTF_EQ(clip_cand.display_rect, gfx::RectF(rect));
  EXPECT_EQ(clip_cand.clip_rect.value(), clip);
}

TEST_F(OverlayCandidateFactoryTest, ClipDelegation_OutOfWindow) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 100, 100), gfx::Rect(), gfx::Transform());
  constexpr gfx::Rect kRect(0, 0, 75, 75);
  constexpr gfx::Rect kClip(0, 0, 50, 50);
  // Transform up, outside the window.
  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(0, -30));
  auto* quad = AddQuad(kRect, transform, &render_pass, kClip, kRect);

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  OverlayCandidateFactory noclip_factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);
  context.supports_clip_rect = true;
  context.supports_out_of_window_clip_rect = true;
  OverlayCandidateFactory clip_factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  OverlayCandidate no_clip_cand;
  OverlayCandidate clip_cand;
  ASSERT_EQ(noclip_factory.FromDrawQuad(quad, no_clip_cand),
            OverlayCandidate::CandidateStatus::kSuccess);
  ASSERT_EQ(clip_factory.FromDrawQuad(quad, clip_cand),
            OverlayCandidate::CandidateStatus::kSuccess);

  // Clip rect can be delegated if supported.
  constexpr gfx::RectF kTransformedClip(0, 0, 50, 45);
  constexpr gfx::RectF kTransformedRect(0, -30, 75, 75);
  EXPECT_RECTF_EQ(no_clip_cand.display_rect, kTransformedClip);
  EXPECT_FALSE(no_clip_cand.clip_rect.has_value());
  EXPECT_RECTF_EQ(clip_cand.display_rect, kTransformedRect);
  EXPECT_EQ(clip_cand.clip_rect.value(), kClip);
}

TEST_F(OverlayCandidateFactoryTest, ClipDelegation_VisibleRect) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 100, 100), gfx::Rect(), gfx::Transform());
  gfx::Rect rect(0, 0, 75, 75);
  gfx::Rect clip(0, 0, 50, 50);
  // Use content clipping.
  gfx::Rect visible_rect = gfx::Rect(0, 10, 65, 65);
  gfx::Transform identity;
  auto* quad = AddQuad(rect, identity, &render_pass, clip, visible_rect);

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  OverlayCandidateFactory noclip_factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);
  context.supports_clip_rect = true;
  OverlayCandidateFactory clip_factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), context);

  OverlayCandidate no_clip_cand;
  OverlayCandidate clip_cand;
  ASSERT_EQ(noclip_factory.FromDrawQuad(quad, no_clip_cand),
            OverlayCandidate::CandidateStatus::kSuccess);
  ASSERT_EQ(clip_factory.FromDrawQuad(quad, clip_cand),
            OverlayCandidate::CandidateStatus::kSuccess);

  // Clip rect can be delegated when the quad has content clipping.
  gfx::RectF clipped2(0, 10, 50, 40);
  EXPECT_RECTF_EQ(no_clip_cand.display_rect, clipped2);
  EXPECT_FALSE(no_clip_cand.clip_rect.has_value());
  EXPECT_RECTF_EQ(clip_cand.display_rect, gfx::RectF(visible_rect));
  EXPECT_EQ(clip_cand.clip_rect.value(), clip);
}

}  // namespace
}  // namespace viz
