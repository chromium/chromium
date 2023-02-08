// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_win.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_switches.h"
#include "ui/latency/latency_info.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::Rect kOverlayBottomRightRect(128, 128, 128, 128);

class MockDCLayerOutputSurface : public FakeSkiaOutputSurface {
 public:
  static std::unique_ptr<MockDCLayerOutputSurface> Create() {
    auto provider = TestContextProvider::Create();
    provider->BindToCurrentSequence();
    return std::make_unique<MockDCLayerOutputSurface>(std::move(provider));
  }

  explicit MockDCLayerOutputSurface(scoped_refptr<ContextProvider> provider)
      : FakeSkiaOutputSurface(std::move(provider)) {
    capabilities_.supports_dc_layers = true;
  }

  // OutputSurface implementation.
  MOCK_METHOD1(SetEnableDCLayers, void(bool));
};

class DCTestOverlayProcessor : public OverlayProcessorWin {
 public:
  explicit DCTestOverlayProcessor(OutputSurface* output_surface)
      : OverlayProcessorWin(output_surface,
                            std::make_unique<DCLayerOverlayProcessor>(
                                &debug_settings_,
                                /*allowed_yuv_overlay_count=*/1,
                                true)) {}
  DebugRendererSettings debug_settings_;
};

std::unique_ptr<AggregatedRenderPass> CreateRenderPass() {
  AggregatedRenderPassId render_pass_id{1};
  gfx::Rect output_rect(0, 0, 256, 256);

  auto pass = std::make_unique<AggregatedRenderPass>();
  pass->SetNew(render_pass_id, output_rect, output_rect, gfx::Transform());

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  return pass;
}

static ResourceId CreateResourceInLayerTree(
    ClientResourceProvider* child_resource_provider,
    const gfx::Size& size,
    bool is_overlay_candidate) {
  auto resource = TransferableResource::MakeGpu(
      gpu::Mailbox::GenerateForSharedImage(), GL_LINEAR, GL_TEXTURE_2D,
      gpu::SyncToken(), size, RGBA_8888, is_overlay_candidate);

  ResourceId resource_id =
      child_resource_provider->ImportResource(resource, base::DoNothing());

  return resource_id;
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          ContextProvider* child_context_provider,
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
  child_resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list,
                                               child_context_provider);
  parent_resource_provider->ReceiveFromChild(child_id, list);

  // Delete it in the child so it won't be leaked, and will be released once
  // returned from the parent.
  child_resource_provider->RemoveImportedResource(resource_id);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  return resource_map[list[0].id];
}

SolidColorDrawQuad* CreateSolidColorQuadAt(
    const SharedQuadState* shared_quad_state,
    SkColor4f color,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect) {
  SolidColorDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, rect, rect, color, false);
  return quad;
}

void CreateOpaqueQuadAt(DisplayResourceProvider* resource_provider,
                        const SharedQuadState* shared_quad_state,
                        AggregatedRenderPass* render_pass,
                        const gfx::Rect& rect,
                        SkColor4f color) {
  DCHECK(color.isOpaque());
  auto* color_quad = render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_quad_state, rect, rect, color, false);
}

YUVVideoDrawQuad* CreateFullscreenCandidateYUVVideoQuad(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass) {
  bool needs_blending = false;
  gfx::Rect rect = render_pass->output_rect;
  gfx::Size resource_size_in_pixels = rect.size();
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  overlay_quad->SetNew(
      shared_quad_state, rect, rect, needs_blending, resource_size_in_pixels,
      gfx::Rect(resource_size_in_pixels), gfx::Size(1, 1), resource_id,
      resource_id, resource_id, resource_id, gfx::ColorSpace::CreateREC601(), 0,
      1.0, 8, gfx::ProtectedVideoType::kClear, absl::nullopt);

  return overlay_quad;
}

AggregatedRenderPassDrawQuad* CreateRenderPassDrawQuadAt(
    AggregatedRenderPass* render_pass,
    const SharedQuadState* shared_quad_state,
    const gfx::Rect& rect,
    AggregatedRenderPassId render_pass_id) {
  AggregatedRenderPassDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, rect, rect, render_pass_id, ResourceId(2),
               gfx::RectF(), gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(),
               gfx::RectF(), false, 1.f);
  return quad;
}

SkM44 GetIdentityColorMatrix() {
  return SkM44();
}

class DCLayerOverlayTest : public testing::Test {
 protected:
  DCLayerOverlayTest() {
    // With DisableVideoOverlayIfMoving, videos are required to be stable for a
    // certain number of frames to be considered for overlay promotion. This
    // complicates tests since it adds behavior dependent on the number of times
    // |Process| is called.
    feature_list_.InitAndDisableFeature(features::kDisableVideoOverlayIfMoving);
  }

  void SetUp() override {
    output_surface_ = MockDCLayerOutputSurface::Create();
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();
    lock_set_for_external_use_.emplace(resource_provider_.get(),
                                       output_surface_.get());

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentSequence();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();

    overlay_processor_ =
        std::make_unique<DCTestOverlayProcessor>(output_surface_.get());
    overlay_processor_->set_using_dc_layers_for_testing(true);
    overlay_processor_->SetViewportSize(gfx::Size(256, 256));
    overlay_processor_
        ->set_frames_since_last_qualified_multi_overlays_for_testing(5);
    EXPECT_TRUE(overlay_processor_->IsOverlaySupported());
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

  void TestRenderPassRootTransform(bool is_overlay);

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockDCLayerOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<DisplayResourceProviderSkia> resource_provider_;
  absl::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<OverlayProcessorWin> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

TEST_F(DCLayerOverlayTest, DisableVideoOverlayIfMovingFeature) {
  auto ProcessForOverlaysSingleVideoRectWithOffset =
      [&](gfx::Vector2d video_rect_offset) {
        auto pass = CreateRenderPass();
        auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
            resource_provider_.get(), child_resource_provider_.get(),
            child_provider_.get(), pass->shared_quad_state_list.back(),
            pass.get());
        video_quad->rect = gfx::Rect(0, 0, 10, 10) + video_rect_offset;
        video_quad->visible_rect = gfx::Rect(0, 0, 10, 10) + video_rect_offset;

        OverlayCandidateList dc_layer_list;
        OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
        OverlayProcessorInterface::FilterOperationsMap
            render_pass_backdrop_filters;

        AggregatedRenderPassList pass_list;
        pass_list.push_back(std::move(pass));

        overlay_processor_->ProcessForOverlays(
            resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
            render_pass_filters, render_pass_backdrop_filters, {}, nullptr,
            &dc_layer_list, &damage_rect_, &content_bounds_);

        return dc_layer_list;
      };

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kDisableVideoOverlayIfMoving);
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset({0, 0}).size());
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset({1, 0}).size());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kDisableVideoOverlayIfMoving);
    // We expect an overlay promotion after a couple frames of no movement
    for (int i = 0; i < 10; i++) {
      ProcessForOverlaysSingleVideoRectWithOffset({0, 0}).size();
    }
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset({0, 0}).size());

    // Since the overlay candidate moved, we expect no overlays
    EXPECT_EQ(0U, ProcessForOverlaysSingleVideoRectWithOffset({1, 0}).size());

    // After some number of frames with no movement, we expect an overlay again
    for (int i = 0; i < 10; i++) {
      ProcessForOverlaysSingleVideoRectWithOffset({1, 0}).size();
    }
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset({1, 0}).size());
  }
}

TEST_F(DCLayerOverlayTest, Occluded) {
  {
    auto pass = CreateRenderPass();
    SharedQuadState* first_shared_state = pass->shared_quad_state_list.back();
    first_shared_state->overlay_damage_index = 0;
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SkColors::kWhite);

    SharedQuadState* second_shared_state =
        pass->CreateAndAppendSharedQuadState();
    second_shared_state->overlay_damage_index = 1;
    auto* first_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force the quad to use hw overlay
    first_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;

    SharedQuadState* third_shared_state =
        pass->CreateAndAppendSharedQuadState();
    third_shared_state->overlay_damage_index = 2;
    auto* second_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force the quad to use hw overlay
    second_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
    second_video_quad->rect.set_origin(gfx::Point(2, 2));
    second_video_quad->visible_rect.set_origin(gfx::Point(2, 2));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(1, 1, 10, 10), gfx::Rect(0, 0, 0, 0), gfx::Rect(0, 0, 0, 0)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(2U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.front().plane_z_order);
    EXPECT_EQ(-2, dc_layer_list.back().plane_z_order);
    // Entire underlay rect must be redrawn.
    EXPECT_EQ(gfx::Rect(0, 0, 256, 256), damage_rect_);
  }
  {
    auto pass = CreateRenderPass();
    SharedQuadState* first_shared_state = pass->shared_quad_state_list.back();
    first_shared_state->overlay_damage_index = 0;
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(3, 3, 100, 100), SkColors::kWhite);

    SharedQuadState* second_shared_state =
        pass->CreateAndAppendSharedQuadState();
    second_shared_state->overlay_damage_index = 1;
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force the quad to use hw overlay
    video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;

    SharedQuadState* third_shared_state =
        pass->CreateAndAppendSharedQuadState();
    third_shared_state->overlay_damage_index = 2;
    auto* second_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    second_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
    second_video_quad->rect.set_origin(gfx::Point(2, 2));
    second_video_quad->visible_rect.set_origin(gfx::Point(2, 2));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(1, 1, 10, 10), gfx::Rect(0, 0, 0, 0), gfx::Rect(0, 0, 0, 0)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(2U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.front().plane_z_order);
    EXPECT_EQ(-2, dc_layer_list.back().plane_z_order);

    // The underlay rectangle is the same, so the damage for first video quad is
    // contained within the combined occluding rects for this and the last
    // frame. Second video quad also adds its damage.
    EXPECT_EQ(gfx::Rect(1, 1, 10, 10), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, DamageRectWithoutVideoDamage) {
  {
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state = pass->shared_quad_state_list.back();
    shared_quad_state->overlay_damage_index = 0;
    // Occluding quad fully contained in video rect.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SkColors::kWhite);
    // Non-occluding quad fully outside video rect
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(210, 210, 20, 20), SkColors::kWhite);

    // Underlay video quad
    SharedQuadState* second_shared_state =
        pass->CreateAndAppendSharedQuadState();
    second_shared_state->overlay_damage_index = 1;
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 210, 20, 20);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(210, 210, 20, 20), gfx::Rect(0, 0, 0, 0)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);
    // All rects must be redrawn at the first frame.
    EXPECT_EQ(gfx::Rect(0, 0, 230, 230), damage_rect_);
  }
  {
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state = pass->shared_quad_state_list.back();
    shared_quad_state->overlay_damage_index = 0;
    // Occluding quad fully contained in video rect.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SkColors::kWhite);
    // Non-occluding quad fully outside video rect
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(210, 210, 20, 20), SkColors::kWhite);

    // Underlay video quad
    SharedQuadState* second_shared_state =
        pass->CreateAndAppendSharedQuadState();
    second_shared_state->overlay_damage_index = 1;
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 210, 20, 20);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(210, 210, 20, 20), gfx::Rect(0, 0, 0, 0)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);
    // Only the non-overlay damaged rect need to be drawn by the gl compositor
    EXPECT_EQ(gfx::Rect(210, 210, 20, 20), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, DamageRect) {
  for (int i = 0; i < 2; i++) {
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state = pass->shared_quad_state_list.back();
    shared_quad_state->overlay_damage_index = 0;
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(1, dc_layer_list.back().plane_z_order);
    // Damage rect should be unchanged on initial frame because of resize, but
    // should be empty on the second frame because everything was put in a
    // layer.
    if (i == 1)
      EXPECT_TRUE(damage_rect_.IsEmpty());
    else
      EXPECT_EQ(gfx::Rect(1, 1, 10, 10), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, ClipRect) {
  // Process twice. The second time through the overlay list shouldn't change,
  // which will allow the damage rect to reflect just the changes in that
  // frame.
  for (size_t i = 0; i < 2; ++i) {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 2, 100, 100), SkColors::kWhite);
    pass->shared_quad_state_list.back()->clip_rect = gfx::Rect(0, 3, 100, 100);

    SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->opacity = 1.f;
    shared_state->overlay_damage_index = 1;
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), shared_state, pass.get());
    // Clipped rect shouldn't be overlapped by clipped opaque quad rect.
    shared_state->clip_rect = gfx::Rect(0, 0, 100, 3);

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 3, 10, 8),
                                                      gfx::Rect(1, 1, 10, 2)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    // Because of clip rects the overlay isn't occluded and shouldn't be an
    // underlay.
    EXPECT_EQ(1, dc_layer_list.back().plane_z_order);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 3), dc_layer_list.back().clip_rect);
    if (i == 1) {
      // The damage rect should only contain contents that aren't in the
      // clipped overlay rect.
      EXPECT_EQ(gfx::Rect(1, 3, 10, 8), damage_rect_);
    }
  }
}

TEST_F(DCLayerOverlayTest, TransparentOnTop) {
  // Process twice. The second time through the overlay list shouldn't change,
  // which will allow the damage rect to reflect just the changes in that
  // frame.
  for (size_t i = 0; i < 2; ++i) {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    pass->shared_quad_state_list.back()->opacity = 0.5f;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(1, dc_layer_list.back().plane_z_order);
    // Quad isn't opaque, so underlying damage must remain the same.
    EXPECT_EQ(gfx::Rect(1, 1, 10, 10), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, UnderlayDamageRectWithQuadOnTopUnchanged) {
  for (int i = 0; i < 3; i++) {
    auto pass = CreateRenderPass();
    // Add a solid color quad on top
    SharedQuadState* shared_state_on_top = pass->shared_quad_state_list.back();
    CreateSolidColorQuadAt(shared_state_on_top, SkColors::kRed, pass.get(),
                           kOverlayBottomRightRect);

    SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->opacity = 1.f;
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), shared_state, pass.get());

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    gfx::Rect damage_rect_ = kOverlayRect;
    shared_state->overlay_damage_index = 1;

    // The quad on top does not give damage on the third frame
    SurfaceDamageRectList surface_damage_rect_list = {kOverlayBottomRightRect,
                                                      kOverlayRect};
    if (i == 2) {
      surface_damage_rect_list[0] = gfx::Rect();
    }

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);
    // Damage rect should be unchanged on initial frame, but should be reduced
    // to the size of quad on top, and empty on the third frame.
    if (i == 0)
      EXPECT_EQ(kOverlayRect, damage_rect_);
    else if (i == 1)
      EXPECT_EQ(kOverlayBottomRightRect, damage_rect_);
    else if (i == 2)
      EXPECT_EQ(gfx::Rect(), damage_rect_);
  }
}

// Test whether quads with rounded corners are supported.
TEST_F(DCLayerOverlayTest, RoundedCorners) {
  // Frame #0
  {
    auto pass = CreateRenderPass();

    // Create a video YUV quad with rounded corner, nothing on top.
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    // Rounded corners
    pass->shared_quad_state_list.back()->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0.f, 0.f, 20.f, 30.f), 5.f));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 256, 256)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    auto* root_pass = pass_list.back().get();
    auto* replaced_quad = root_pass->quad_list.back();
    auto* replaced_sqs = replaced_quad->shared_quad_state;

    // The video should be forced to an underlay mode, even there is nothing on
    // top.
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);

    // Check whether there is a replaced quad in the quad list.
    EXPECT_EQ(1U, root_pass->quad_list.size());

    // Check whether blend mode == kDstOut, color == black and still have the
    // rounded corner mask filter for the replaced solid quad.
    EXPECT_EQ(replaced_sqs->blend_mode, SkBlendMode::kDstOut);
    EXPECT_EQ(SolidColorDrawQuad::MaterialCast(replaced_quad)->color,
              SkColors::kBlack);
    EXPECT_TRUE(replaced_sqs->mask_filter_info.HasRoundedCorners());

    // The whole frame is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 256, 256), damage_rect_);
  }

  // Frame #1
  {
    auto pass = CreateRenderPass();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad with rounded corners below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;
    // Rounded corners
    pass->shared_quad_state_list.back()->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0.f, 0.f, 20.f, 30.f), 5.f));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 32, 32), gfx::Rect(0, 0, 256, 256)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    auto* root_pass = pass_list.back().get();
    auto* replaced_quad = root_pass->quad_list.back();
    auto* replaced_sqs = replaced_quad->shared_quad_state;

    // still in an underlay mode.
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);

    // Check whether the red quad on top and the replacedment of the YUV quad
    // are still in the render pass.
    EXPECT_EQ(2U, root_pass->quad_list.size());

    // Check whether blend mode is kDstOut, color is black, and still have the
    // rounded corner mask filter for the replaced solid quad.
    EXPECT_EQ(replaced_sqs->blend_mode, SkBlendMode::kDstOut);
    EXPECT_EQ(SolidColorDrawQuad::MaterialCast(replaced_quad)->color,
              SkColors::kBlack);
    EXPECT_TRUE(replaced_sqs->mask_filter_info.HasRoundedCorners());

    // Only the UI is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 32, 32), damage_rect_);
  }

  // Frame #2
  {
    auto pass = CreateRenderPass();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad with rounded corners below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    // Rounded corners
    pass->shared_quad_state_list.back()->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0.f, 0.f, 20.f, 30.f), 5.f));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 256, 256)};

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    auto* root_pass = pass_list.back().get();
    auto* replaced_quad = root_pass->quad_list.back();
    auto* replaced_sqs = replaced_quad->shared_quad_state;

    // still in an underlay mode.
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);

    // Check whether the red quad on top and the replacedment of the YUV quad
    // are still in the render pass.
    EXPECT_EQ(2U, root_pass->quad_list.size());

    // Check whether blend mode is kDstOut and color is black for the replaced
    // solid quad.
    EXPECT_EQ(replaced_sqs->blend_mode, SkBlendMode::kDstOut);
    EXPECT_EQ(SolidColorDrawQuad::MaterialCast(replaced_quad)->color,
              SkColors::kBlack);
    EXPECT_TRUE(replaced_sqs->mask_filter_info.HasRoundedCorners());

    // Zero root damage rect.
    EXPECT_TRUE(damage_rect_.IsEmpty());
  }
}

// If there are multiple yuv overlay quad candidates, no overlay will be
// promoted to save power.
TEST_F(DCLayerOverlayTest, MultipleYUVOverlays) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNoUndamagedOverlayPromotion);
  {
    auto pass = CreateRenderPass();
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 256, 256), SkColors::kWhite);

    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(10, 10, 80, 80);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;

    auto* second_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect second_rect(100, 100, 120, 120);
    second_video_quad->rect = second_rect;
    second_video_quad->visible_rect = second_rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 2;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;
    surface_damage_rect_list.push_back(gfx::Rect(0, 0, 256, 256));
    surface_damage_rect_list.push_back(video_quad->rect);
    surface_damage_rect_list.push_back(second_video_quad->rect);

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    // Skip overlays.
    EXPECT_EQ(0U, dc_layer_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 220, 220), damage_rect_);

    // Check whether all 3 quads including two YUV quads are still in the render
    // pass
    auto* root_pass = pass_list.back().get();
    int quad_count = root_pass->quad_list.size();
    EXPECT_EQ(3, quad_count);
  }
}

TEST_F(DCLayerOverlayTest, SetEnableDCLayers) {
  // Start without DC layers.
  overlay_processor_->set_using_dc_layers_for_testing(false);

  // Draw 60 frames with overlay video quads.
  for (int i = 0; i < 60; i++) {
    auto pass = CreateRenderPass();
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    SurfaceDamageRectList surface_damage_rect_list;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);

    // There will be full damage and SetEnableDCLayers(true) will be called on
    // the first frame.
    const gfx::Rect expected_damage =
        (i == 0) ? pass_list.back()->output_rect : gfx::Rect();

    if (i == 0)
      EXPECT_CALL(*output_surface_.get(), SetEnableDCLayers(true)).Times(1);
    else
      EXPECT_CALL(*output_surface_.get(), SetEnableDCLayers(_)).Times(0);

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(1, dc_layer_list.back().plane_z_order);
    EXPECT_EQ(damage_rect_, expected_damage);

    Mock::VerifyAndClearExpectations(output_surface_.get());
  }

  // Draw 65 frames without overlays.
  for (int i = 0; i < 65; i++) {
    auto pass = CreateRenderPass();

    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    auto* quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    quad->SetNew(pass->CreateAndAppendSharedQuadState(), damage_rect_,
                 damage_rect_, SkColors::kRed, false);

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    damage_rect_ = gfx::Rect(1, 1, 10, 10);

    // There will be full damage and SetEnableDCLayers(false) will be called
    // after 60 consecutive frames with no overlays. The first frame without
    // overlays will also have full damage, but no call to SetEnableDCLayers.
    const gfx::Rect expected_damage = (i == 0 || (i + 1) == 60)
                                          ? pass_list.back()->output_rect
                                          : damage_rect_;

    if (i + 1 == 60)
      EXPECT_CALL(*output_surface_.get(), SetEnableDCLayers(false)).Times(1);
    else
      EXPECT_CALL(*output_surface_.get(), SetEnableDCLayers(_)).Times(0);

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(0u, dc_layer_list.size());
    EXPECT_EQ(damage_rect_, expected_damage);

    Mock::VerifyAndClearExpectations(output_surface_.get());
  }
}

// Test that the video is forced to underlay if the expanded quad of pixel
// moving foreground filter is on top.
TEST_F(DCLayerOverlayTest, PixelMovingForegroundFilter) {
  AggregatedRenderPassList pass_list;

  // Create a non-root render pass with a pixel-moving foreground filter.
  AggregatedRenderPassId filter_render_pass_id{2};
  gfx::Rect filter_rect = gfx::Rect(260, 260, 100, 100);
  cc::FilterOperations blur_filter;
  blur_filter.Append(cc::FilterOperation::CreateBlurFilter(10.f));
  auto filter_pass = std::make_unique<AggregatedRenderPass>();
  filter_pass->SetNew(filter_render_pass_id, filter_rect, filter_rect,
                      gfx::Transform());
  filter_pass->filters = blur_filter;

  // Add a solid quad to the non-root pass.
  SharedQuadState* shared_state_filter =
      filter_pass->CreateAndAppendSharedQuadState();
  CreateSolidColorQuadAt(shared_state_filter, SkColors::kRed, filter_pass.get(),
                         filter_rect);
  shared_state_filter->opacity = 1.f;
  pass_list.push_back(std::move(filter_pass));

  // Create a root render pass.
  auto pass = CreateRenderPass();
  // Add a RenderPassDrawQuad to the root render pass.
  SharedQuadState* shared_quad_state_rpdq = pass->shared_quad_state_list.back();
  // The pixel-moving render pass draw quad itself (rpdq->rect) doesn't
  // intersect with kOverlayRect(0, 0, 256, 256), but the expanded draw quad
  // (rpdq->rect(260, 260, 100, 100) + MaximumPixelMovement (2 * 10.f) = (240,
  // 240, 140, 140)) does.

  CreateRenderPassDrawQuadAt(pass.get(), shared_quad_state_rpdq, filter_rect,
                             filter_render_pass_id);

  // Add a video quad to the root render pass.
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateFullscreenCandidateYUVVideoQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), shared_state, pass.get());
  // Make the root render pass output rect bigger enough to cover the video
  // quad kOverlayRect(0, 0, 256, 256) and the render pass draw quad (260, 260,
  // 100, 100).
  pass->output_rect = gfx::Rect(0, 0, 512, 512);

  OverlayCandidateList dc_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  render_pass_filters[filter_render_pass_id] = &blur_filter;

  pass_list.push_back(std::move(pass));
  // filter_rect + kOverlayRect. Both are damaged.
  gfx::Rect damage_rect_ = gfx::Rect(0, 0, 360, 360);
  shared_state->overlay_damage_index = 1;

  SurfaceDamageRectList surface_damage_rect_list = {filter_rect, kOverlayRect};

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, dc_layer_list.size());
  // Make sure the video is in an underlay mode if the overlay quad intersects
  // with (rpdq->rect + MaximumPixelMovement()).
  EXPECT_EQ(-1, dc_layer_list.back().plane_z_order);
  EXPECT_EQ(gfx::Rect(0, 0, 360, 360), damage_rect_);
}

// Test that the video is not promoted if a quad on top has backdrop filters.
TEST_F(DCLayerOverlayTest, BackdropFilter) {
  AggregatedRenderPassList pass_list;

  // Create a non-root render pass with a backdrop filter.
  AggregatedRenderPassId backdrop_filter_render_pass_id{2};
  gfx::Rect backdrop_filter_rect = gfx::Rect(200, 200, 100, 100);
  cc::FilterOperations backdrop_filter;
  backdrop_filter.Append(cc::FilterOperation::CreateBlurFilter(10.f));
  auto backdrop_filter_pass = std::make_unique<AggregatedRenderPass>();
  backdrop_filter_pass->SetNew(backdrop_filter_render_pass_id,
                               backdrop_filter_rect, backdrop_filter_rect,
                               gfx::Transform());
  backdrop_filter_pass->backdrop_filters = backdrop_filter;

  // Add a transparent solid quad to the non-root pass.
  SharedQuadState* shared_state_backdrop_filter =
      backdrop_filter_pass->CreateAndAppendSharedQuadState();
  CreateSolidColorQuadAt(shared_state_backdrop_filter, SkColors::kGreen,
                         backdrop_filter_pass.get(), backdrop_filter_rect);
  shared_state_backdrop_filter->opacity = 0.1f;
  pass_list.push_back(std::move(backdrop_filter_pass));

  // Create a root render pass.
  auto pass = CreateRenderPass();
  // Add a RenderPassDrawQuad to the root render pass, on top of the video.
  SharedQuadState* shared_quad_state_rpdq = pass->shared_quad_state_list.back();
  shared_quad_state_rpdq->opacity = 0.1f;
  // The render pass draw quad rpdq->rect intersects with the overlay quad
  // kOverlayRect(0, 0, 256, 256).
  CreateRenderPassDrawQuadAt(pass.get(), shared_quad_state_rpdq,
                             backdrop_filter_rect,
                             backdrop_filter_render_pass_id);

  // Add a video quad to the root render pass.
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateFullscreenCandidateYUVVideoQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), shared_state, pass.get());
  // Make the root render pass output rect bigger enough to cover the video
  // quad kOverlayRect(0, 0, 256, 256) and the render pass draw quad (200, 200,
  // 100, 100).
  pass->output_rect = gfx::Rect(0, 0, 512, 512);

  OverlayCandidateList dc_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  render_pass_backdrop_filters[backdrop_filter_render_pass_id] =
      &backdrop_filter;

  pass_list.push_back(std::move(pass));
  // backdrop_filter_rect + kOverlayRect. Both are damaged.
  gfx::Rect damage_rect_ = gfx::Rect(0, 0, 300, 300);
  shared_state->overlay_damage_index = 1;

  SurfaceDamageRectList surface_damage_rect_list = {backdrop_filter_rect,
                                                    kOverlayRect};

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
      &damage_rect_, &content_bounds_);

  // Make sure the video is not promoted if the overlay quad intersects
  // with the backdrop filter rpdq->rect.
  EXPECT_EQ(0U, dc_layer_list.size());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 300), damage_rect_);
}

// Test if overlay is not used when video capture is on.
TEST_F(DCLayerOverlayTest, VideoCapture) {
  // Frame #0
  {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 32, 32), gfx::Rect(0, 0, 256, 256)};
    // No video capture in this frame.
    overlay_processor_->SetIsVideoCaptureEnabled(false);
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    // Use overlay for the video quad.
    EXPECT_EQ(1U, dc_layer_list.size());
  }

  // Frame #1
  {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 256, 256)};

    // Now video capture is enabled.
    overlay_processor_->SetIsVideoCaptureEnabled(true);
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);

    // Should not use overlay for the video when video capture is on.
    EXPECT_EQ(0U, dc_layer_list.size());

    // Check whether both quads including the YUV quads are still in the render
    // pass.
    auto* root_pass = pass_list.back().get();
    int quad_count = root_pass->quad_list.size();
    EXPECT_EQ(2, quad_count);
  }
}

TEST_F(DCLayerOverlayTest, RenderPassRootTransformOverlay) {
  TestRenderPassRootTransform(/*is_overlay*/ true);
}

TEST_F(DCLayerOverlayTest, RenderPassRootTransformUnderlay) {
  TestRenderPassRootTransform(/*is_overlay*/ false);
}

// Tests processing overlays/underlays in a render pass that contains a
// non-identity transform to root.
void DCLayerOverlayTest::TestRenderPassRootTransform(bool is_overlay) {
  const gfx::Rect kOutputRect = gfx::Rect(0, 0, 256, 256);
  const gfx::Rect kVideoRect = gfx::Rect(0, 0, 100, 100);
  const gfx::Rect kOpaqueRect = gfx::Rect(90, 80, 15, 30);
  const gfx::Transform kRenderPassToRootTransform =
      gfx::Transform::MakeTranslation(27, 45);
  const SurfaceDamageRectList kSurfaceDamageRectList = {
      gfx::Rect(25, 20, 10, 10),    // above overlay
      gfx::Rect(27, 45, 100, 100),  // damage rect of video overlay
      gfx::Rect(30, 25, 50, 50)};   // below overlay
  const size_t kOverlayDamageIndex = 1;

  for (size_t frame = 0; frame < 3; frame++) {
    auto pass = CreateRenderPass();
    pass->transform_to_root_target = kRenderPassToRootTransform;
    pass->shared_quad_state_list.back()->overlay_damage_index =
        kOverlayDamageIndex;

    if (!is_overlay) {
      // Create a quad that occludes the video to force it to an underlay.
      CreateOpaqueQuadAt(resource_provider_.get(),
                         pass->shared_quad_state_list.back(), pass.get(),
                         kOpaqueRect, SkColors::kWhite);
    }

    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(kVideoRect);
    video_quad->visible_rect = video_quad->rect;

    std::vector<OverlayCandidate> dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = kSurfaceDamageRectList;

    damage_rect_ = kOutputRect;
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &dc_layer_list,
        &damage_rect_, &content_bounds_);
    LOG(INFO) << damage_rect_.ToString();

    EXPECT_EQ(dc_layer_list.size(), 1u);
    EXPECT_TRUE(
        absl::holds_alternative<gfx::Transform>(dc_layer_list[0].transform));
    EXPECT_EQ(absl::get<gfx::Transform>(dc_layer_list[0].transform),
              kRenderPassToRootTransform);
    if (is_overlay) {
      EXPECT_GT(dc_layer_list[0].plane_z_order, 0);
    } else {
      EXPECT_LT(dc_layer_list[0].plane_z_order, 0);
    }

    if (frame == 0) {
      // On the first frame, the damage rect should be unchanged since the
      // overlays are being processed for the first time.
      EXPECT_EQ(gfx::Rect(0, 0, 256, 256), damage_rect_);
    } else {
      // With the render pass to root transform, the video overlay should have
      // been translated to (27,45 100x100). The final damage rect should
      // include (25,20 10x10), which doesn't intersect the overlay. The
      // (30,25 50x50) surface damage is partially under the overlay, so the
      // overlay damage can be subtracted to become (30,25 50x15). The final
      // damage rect is (25,20 10x10) union (30,25 50x15).
      EXPECT_EQ(damage_rect_, gfx::Rect(25, 20, 55, 25));
    }
  }
}

}  // namespace
}  // namespace viz
