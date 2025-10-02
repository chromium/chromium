// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_proposed_candidate.h"

#include <tuple>
#include <unordered_map>
#include <vector>

#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display/display_resource_provider_null.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "components/viz/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using RoundedDisplayMasksInfo = TextureDrawQuad::RoundedDisplayMasksInfo;

const auto kTestQuadRect = gfx::Rect(0, 0, 100, 100);

class TestOverlayStrategy : public OverlayProcessorStrategy {
 public:
  TestOverlayStrategy() = default;

  TestOverlayStrategy(const TestOverlayStrategy&) = delete;
  TestOverlayStrategy& operator=(const TestOverlayStrategy&) = delete;

  ~TestOverlayStrategy() override = default;

  void Propose(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      std::vector<OverlayProposedCandidate>* candidates,
      std::vector<gfx::Rect>* content_bounds) override {}

  bool Attempt(
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_pass_list,
      SurfaceDamageRectList* surface_damage_rect_list,
      const PrimaryPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds,
      const OverlayProposedCandidate& proposed_candidate) override {
    return true;
  }

  void CommitCandidate(const OverlayProposedCandidate& proposed_candidate,
                       AggregatedRenderPass* render_pass) override {}
};

// TODO(zoraiznaeem): Move resource creation code into OverlayTestBase class.
class OverlayProposedCandidateTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<RoundedDisplayMasksInfo, gfx::Rect, gfx::Rect>> {
 public:
  OverlayProposedCandidateTest()
      : mask_info_(std::get<0>(GetParam())),
        expected_origin_mask_bounds_(std::get<1>(GetParam())),
        expected_other_mask_bounds_(std::get<2>(GetParam())) {}

  OverlayProposedCandidateTest(const OverlayProposedCandidateTest&) = delete;
  OverlayProposedCandidateTest& operator=(const OverlayProposedCandidateTest&) =
      delete;

  ~OverlayProposedCandidateTest() override = default;

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

  void AddQuadWithRoundedDisplayMasks(
      gfx::Rect quad_rect,
      bool is_overlay_candidate,
      const gfx::Transform& quad_to_target_transform,
      const RoundedDisplayMasksInfo& rounded_display_masks_info,
      AggregatedRenderPass* render_pass) {
    SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

    quad_state->SetAll(
        /*transform=*/quad_to_target_transform, quad_rect,
        /*visible_layer_rect=*/quad_rect,
        /*filter_info=*/gfx::MaskFilterInfo(),
        /*clip=*/std::nullopt,
        /*are contents opaque=*/true,
        /*opacity_f=*/1.f,
        /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0, /*layer_id=*/0u,
        /*fast_rounded_corner=*/false);

    TextureDrawQuad* texture_quad =
        render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    texture_quad->SetNew(
        quad_state, quad_rect, quad_rect,
        /*needs_blending=*/true, CreateResource(is_overlay_candidate),
        /*premultiplied=*/true, gfx::PointF(), gfx::PointF(),
        /*background=*/SkColors::kTransparent,
        /*flipped=*/false,
        /*nearest=*/false,
        /*secure_output=*/false, gfx::ProtectedVideoType::kClear);

    texture_quad->rounded_display_masks_info = rounded_display_masks_info;
  }

  ClientResourceProvider child_resource_provider_;
  DisplayResourceProviderNull resource_provider_;
  SurfaceDamageRectList surface_damage_list_;
  SkM44 identity_;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters_;

  RoundedDisplayMasksInfo mask_info_;
  gfx::Rect expected_origin_mask_bounds_;
  gfx::Rect expected_other_mask_bounds_;
};

TEST_P(OverlayProposedCandidateTest, CorrectRoundedDisplayMaskBounds) {
  AggregatedRenderPass render_pass;
  render_pass.SetNew(AggregatedRenderPassId::FromUnsafeValue(1),
                     gfx::Rect(0, 0, 1, 1), gfx::Rect(), gfx::Transform());

  gfx::Transform identity;
  identity.MakeIdentity();

  AddQuadWithRoundedDisplayMasks(kTestQuadRect,
                                 /*is_overlay_candidate=*/true, identity,
                                 mask_info_, &render_pass);

  OverlayCandidateFactory::OverlayContext context;
  context.supports_rounded_display_masks = true;
  OverlayCandidateFactory factory = OverlayCandidateFactory(
      &render_pass, &resource_provider_, &surface_damage_list_, &identity_,
      gfx::RectF(render_pass.output_rect), &render_pass_filters_, context);

  OverlayCandidate candidate;
  OverlayCandidateFactory::CandidateStatus status =
      factory.FromDrawQuad(*render_pass.quad_list.begin(), candidate);
  ASSERT_EQ(status, OverlayCandidateFactory::CandidateStatus::kSuccess);

  TestOverlayStrategy strategy;
  OverlayProposedCandidate proposed_candidate(render_pass.quad_list.begin(),
                                              candidate, &strategy);

  auto mask_bounds = OverlayProposedCandidate::GetRoundedDisplayMasksBounds(
      proposed_candidate);

  EXPECT_EQ(
      mask_bounds[RoundedDisplayMasksInfo::kOriginRoundedDisplayMaskIndex],
      expected_origin_mask_bounds_);
  EXPECT_EQ(mask_bounds[RoundedDisplayMasksInfo::kOtherRoundedDisplayMaskIndex],
            expected_other_mask_bounds_);
}

INSTANTIATE_TEST_SUITE_P(
    /*no_prefix*/,
    OverlayProposedCandidateTest,
    testing::Values(
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/10,
                /*other_rounded_display_mask_radius=*/15,
                /*is_horizontally_positioned=*/true),
            /*expected_origin_mask_bounds=*/gfx::Rect(0, 0, 10, 10),
            /*expected_other_mask_bounds=*/gfx::Rect(85, 0, 15, 15)),
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/10,
                /*other_rounded_display_mask_radius=*/15,
                /*is_horizontally_positioned=*/false),
            /*expected_origin_mask_bounds=*/gfx::Rect(0, 0, 10, 10),
            /*expected_other_mask_bounds=*/gfx::Rect(0, 85, 15, 15)),
        std::make_tuple(
            RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                /*origin_rounded_display_mask_radius=*/0,
                /*other_rounded_display_mask_radius=*/15,
                /*is_horizontally_positioned=*/false),
            /*expected_origin_mask_bounds=*/gfx::Rect(),
            /*expected_other_mask_bounds=*/gfx::Rect(0, 85, 15, 15)),
        std::make_tuple(RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                            /*origin_rounded_display_mask_radius=*/10,
                            /*other_rounded_display_mask_radius=*/0,
                            /*is_horizontally_positioned=*/false),
                        /*expected_origin_mask_bounds=*/gfx::Rect(0, 0, 10, 10),
                        /*expected_other_mask_bounds=*/gfx::Rect()),
        std::make_tuple(RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
                            /*origin_rounded_display_mask_radius=*/0,
                            /*other_rounded_display_mask_radius=*/0,
                            /*is_horizontally_positioned=*/false),
                        /*expected_origin_mask_bounds=*/gfx::Rect(),
                        /*expected_other_mask_bounds=*/gfx::Rect())));

}  // namespace
}  // namespace viz
