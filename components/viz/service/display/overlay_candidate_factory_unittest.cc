// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/display_resource_provider_null.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gfx/video_types.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

class OverlayCandidateFactoryTestBase : public testing::Test {
 protected:
  void SetUp() override {
    scoped_refptr<ContextProvider> child_context_provider =
        TestContextProvider::Create();

    child_context_provider->BindToCurrentSequence();

    auto resource = TransferableResource::MakeGpu(
        gpu::Mailbox::GenerateForSharedImage(), GL_LINEAR, GL_TEXTURE_2D,
        gpu::SyncToken(), gfx::Size(1, 1), ResourceFormat::RGBA_8888, true);

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
    overlay_resource_id_ = resource_map[list[0].id];
  }

  void TearDown() override {
    child_resource_provider_.ReleaseAllExportedResources(true);
  }

  OverlayCandidateFactory CreateCandidateFactory(
      const AggregatedRenderPass& render_pass,
      const gfx::RectF& primary_rect,
      bool has_clip_support = true,
      bool has_arbitrary_transform_support = true) {
    return OverlayCandidateFactory(
        &render_pass, &resource_provider_, &surface_damage_list_, &identity_,
        primary_rect, /*is_delegated_context=*/true, has_clip_support,
        has_arbitrary_transform_support);
  }

  ResourceId overlay_resource_id_;
  ClientResourceProvider child_resource_provider_;
  DisplayResourceProviderNull resource_provider_;
  SurfaceDamageRectList surface_damage_list_;
  SkM44 identity_;
};

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
    float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
    quad.SetNew(sqs, quad_rect, quad_rect, false, overlay_resource_id_, false,
                gfx::PointF(), gfx::PointF(1, 1), SkColors::kTransparent,
                vertex_opacity, false, false, false,
                gfx::ProtectedVideoType::kClear);

    return quad;
  }
};

// Check that even axis-aligned transforms are stored separately from the
// display rect.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       AxisAlignedNotBakedIntoDisplayRect) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());
  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect));

  gfx::Transform transform;
  transform.Translate(1, 2);
  transform.Scale(3, 4);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
  EXPECT_EQ(absl::get<gfx::Transform>(candidate.transform), transform);
  EXPECT_EQ(candidate.display_rect, gfx::RectF(0, 0, 1, 1));
}

// Check that even arbitrary transforms are preserved on the overlay candidate.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest, SupportsNonAxisAligned) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());
  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect));

  gfx::Transform transform;
  transform.Rotate(1);
  transform.Skew(2, 3);
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
  EXPECT_EQ(absl::get<gfx::Transform>(candidate.transform), transform);
  EXPECT_EQ(candidate.display_rect, gfx::RectF(0, 0, 1, 1));
}

// Check that we include the Y-flip state with our arbitrary transform since we
// don't include it on the gfx::OverlayTransform in this case.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest, TransformIncludesYFlip) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());
  OverlayCandidateFactory factory =
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect));

  gfx::Transform transform;
  auto quad = CreateUnclippedDrawQuad(render_pass, gfx::Rect(1, 1), transform);
  quad.y_flipped = true;

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus result =
      factory.FromDrawQuad(&quad, candidate);
  ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);

  gfx::Transform transform_y_flipped;
  transform_y_flipped.Translate(0, 1);
  transform_y_flipped.Scale(1, -1);
  EXPECT_EQ(absl::get<gfx::Transform>(candidate.transform),
            transform_y_flipped);
  gfx::PointF display_rect_origin =
      absl::get<gfx::Transform>(candidate.transform)
          .MapPoint(candidate.display_rect.origin());
  EXPECT_EQ(display_rect_origin, gfx::PointF(0, 1));
  EXPECT_EQ(candidate.display_rect, gfx::RectF(0, 0, 1, 1));
}

#if DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST)
// Check that OverlayCandidateFactory isn't changed to allow for arbitrary
// transform support when clip support is not available. Such a configuration
// would likely be incorrect since clip rects are generally provided in target
// space and cannot be baked into the display rect when there is an arbitrary
// transform in between.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest, DeathOnNoClipSupport) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());
  EXPECT_DEATH(
      CreateCandidateFactory(render_pass, gfx::RectF(render_pass.output_rect),
                             false, true),
      "supports_clip_rect_ \\|\\| !supports_arbitrary_transform_");
}

// Resource-less overlays use the overlay quad in target space for damage
// calculation. This doesn't make sense with arbitrary transforms, so we expect
// a DCHECK to trip.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       DeathOnResourcelessAndArbitraryTransform) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 2, 2), gfx::Rect(0, 0, 1, 1),
                     gfx::Transform());
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), true, true);

  SharedQuadState* sqs = render_pass.CreateAndAppendSharedQuadState();
  sqs->quad_to_target_transform.Rotate(1);

  SolidColorDrawQuad quad;
  quad.SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1), SkColors::kRed,
              true);
  OverlayCandidate candidate;
  EXPECT_DEATH(factory.FromDrawQuad(&quad, candidate),
               "absl::holds_alternative<gfx::OverlayTransform>");
}
#endif

// Check that a factory fails to promote a quad with a non-axis-aligned
// transform when it doesn't support arbitrary transforms.
TEST_F(OverlayCandidateFactoryArbitraryTransformTest,
       NoArbitraryTransformSupportFails) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), true, false);

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
  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), true, false);

  QuadList quad_list;
  AggregatedRenderPassDrawQuad* rpdq =
      quad_list.AllocateAndConstruct<AggregatedRenderPassDrawQuad>();
  rpdq->SetNew(render_pass.CreateAndAppendSharedQuadState(),
               gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 1, 1, 1), render_pass_id,
               kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(0, 0), gfx::RectF(), false,
               1.0);

  base::flat_map<AggregatedRenderPassId, cc::FilterOperations*> filter_map;
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
  surface_damage_list_.push_back(gfx::Rect(1, 1, 1, 1));

  OverlayCandidateFactory factory = CreateCandidateFactory(
      render_pass, gfx::RectF(render_pass.output_rect), true, true);

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
    QuadList quad_list;
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
    float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
    quad.SetNew(sqs, quad_rect, quad_rect, false, overlay_resource_id_, false,
                quad_uv_rect.origin(), quad_uv_rect.bottom_right(),
                SkColors::kTransparent, vertex_opacity, false, false, false,
                gfx::ProtectedVideoType::kClear);

    return quad;
  }

  void RunClipToTopLeftCornerTest(gfx::OverlayTransform overlay_transform,
                                  gfx::RectF quad_uvs,
                                  gfx::RectF expected_uvs) {
    AggregatedRenderPass render_pass;
    gfx::Rect bounds(100, 100);
    render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1), bounds,
                       gfx::Rect(), gfx::Transform());
    // Create a factory without clip rect or arbitrary transform delegation, so
    // that any clips will be baked into the candidate.
    OverlayCandidateFactory factory = CreateCandidateFactory(
        render_pass, gfx::RectF(render_pass.output_rect), false, false);

    // |transform| maps the rect (0,0 1x1) to (50,50 100x100).
    gfx::Transform transform =
        gfx::OverlayTransformToTransform(overlay_transform, gfx::SizeF(1, 1));
    transform.PostScale(100, 100);
    transform.PostTranslate(50, 50);
    auto quad = CreateClippedDrawQuad(render_pass, gfx::Rect(1, 1), transform,
                                      bounds, quad_uvs);

    OverlayCandidate candidate;
    OverlayCandidate::CandidateStatus result =
        factory.FromDrawQuad(&quad, candidate);
    ASSERT_EQ(result, OverlayCandidate::CandidateStatus::kSuccess);
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
  RunClipToTopLeftCornerTest(gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_90,
                             gfx::RectF(1, 1),
                             gfx::RectF(0.0f, 0.5f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, Rotate180) {
  // If the candidate is rotated by 180 degrees, the top-left corner of the quad
  // corresponds to the bottom-right corner in UV space.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_180, gfx::RectF(1, 1),
      gfx::RectF(0.5f, 0.5f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, Rotate270) {
  // If the candidate is rotated by 270 degrees, the top-left corner of the quad
  // corresponds to the top-right corner in UV space.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_270, gfx::RectF(1, 1),
      gfx::RectF(0.5f, 0.0f, 0.5f, 0.5f));
}

TEST_F(TransformedOverlayClipRectTest, ClippedUvs) {
  // Check that the clip is calculated correctly if the candidate's |uv_rect| is
  // not full size, and offset from the origin.
  RunClipToTopLeftCornerTest(
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_180,
      gfx::RectF(0.1f, 0.2f, 0.4f, 0.4f), gfx::RectF(0.3f, 0.4f, 0.2f, 0.2f));
}

}  // namespace
}  // namespace viz
