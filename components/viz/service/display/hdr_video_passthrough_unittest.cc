// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/hdr_video_passthrough.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "cc/paint/paint_flags.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/display_resource_provider_null.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"

namespace viz {
namespace {

constexpr gfx::Rect kOutputRect(0, 0, 1920, 1080);

class HdrVideoPassthroughTest : public testing::Test {
 public:
  HdrVideoPassthroughTest() = default;
  ~HdrVideoPassthroughTest() override = default;

 protected:
  void TearDown() override {
    child_resource_provider_.ReleaseAllExportedResources(true);
  }

  ResourceId CreateResource(
      const gfx::ColorSpace& color_space,
      const gfx::HDRMetadata& hdr_metadata = gfx::HDRMetadata()) {
    scoped_refptr<RasterContextProvider> child_context_provider =
        TestContextProvider::CreateGLES();
    child_context_provider->BindToCurrentSequence();

    auto resource = TransferableResource::Make(
        gpu::ClientSharedImage::CreateForTesting(
            {SinglePlaneFormat::kRGBA_1010102, gfx::Size(64, 64), color_space,
             kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
             gpu::SHARED_IMAGE_USAGE_DISPLAY_READ},
            GL_TEXTURE_2D),
        TransferableResource::ResourceSource::kTest, gpu::SyncToken());
    resource.hdr_metadata = hdr_metadata;

    ResourceId resource_id =
        child_resource_provider_.ImportResource(resource, base::DoNothing());
    int child_id =
        resource_provider_.CreateChild(base::DoNothing(), SurfaceId());
    std::vector<TransferableResource> list;
    child_resource_provider_.PrepareSendToParent(
        {resource_id}, &list, child_context_provider->SharedImageInterface());
    resource_provider_.ReceiveFromChild(child_id, list);
    child_resource_provider_.RemoveImportedResource(resource_id);

    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        resource_provider_.GetChildToParentMap(child_id);
    return resource_map[list[0].id];
  }

  AggregatedRenderPass* AddRenderPass(gfx::ContentColorUsage usage) {
    auto pass = std::make_unique<AggregatedRenderPass>();
    pass->SetNew(AggregatedRenderPassId::FromUnsafeValue(passes_.size() + 1),
                 kOutputRect, kOutputRect, gfx::Transform());
    pass->content_color_usage = usage;
    passes_.push_back(std::move(pass));
    return passes_.back().get();
  }

  TextureDrawQuad* AddTextureQuad(
      AggregatedRenderPass* pass,
      const gfx::Rect& rect,
      ResourceId resource_id,
      bool needs_blending = false,
      std::optional<gfx::Rect> clip = std::nullopt,
      const gfx::Transform& transform = gfx::Transform()) {
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_layer_rect = rect;
    sqs->visible_quad_layer_rect = rect;
    sqs->clip_rect = clip;
    sqs->quad_to_target_transform = transform;
    auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    quad->SetNew(sqs, rect, rect, needs_blending, resource_id,
                 gfx::PointF(0, 0), gfx::PointF(1, 1), SkColors::kTransparent,
                 /*nearest=*/false, /*secure_output=*/false,
                 gfx::ProtectedVideoType::kClear,
                 /*is_tex_coords_normalized=*/true);
    return quad;
  }

  void AddSolidColorQuad(AggregatedRenderPass* pass,
                         const gfx::Rect& rect,
                         SkColor4f color = SkColors::kWhite) {
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_layer_rect = rect;
    sqs->visible_quad_layer_rect = rect;
    auto* quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    quad->SetNew(sqs, rect, rect, color, /*anti_aliasing_off=*/false);
  }

  // Adds the black background that full screen videos are drawn on.
  void AddBlackBackground(AggregatedRenderPass* pass) {
    AddSolidColorQuad(pass, kOutputRect, SkColors::kBlack);
  }

  const DrawQuad* Find(bool is_page_fullscreen = true) {
    return FindHdrVideoPassthroughQuad(&resource_provider_, passes_,
                                       gfx::ColorSpace::CreateHDR10(),
                                       is_page_fullscreen);
  }

  ClientResourceProvider child_resource_provider_;
  DisplayResourceProviderNull resource_provider_;
  AggregatedRenderPassList passes_;
};

// A full screen HDR10 video with SDR UI on top is passed through.
TEST_F(HdrVideoPassthroughTest, FullscreenVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  // Quads are added front to back.
  AddSolidColorQuad(root, gfx::Rect(0, 1000, 1920, 80));
  AddTextureQuad(root, gfx::Rect(10, 10, 20, 20),
                 CreateResource(gfx::ColorSpace::CreateSRGB()));
  const DrawQuad* video = AddTextureQuad(
      root, kOutputRect, CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_EQ(Find(), video);
}

// A letterboxed full screen 2.39:1 video is passed through.
TEST_F(HdrVideoPassthroughTest, LetterboxedVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  // 1920 / 2.39 = 803, centered vertically in 1080.
  const DrawQuad* video =
      AddTextureQuad(root, gfx::Rect(0, 138, 1920, 803),
                     CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_EQ(Find(), video);
}

// A pillarboxed full screen 4:3 video is passed through, also when it's
// placed using a transform.
TEST_F(HdrVideoPassthroughTest, PillarboxedVideoWithTransform) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  // 640x480 scaled to 1440x1080, centered horizontally in 1920.
  gfx::Transform transform;
  transform.Translate(240, 0);
  transform.Scale(2.25, 2.25);
  const DrawQuad* video = AddTextureQuad(
      root, gfx::Rect(0, 0, 640, 480),
      CreateResource(gfx::ColorSpace::CreateHDR10()),
      /*needs_blending=*/false, /*clip=*/std::nullopt, transform);
  AddBlackBackground(root);
  EXPECT_EQ(Find(), video);
}

// Without page fullscreen mode, nothing is passed through.
TEST_F(HdrVideoPassthroughTest, NotPageFullscreen) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find(/*is_page_fullscreen=*/false));
}

// An off-center video is not full screen.
TEST_F(HdrVideoPassthroughTest, OffCenterVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, gfx::Rect(0, 0, 1920, 803),
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// A video that doesn't fill the output in either dimension is not full
// screen.
TEST_F(HdrVideoPassthroughTest, SmallVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, gfx::Rect(160, 90, 1600, 900),
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// The quad below the video must be black (or tiled content) covering the
// output.
TEST_F(HdrVideoPassthroughTest, NonBlackBackground) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddSolidColorQuad(root, kOutputRect, SkColors::kWhite);
  EXPECT_FALSE(Find());
}

// A clipped video is not full screen.
TEST_F(HdrVideoPassthroughTest, ClippedVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()),
                 /*needs_blending=*/false, gfx::Rect(0, 0, 960, 1080));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// The output must use PQ.
TEST_F(HdrVideoPassthroughTest, RequiresPQOutput) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_FALSE(FindHdrVideoPassthroughQuad(&resource_provider_, passes_,
                                           gfx::ColorSpace::CreateHLG(), true));
  EXPECT_FALSE(FindHdrVideoPassthroughQuad(
      &resource_provider_, passes_, gfx::ColorSpace::CreateSRGB(), true));
}

// Two HDR quads are never passed through.
TEST_F(HdrVideoPassthroughTest, MultipleHdrQuads) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, gfx::Rect(0, 0, 10, 10),
                 CreateResource(gfx::ColorSpace::CreateHLG()));
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// HDR content in a non-root render pass prevents passthrough.
TEST_F(HdrVideoPassthroughTest, HdrInChildRenderPass) {
  AddRenderPass(gfx::ContentColorUsage::kHDR);
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// Only PQ content is passed through.
TEST_F(HdrVideoPassthroughTest, HlgVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHLG()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// Content with AGTM metadata is tone mapped by viz.
TEST_F(HdrVideoPassthroughTest, AgtmMetadata) {
  gfx::HDRMetadata metadata;
  metadata.SetHdrReferenceWhite(203.f);
  ASSERT_TRUE(metadata.HasAgtm());
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10(), metadata));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// Blended video is not passed through.
TEST_F(HdrVideoPassthroughTest, BlendedVideo) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateHDR10()),
                 /*needs_blending=*/true);
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// Video that requested a constrained dynamic range must be tone mapped by the
// renderer, so it is not passed through.
TEST_F(HdrVideoPassthroughTest, ConstrainedDynamicRangeLimit) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kHDR);
  TextureDrawQuad* video = AddTextureQuad(
      root, kOutputRect, CreateResource(gfx::ColorSpace::CreateHDR10()));
  AddBlackBackground(root);
  EXPECT_EQ(Find(), video);

  // dynamic-range-limit: standard.
  video->dynamic_range_limit =
      cc::PaintFlags::DynamicRangeLimitMixture(/*standard_mix=*/1.f,
                                               /*constrained_high_mix=*/0.f);
  EXPECT_FALSE(Find());

  // dynamic-range-limit: constrained.
  video->dynamic_range_limit =
      cc::PaintFlags::DynamicRangeLimitMixture(/*standard_mix=*/0.f,
                                               /*constrained_high_mix=*/1.f);
  EXPECT_FALSE(Find());

  // A partial mix also requires tone mapping.
  video->dynamic_range_limit =
      cc::PaintFlags::DynamicRangeLimitMixture(/*standard_mix=*/0.f,
                                               /*constrained_high_mix=*/0.25f);
  EXPECT_FALSE(Find());

  // dynamic-range-limit: no-limit.
  video->dynamic_range_limit = cc::PaintFlags::DynamicRangeLimitMixture(
      /*standard_mix=*/0.f, /*constrained_high_mix=*/0.f);
  EXPECT_EQ(Find(), video);
}

// Without HDR content in the root render pass, there is nothing to pass
// through.
TEST_F(HdrVideoPassthroughTest, NoHdrContent) {
  AggregatedRenderPass* root = AddRenderPass(gfx::ContentColorUsage::kSRGB);
  AddTextureQuad(root, kOutputRect,
                 CreateResource(gfx::ColorSpace::CreateSRGB()));
  AddBlackBackground(root);
  EXPECT_FALSE(Find());
}

// The content's metadata is scaled from the content's SDR white level to the
// output's.
TEST_F(HdrVideoPassthroughTest, MetadataScaledToOutputSdrWhite) {
  const gfx::ColorSpace content_color_space =
      gfx::ColorSpace::CreateHDR10().GetWithSdrWhiteLevel(200.f);
  const gfx::HDRMetadata content_metadata(
      skhdr::MasteringDisplayColorVolume{
          .fDisplayPrimaries = SkNamedPrimaries::kSMPTE_EG_432_1,
          .fMaximumDisplayMasteringLuminance = 4000.f,
          .fMinimumDisplayMasteringLuminance = 0.01f,
      },
      skhdr::ContentLightLevelInformation{.fMaxCLL = 1000.f,
                                          .fMaxFALL = 300.f});
  const gfx::HDRMetadata result = ComputeHdrVideoPassthroughMetadata(
      content_color_space, content_metadata, /*output_sdr_white_level=*/500.f);
  ASSERT_TRUE(result.HasMDCV());
  ASSERT_TRUE(result.HasCLLI());
  EXPECT_EQ(result.GetMDCV().fDisplayPrimaries,
            SkNamedPrimaries::kSMPTE_EG_432_1);
  EXPECT_FLOAT_EQ(result.GetMDCV().fMaximumDisplayMasteringLuminance, 10000.f);
  EXPECT_FLOAT_EQ(result.GetMDCV().fMinimumDisplayMasteringLuminance, 0.025f);
  EXPECT_FLOAT_EQ(result.GetCLLI().fMaxCLL, 2500.f);
  EXPECT_FLOAT_EQ(result.GetCLLI().fMaxFALL, 750.f);
}

// Missing metadata is populated with the HDR10 defaults.
TEST_F(HdrVideoPassthroughTest, MetadataDefaults) {
  const gfx::HDRMetadata result = ComputeHdrVideoPassthroughMetadata(
      gfx::ColorSpace::CreateHDR10(), gfx::HDRMetadata(),
      gfx::ColorSpace::kDefaultSDRWhiteLevel);
  ASSERT_TRUE(result.HasMDCV());
  EXPECT_EQ(result.GetMDCV().fDisplayPrimaries, SkNamedPrimaries::kRec2020);
  EXPECT_FLOAT_EQ(result.GetMDCV().fMaximumDisplayMasteringLuminance, 1000.f);
  EXPECT_FALSE(result.HasCLLI());
}

}  // namespace
}  // namespace viz
