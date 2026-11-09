// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_mac.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/overlay_candidate_matchers.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/cocoa/ca_backdrop_filter_api.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"
#include "ui/latency/latency_info.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);
const gfx::Rect kRenderPassOutputRect(0, 0, 256, 256);
const gfx::Rect kOverlayDamageRect(0, 0, 100, 100);

class CATestOverlayProcessor : public OverlayProcessorMac {
 public:
  CATestOverlayProcessor() : OverlayProcessorMac() {}
};

std::unique_ptr<AggregatedRenderPass> CreateRenderPass() {
  AggregatedRenderPassId render_pass_id{1};

  auto pass = std::make_unique<AggregatedRenderPass>();
  pass->SetNew(render_pass_id, kRenderPassOutputRect, kRenderPassOutputRect,
               gfx::Transform());

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  return pass;
}

static ResourceId CreateResourceInLayerTree(
    ClientResourceProvider* child_resource_provider,
    const gfx::Size& size,
    bool is_overlay_candidate) {
  gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  if (is_overlay_candidate) {
    usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
  }
  auto resource = TransferableResource::Make(
      gpu::ClientSharedImage::CreateForTesting(
          {SinglePlaneFormat::kRGBA_8888, size, gfx::ColorSpace(),
           kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage},
          GL_TEXTURE_2D),
      TransferableResource::ResourceSource::kTest, gpu::SyncToken());

  ResourceId resource_id =
      child_resource_provider->ImportResource(resource, base::DoNothing());

  return resource_id;
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          RasterContextProvider* child_context_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate) {
  ResourceId resource_id = CreateResourceInLayerTree(
      child_resource_provider, size, is_overlay_candidate);

  int child_id =
      parent_resource_provider->CreateChild(base::DoNothing(), SurfaceId());

  // Transfer resource to the parent.
  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;

  CHECK(child_context_provider);
  child_resource_provider->PrepareSendToParent(
      resource_ids_to_transfer, &list,
      child_context_provider->SharedImageInterface());
  parent_resource_provider->ReceiveFromChild(child_id, list);

  // Delete it in the child so it won't be leaked, and will be released once
  // returned from the parent.
  child_resource_provider->RemoveImportedResource(resource_id);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  return resource_map[list[0].id];
}

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::ProtectedVideoType protected_video_type) {
  bool needs_blending = false;
  bool nearest_neighbor = false;
  gfx::Size resource_size_in_pixels = rect.size();
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      shared_quad_state, rect, rect, needs_blending, resource_id,
      /*top_left=*/gfx::ScalePoint(kUVTopLeft, rect.width(), rect.height()),
      /*bottom_right=*/
      gfx::ScalePoint(kUVBottomRight, rect.width(), rect.height()),
      SkColors::kTransparent, nearest_neighbor,
      /*secure_output=*/false, protected_video_type,
      /*is_tex_coords_normalized=*/false);

  return overlay_quad;
}

TextureDrawQuad* CreateFullscreenCandidateQuad(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, render_pass->output_rect,
      gfx::ProtectedVideoType::kClear);
}

SkM44 GetIdentityColorMatrix() {
  return SkM44();
}

class CALayerOverlayTest : public testing::Test {
 protected:
  void SetUp() override {
    output_surface_ = FakeSkiaOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();
    lock_set_for_external_use_.emplace(resource_provider_.get(),
                                       output_surface_.get());

    child_provider_ = TestContextProvider::CreateGLES();
    child_provider_->BindToCurrentSequence();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();

    overlay_processor_ = std::make_unique<CATestOverlayProcessor>();
  }

  void TearDown() override {
    overlay_processor_ = nullptr;
    child_resource_provider_->ShutdownAndReleaseAllResources();
    child_resource_provider_ = nullptr;
    child_provider_ = nullptr;
    lock_set_for_external_use_.reset();
    resource_provider_ = nullptr;
    output_surface_ = nullptr;
  }

  OverlayProcessorInterface::PrimaryPlaneParams GetDefaultPrimaryPlane(
      const gfx::Size& primary_plane_size) {
    return OverlayProcessorInterface::PrimaryPlaneParams{
        .viewport_size = primary_plane_size,
        .resource_size_in_pixels = primary_plane_size,
        .supports_hdr = false,
        .is_opaque = true,
        .si_format = SinglePlaneFormat::kRGBA_8888,
        .color_space = gfx::ColorSpace::CreateSRGB(),
    };
  }

  std::unique_ptr<SkiaOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<DisplayResourceProviderSkia> resource_provider_;
  std::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<CATestOverlayProcessor> overlay_processor_;
  gfx::Rect damage_rect_ = kOverlayDamageRect;


 private:
  std::optional<OverlayCandidate> primary_plane_;
};

TEST_F(CALayerOverlayTest, AllowNonAxisAlignedTransform) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(45.f);

  OverlayCandidateList ca_layer_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      std::move(surface_damage_rect_list),
      GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
      &ca_layer_list, &damage_rect_);
  EXPECT_EQ(gfx::Rect(), damage_rect_);
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
  gfx::Rect overlay_damage = overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_EQ(kRenderPassOutputRect, overlay_damage);
}

TEST_F(CALayerOverlayTest, ThreeDTransform) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  OverlayCandidateList ca_layer_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      std::move(surface_damage_rect_list),
      GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
      &ca_layer_list, &damage_rect_);
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
  gfx::Rect overlay_damage = overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_EQ(kRenderPassOutputRect, overlay_damage);
  gfx::Transform expected_transform;
  expected_transform.RotateAboutXAxis(45.f);
  gfx::Transform actual_transform(
      std::get<gfx::Transform>(ca_layer_list.back().transform));
  EXPECT_EQ(expected_transform.ToString(), actual_transform.ToString());
}

TEST_F(CALayerOverlayTest, AllowContainingClip) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->clip_rect = kOverlayRect;

  OverlayCandidateList ca_layer_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      std::move(surface_damage_rect_list),
      GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
      &ca_layer_list, &damage_rect_);
  EXPECT_EQ(gfx::Rect(), damage_rect_);
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
}

TEST_F(CALayerOverlayTest, NontrivialClip) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->clip_rect = gfx::Rect(64, 64, 128, 128);

  OverlayCandidateList ca_layer_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      std::move(surface_damage_rect_list),
      GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
      &ca_layer_list, &damage_rect_);
  EXPECT_EQ(gfx::Rect(), damage_rect_);
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
  EXPECT_EQ(gfx::Rect(64, 64, 128, 128),
            ca_layer_list.back().clip_rect.value());
}

TEST_F(CALayerOverlayTest, SkipTransparent) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->opacity = 0;

  OverlayCandidateList ca_layer_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      std::move(surface_damage_rect_list),
      GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
      &ca_layer_list, &damage_rect_);
  EXPECT_EQ(gfx::Rect(), damage_rect_);
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
}

TEST_F(CALayerOverlayTest, SkipNonVisible) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->quad_list.back()->visible_rect.set_size(gfx::Size());

  OverlayCandidateList ca_layer_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      std::move(surface_damage_rect_list),
      GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
      &ca_layer_list, &damage_rect_);
  EXPECT_EQ(gfx::Rect(), damage_rect_);
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
}

TEST_F(CALayerOverlayTest, TextureDrawQuadVideoOverlay) {
  const gfx::Size size(640, 480);
  bool is_overlay_candidate = true;
  ResourceId resource_id =
      CreateResource(resource_provider_.get(), child_resource_provider_.get(),
                     child_provider_.get(), size, is_overlay_candidate);

  // Video frames of TextureDrawQuad should be promoted to overlays.
  {
    auto pass = CreateRenderPass();
    auto* texture_video_quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    texture_video_quad->SetNew(
        pass->shared_quad_state_list.back(), gfx::Rect(size), gfx::Rect(size),
        /*needs_blending=*/false, resource_id,
        /*top_left=*/gfx::ScalePoint(kUVTopLeft, size.width(), size.height()),
        /*bottom_right=*/
        gfx::ScalePoint(kUVBottomRight, size.width(), size.height()),
        SkColors::kTransparent,
        /*nearest_neighbor=*/false,
        /*secure_output_only=*/false,
        /*video_type=*/gfx::ProtectedVideoType::kClear,
        /*is_tex_coords_normalized=*/false);
    texture_video_quad->is_video_frame = true;

    OverlayCandidateList ca_layer_list;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        std::move(surface_damage_rect_list),
        GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
        &ca_layer_list, &damage_rect_);
    EXPECT_EQ(gfx::Rect(), damage_rect_);
    EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
  }
}

TEST_F(CALayerOverlayTest, OverlayErrorCode) {
  // Frame #1
  {
    auto pass = CreateRenderPass();
    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList ca_layer_list;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        std::move(surface_damage_rect_list),
        GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
        &ca_layer_list, &damage_rect_);

    // There should be no error.
    gfx::CALayerResult error_code = overlay_processor_->GetCALayerErrorCode();
    EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));
    // kCALayerSuccess = 0,
    EXPECT_EQ(0, error_code);
  }

  // Frame #2
  {
    auto pass = CreateRenderPass();
    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    // Add a copy request to the render pass
    pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

    OverlayCandidateList ca_layer_list;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        std::move(surface_damage_rect_list),
        GetDefaultPrimaryPlane(pass_list.back()->output_rect.size()),
        &ca_layer_list, &damage_rect_);

    // Overlay should fail when there is a copy request.
    EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list));

    // kCALayerFailedCopyRequests = 31,
    gfx::CALayerResult error_code = overlay_processor_->GetCALayerErrorCode();
    EXPECT_EQ(31, error_code);
  }
}

class CALayerOverlayRPDQTest : public CALayerOverlayTest {
 protected:
  void SetUp() override {
    CALayerOverlayTest::SetUp();
    pass_list_.push_back(CreateRenderPass());
    pass_ = pass_list_.back().get();
    quad_ = pass_->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    render_pass_id_ = AggregatedRenderPassId{3};
  }

  void ProcessForOverlays() {
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list_, GetIdentityColorMatrix(),
        std::move(surface_damage_rect_list_),
        GetDefaultPrimaryPlane(pass_->output_rect.size()), &ca_layer_list_,
        &damage_rect_);
  }

  void SetBackdropFilter(
      cc::FilterOperations backdrop_filters,
      std::optional<SkPath> backdrop_filter_bounds = std::nullopt,
      gfx::Vector2dF filters_scale = gfx::Vector2dF(1, 1)) {
    quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                  kOverlayRect, render_pass_id_, kInvalidResourceId,
                  gfx::RectF(), gfx::Size(), false);
    quad_->SetFilters(/*filters=*/{}, std::move(backdrop_filters),
                      std::move(backdrop_filter_bounds), filters_scale,
                      /*filters_origin=*/gfx::PointF(),
                      /*backdrop_filter_quality=*/1.0f);
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      ui::kCoreAnimationBackdropFilter};
  AggregatedRenderPassList pass_list_;
  raw_ptr<AggregatedRenderPass> pass_;
  raw_ptr<AggregatedRenderPassDrawQuad> quad_;
  AggregatedRenderPassId render_pass_id_;
  OverlayCandidateList ca_layer_list_;
  SurfaceDamageRectList surface_damage_rect_list_;
};

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadNoFilters) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  ProcessForOverlays();

  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadAllValidFilters) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateGrayscaleFilter(0.1f));
  filters.Append(cc::FilterOperation::CreateSepiaFilter(0.2f));
  filters.Append(cc::FilterOperation::CreateSaturateFilter(0.3f));
  filters.Append(cc::FilterOperation::CreateHueRotateFilter(0.4f));
  filters.Append(cc::FilterOperation::CreateInvertFilter(0.5f));
  filters.Append(cc::FilterOperation::CreateBrightnessFilter(0.6f));
  filters.Append(cc::FilterOperation::CreateContrastFilter(0.7f));
  filters.Append(cc::FilterOperation::CreateOpacityFilter(0.8f));
  filters.Append(cc::FilterOperation::CreateBlurFilter(0.9f));
  filters.Append(cc::FilterOperation::CreateDropShadowFilter(
      gfx::Point(10, 20), 1.0f, SkColors::kGreen));
  quad_->SetFilters(filters,
                    /*backdrop_filters=*/{},
                    /*backdrop_filter_bounds=*/std::nullopt,
                    /*filters_scale=*/gfx::Vector2dF(1, 1),
                    /*filters_origin=*/gfx::PointF(),
                    /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();

  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadOpacityFilterScale) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateOpacityFilter(0.8f)}),
      /*backdrop_filters=*/{},
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 2),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBlurFilterScale) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateBlurFilter(0.8f)}),
      /*backdrop_filters=*/{},
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 2),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadDropShadowFilterScale) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateDropShadowFilter(gfx::Point(10, 20), 1.0f,
                                                       SkColors::kGreen)}),
      /*backdrop_filters=*/{},
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 2),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBackgroundFilter) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/{},
      /*backdrop_filters=*/
      cc::FilterOperations({cc::FilterOperation::CreateGrayscaleFilter(0.1f)}),
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 1),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBlurBackdropFilter) {
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_EQ(15.0f, ca_layer_list_.back().backdrop_filter_params->blur_radius);
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBlurBackdropFilterScale) {
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(2, 2));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  // Blur radius 15.0 scaled by filters_scale 2.0 = 30.0.
  EXPECT_EQ(30.0f, ca_layer_list_.back().backdrop_filter_params->blur_radius);
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBlurBackdropFilterWithBounds) {
  SkRRect rrect = SkRRect::MakeRectXY(
      SkRect::MakeXYWH(kOverlayRect.x(), kOverlayRect.y(), kOverlayRect.width(),
                       kOverlayRect.height()),
      12.0f, 12.0f);
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      SkPath::RRect(rrect));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_EQ(15.0f, ca_layer_list_.back().backdrop_filter_params->blur_radius);
  EXPECT_EQ(gfx::RRectF(rrect), ca_layer_list_.back().backdrop_filter_bounds);
  EXPECT_TRUE(ca_layer_list_.back().rounded_corners.IsEmpty());
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterBoundsOvalRejected) {
  SkRect oval_rect =
      SkRect::MakeXYWH(kOverlayRect.x(), kOverlayRect.y(), kOverlayRect.width(),
                       kOverlayRect.height() / 2);
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      SkPath::Oval(oval_rect));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterBoundsAsymmetricRejected) {
  SkVector radii[4] = {
      {40.0f, 40.0f}, {10.0f, 10.0f}, {40.0f, 40.0f}, {10.0f, 10.0f}};
  SkRRect rrect;
  rrect.setRectRadii(
      SkRect::MakeXYWH(kOverlayRect.x(), kOverlayRect.y(), kOverlayRect.width(),
                       kOverlayRect.height()),
      radii);
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      SkPath::RRect(rrect));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBackdropFilterChainRejected) {
  // Multi-filter chain: Blur + Sepia. Must not be promoted to CoreAnimation
  // overlay; SkiaRenderer must handle the full chain.
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f),
                            cc::FilterOperation::CreateSepiaFilter(0.8f)}));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterChainBlurSaturateBrightnessContrast) {
  // Glassmorphic multi-filter chain: blur(28px) saturate(135%) brightness(105%)
  // contrast(100%). Should be promoted to CoreAnimation with a color matrix
  // composing saturate(1.35) and brightness(1.05).
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(28.0f),
                            cc::FilterOperation::CreateSaturateFilter(1.35f),
                            cc::FilterOperation::CreateBrightnessFilter(1.05f),
                            cc::FilterOperation::CreateContrastFilter(1.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  const auto& params = ca_layer_list_.back().backdrop_filter_params;
  ASSERT_TRUE(params.has_value());
  EXPECT_FLOAT_EQ(28.0f, params->blur_radius);
  ASSERT_TRUE(params->color_matrix.has_value());
  // Diagonal entry m11 = 1.05 * (0.213 + 0.787 * 1.35).
  EXPECT_NEAR(1.05f * (0.213f + 0.787f * 1.35f), (*params->color_matrix)[0],
              1e-5f);
  // Alpha scale m44 = 1.0.
  EXPECT_FLOAT_EQ(1.0f, (*params->color_matrix)[18]);
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterIdentityOperationsSkipped) {
  // Pure blur accompanied by identity saturate(100%), brightness(100%),
  // contrast(100%). The identity operations should be skipped without creating
  // an unnecessary colorMatrix filter.
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(35.0f),
                            cc::FilterOperation::CreateSaturateFilter(1.0f),
                            cc::FilterOperation::CreateBrightnessFilter(1.0f),
                            cc::FilterOperation::CreateContrastFilter(1.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_FLOAT_EQ(35.0f,
                  ca_layer_list_.back().backdrop_filter_params->blur_radius);
  EXPECT_FALSE(
      ca_layer_list_.back().backdrop_filter_params->color_matrix.has_value());

  // All-identity chain (blur(0px) saturate(100%)) should promote the RPDQ
  // with zero blur radius and no colorMatrix filter.
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(0.0f),
                            cc::FilterOperation::CreateSaturateFilter(1.0f)}));
  ca_layer_list_.clear();
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_FLOAT_EQ(0.0f,
                  ca_layer_list_.back().backdrop_filter_params->blur_radius);
  EXPECT_FALSE(
      ca_layer_list_.back().backdrop_filter_params->color_matrix.has_value());
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterColorMatrixWithoutBlur) {
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateContrastFilter(1.2f)}));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  const auto& params = ca_layer_list_.back().backdrop_filter_params;
  ASSERT_TRUE(params.has_value());
  EXPECT_FLOAT_EQ(0.0f, params->blur_radius);
  ASSERT_TRUE(params->color_matrix.has_value());
  EXPECT_FLOAT_EQ(1.2f, (*params->color_matrix)[0]);
  EXPECT_NEAR(-0.5f * 1.2f + 0.5f, (*params->color_matrix)[4], 1e-5f);
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterMultipleBlursRejected) {
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(10.0f),
                            cc::FilterOperation::CreateBlurFilter(20.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));

  // Non-identity color matrix before blur() is also rejected so filter order
  // matches SkiaRenderer.
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateContrastFilter(1.2f),
                            cc::FilterOperation::CreateBlurFilter(10.0f)}));
  ca_layer_list_.clear();
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterNonBlurRejected) {
  // Non-blur backdrop filter: Invert. Must not be promoted to CoreAnimation.
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateInvertFilter(1.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterComplexPathRejected) {
  // A triangular path that cannot be represented as an RRect or Oval.
  SkPath triangle_path = SkPathBuilder()
                             .moveTo(0, 0)
                             .lineTo(100, 0)
                             .lineTo(50, 100)
                             .close()
                             .detach();
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      triangle_path);
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterWithForegroundFilterRejected) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateBrightnessFilter(0.5f)}),
      /*backdrop_filters=*/
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 1),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterWithMaskRejected) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, ResourceId(2), gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/{},
      /*backdrop_filters=*/
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 1),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterWithNonUnitOpacityRejected) {
  pass_->shared_quad_state_list.back()->opacity = 0.5f;
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBlurBackdropFilterTransformScale) {
  // Apply a uniform 1.5x scale to quad_to_target_transform (e.g. pinch-to-zoom
  // or CSS transform: scale(1.5)). CARendererLayerTree::TransformLayer applies
  // `quad_to_target_transform` in WindowServer, so `blur_radius` in
  // CABackdropFilterParams must stay in RenderPass local space (`amount *
  // filters_scale`) rather than double-scaling by `quad_to_target_transform`.
  gfx::Transform transform;
  transform.Scale(1.5f, 1.5f);
  pass_->shared_quad_state_list.back()->quad_to_target_transform = transform;

  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_FLOAT_EQ(15.0f,
                  ca_layer_list_.back().backdrop_filter_params->blur_radius);

  // When filters_scale is 2.0 (e.g. 2x Retina display) and
  // quad_to_target_transform is 1.5x, RenderPass local blur_radius is
  // 15.0 * 2.0 = 30.0 (and WindowServer scales by 1.5x to 45.0 physical px).
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(2.0f, 2.0f));
  ca_layer_list_.clear();
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_FLOAT_EQ(30.0f,
                  ca_layer_list_.back().backdrop_filter_params->blur_radius);

  // Non-uniform 2D scales cannot be represented by CoreAnimation's scalar
  // isotropic inputRadius and must fall back to SkiaRenderer.
  gfx::Transform non_uniform_transform;
  non_uniform_transform.Scale(2.0f, 0.5f);
  pass_->shared_quad_state_list.back()->quad_to_target_transform =
      non_uniform_transform;
  ca_layer_list_.clear();
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(ui::kCoreAnimationBackdropFilter);

  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest,
       RenderPassDrawQuadBackdropFilterEmptyBoundsRejected) {
  // Bounds disjoint from kOverlayRect (visible_rect) clip to empty and must be
  // rejected rather than falling back to the full quad rect in
  // CARendererLayerTree.
  SkRect disjoint_rect = SkRect::MakeXYWH(kOverlayRect.right() + 100,
                                          kOverlayRect.bottom() + 100, 50, 50);
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      SkPath::Rect(disjoint_rect));
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));

  // Rectangular bounds extending even slightly (< 1px) outside visible_rect
  // should be clipped to visible_rect.
  SkRect slightly_larger_rect = SkRect::MakeXYWH(
      kOverlayRect.x() - 0.5f, kOverlayRect.y() - 0.5f,
      kOverlayRect.width() + 1.0f, kOverlayRect.height() + 1.0f);
  SetBackdropFilter(
      cc::FilterOperations({cc::FilterOperation::CreateBlurFilter(15.0f)}),
      SkPath::Rect(slightly_larger_rect));
  ca_layer_list_.clear();
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
  ASSERT_TRUE(ca_layer_list_.back().backdrop_filter_params.has_value());
  EXPECT_EQ(gfx::RRectF(gfx::RectF(kOverlayRect)),
            ca_layer_list_.back().backdrop_filter_bounds);
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadMask) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, ResourceId(2), gfx::RectF(),
                gfx::Size(), false);
  ProcessForOverlays();
  EXPECT_EQ(1U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadUnsupportedFilter) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), false);
  quad_->SetFilters(
      /*filters=*/cc::FilterOperations(
          {cc::FilterOperation::CreateZoomFilter(0.9f, 1)}),
      /*backdrop_filters=*/{},
      /*backdrop_filter_bounds=*/std::nullopt,
      /*filters_scale=*/gfx::Vector2dF(1, 1),
      /*filters_origin=*/gfx::PointF(), /*backdrop_filter_quality=*/1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

TEST_F(CALayerOverlayRPDQTest, TooManyRenderPassDrawQuads) {
  int count = 35;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, ResourceId(2), gfx::RectF(),
                gfx::Size(), false);
  for (int i = 1; i < count; ++i) {
    auto* quad = pass_->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    quad->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                 kOverlayRect, render_pass_id_, ResourceId(2), gfx::RectF(),
                 gfx::Size(), false);
  }

  ProcessForOverlays();
  EXPECT_EQ(0U, test::NumOverlaysExcludingPrimaryPlane(ca_layer_list_));
}

}  // namespace
}  // namespace viz
