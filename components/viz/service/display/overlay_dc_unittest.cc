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
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "cc/paint/filter_operation.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/fake_output_surface_client.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_win.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/overlay_candidate_matchers.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::Rect kOverlayBottomRightRect(128, 128, 128, 128);

// An arbitrary render pass ID that can be treated as the implicit root pass ID
// by the test suites and helper functions.
const AggregatedRenderPassId kDefaultRootPassId{1};

std::unique_ptr<AggregatedRenderPass> CreateRenderPass(
    AggregatedRenderPassId render_pass_id = kDefaultRootPassId) {
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
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    SharedImageFormat format,
    bool is_overlay_candidate) {
  auto resource = TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D, gpu::SyncToken(), size, format,
      is_overlay_candidate);
  resource.color_space = color_space;
  resource.hdr_metadata = hdr_metadata;

  ResourceId resource_id =
      child_resource_provider->ImportResource(resource, base::DoNothing());

  return resource_id;
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          RasterContextProvider* child_context_provider,
                          const gfx::Size& size,
                          const gfx::ColorSpace& color_space,
                          const gfx::HDRMetadata& hdr_metadata,
                          SharedImageFormat format,
                          bool is_overlay_candidate) {
  ResourceId resource_id =
      CreateResourceInLayerTree(child_resource_provider, size, color_space,
                                hdr_metadata, format, is_overlay_candidate);

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

TextureDrawQuad* CreateTextureQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    bool is_overlay_candidate = true) {
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      rect.size(), gfx::ColorSpace(), gfx::HDRMetadata(),
      SinglePlaneFormat::kRGBA_8888, is_overlay_candidate);
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_quad_state, rect, /*visible_rect=*/rect,
               /*needs_blending=*/false, resource_id, /*premultiplied=*/true,
               /*top_left=*/gfx::PointF(0, 0),
               /*bottom_right=*/gfx::PointF(1, 1),
               /*background=*/SkColors::kBlack, /*flipped=*/false,
               /*nearest=*/false, /*secure_output=*/false,
               gfx::ProtectedVideoType::kClear);
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

TextureDrawQuad* CreateFullscreenCandidateYUVTextureQuad(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::ColorSpace& color_space = gfx::ColorSpace(),
    const gfx::HDRMetadata& hdr_metadata = gfx::HDRMetadata(),
    SharedImageFormat format = SinglePlaneFormat::kRGBA_8888) {
  gfx::Rect rect = render_pass->output_rect;
  gfx::Size resource_size_in_pixels = rect.size();
  bool is_overlay_candidate = true;
  ResourceId resource_id =
      CreateResource(parent_resource_provider, child_resource_provider,
                     child_context_provider, resource_size_in_pixels,
                     color_space, hdr_metadata, format, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, /*visible_rect=*/rect,
                       /*needs_blending=*/false, resource_id,
                       /*premultiplied=*/true,
                       /*top_left=*/gfx::PointF(0, 0),
                       /*bottom_right=*/gfx::PointF(1, 1),
                       /*background=*/SkColors::kBlack, /*flipped=*/false,
                       /*nearest=*/false, /*secure_output=*/false,
                       gfx::ProtectedVideoType::kClear);
  // Content is video frame type.
  overlay_quad->is_video_frame = true;

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

class OverlayProcessorTestBase : public testing::Test {
 protected:
  OverlayProcessorTestBase() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    output_surface_ = FakeSkiaOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();
    lock_set_for_external_use_.emplace(resource_provider_.get(),
                                       output_surface_.get());

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentSequence();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
  }

  void TearDown() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
    child_resource_provider_ = nullptr;
    child_provider_ = nullptr;
    lock_set_for_external_use_.reset();
    resource_provider_ = nullptr;
    output_surface_ = nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeSkiaOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<DisplayResourceProviderSkia> resource_provider_;
  std::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
};

class DCLayerOverlayProcessorTest : public OverlayProcessorTestBase {
 protected:
  void InitializeDCLayerOverlayProcessor(int allowed_yuv_overlay_count = 1) {
    CHECK(!dc_layer_overlay_processor_);

    // With disable_video_overlay_if_moving enabled, videos are required to be
    // stable for a certain number of frames to be considered for overlay
    // promotion. This complicates tests since it adds behavior dependent on
    // the number of times |Process| is called.
    dc_layer_overlay_processor_ = std::make_unique<DCLayerOverlayProcessor>(
        allowed_yuv_overlay_count,
        /*disable_video_overlay_if_moving=*/false,
        /*skip_initialization_for_testing=*/true);

    dc_layer_overlay_processor_
        ->set_frames_since_last_qualified_multi_overlays_for_testing(5);
  }

  void TearDown() override {
    dc_layer_overlay_processor_.reset();
    OverlayProcessorTestBase::TearDown();
  }

  DCLayerOverlayProcessor::RenderPassOverlayData ProcessRootPassForOverlays(
      const AggregatedRenderPassList* render_passes,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list_in_root_space) {
    DCLayerOverlayProcessor::RenderPassOverlayDataMap
        render_pass_overlay_data_map;
    auto emplace_pair = render_pass_overlay_data_map.emplace(
        render_passes->back().get(),
        DCLayerOverlayProcessor::RenderPassOverlayData());
    DCLayerOverlayProcessor::RenderPassOverlayData&
        root_render_pass_overlay_data = emplace_pair.first->second;

    root_render_pass_overlay_data.damage_rect =
        render_passes->back()->damage_rect;

    dc_layer_overlay_processor_->Process(
        resource_provider_.get(), render_pass_filters,
        render_pass_backdrop_filters, surface_damage_rect_list_in_root_space,
        /*is_page_fullscreen_mode=*/false, render_pass_overlay_data_map);

    // |DCLayerOverlayProcessor::Process| doesn't guarantee a specific ordering
    // for its overlays so we sort front-to-back so tests can make expectations
    // with the same ordering as the input draw quads.
    base::ranges::sort(root_render_pass_overlay_data.promoted_overlays,
                       base::ranges::greater(),
                       &OverlayCandidate::plane_z_order);

    return std::move(root_render_pass_overlay_data);
  }

  void TestRenderPassRootTransform(bool is_overlay);

  std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor_;
};

TEST_F(DCLayerOverlayProcessorTest, DisableVideoOverlayIfMovingWorkaround) {
  InitializeDCLayerOverlayProcessor();
  auto ProcessForOverlaysSingleVideoRectWithOffset =
      [&](gfx::Vector2d video_rect_offset, bool is_hdr = false,
          bool is_sdr_to_hdr = false) {
        auto pass = CreateRenderPass();

        SharedImageFormat format = SinglePlaneFormat::kRGBA_8888;
        gfx::ColorSpace color_space;
        gfx::HDRMetadata hdr_metadata;

        if (is_hdr) {
          // Content is 10bit P010 content.
          format = MultiPlaneFormat::kP010;
          // Content has HDR10 colorspace.
          color_space = gfx::ColorSpace::CreateHDR10();

          // Content has valid HDR metadata.
          hdr_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(1000, 400);
          hdr_metadata.smpte_st_2086 = gfx::HdrMetadataSmpteSt2086(
              SkNamedPrimariesExt::kRec2020, 1000, 0.0001);

          // Render Pass has HDR content usage.
          pass->content_color_usage = gfx::ContentColorUsage::kHDR;

          // Device has RGB10A2 overlay support.
          gl::SetDirectCompositionScaledOverlaysSupportedForTesting(true);

          // Device has HDR-enabled display and no non-HDR-enabled display.
          dc_layer_overlay_processor_
              ->set_system_hdr_disabled_on_any_display_for_testing(false);

          // Device has video processor support.
          dc_layer_overlay_processor_
              ->set_has_p010_video_processor_support_for_testing(true);
        } else if (is_sdr_to_hdr) {
          // Content is 8bit NV12 content.
          format = MultiPlaneFormat::kNV12;
          // Content has 709 colorspace.
          color_space = gfx::ColorSpace::CreateREC709();

          // Render Pass has SDR content usage.
          pass->content_color_usage = gfx::ContentColorUsage::kSRGB;

          // Device is not using battery power.
          dc_layer_overlay_processor_->set_is_on_battery_power_for_testing(
              false);

          // Device has at least one HDR-enabled display.
          dc_layer_overlay_processor_
              ->set_system_hdr_enabled_on_any_display_for_testing(true);

          // Device has video processor auto hdr support.
          dc_layer_overlay_processor_
              ->set_has_auto_hdr_video_processor_support_for_testing(true);
        }

        auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
            resource_provider_.get(), child_resource_provider_.get(),
            child_provider_.get(), pass->shared_quad_state_list.back(),
            pass.get(), color_space, hdr_metadata, format);
        video_quad->rect = gfx::Rect(0, 0, 10, 10) + video_rect_offset;
        video_quad->visible_rect = gfx::Rect(0, 0, 10, 10) + video_rect_offset;

        OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
        OverlayProcessorInterface::FilterOperationsMap
            render_pass_backdrop_filters;

        AggregatedRenderPassList pass_list;
        pass_list.push_back(std::move(pass));

        auto overlay_data = ProcessRootPassForOverlays(
            &pass_list, render_pass_filters, render_pass_backdrop_filters, {});

        return std::move(overlay_data.promoted_overlays);
      };

  {
    dc_layer_overlay_processor_
        ->set_disable_video_overlay_if_moving_for_testing(false);
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset({0, 0}).size());
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset({1, 0}).size());
  }

  {
    dc_layer_overlay_processor_
        ->set_disable_video_overlay_if_moving_for_testing(true);
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

  {
    dc_layer_overlay_processor_
        ->set_disable_video_overlay_if_moving_for_testing(true);
    // We expect an overlay promotion after a couple frames of no movement
    for (int i = 0; i < 10; i++) {
      ProcessForOverlaysSingleVideoRectWithOffset({0, 0}, /*is_hdr=*/false,
                                                  /*is_sdr_to_hdr*/ true)
          .size();
    }
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset(
                      {0, 0}, /*is_hdr=*/false, /*is_sdr_to_hdr*/ true)
                      .size());
    // We still expect an overlay promotion for SDR video when auto hdr is
    // enabled and when moving to ensure uniform tone mapping results between
    // viz and GPU driver.
    EXPECT_EQ(1U, ProcessForOverlaysSingleVideoRectWithOffset(
                      {1, 0}, /*is_hdr=*/false, /*is_sdr_to_hdr*/ true)
                      .size());
  }

  {
    dc_layer_overlay_processor_
        ->set_disable_video_overlay_if_moving_for_testing(true);
    // We expect an overlay promotion after a couple frames of no movement
    for (int i = 0; i < 10; i++) {
      ProcessForOverlaysSingleVideoRectWithOffset({0, 0}, /*is_hdr=*/true)
          .size();
    }
    EXPECT_EQ(
        1U, ProcessForOverlaysSingleVideoRectWithOffset({0, 0}, /*is_hdr=*/true)
                .size());
    // We still expect an overlay promotion for HDR video when moving to
    // ensure uniform tone mapping results between viz and GPU driver.
    EXPECT_EQ(
        1U, ProcessForOverlaysSingleVideoRectWithOffset({1, 0}, /*is_hdr=*/true)
                .size());
  }
}

TEST_F(DCLayerOverlayProcessorTest, Occluded) {
  InitializeDCLayerOverlayProcessor();
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
    auto* first_video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force the quad to use hw overlay
    first_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;

    SharedQuadState* third_shared_state =
        pass->CreateAndAppendSharedQuadState();
    third_shared_state->overlay_damage_index = 2;
    auto* second_video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force the quad to use hw overlay
    second_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
    second_video_quad->rect.set_origin(gfx::Point(2, 2));
    second_video_quad->visible_rect.set_origin(gfx::Point(2, 2));

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(1, 1, 10, 10), gfx::Rect(0, 0, 0, 0), gfx::Rect(0, 0, 0, 0)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    EXPECT_EQ(2U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.front().plane_z_order);
    EXPECT_EQ(-2, overlay_data.promoted_overlays.back().plane_z_order);
    // Entire underlay rect must be redrawn.
    EXPECT_EQ(gfx::Rect(0, 0, 256, 256), overlay_data.damage_rect);
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
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force the quad to use hw overlay
    video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;

    SharedQuadState* third_shared_state =
        pass->CreateAndAppendSharedQuadState();
    third_shared_state->overlay_damage_index = 2;
    auto* second_video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    second_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
    second_video_quad->rect.set_origin(gfx::Point(2, 2));
    second_video_quad->visible_rect.set_origin(gfx::Point(2, 2));

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(1, 1, 10, 10), gfx::Rect(0, 0, 0, 0), gfx::Rect(0, 0, 0, 0)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    EXPECT_EQ(2U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.front().plane_z_order);
    EXPECT_EQ(-2, overlay_data.promoted_overlays.back().plane_z_order);

    // The underlay rectangle is the same, so the damage for first video quad is
    // contained within the combined occluding rects for this and the last
    // frame. Second video quad also adds its damage.
    EXPECT_EQ(gfx::Rect(1, 1, 10, 10), overlay_data.damage_rect);
  }
}

TEST_F(DCLayerOverlayProcessorTest, DamageRectWithoutVideoDamage) {
  InitializeDCLayerOverlayProcessor();
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
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    pass->damage_rect = gfx::Rect(210, 210, 20, 20);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(210, 210, 20, 20), gfx::Rect(0, 0, 0, 0)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);
    // All rects must be redrawn at the first frame.
    EXPECT_EQ(gfx::Rect(0, 0, 230, 230), overlay_data.damage_rect);
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
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    pass->damage_rect = gfx::Rect(210, 210, 20, 20);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(210, 210, 20, 20), gfx::Rect(0, 0, 0, 0)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);
    // Only the non-overlay damaged rect need to be drawn by the gl compositor
    EXPECT_EQ(gfx::Rect(210, 210, 20, 20), overlay_data.damage_rect);
  }
}

TEST_F(DCLayerOverlayProcessorTest, DamageRect) {
  InitializeDCLayerOverlayProcessor();
  for (int i = 0; i < 2; i++) {
    SCOPED_TRACE(base::StringPrintf("Frame %d", i));
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state = pass->shared_quad_state_list.back();
    shared_quad_state->overlay_damage_index = 0;
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(1, overlay_data.promoted_overlays.back().plane_z_order);
    // Damage rect should be unchanged on initial frame because of resize, but
    // should be empty on the second frame because everything was put in a
    // layer.
    if (i == 1)
      EXPECT_TRUE(overlay_data.damage_rect.IsEmpty());
    else
      EXPECT_EQ(gfx::Rect(1, 1, 10, 10), overlay_data.damage_rect);
  }
}

TEST_F(DCLayerOverlayProcessorTest, ClipRect) {
  InitializeDCLayerOverlayProcessor();
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
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), shared_state, pass.get());
    // Clipped rect shouldn't be overlapped by clipped opaque quad rect.
    shared_state->clip_rect = gfx::Rect(0, 0, 100, 3);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 3, 10, 8),
                                                      gfx::Rect(1, 1, 10, 2)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    // Because of clip rects the overlay isn't occluded and shouldn't be an
    // underlay.
    EXPECT_EQ(1, overlay_data.promoted_overlays.back().plane_z_order);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 3),
              overlay_data.promoted_overlays.back().clip_rect);
    if (i == 1) {
      // The damage rect should only contain contents that aren't in the
      // clipped overlay rect.
      EXPECT_EQ(gfx::Rect(1, 3, 10, 8), overlay_data.damage_rect);
    }
  }
}

TEST_F(DCLayerOverlayProcessorTest, TransparentOnTop) {
  InitializeDCLayerOverlayProcessor();
  // Process twice. The second time through the overlay list shouldn't change,
  // which will allow the damage rect to reflect just the changes in that
  // frame.
  for (size_t i = 0; i < 2; ++i) {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    pass->shared_quad_state_list.back()->opacity = 0.5f;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(1, overlay_data.promoted_overlays.back().plane_z_order);
    // Quad isn't opaque, so underlying damage must remain the same.
    EXPECT_EQ(gfx::Rect(1, 1, 10, 10), overlay_data.damage_rect);
  }
}

TEST_F(DCLayerOverlayProcessorTest, UnderlayDamageRectWithQuadOnTopUnchanged) {
  InitializeDCLayerOverlayProcessor();
  for (int i = 0; i < 3; i++) {
    auto pass = CreateRenderPass();
    // Add a solid color quad on top
    SharedQuadState* shared_state_on_top = pass->shared_quad_state_list.back();
    CreateSolidColorQuadAt(shared_state_on_top, SkColors::kRed, pass.get(),
                           kOverlayBottomRightRect);

    SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->opacity = 1.f;
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), shared_state, pass.get());

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = kOverlayRect;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    shared_state->overlay_damage_index = 1;

    // The quad on top does not give damage on the third frame
    SurfaceDamageRectList surface_damage_rect_list = {kOverlayBottomRightRect,
                                                      kOverlayRect};
    if (i == 2) {
      surface_damage_rect_list[0] = gfx::Rect();
    }

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);
    // Damage rect should be unchanged on initial frame, but should be reduced
    // to the size of quad on top, and empty on the third frame.
    if (i == 0)
      EXPECT_EQ(kOverlayRect, overlay_data.damage_rect);
    else if (i == 1)
      EXPECT_EQ(kOverlayBottomRightRect, overlay_data.damage_rect);
    else if (i == 2)
      EXPECT_EQ(gfx::Rect(), overlay_data.damage_rect);
  }
}

// Test whether quads with rounded corners are supported.
TEST_F(DCLayerOverlayProcessorTest, RoundedCorners) {
  InitializeDCLayerOverlayProcessor();
  // Frame #0
  {
    auto pass = CreateRenderPass();

    // Create a video YUV quad with rounded corner, nothing on top.
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    // Rounded corners
    pass->shared_quad_state_list.back()->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0.f, 0.f, 20.f, 30.f), 5.f));

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 256, 256)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    auto* root_pass = pass_list.back().get();
    auto* replaced_quad = root_pass->quad_list.back();
    auto* replaced_sqs = replaced_quad->shared_quad_state;

    // The video should be forced to an underlay mode, even there is nothing on
    // top.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);

    // Check whether there is a replaced quad in the quad list.
    EXPECT_EQ(1U, root_pass->quad_list.size());

    // Check whether blend mode == kDstOut, color == black and still have the
    // rounded corner mask filter for the replaced solid quad.
    EXPECT_EQ(replaced_sqs->blend_mode, SkBlendMode::kDstOut);
    EXPECT_EQ(SolidColorDrawQuad::MaterialCast(replaced_quad)->color,
              SkColors::kBlack);
    EXPECT_TRUE(replaced_sqs->mask_filter_info.HasRoundedCorners());

    // The whole frame is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 256, 256), overlay_data.damage_rect);
  }

  // Frame #1
  {
    auto pass = CreateRenderPass();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad with rounded corners below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;
    // Rounded corners
    pass->shared_quad_state_list.back()->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0.f, 0.f, 20.f, 30.f), 5.f));

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 32, 32), gfx::Rect(0, 0, 256, 256)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    auto* root_pass = pass_list.back().get();
    auto* replaced_quad = root_pass->quad_list.back();
    auto* replaced_sqs = replaced_quad->shared_quad_state;

    // still in an underlay mode.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);

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
    EXPECT_EQ(gfx::Rect(0, 0, 32, 32), overlay_data.damage_rect);
  }

  // Frame #2
  {
    auto pass = CreateRenderPass();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad with rounded corners below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    // Rounded corners
    pass->shared_quad_state_list.back()->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0.f, 0.f, 20.f, 30.f), 5.f));

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 256, 256)};

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    auto* root_pass = pass_list.back().get();
    auto* replaced_quad = root_pass->quad_list.back();
    auto* replaced_sqs = replaced_quad->shared_quad_state;

    // still in an underlay mode.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);

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
    EXPECT_TRUE(overlay_data.damage_rect.IsEmpty());
  }
}

// If there are multiple yuv overlay quad candidates, no overlay will be
// promoted to save power.
TEST_F(DCLayerOverlayProcessorTest, MultipleYUVOverlays) {
  InitializeDCLayerOverlayProcessor();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNoUndamagedOverlayPromotion);
  {
    auto pass = CreateRenderPass();
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 256, 256), SkColors::kWhite);

    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(10, 10, 80, 80);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;

    auto* second_video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect second_rect(100, 100, 120, 120);
    second_video_quad->rect = second_rect;
    second_video_quad->visible_rect = second_rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 2;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;
    surface_damage_rect_list.push_back(gfx::Rect(0, 0, 256, 256));
    surface_damage_rect_list.push_back(video_quad->rect);
    surface_damage_rect_list.push_back(second_video_quad->rect);

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());
    EXPECT_EQ(gfx::Rect(0, 0, 220, 220), overlay_data.damage_rect);

    // Check whether all 3 quads including two YUV quads are still in the render
    // pass
    auto* root_pass = pass_list.back().get();
    int quad_count = root_pass->quad_list.size();
    EXPECT_EQ(3, quad_count);
  }
}

// Test that the video is forced to underlay if the expanded quad of pixel
// moving foreground filter is on top.
TEST_F(DCLayerOverlayProcessorTest, PixelMovingForegroundFilter) {
  InitializeDCLayerOverlayProcessor();
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
  // (rpdq->rect(260, 260, 100, 100) + blur filter pixel movement (2 * 10.f) =
  // (240, 240, 140, 140)) does.

  CreateRenderPassDrawQuadAt(pass.get(), shared_quad_state_rpdq, filter_rect,
                             filter_render_pass_id);

  // Add a video quad to the root render pass.
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateFullscreenCandidateYUVTextureQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), shared_state, pass.get());
  // Make the root render pass output rect bigger enough to cover the video
  // quad kOverlayRect(0, 0, 256, 256) and the render pass draw quad (260, 260,
  // 100, 100).
  pass->output_rect = gfx::Rect(0, 0, 512, 512);

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  render_pass_filters[filter_render_pass_id] = &blur_filter;

  // filter_rect + kOverlayRect. Both are damaged.
  pass->damage_rect = gfx::Rect(0, 0, 360, 360);
  pass_list.push_back(std::move(pass));
  shared_state->overlay_damage_index = 1;

  SurfaceDamageRectList surface_damage_rect_list = {filter_rect, kOverlayRect};

  auto overlay_data = ProcessRootPassForOverlays(
      &pass_list, render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list));

  EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
  // Make sure the video is in an underlay mode if the overlay quad intersects
  // with expanded rpdq->rect.
  EXPECT_EQ(-1, overlay_data.promoted_overlays.back().plane_z_order);
  EXPECT_EQ(gfx::Rect(0, 0, 360, 360), overlay_data.damage_rect);
}

// Test that the video is not promoted if a quad on top has backdrop filters.
TEST_F(DCLayerOverlayProcessorTest, BackdropFilter) {
  InitializeDCLayerOverlayProcessor();
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
  CreateFullscreenCandidateYUVTextureQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), shared_state, pass.get());
  // Make the root render pass output rect bigger enough to cover the video
  // quad kOverlayRect(0, 0, 256, 256) and the render pass draw quad (200, 200,
  // 100, 100).
  pass->output_rect = gfx::Rect(0, 0, 512, 512);

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  render_pass_backdrop_filters[backdrop_filter_render_pass_id] =
      &backdrop_filter;

  // backdrop_filter_rect + kOverlayRect. Both are damaged.
  pass->damage_rect = gfx::Rect(0, 0, 300, 300);
  pass_list.push_back(std::move(pass));
  shared_state->overlay_damage_index = 1;

  SurfaceDamageRectList surface_damage_rect_list = {backdrop_filter_rect,
                                                    kOverlayRect};

  auto overlay_data = ProcessRootPassForOverlays(
      &pass_list, render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list));

  // Make sure the video is not promoted if the overlay quad intersects
  // with the backdrop filter rpdq->rect.
  EXPECT_EQ(0U, overlay_data.promoted_overlays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 300), overlay_data.damage_rect);
}

// Test if overlay is not used when video capture is on.
TEST_F(DCLayerOverlayProcessorTest, VideoCapture) {
  InitializeDCLayerOverlayProcessor();
  // Frame #0
  {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back();
    // Create a solid quad.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 32, 32), gfx::Rect(0, 0, 256, 256)};
    // No video capture in this frame.
    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Use overlay for the video quad.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
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
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 0;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 256, 256);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {
        gfx::Rect(0, 0, 256, 256)};

    // Now video capture is enabled.
    pass_list.back()->video_capture_enabled = true;
    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should not use overlay for the video when video capture is on.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());

    // Check whether both quads including the YUV quads are still in the render
    // pass.
    auto* root_pass = pass_list.back().get();
    int quad_count = root_pass->quad_list.size();
    EXPECT_EQ(2, quad_count);
  }
}

// Check that video capture on a non-root pass does not affect overlay promotion
// on the root pass itself.
TEST_F(DCLayerOverlayProcessorTest, VideoCaptureOnIsolatedRenderPass) {
  InitializeDCLayerOverlayProcessor();

  AggregatedRenderPassList pass_list;

  // Create a render pass with video capture enabled. This could represent e.g.
  // capture of a background tab for stream.
  {
    auto pass = CreateRenderPass();
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 32, 32), SkColors::kRed);
    pass->video_capture_enabled = true;
    pass_list.push_back(std::move(pass));
  }

  // Create a root render pass with a video quad that can be promoted to
  // overlay.
  {
    auto root_pass = CreateRenderPass();
    // Create a solid quad.
    CreateOpaqueQuadAt(
        resource_provider_.get(), root_pass->shared_quad_state_list.back(),
        root_pass.get(), gfx::Rect(0, 0, 32, 32), SkColors::kRed);

    // Create a video YUV quad below the red solid quad.
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), root_pass->shared_quad_state_list.back(),
        root_pass.get());
    gfx::Rect rect(0, 0, 256, 256);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    root_pass->shared_quad_state_list.back()->overlay_damage_index = 0;
    root_pass->damage_rect = gfx::Rect(0, 0, 256, 256);
    pass_list.push_back(std::move(root_pass));
  }

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

  SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(0, 0, 256, 256)};

  auto overlay_data = ProcessRootPassForOverlays(
      &pass_list, render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list));

  EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
}

TEST_F(DCLayerOverlayProcessorTest, RenderPassRootTransformOverlay) {
  TestRenderPassRootTransform(/*is_overlay*/ true);
}

TEST_F(DCLayerOverlayProcessorTest, RenderPassRootTransformUnderlay) {
  TestRenderPassRootTransform(/*is_overlay*/ false);
}

// Tests processing overlays/underlays in a render pass that contains a
// non-identity transform to root.
void DCLayerOverlayProcessorTest::TestRenderPassRootTransform(bool is_overlay) {
  InitializeDCLayerOverlayProcessor();
  const gfx::Rect kOutputRect = gfx::Rect(0, 0, 256, 256);
  const gfx::Rect kVideoRect = gfx::Rect(0, 0, 100, 100);
  const gfx::Rect kOpaqueRect = gfx::Rect(90, 80, 15, 30);
  const gfx::Transform kRenderPassToRootTransform =
      gfx::Transform::MakeTranslation(20, 45);
  // Surface damages in root space.
  const SurfaceDamageRectList kSurfaceDamageRectList = {
      // On top and does not intersect overlay. Translates to (110,5 20x10) in
      // render pass space.
      gfx::Rect(130, 50, 20, 10),
      // The video overlay damage rect. (0,0 100x100) in render pass space.
      gfx::Rect(20, 45, 100, 100),
      // Under and intersects the overlay. Translates to (95,25 20x10) in
      // render pass space.
      gfx::Rect(115, 70, 20, 10)};
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

    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(kVideoRect);
    video_quad->visible_rect = video_quad->rect;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = kOutputRect;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = kSurfaceDamageRectList;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));
    LOG(INFO) << "frame " << frame
              << " damage rect: " << overlay_data.damage_rect.ToString();

    EXPECT_EQ(overlay_data.promoted_overlays.size(), 1u);
    EXPECT_TRUE(absl::holds_alternative<gfx::Transform>(
        overlay_data.promoted_overlays[0].transform));
    EXPECT_EQ(
        absl::get<gfx::Transform>(overlay_data.promoted_overlays[0].transform),
        kRenderPassToRootTransform);
    if (is_overlay) {
      EXPECT_GT(overlay_data.promoted_overlays[0].plane_z_order, 0);
    } else {
      EXPECT_LT(overlay_data.promoted_overlays[0].plane_z_order, 0);
    }

    if (frame == 0) {
      // On the first frame, the damage rect should be unchanged since the
      // overlays are being processed for the first time.
      EXPECT_EQ(gfx::Rect(0, 0, 256, 256), overlay_data.damage_rect);
    } else {
      // To calculate the damage rect in root space, we first subtract the video
      // damage from (115,70 20x10) since this damage is under the video. This
      // results in (120,70 20x10). This then gets unioned with (130,50 20x10),
      // which doesn't intersect the video. This results in (120,50 30x30).
      // The damage rect returned from the DCLayerOverlayProcessor is in
      // render pass space, so we apply the (20, 45) inverse transform,
      // resulting in (100,5 30x30).
      EXPECT_EQ(overlay_data.damage_rect, gfx::Rect(100, 5, 30, 30));
    }
  }
}

// Tests processing overlays/underlays on multiple render passes per frame,
// where only one render pass has an overlay.
TEST_F(DCLayerOverlayProcessorTest, MultipleRenderPassesOneOverlay) {
  InitializeDCLayerOverlayProcessor(/*allowed_yuv_overlay_count*/ 1);
  const gfx::Rect output_rect = {0, 0, 256, 256};
  const size_t num_render_passes = 3;
  for (size_t frame = 0; frame < 3; frame++) {
    AggregatedRenderPassList render_passes;  // Used to keep render passes alive
    DCLayerOverlayProcessor::RenderPassOverlayDataMap
        render_pass_overlay_data_map;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    SurfaceDamageRectList surface_damage_rect_list;

    // Create 3 render passes, with only one containing an overlay candidate.
    for (size_t id = 1; id <= num_render_passes; id++) {
      auto pass = CreateRenderPass(AggregatedRenderPassId{id});
      pass->transform_to_root_target = gfx::Transform::MakeTranslation(id, 0);

      gfx::Rect quad_rect_in_root_space =
          gfx::Rect(0, 0, id * 16, pass->output_rect.height());

      if (id == 1) {
        // Create an overlay quad in the first render pass.
        auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
            resource_provider_.get(), child_resource_provider_.get(),
            child_provider_.get(), pass->shared_quad_state_list.back(),
            pass.get());
        gfx::Rect quad_rect_in_quad_space =
            pass->transform_to_root_target
                .InverseMapRect(quad_rect_in_root_space)
                .value();
        video_quad->rect = quad_rect_in_quad_space;
        video_quad->visible_rect = quad_rect_in_quad_space;
        pass->shared_quad_state_list.back()->overlay_damage_index = id - 1;
      } else {
        // Create a quad that's not an overlay.
        CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                               SkColors::kBlue, pass.get(),
                               pass->transform_to_root_target
                                   .InverseMapRect(quad_rect_in_root_space)
                                   .value());
      }

      surface_damage_rect_list.emplace_back(quad_rect_in_root_space);
      render_pass_overlay_data_map[pass.get()].damage_rect = output_rect;
      render_passes.emplace_back(std::move(pass));
    }

    surface_damage_rect_list.emplace_back(0, 0, 256, 256);

    dc_layer_overlay_processor_->Process(
        resource_provider_.get(), render_pass_filters,
        render_pass_backdrop_filters, std::move(surface_damage_rect_list),
        /*is_page_fullscreen_mode=*/false, render_pass_overlay_data_map);

    for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
      LOG(INFO) << "frame " << frame << " render pass " << render_pass->id
                << " damage rect : " << overlay_data.damage_rect.ToString();
      LOG(INFO) << "frame " << frame << " render pass " << render_pass->id
                << " number of overlays: "
                << overlay_data.promoted_overlays.size();

      if (render_pass->id == AggregatedRenderPassId(1)) {
        // The render pass that contains an overlay.
        EXPECT_EQ(overlay_data.promoted_overlays.size(), 1u);
        EXPECT_EQ(absl::get<gfx::Transform>(
                      overlay_data.promoted_overlays[0].transform),
                  gfx::Transform::MakeTranslation(1, 0));
        EXPECT_GT(overlay_data.promoted_overlays[0].plane_z_order, 0);

        // The rect of the candidate should be in render pass space, which is an
        // arbitrary space. Combining with the render pass to root transform
        // results in a rect in root space. The transform is defined above as an
        // x translation of -1.
        EXPECT_EQ(overlay_data.promoted_overlays[0].display_rect,
                  gfx::RectF(-1, 0, 16, 256));

        if (frame == 0) {
          // On the first frame, the damage rect should be unchanged since the
          // overlays are being processed for the first time.
          EXPECT_EQ(overlay_data.damage_rect, output_rect);
        } else {
          // On subsequent frames, the video rect should be subtracted from
          // the damage rect. The x coordinate is 15 instead of 16 because of
          // the root_to_transform_target. The surface_damage_rect_list damages
          // are in root space, while the damage_rect output is in render pass
          // space.
          EXPECT_EQ(overlay_data.damage_rect, gfx::Rect(15, 0, 240, 256));
        }
      } else {
        // All other render passes do not have overlays.
        EXPECT_TRUE(overlay_data.promoted_overlays.empty());

        // With no overlays, the damage should be unchanged since there are no
        // overlays to subtract.
        EXPECT_EQ(overlay_data.damage_rect, output_rect);
      }
    }
  }
}

// Tests processing overlays/underlays on multiple render passes per frame, with
// each render pass having an overlay. This exceeds the maximum allowed number
// of overlays, so all overlays should be rejected.
TEST_F(DCLayerOverlayProcessorTest,
       MultipleRenderPassesExceedsOverlayAllowance) {
  const gfx::Rect output_rect = {0, 0, 256, 256};
  const size_t num_render_passes = 3;
  InitializeDCLayerOverlayProcessor(num_render_passes - 1);
  for (size_t frame = 1; frame <= 3; frame++) {
    AggregatedRenderPassList
        render_passes;  // Used to keep render passes alive.
    DCLayerOverlayProcessor::RenderPassOverlayDataMap
        render_pass_overlay_data_map;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    SurfaceDamageRectList surface_damage_rect_list;

    // Create 3 render passes that all have a video overlay candidate. Start
    // at the frame number so that we switch up the render pass IDs to verify
    // that render passes that do not exist are not kept in
    // |DCLayerOverlayProcessor::previous_frame_render_pass_states_|.
    for (size_t id = frame; id < num_render_passes + frame; id++) {
      auto pass = CreateRenderPass(AggregatedRenderPassId{id});
      pass->transform_to_root_target = gfx::Transform::MakeTranslation(id, 0);
      pass->shared_quad_state_list.back()->overlay_damage_index = id - 1;

      gfx::Rect video_rect_in_root_space =
          gfx::Rect(0, 0, id * 16, pass->output_rect.height());

      gfx::Rect video_rect_in_render_pass_space =
          pass->transform_to_root_target
              .InverseMapRect(video_rect_in_root_space)
              .value();
      auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.back(),
          pass.get());
      video_quad->rect = video_rect_in_render_pass_space;
      video_quad->visible_rect = video_rect_in_render_pass_space;

      surface_damage_rect_list.emplace_back(video_rect_in_root_space);
      render_pass_overlay_data_map[pass.get()].damage_rect = output_rect;
      render_passes.emplace_back(std::move(pass));
    }

    surface_damage_rect_list.emplace_back(0, 0, 256, 256);

    dc_layer_overlay_processor_->Process(
        resource_provider_.get(), render_pass_filters,
        render_pass_backdrop_filters, std::move(surface_damage_rect_list),
        /*is_page_fullscreen_mode=*/false, render_pass_overlay_data_map);

    // Verify that the previous frame states contain only 3 render passes and
    // that they have the IDs that we set them to.
    EXPECT_EQ(
        3U,
        dc_layer_overlay_processor_->get_previous_frame_render_pass_count());
    std::vector<AggregatedRenderPassId> previous_frame_render_pass_ids =
        dc_layer_overlay_processor_->get_previous_frame_render_pass_ids();
    std::sort(previous_frame_render_pass_ids.begin(),
              previous_frame_render_pass_ids.end());
    for (size_t id = frame; id < num_render_passes + frame; id++) {
      EXPECT_EQ(id, previous_frame_render_pass_ids[id - frame].value());
    }

    for (auto& [render_pass, overlay_data] : render_pass_overlay_data_map) {
      LOG(INFO) << "frame " << frame << " render pass " << render_pass->id
                << " damage rect : " << overlay_data.damage_rect.ToString();
      LOG(INFO) << "frame " << frame << " render pass " << render_pass->id
                << " number of overlays: "
                << overlay_data.promoted_overlays.size();

      // Since there is more than one overlay, all overlays should be rejected.
      EXPECT_EQ(overlay_data.promoted_overlays.size(), 0u);

      // With no overlays, the damage should be unchanged since there are no
      // overlays to subtract.
      EXPECT_EQ(overlay_data.damage_rect, output_rect);
    }
  }
}

// When there are multiple videos intersected with each other, only the topmost
// of them should be considered as "overlay".
TEST_F(DCLayerOverlayProcessorTest, MultipleYUVOverlaysIntersected) {
  InitializeDCLayerOverlayProcessor(/*allowed_yuv_overlay_count=*/2);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNoUndamagedOverlayPromotion);
  {
    auto pass = CreateRenderPass();

    // Video 1: Topmost video.
    auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(150, 150, 50, 50);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 1;

    // Video 2: Intersected with and under the 1st video.
    auto* second_video_quad = CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect second_rect(100, 100, 120, 120);
    second_video_quad->rect = second_rect;
    second_video_quad->visible_rect = second_rect;
    pass->shared_quad_state_list.back()->overlay_damage_index = 2;

    // Background.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 256, 256), SkColors::kWhite);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    surface_damage_rect_list.push_back(video_quad->rect);
    surface_damage_rect_list.push_back(second_video_quad->rect);
    surface_damage_rect_list.push_back(gfx::Rect(0, 0, 256, 256));

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    int overlay_cnt = 0;
    for (auto& dc : overlay_data.promoted_overlays) {
      if (dc.plane_z_order > 0) {
        // The overlay video should be the topmost one.
        EXPECT_EQ(gfx::Rect(150, 150, 50, 50),
                  gfx::ToEnclosingRect(dc.display_rect));
        overlay_cnt++;
      }
    }

    EXPECT_EQ(1, overlay_cnt);
  }
}

TEST_F(DCLayerOverlayProcessorTest, HDR10VideoOverlay) {
  InitializeDCLayerOverlayProcessor();
  // Prepare a valid hdr metadata.
  gfx::HDRMetadata valid_hdr_metadata;
  valid_hdr_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(1000, 400);
  valid_hdr_metadata.smpte_st_2086 =
      gfx::HdrMetadataSmpteSt2086(SkNamedPrimariesExt::kRec2020, 1000, 0.0001);

  // Device has RGB10A2 overlay support.
  gl::SetDirectCompositionScaledOverlaysSupportedForTesting(true);

  // Device has HDR-enabled display and no non-HDR-enabled display.
  dc_layer_overlay_processor_
      ->set_system_hdr_disabled_on_any_display_for_testing(false);

  // Device has video processor support.
  dc_layer_overlay_processor_->set_has_p010_video_processor_support_for_testing(
      true);

  // Frame 1 should promote overlay as all conditions satisfied.
  {
    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content is 10bit P010 content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), valid_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should promote overlay.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
  }

  // Frame 2 should skip overlay as bit depth not satisfied.
  {
    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content is 8bit NV12 (not satisfied) content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), valid_hdr_metadata,
        MultiPlaneFormat::kNV12);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());
  }

  // Frame 3 should skip overlay as hdr metadata is invalid.
  {
    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;
    // Content is 10bit P010 content with HDR10 colorspace, but invalid HDR
    // metadata (not satisfied).
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), gfx::HDRMetadata(),
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());
  }

  // Frame 4 should promote overlay as hdr metadata contains cta_861_3.
  {
    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content has HDR metadata which contains cta_861_3.
    gfx::HDRMetadata cta_861_3_hdr_metadata;
    cta_861_3_hdr_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(0, 400);
    // Content is 10bit P010 content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), cta_861_3_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should promote overlay.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
  }

  // Frame 5 should promote overlay as hdr metadata contains smpte_st_2086.
  {
    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content has HDR metadata which contains smpte_st_2086.
    gfx::HDRMetadata smpte_st_2086_hdr_metadata;
    smpte_st_2086_hdr_metadata.smpte_st_2086 = gfx::HdrMetadataSmpteSt2086(
        SkNamedPrimariesExt::kRec2020, 1000, 0.0001);
    // Content is 10bit P010 content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), smpte_st_2086_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should promote overlay.
    EXPECT_EQ(1U, overlay_data.promoted_overlays.size());
  }

  // Frame 6 should skip overlay as color space not satisfied.
  {
    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content is 10bit P010 content with HDR colorspace but not in PQ transfer
    // (not satisfied).
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHLG(), valid_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());
  }

  // Frame 7 should skip overlay as no P010 video processor support.
  {
    dc_layer_overlay_processor_
        ->set_has_p010_video_processor_support_for_testing(false);

    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content is 10bit P010 content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), valid_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());

    // Recover config.
    dc_layer_overlay_processor_
        ->set_has_p010_video_processor_support_for_testing(true);
  }

  // Frame 8 should skip overlay as non-HDR-enabled display exists.
  {
    dc_layer_overlay_processor_
        ->set_system_hdr_disabled_on_any_display_for_testing(true);

    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content is 10bit P010 content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), valid_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());

    // Recover config.
    dc_layer_overlay_processor_
        ->set_system_hdr_disabled_on_any_display_for_testing(false);
  }

  // Frame 9 should skip overlay as no rgb10a2 overlay support.
  {
    gl::SetDirectCompositionScaledOverlaysSupportedForTesting(false);

    auto pass = CreateRenderPass();
    pass->content_color_usage = gfx::ContentColorUsage::kHDR;

    // Content is 10bit P010 content with HDR10 colorspace.
    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        gfx::ColorSpace::CreateHDR10(), valid_hdr_metadata,
        MultiPlaneFormat::kP010);

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    pass->damage_rect = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    auto overlay_data = ProcessRootPassForOverlays(
        &pass_list, render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list));

    // Should skip overlay.
    EXPECT_EQ(0U, overlay_data.promoted_overlays.size());

    // Recover config.
    gl::SetDirectCompositionScaledOverlaysSupportedForTesting(true);
  }
}

class OverlayProcessorWinStaticTest : public testing::Test {};

MATCHER(ResourceIdEq, "") {
  return std::get<0>(arg).resource_id == ResourceId(std::get<1>(arg));
}

MATCHER(PlaneZOrdersAreUnique, "") {
  const OverlayCandidateList& candidates = arg;
  base::flat_set<int> z_orders;
  for (const auto& candidate : candidates) {
    z_orders.insert(candidate.plane_z_order);
  }
  return candidates.size() == z_orders.size();
}

// Checks that, when the overlay candidates list is sorted by z-order, the
// resource IDs of the candidates matches |expected_resource_ids|. Note these
// resource IDs are not real and a just used to identify overlay candidates in
// tests.
testing::Matcher<const OverlayCandidateList&>
WhenCandidatesAreSortedResourceIdsAre(
    const std::vector<int>& expected_resource_ids) {
  return testing::AllOf(
      PlaneZOrdersAreUnique(),
      testing::WhenSortedBy(
          test::PlaneZOrderAscendingComparator(),
          testing::Pointwise(ResourceIdEq(), expected_resource_ids)));
}

TEST_F(OverlayProcessorWinStaticTest, InsertSurfaceContentOverlay) {
  // Set up a dummy render pass and RPDQ
  AggregatedRenderPass pass;
  pass.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq;
  rpdq.render_pass_id = pass.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(3);
    overlay_data.promoted_overlays.back().plane_z_order = 1;
  }
  surface_content_render_passes.insert({&pass, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(4);

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(2);
    candidates.back().rpdq = &rpdq;

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates, WhenCandidatesAreSortedResourceIdsAre({1, 2, 3, 4}));
}

TEST_F(OverlayProcessorWinStaticTest, InsertSurfaceContentUnderlay) {
  // Set up a dummy render pass and RPDQ
  AggregatedRenderPass pass;
  pass.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq;
  rpdq.render_pass_id = pass.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(3);
    overlay_data.promoted_overlays.back().plane_z_order = -1;
  }
  surface_content_render_passes.insert({&pass, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(4);

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(2);
    candidates.back().rpdq = &rpdq;

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates, WhenCandidatesAreSortedResourceIdsAre({1, 3, 2, 4}));
}

// Check that |InsertSurfaceContentOverlaysAndSetPlaneZOrder| supports promoted
// overlay candidates that have gaps in the z-order.
TEST_F(OverlayProcessorWinStaticTest,
       InsertSurfaceContentOverlaysWithGapsInZOrder) {
  // Set up a dummy render pass and RPDQ
  AggregatedRenderPass pass;
  pass.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq;
  rpdq.render_pass_id = pass.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(2);
    overlay_data.promoted_overlays.back().plane_z_order = -3;

    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(4);
    overlay_data.promoted_overlays.back().plane_z_order = 3;
  }
  surface_content_render_passes.insert({&pass, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(5);

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(3);
    candidates.back().rpdq = &rpdq;

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates,
              WhenCandidatesAreSortedResourceIdsAre({1, 2, 3, 4, 5}));
}

TEST_F(OverlayProcessorWinStaticTest,
       InsertSurfaceContentOverlaysWithNoPromotedOverlays) {
  // Set up a dummy render pass and RPDQ
  AggregatedRenderPass pass;
  pass.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq;
  rpdq.render_pass_id = pass.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  // No candidates in |overlay_data|.
  surface_content_render_passes.insert({&pass, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(2);
    candidates.back().rpdq = &rpdq;

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates, WhenCandidatesAreSortedResourceIdsAre({1, 2}));
}

TEST_F(OverlayProcessorWinStaticTest,
       InsertSurfaceContentOverlaysWithUnderlays) {
  // Set up a dummy render pass and RPDQ
  AggregatedRenderPass pass;
  pass.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq;
  rpdq.render_pass_id = pass.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(5);
    overlay_data.promoted_overlays.back().plane_z_order = 1;

    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(2);
    overlay_data.promoted_overlays.back().plane_z_order = -2;

    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(3);
    overlay_data.promoted_overlays.back().plane_z_order = -1;

    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(6);
    overlay_data.promoted_overlays.back().plane_z_order = 2;
  }
  surface_content_render_passes.insert({&pass, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(8);

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(7);

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(4);
    candidates.back().rpdq = &rpdq;

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates,
              WhenCandidatesAreSortedResourceIdsAre({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST_F(OverlayProcessorWinStaticTest,
       InsertSurfaceContentOverlaysMultipleSurfaces) {
  // Set up dummy render passes and RPDQs
  AggregatedRenderPass pass1;
  pass1.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq1;
  rpdq1.render_pass_id = pass1.id;
  AggregatedRenderPass pass2;
  pass2.id = AggregatedRenderPassId(2);
  AggregatedRenderPassDrawQuad rpdq2;
  rpdq2.render_pass_id = pass2.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(2);
    overlay_data.promoted_overlays.back().plane_z_order = 1;
  }
  surface_content_render_passes.insert({&pass1, std::move(overlay_data)});

  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(4);
    overlay_data.promoted_overlays.back().plane_z_order = 1;
  }
  surface_content_render_passes.insert({&pass2, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(5);

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(3);
    candidates.back().rpdq = &rpdq2;

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
    candidates.back().rpdq = &rpdq1;
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates,
              WhenCandidatesAreSortedResourceIdsAre({1, 2, 3, 4, 5}));
}

TEST_F(OverlayProcessorWinStaticTest,
       InsertSurfaceContentOverlaysSameSurfaceEmbeddedTwice) {
  // Set up a dummy render pass and RPDQ
  AggregatedRenderPass pass;
  pass.id = AggregatedRenderPassId(1);
  AggregatedRenderPassDrawQuad rpdq;
  rpdq.render_pass_id = pass.id;

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;
  DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
  {
    overlay_data.promoted_overlays.emplace_back();
    overlay_data.promoted_overlays.back().resource_id = ResourceId(3);
    overlay_data.promoted_overlays.back().plane_z_order = 1;
  }
  surface_content_render_passes.insert({&pass, std::move(overlay_data)});

  OverlayCandidateList candidates;
  {
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(6);

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(4);
    candidates.back().rpdq = &rpdq;

    // Pretend this candidate is a RPDQ that we've pulled overlays from.
    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(2);
    candidates.back().rpdq = &rpdq;

    candidates.emplace_back();
    candidates.back().resource_id = ResourceId(1);
  }

  std::ignore = OverlayProcessorWin::
      InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
          std::move(surface_content_render_passes), candidates);

  EXPECT_THAT(candidates, WhenCandidatesAreSortedResourceIdsAre(
                              {1, 2, 3, 4,
                               3,  // We've embedded this overlay twice
                               6}));
}

class TestOverlayProcessorWin : public OverlayProcessorWin {
 public:
  explicit TestOverlayProcessorWin(int allowed_yuv_overlay_count,
                                   bool disable_video_overlay_if_moving)
      : OverlayProcessorWin(OutputSurface::DCSupportLevel::kDCompTexture,
                            &debug_settings_,
                            std::make_unique<DCLayerOverlayProcessor>(
                                allowed_yuv_overlay_count,
                                disable_video_overlay_if_moving,
                                /*skip_initialization_for_testing=*/true)) {}
  DebugRendererSettings debug_settings_;
};

class OverlayProcessorWinTest : public OverlayProcessorTestBase {
 protected:
  void SetUp() override {
    OverlayProcessorTestBase::SetUp();

    // With disable_video_overlay_if_moving enabled, videos are required to be
    // stable for a certain number of frames to be considered for overlay
    // promotion. This complicates tests since it adds behavior dependent on
    // the number of times |Process| is called.
    overlay_processor_ = std::make_unique<TestOverlayProcessorWin>(
        /*allowed_yuv_overlay_count=*/1,
        /*disable_video_overlay_if_moving=*/false);
    overlay_processor_->SetUsingDCLayersForTesting(kDefaultRootPassId, true);
    overlay_processor_->SetViewportSize(gfx::Size(256, 256));

    EXPECT_TRUE(overlay_processor_->IsOverlaySupported());

    output_surface_plane_ =
        OverlayProcessorInterface::OutputSurfaceOverlayPlane();
  }

  void TearDown() override {
    overlay_processor_ = nullptr;
    OverlayProcessorTestBase::TearDown();
  }

  OverlayProcessorInterface::OutputSurfaceOverlayPlane*
  GetOutputSurfacePlane() {
    EXPECT_TRUE(output_surface_plane_.has_value());
    return &output_surface_plane_.value();
  }

  std::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
      output_surface_plane_;
  std::unique_ptr<OverlayProcessorWin> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

enum class SurfaceTestMode {
  RootSurface,
  SimulatePartiallyDelegated,
};

// Tests that check the behavior of surface planes returned from
// OverlayProcessorWin. These planes are render passes that OverlayProcessorWin
// treats as a surface to "hole punch" overlays out of. This is normally the
// root render pass. In partially delegated compositing, any web contents pass.
class OverlayProcessorWinSurfacePlaneTest
    : public OverlayProcessorWinTest,
      public testing::WithParamInterface<SurfaceTestMode> {
 public:
  static std::string GetParamName(
      const testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case SurfaceTestMode::RootSurface:
        return "RootSurface";
      case SurfaceTestMode::SimulatePartiallyDelegated:
        return "SimulatePartiallyDelegated";
    }
  }

 protected:
  OverlayProcessorWinSurfacePlaneTest() {
    switch (GetParam()) {
      case SurfaceTestMode::RootSurface:
        feature_list_.InitAndDisableFeature(features::kDelegatedCompositing);
        break;
      case SurfaceTestMode::SimulatePartiallyDelegated:
        feature_list_.InitAndEnableFeatureWithParameters(
            features::kDelegatedCompositing, {{"mode", "limit_to_ui"}});
        break;
    }
  }

  void ProcessForOverlays(
      AggregatedRenderPassList* render_passes,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list_in_root_space,
      OverlayCandidateList* candidates) {
    // Wraps the root pass of |pass_list| in a RPDQ to simulate the
    // SurfaceAggregator not merging the web contents root pass, which is what
    // happens during partial delegation.
    //
    // On cleanup, we remove the root pass that we added and the RPDQ overlay
    // added as the explicit output surface plane. We also adjust candidate's
    // z-orders to be relative to 0 instead of the RPDQ overlay we removed.
    //
    // Note this does not touch the |OutputSurfaceOverlayPlane| that the overlay
    // processor modifies.
    class ScopedSimulateUnmergedWebContentsSurface {
     public:
      ScopedSimulateUnmergedWebContentsSurface(
          AggregatedRenderPassList* pass_list,
          OverlayCandidateList* candidates,
          gfx::Rect* damage_rect)
          : pass_list_(pass_list),
            candidates_(candidates),
            damage_rect_(damage_rect) {
        // Some tests provide a smaller |damage_rect_| than root pass damage
        // rect. This is normally not possible.
        pass_list_->back()->damage_rect.Intersect(*damage_rect_);

        const AggregatedRenderPassId max_pass_id =
            base::ranges::max_element(*pass_list_, base::ranges::less(),
                                      &AggregatedRenderPass::id)
                ->get()
                ->id;
        const AggregatedRenderPassId unused_pass_id(max_pass_id.value() + 1);

        auto pass = std::make_unique<AggregatedRenderPass>();
        pass->SetNew(unused_pass_id, pass_list_->back()->output_rect,
                     pass_list_->back()->damage_rect, gfx::Transform());

        auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
        shared_quad_state->SetAll(
            /*transform=*/gfx::Transform(),
            /*layer_rect=*/pass_list_->back()->output_rect,
            /*visible_layer_rect=*/pass_list_->back()->output_rect,
            gfx::MaskFilterInfo(),
            /*clip=*/std::nullopt, /*contents_opaque=*/false,
            /*opacity_f=*/1.f, SkBlendMode::kSrc, /*sorting_context=*/0,
            /*layer_id=*/0u,
            /*fast_rounded_corner=*/false);

        auto* quad =
            pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
        quad->SetNew(
            shared_quad_state, /*rect=*/pass_list_->back()->output_rect,
            /*visible_rect=*/pass_list_->back()->output_rect,
            pass_list_->back()->id,
            /*mask_resource_id=*/kInvalidResourceId,
            /*mask_uv_rect=*/gfx::RectF(),
            /*mask_texture_size=*/gfx::Size(),
            /*filters_scale=*/gfx::Vector2dF(1.0f, 1.0f),
            /*filters_origin=*/gfx::PointF(),
            /*tex_coord_rect=*/gfx::RectF(pass_list_->back()->output_rect),
            /*force_anti_aliasing_off=*/false,
            /*backdrop_filter_quality=*/1.0f);

        // Pretend that our old root pass is actually the root pass of a
        // surface.
        pass_list_->back()->is_from_surface_root_pass = true;

        pass_list_->push_back(std::move(pass));
      }

      ~ScopedSimulateUnmergedWebContentsSurface() {
        auto it =
            base::ranges::find_if(*candidates_, [](const auto& candidate) {
              return candidate.rpdq &&
                     candidate.rpdq->render_pass_id == kDefaultRootPassId;
            });
        CHECK(it != candidates_->end());
        const int surface_z_order = it->plane_z_order;
        candidates_->erase(it);
        for (auto& candidate : *candidates_) {
          candidate.plane_z_order = candidate.plane_z_order - surface_z_order;
        }

        pass_list_->pop_back();
        // The last render pass should now be the surface pass.
        *damage_rect_ = pass_list_->back()->damage_rect;
      }

     private:
      raw_ptr<AggregatedRenderPassList> pass_list_;
      raw_ptr<OverlayCandidateList> candidates_;
      raw_ptr<gfx::Rect> damage_rect_;
    };

    std::optional<ScopedSimulateUnmergedWebContentsSurface>
        simulate_unmerged_web_contents_surface;
    if (GetParam() == SurfaceTestMode::SimulatePartiallyDelegated) {
      simulate_unmerged_web_contents_surface.emplace(render_passes, candidates,
                                                     &damage_rect_);
    }

    output_surface_plane_ =
        OverlayProcessorInterface::OutputSurfaceOverlayPlane();
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), render_passes, SkM44(), render_pass_filters,
        render_pass_backdrop_filters,
        std::move(surface_damage_rect_list_in_root_space),
        &output_surface_plane_.value(), candidates, &damage_rect_,
        &content_bounds_);
    overlay_processor_->AdjustOutputSurfaceOverlay(&output_surface_plane_);

    // Sort candidates front-to-back so tests can assume they appear in the same
    // order as the input draw quads.
    base::ranges::sort(*candidates, base::ranges::greater(),
                       &OverlayCandidate::plane_z_order);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that we can promote an overlay in the simple case from a surface.
TEST_P(OverlayProcessorWinSurfacePlaneTest, PromoteOverlayFromSurface) {
  AggregatedRenderPassList pass_list;
  auto pass = CreateRenderPass();
  CreateTextureQuadAt(resource_provider_.get(), child_resource_provider_.get(),
                      child_provider_.get(),
                      pass->CreateAndAppendSharedQuadState(), pass.get(),
                      gfx::Rect(0, 0, 50, 50),
                      /*is_overlay_candidate=*/true);
  pass_list.push_back(std::move(pass));

  damage_rect_ = pass_list.back()->output_rect;

  OverlayCandidateList dc_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  ProcessForOverlays(&pass_list, render_pass_filters,
                     render_pass_backdrop_filters, SurfaceDamageRectList(),
                     &dc_layer_list);

  EXPECT_TRUE(pass_list.back()->needs_synchronous_dcomp_commit);
  EXPECT_EQ(1U, dc_layer_list.size());
}

// Check that we don't accidentally end up in a case where we try to read back a
// DComp surface, which can happen if one issues a copy request while we're in
// the hysteresis when switching from a DComp surface back to a swap chain.
TEST_P(OverlayProcessorWinSurfacePlaneTest, ForceSwapChainForCapture) {
  // Frame with no overlays, but we expect to still be in DComp surface mode,
  // due to one-sided hysteresis intended to prevent allocation churn.
  {
    AggregatedRenderPassList pass_list;
    pass_list.push_back(CreateRenderPass());

    damage_rect_ = pass_list.back()->output_rect;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    ProcessForOverlays(&pass_list, render_pass_filters,
                       render_pass_backdrop_filters, SurfaceDamageRectList(),
                       &dc_layer_list);

    EXPECT_TRUE(pass_list.back()->needs_synchronous_dcomp_commit);
  }

  // Frame with a copy request. Even though we're still in the hysteresis, we
  // expect to forcibly switch to swap chain mode so that the copy request
  // succeeds.
  {
    AggregatedRenderPassList pass_list;
    pass_list.push_back(CreateRenderPass());

    pass_list.back()->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());

    damage_rect_ = pass_list.back()->output_rect;

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    ProcessForOverlays(&pass_list, render_pass_filters,
                       render_pass_backdrop_filters, SurfaceDamageRectList(),
                       &dc_layer_list);

    EXPECT_FALSE(pass_list.back()->needs_synchronous_dcomp_commit);
  }
}

TEST_P(OverlayProcessorWinSurfacePlaneTest, UseDCompSurfaceWithVideo) {
  overlay_processor_->SetUsingDCLayersForTesting(kDefaultRootPassId, false);
  // Draw 60 frames with overlay video quads.
  for (int i = 0; i < 60; i++) {
    SCOPED_TRACE(base::StringPrintf("Frame with overlay %d", i));
    auto pass = CreateRenderPass();
    // Use an opaque pass to check that the overlay processor makes it
    // transparent in the case of overlays.
    pass->has_transparent_background = false;

    CreateFullscreenCandidateYUVTextureQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    SurfaceDamageRectList surface_damage_rect_list;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);

    // Full damage on the first frame.
    const gfx::Rect expected_damage =
        (i == 0) ? pass_list.back()->output_rect : gfx::Rect();

    ProcessForOverlays(&pass_list, render_pass_filters,
                       render_pass_backdrop_filters, SurfaceDamageRectList(),
                       &dc_layer_list);

    EXPECT_TRUE(pass_list.back()->needs_synchronous_dcomp_commit);
    EXPECT_TRUE(pass_list.back()->has_transparent_background);
    if (GetParam() == SurfaceTestMode::RootSurface) {
      EXPECT_TRUE(output_surface_plane_);
      EXPECT_EQ(output_surface_plane_->enable_blending,
                pass_list.back()->has_transparent_background);
    } else {
      // Delegated compositing removes the output surface plane.
    }

    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(1, dc_layer_list.back().plane_z_order);
    EXPECT_EQ(damage_rect_, expected_damage);

    Mock::VerifyAndClearExpectations(output_surface_.get());
  }

  // Draw 65 frames without overlays.
  for (int i = 0; i < 65; i++) {
    SCOPED_TRACE(base::StringPrintf("Frame without overlay %d", i));
    auto pass = CreateRenderPass();
    pass->has_transparent_background = false;

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

    // There will be full damage and needs_synchronous_dcomp_commit will be
    // false after 60 consecutive frames with no overlays. The first frame
    // without overlays will also have full damage.
    const gfx::Rect expected_damage = (i == 0 || (i + 1) == 60)
                                          ? pass_list.back()->output_rect
                                          : damage_rect_;

    const bool in_dc_layer_hysteresis = i + 1 < 60;

    ProcessForOverlays(&pass_list, render_pass_filters,
                       render_pass_backdrop_filters, SurfaceDamageRectList(),
                       &dc_layer_list);

    EXPECT_EQ(pass_list.back()->needs_synchronous_dcomp_commit,
              in_dc_layer_hysteresis);
    EXPECT_EQ(pass_list.back()->has_transparent_background,
              in_dc_layer_hysteresis);
    if (GetParam() == SurfaceTestMode::RootSurface) {
      EXPECT_TRUE(output_surface_plane_);
      EXPECT_EQ(output_surface_plane_->enable_blending,
                pass_list.back()->has_transparent_background);
    } else {
      // Delegated compositing removes the output surface plane.
    }

    EXPECT_EQ(0u, dc_layer_list.size());
    EXPECT_EQ(damage_rect_, expected_damage);

    Mock::VerifyAndClearExpectations(output_surface_.get());
  }
}

// Tests that Delegated Ink in the frame correctly sets
// needs_synchronous_dcomp_commit on the render pass.
TEST_P(OverlayProcessorWinSurfacePlaneTest, FrameHasDelegatedInk) {
  overlay_processor_->SetUsingDCLayersForTesting(kDefaultRootPassId, false);
  // Test that needs_synchronous_dcomp_commit on the render pass gets set to
  // false as default.
  {
    auto pass = CreateRenderPass();
    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

    EXPECT_FALSE(pass_list[0]->needs_synchronous_dcomp_commit);
    ProcessForOverlays(&pass_list, render_pass_filters,
                       render_pass_backdrop_filters, SurfaceDamageRectList(),
                       &dc_layer_list);
    EXPECT_FALSE(pass_list[0]->needs_synchronous_dcomp_commit);
  }

  // Test that needs_synchronous_dcomp_commit gets set to true when the frame
  // has delegated ink.
  overlay_processor_->SetFrameHasDelegatedInk();
  auto pass = CreateRenderPass();
  OverlayCandidateList dc_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  damage_rect_ = gfx::Rect(1, 1, 10, 10);
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

  EXPECT_FALSE(pass_list[0]->needs_synchronous_dcomp_commit);
  ProcessForOverlays(&pass_list, render_pass_filters,
                     render_pass_backdrop_filters, SurfaceDamageRectList(),
                     &dc_layer_list);
  // Make sure |frame_has_delegated_ink_| has been set to false.
  EXPECT_FALSE(overlay_processor_->frame_has_delegated_ink_for_testing());
  EXPECT_TRUE(pass_list[0]->needs_synchronous_dcomp_commit);
}

// Ensure needs_synchronous_dcomp_commit lasts for 60 frames after
// |SetFrameHasDelegatedInk| has been called (once). Based on
// kNumberOfFramesBeforeDisablingDCLayers in
// components/viz/service/display/overlay_processor_win.cc.
TEST_P(OverlayProcessorWinSurfacePlaneTest, DelegatedInkSurfaceHysteresis) {
  overlay_processor_->SetUsingDCLayersForTesting(kDefaultRootPassId, false);

  overlay_processor_->SetFrameHasDelegatedInk();
  for (int frame = 1; frame <= 61; frame++) {
    auto pass = CreateRenderPass();
    OverlayCandidateList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list = {gfx::Rect(1, 1, 10, 10)};

    EXPECT_FALSE(pass_list[0]->needs_synchronous_dcomp_commit);
    ProcessForOverlays(&pass_list, render_pass_filters,
                       render_pass_backdrop_filters, SurfaceDamageRectList(),
                       &dc_layer_list);
    // Make sure |frame_has_delegated_ink_| has been set to false.
    EXPECT_FALSE(overlay_processor_->frame_has_delegated_ink_for_testing());
    if (frame <= 60) {
      EXPECT_TRUE(pass_list[0]->needs_synchronous_dcomp_commit);
    } else {
      EXPECT_FALSE(pass_list[0]->needs_synchronous_dcomp_commit);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    OverlayProcessorWinSurfacePlaneTest,
    testing::Values(SurfaceTestMode::RootSurface,
                    SurfaceTestMode::SimulatePartiallyDelegated),
    &OverlayProcessorWinSurfacePlaneTest::GetParamName);

class OverlayProcessorWinDelegatedCompositingTest
    : public OverlayProcessorWinTest {
 protected:
  OverlayProcessorWinDelegatedCompositingTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kDelegatedCompositing, {{"mode", "full"}});
  }

  class DelegationResult {
   public:
    DelegationResult(OverlayCandidateList candidates,
                     bool delegation_succeeded,
                     gfx::Rect original_root_surface_damage,
                     gfx::Rect root_surface_damage)
        : candidates_(std::move(candidates)),
          delegation_succeeded_(delegation_succeeded),
          original_root_surface_damage_(original_root_surface_damage),
          root_surface_damage_(root_surface_damage) {}

    void ExpectDelegationSuccess() const {
      EXPECT_TRUE(delegation_succeeded_);
      EXPECT_EQ(gfx::Rect(), root_surface_damage_);
    }

    void ExpectDelegationFailure() const {
      EXPECT_FALSE(delegation_succeeded_);
      EXPECT_EQ(original_root_surface_damage_, root_surface_damage_);
    }

    const OverlayCandidateList& candidates() const { return candidates_; }

   private:
    OverlayCandidateList candidates_;
    bool delegation_succeeded_ = false;
    gfx::Rect original_root_surface_damage_;
    gfx::Rect root_surface_damage_;
  };

  DelegationResult TryProcessForDelegatedOverlays(
      AggregatedRenderPassList& pass_list,
      SurfaceDamageRectList surface_damage_rect_list = {}) {
    if (!output_surface_plane_) {
      // Reset the output surface plane in case we're calling
      // |TryProcessForDelegatedOverlays| multiple times.
      output_surface_plane_ =
          OverlayProcessorInterface::OutputSurfaceOverlayPlane();
    }

    const gfx::Rect original_root_surface_damage =
        pass_list.back()->damage_rect;

    OverlayCandidateList candidates;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

    for (const auto& pass : pass_list) {
      if (!pass->filters.IsEmpty()) {
        render_pass_filters[pass->id] = &pass->filters;
      }
      if (!pass->backdrop_filters.IsEmpty()) {
        render_pass_backdrop_filters[pass->id] = &pass->backdrop_filters;
      }
    }

    damage_rect_ = original_root_surface_damage;
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), GetOutputSurfacePlane(),
        &candidates, &damage_rect_, &content_bounds_);

    overlay_processor_->AdjustOutputSurfaceOverlay(&output_surface_plane_);
    const bool delegation_succeeded = !output_surface_plane_.has_value();

    return DelegationResult(candidates, delegation_succeeded,
                            original_root_surface_damage, damage_rect_);
  }

  testing::Matcher<const OverlayCandidateList&>
  WhenCandidatesAreSortedElementsAre(
      std::vector<testing::Matcher<const OverlayCandidate&>> element_matchers) {
    return testing::AllOf(
        PlaneZOrdersAreUnique(),
        testing::WhenSortedBy(test::PlaneZOrderAscendingComparator(),
                              testing::ElementsAreArray(element_matchers)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that we can do delegated compositing of a single quad.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, SingleQuad) {
  AggregatedRenderPassList pass_list;

  auto pass = CreateRenderPass();
  CreateSolidColorQuadAt(pass->CreateAndAppendSharedQuadState(), SkColors::kRed,
                         pass.get(), gfx::Rect(0, 0, 50, 50));
  pass_list.push_back(std::move(pass));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();
  EXPECT_THAT(result.candidates(),
              WhenCandidatesAreSortedElementsAre({
                  test::IsSolidColorOverlay(SkColors::kRed),
              }));
}

// Check that we don't try delegated compositing when there are too many quads.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, TooManyQuads) {
  AggregatedRenderPassList pass_list;

  auto pass = CreateRenderPass();
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  for (int i = 0; i < 2049; i++) {
    CreateSolidColorQuadAt(sqs, SkColors::kRed, pass.get(),
                           gfx::Rect(i, 0, 50, 50));
  }
  pass_list.push_back(std::move(pass));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationFailure();
  EXPECT_THAT(result.candidates(), testing::IsEmpty());
}

// Check that we don't try delegated compositing when there are too many complex
// quads. This limit is lower since they have a larger performance hit in DWM.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, TooManyComplexQuads) {
  AggregatedRenderPassList pass_list;

  auto pass = CreateRenderPass();
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(0, 0, 50, 50), 50));
  for (int i = 0; i < 257; i++) {
    CreateSolidColorQuadAt(sqs, SkColors::kRed, pass.get(),
                           gfx::Rect(i, 0, 50, 50));
  }
  pass_list.push_back(std::move(pass));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationFailure();
  EXPECT_THAT(result.candidates(), testing::IsEmpty());
}

// Check that, when delegated compositing fails, we still successfully promote
// videos to overlay.
TEST_F(OverlayProcessorWinDelegatedCompositingTest,
       DelegationFailStillPromotesVideos) {
  AggregatedRenderPassList pass_list;

  auto pass = CreateRenderPass();

  // Add a video quad we expect to go to overlay.
  auto* video_quad = CreateFullscreenCandidateYUVTextureQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  ResourceId video_resource_id = video_quad->resource_id();

  {
    // A RPDQ with a backdrop filter occluding another quad will cause delegated
    // compositing to fail.
    auto child_pass = CreateRenderPass(AggregatedRenderPassId(2));
    child_pass->backdrop_filters.Append(
        cc::FilterOperation::CreateBlurFilter(1.f));
    CreateRenderPassDrawQuadAt(pass.get(), pass->shared_quad_state_list.back(),
                               gfx::Rect(0, 0, 50, 50), child_pass->id);
    pass_list.push_back(std::move(child_pass));

    CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                           SkColors::kGreen, pass.get(),
                           gfx::Rect(0, 0, 50, 50));
  }

  pass_list.push_back(std::move(pass));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationFailure();
  EXPECT_THAT(result.candidates(),
              WhenCandidatesAreSortedElementsAre({
                  test::OverlayHasResource(video_resource_id),
              }))
      << "The overlay processor fall back to using DCLayerOverlayProcessor on "
         "the root surface.";
}

// Test that when |OverlayCandidateFactory| returns |kFailVisible| we just skip
// the quad instead of failing delegation.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, SkipNonVisibleOverlays) {
  AggregatedRenderPassList pass_list;

  auto pass = CreateRenderPass();
  CreateSolidColorQuadAt(pass->CreateAndAppendSharedQuadState(), SkColors::kRed,
                         pass.get(), gfx::Rect(0, 0, 0, 0));
  pass_list.push_back(std::move(pass));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();
  EXPECT_THAT(result.candidates(), testing::IsEmpty());
}

// Check that delegated compositing fails when there is a color conversion pass.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, HdrNotSupported) {
  AggregatedRenderPassList pass_list;

  pass_list.push_back(CreateRenderPass(AggregatedRenderPassId{2}));

  auto pass = CreateRenderPass();
  pass->is_color_conversion_pass = true;
  pass_list.push_back(std::move(pass));

  damage_rect_ = pass_list.back()->damage_rect;

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationFailure();
}

// Check that delegated compositing fails when the root is being captured.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, CaptureNotSupported) {
  AggregatedRenderPassList pass_list;

  auto pass = CreateRenderPass();
  pass->video_capture_enabled = true;
  pass_list.push_back(std::move(pass));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationFailure();
}

// Check that delegated compositing fails when there is a backdrop filter that
// would need to read another overlay candidate.
TEST_F(OverlayProcessorWinDelegatedCompositingTest,
       OccludedByFilteredQuadNotSupported) {
  AggregatedRenderPassList pass_list;

  AggregatedRenderPassId child_pass_id{2};

  // Create a pass with a backdrop filter.
  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->backdrop_filters = cc::FilterOperations({
        cc::FilterOperation::CreateGrayscaleFilter(1.0f),
    });
    pass_list.push_back(std::move(child_pass));
  }

  {
    auto pass = CreateRenderPass();

    const gfx::Rect rect(0, 0, 50, 50);

    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(), rect,
                               child_pass_id);

    // Create a quad that will be occluded by the backdrop-filtered RPDQ above.
    CreateSolidColorQuadAt(pass->CreateAndAppendSharedQuadState(),
                           SkColors::kRed, pass.get(), rect);

    pass_list.push_back(std::move(pass));
  }

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationFailure();
}

// Check that the various ways we can set |will_backing_be_read_by_viz| work as
// expected.
TEST_F(OverlayProcessorWinDelegatedCompositingTest, BackingWillBeReadInViz) {
  AggregatedRenderPassList pass_list;

  AggregatedRenderPassId::Generator id_generator;
  base::flat_map<AggregatedRenderPassId, const char*> pass_names;
  base::flat_set<AggregatedRenderPassId> passes_to_embed_in_root;

  auto CreateNamedPass =
      [&](const char* name, bool embed_in_root,
          base::OnceCallback<void(AggregatedRenderPass*)> update_pass) {
        AggregatedRenderPassId pass_id = id_generator.GenerateNextId();
        pass_names.insert({pass_id, name});
        if (embed_in_root) {
          passes_to_embed_in_root.insert(pass_id);
        }

        std::unique_ptr<AggregatedRenderPass> pass = CreateRenderPass(pass_id);
        std::move(update_pass).Run(pass.get());
        pass_list.push_back(std::move(pass));

        return pass_id;
      };

  CreateNamedPass("video capture enabled", true,
                  base::BindOnce([](AggregatedRenderPass* pass) {
                    pass->video_capture_enabled = true;
                  }));

  CreateNamedPass("filters", true,
                  base::BindOnce([](AggregatedRenderPass* pass) {
                    pass->filters = cc::FilterOperations({
                        cc::FilterOperation::CreateGrayscaleFilter(1.0f),
                    });
                  }));

  CreateNamedPass("generate mipmaps", true,
                  base::BindOnce([](AggregatedRenderPass* pass) {
                    pass->generate_mipmap = true;
                  }));

  auto non_overlay_embeddee_id = CreateNamedPass(
      "normal pass with non-overlay embedder", true, base::DoNothing());
  CreateNamedPass("non-overlay embedder", false,
                  base::BindLambdaForTesting([&](AggregatedRenderPass* pass) {
                    CreateRenderPassDrawQuadAt(
                        pass, pass->CreateAndAppendSharedQuadState(),
                        pass->output_rect, non_overlay_embeddee_id);
                  }));

  auto complex_mask_embeddee_id = CreateNamedPass(
      "normal pass with gradient mask embedder", true, base::DoNothing());
  CreateNamedPass("gradient mask embedder", false,
                  base::BindLambdaForTesting([&](AggregatedRenderPass* pass) {
                    auto* sqs = pass->CreateAndAppendSharedQuadState();
                    CreateRenderPassDrawQuadAt(pass, sqs, pass->output_rect,
                                               complex_mask_embeddee_id);

                    // We can delegated rounded corners fine, so set a complex
                    // mask filter that we will handle with an intermediate
                    // surface in |SkiaRenderer|.
                    gfx::LinearGradient gradient;
                    gradient.AddStep(0.f, 0);
                    gradient.AddStep(1.f, 0xff);
                    sqs->mask_filter_info = gfx::MaskFilterInfo(
                        gfx::RRectF(gfx::RectF(pass->output_rect)), gradient);
                  }));

  CreateNamedPass("root pass", false,
                  base::BindLambdaForTesting([&](AggregatedRenderPass* pass) {
                    for (auto id : passes_to_embed_in_root) {
                      CreateRenderPassDrawQuadAt(
                          pass, pass->CreateAndAppendSharedQuadState(),
                          pass->output_rect, id);
                    }
                  }));

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  // In this test, we expect every pass except the root pass to be read by viz.
  // Passes that are not composited as overlays are assumed to be read by viz
  // e.g. for copy output requests, etc.
  for (size_t i = 0u; i < pass_list.size(); i++) {
    SCOPED_TRACE(base::StringPrintf("pass_list[%zu]: %s", i,
                                    pass_names[pass_list[i]->id]));
    if (pass_list[i] == pass_list.back()) {
      EXPECT_FALSE(pass_list[i]->will_backing_be_read_by_viz);
    } else {
      EXPECT_TRUE(pass_list[i]->will_backing_be_read_by_viz);
    }
  }
}

// Tests that check that overlay promotion is supported from non-root render
// passes in the partially delegated case.
class OverlayProcessorWinPartiallyDelegatedCompositingTest
    : public OverlayProcessorWinDelegatedCompositingTest {
 protected:
  OverlayProcessorWinPartiallyDelegatedCompositingTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kDelegatedCompositing, {{"mode", "limit_to_ui"}});
  }

  TextureDrawQuad* CreateOverlayQuadWithSurfaceDamageAt(
      AggregatedRenderPass* pass,
      SurfaceDamageRectList& surface_damage_rect_list,
      const gfx::Rect& rect) {
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    auto* quad = CreateTextureQuadAt(resource_provider_.get(),
                                     child_resource_provider_.get(),
                                     child_provider_.get(), sqs, pass, rect,
                                     /*is_overlay_candidate=*/true);

    pass->damage_rect.Union(
        sqs->quad_to_target_transform.MapRect(quad->visible_rect));

    gfx::Transform quad_to_root_transform(sqs->quad_to_target_transform);
    quad_to_root_transform.PostConcat(pass->transform_to_root_target);

    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.push_back(
        quad_to_root_transform.MapRect(quad->visible_rect));

    return quad;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that an overlay candidate can be promoted from a non-root pass
// representing a surface.
TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatePromotedFromNonRootSurface) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_texture_id;

  // Create a pass with just an overlay quad.
  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), child_pass->CreateAndAppendSharedQuadState(),
        child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  {
    auto pass = CreateRenderPass();
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               gfx::Rect(0, 0, 50, 50), child_pass_id);
    pass_list.push_back(std::move(pass));
  }

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  // We expect both the RPDQ and the inner video to be promoted.
  EXPECT_THAT(result.candidates(),
              WhenCandidatesAreSortedElementsAre({
                  test::IsRenderPassOverlay(child_pass_id),
                  test::OverlayHasResource(child_pass_texture_id),
              }));
}

// Check that an overlay candidate can be promoted from a non-root pass
// representing a surface, but will be placed behind the output surface plane if
// it is occluded by something in the surface.
TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatePromotedFromNonRootSurfaceAsUnderlay) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_texture_id;

  // Create a pass with an overlay quad that is occluded by some other quad.
  // This forces the overlay candidate to appear as an underlay to the surface.
  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    CreateSolidColorQuadAt(child_pass->CreateAndAppendSharedQuadState(),
                           SkColors::kRed, child_pass.get(),
                           gfx::Rect(5, 5, 10, 10));
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), child_pass->CreateAndAppendSharedQuadState(),
        child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  {
    auto pass = CreateRenderPass();
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               gfx::Rect(0, 0, 50, 50), child_pass_id);
    CreateSolidColorQuadAt(pass->CreateAndAppendSharedQuadState(),
                           SkColors::kBlue, pass.get(), pass->output_rect);
    pass_list.push_back(std::move(pass));
  }

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  // We expect both the RPDQ and the inner video to be promoted and in front of
  // the solid color background in the root pass.
  EXPECT_THAT(result.candidates(),
              WhenCandidatesAreSortedElementsAre({
                  test::IsSolidColorOverlay(SkColors::kBlue),
                  test::OverlayHasResource(child_pass_texture_id),
                  test::IsRenderPassOverlay(child_pass_id),
              }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatesPromotedFromMultipleSurfaces) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_video_id;
  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), child_pass->CreateAndAppendSharedQuadState(),
        child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_video_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  const AggregatedRenderPassId other_child_pass_id{3};
  ResourceId other_child_pass_video_id;
  ResourceId other_child_pass_video_2_id;
  {
    auto other_child_pass = CreateRenderPass(other_child_pass_id);
    other_child_pass->is_from_surface_root_pass = true;
    // Make this first quad partially occlude the next.
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(),
        other_child_pass->CreateAndAppendSharedQuadState(),
        other_child_pass.get(), gfx::Rect(10, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    other_child_pass_video_id = texture_quad->resource_id();
    auto* texture_quad_2 = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(),
        other_child_pass->CreateAndAppendSharedQuadState(),
        other_child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    other_child_pass_video_2_id = texture_quad_2->resource_id();
    pass_list.push_back(std::move(other_child_pass));
  }

  {
    auto pass = CreateRenderPass();
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               gfx::Rect(0, 0, 50, 50), child_pass_id);
    CreateSolidColorQuadAt(pass->CreateAndAppendSharedQuadState(),
                           SkColors::kBlue, pass.get(), pass->output_rect);
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               gfx::Rect(50, 0, 50, 50), other_child_pass_id);
    pass_list.push_back(std::move(pass));
  }

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  // We expect both the RPDQ and the inner video(s) to be promoted for both
  // RPDQs.
  EXPECT_THAT(result.candidates(),
              WhenCandidatesAreSortedElementsAre({
                  test::OverlayHasResource(other_child_pass_video_2_id),
                  test::IsRenderPassOverlay(other_child_pass_id),
                  test::OverlayHasResource(other_child_pass_video_id),
                  test::IsSolidColorOverlay(SkColors::kBlue),
                  test::IsRenderPassOverlay(child_pass_id),
                  test::OverlayHasResource(child_pass_video_id),
              }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatePromotionRespectsAllowedYuvOverlayCount) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), child_pass->CreateAndAppendSharedQuadState(),
        child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    texture_quad->is_video_frame = true;
    pass_list.push_back(std::move(child_pass));
  }

  const AggregatedRenderPassId other_child_pass_id{3};
  {
    auto other_child_pass = CreateRenderPass(other_child_pass_id);
    other_child_pass->is_from_surface_root_pass = true;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(),
        other_child_pass->CreateAndAppendSharedQuadState(),
        other_child_pass.get(), gfx::Rect(0, 0, 50, 50));
    texture_quad->is_video_frame = true;
    pass_list.push_back(std::move(other_child_pass));
  }

  {
    auto pass = CreateRenderPass();
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               gfx::Rect(0, 0, 50, 50), child_pass_id);
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               gfx::Rect(50, 0, 50, 50), other_child_pass_id);
    pass_list.push_back(std::move(pass));
  }

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  // We expect both the RPDQs to be promoted, but neither of the videos.
  EXPECT_THAT(result.candidates(),
              WhenCandidatesAreSortedElementsAre({
                  test::IsRenderPassOverlay(other_child_pass_id),
                  test::IsRenderPassOverlay(child_pass_id),
              }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatesInheritSurfaceEmbeddersBounds) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_texture_id;

  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), child_pass->CreateAndAppendSharedQuadState(),
        child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  const gfx::Rect rpdq_bounds = gfx::Rect(0, 0, 20, 30);
  gfx::Rect expected_overlay_clip;

  {
    auto pass = CreateRenderPass();
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform.Translate(1, 2);
    CreateRenderPassDrawQuadAt(pass.get(), sqs, rpdq_bounds, child_pass_id);

    expected_overlay_clip = sqs->quad_to_target_transform.MapRect(rpdq_bounds);

    pass_list.push_back(std::move(pass));
  }

  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  EXPECT_THAT(
      result.candidates(),
      WhenCandidatesAreSortedElementsAre({
          test::IsRenderPassOverlay(child_pass_id),
          testing::AllOf(test::OverlayHasResource(child_pass_texture_id),
                         test::OverlayHasClip(expected_overlay_clip)),
      }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatesInheritSurfaceEmbeddersClip) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_texture_id;

  const gfx::Transform child_to_root = gfx::Transform::MakeTranslation(1, 2);

  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    child_pass->transform_to_root_target = child_to_root;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), child_pass->CreateAndAppendSharedQuadState(),
        child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  const gfx::Rect rpdq_clip_rect = gfx::Rect(10, 20, 5, 15);
  const gfx::Rect rpdq_bounds = gfx::Rect(0, 0, 20, 30);
  gfx::Rect expected_overlay_clip;

  {
    auto pass = CreateRenderPass();
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform = child_to_root;
    sqs->clip_rect = rpdq_clip_rect;
    CreateRenderPassDrawQuadAt(pass.get(), sqs, rpdq_bounds, child_pass_id);

    expected_overlay_clip = sqs->quad_to_target_transform.MapRect(rpdq_bounds);
    expected_overlay_clip.Intersect(rpdq_clip_rect);

    pass_list.push_back(std::move(pass));
  }
  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  EXPECT_THAT(
      result.candidates(),
      WhenCandidatesAreSortedElementsAre({
          testing::AllOf(test::IsRenderPassOverlay(child_pass_id),
                         test::OverlayHasClip(rpdq_clip_rect)),
          testing::AllOf(test::OverlayHasResource(child_pass_texture_id),
                         test::OverlayHasClip(expected_overlay_clip)),
      }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatesInheritSurfaceEmbeddersClipAndIntersect) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_texture_id;

  const gfx::Rect inner_quad_clip = gfx::Rect(10, 15, 1, 2);
  const gfx::Transform child_to_root = gfx::Transform::MakeTranslation(1, 2);

  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    child_pass->transform_to_root_target = child_to_root;
    SharedQuadState* sqs = child_pass->CreateAndAppendSharedQuadState();
    sqs->clip_rect = inner_quad_clip;
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), sqs, child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  const gfx::Rect rpdq_bounds = gfx::Rect(0, 0, 20, 30);
  gfx::Rect expected_overlay_clip;

  {
    auto pass = CreateRenderPass();
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform = child_to_root;
    CreateRenderPassDrawQuadAt(pass.get(), sqs, rpdq_bounds, child_pass_id);

    expected_overlay_clip =
        sqs->quad_to_target_transform.MapRect(inner_quad_clip);
    expected_overlay_clip.Intersect(rpdq_bounds);

    pass_list.push_back(std::move(pass));
  }
  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  EXPECT_THAT(
      result.candidates(),
      WhenCandidatesAreSortedElementsAre({
          test::IsRenderPassOverlay(child_pass_id),
          testing::AllOf(test::OverlayHasResource(child_pass_texture_id),
                         test::OverlayHasClip(expected_overlay_clip)),
      }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       CandidatesInheritSurfaceEmbeddersClipAndRoundedCorners) {
  AggregatedRenderPassList pass_list;

  const AggregatedRenderPassId child_pass_id{2};
  ResourceId child_pass_texture_id;

  {
    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->is_from_surface_root_pass = true;
    SharedQuadState* sqs = child_pass->CreateAndAppendSharedQuadState();
    // We expect this rounded corner to be painted into the |child_pass|
    // surface.
    sqs->mask_filter_info =
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(10, 10), 1));
    auto* texture_quad = CreateTextureQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), sqs, child_pass.get(), gfx::Rect(0, 0, 50, 50),
        /*is_overlay_candidate=*/true);
    child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));
  }

  const gfx::RRectF rpdq_rounded_corners = gfx::RRectF(gfx::RectF(10, 10), 1);

  {
    auto pass = CreateRenderPass();
    SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->mask_filter_info = gfx::MaskFilterInfo(rpdq_rounded_corners);
    CreateRenderPassDrawQuadAt(pass.get(), sqs, gfx::Rect(0, 0, 50, 50),
                               child_pass_id);

    pass_list.push_back(std::move(pass));
  }
  auto result = TryProcessForDelegatedOverlays(pass_list);
  result.ExpectDelegationSuccess();

  // Our texture quad is behind the surface, due to having its own rounded
  // corners.
  EXPECT_THAT(
      result.candidates(),
      WhenCandidatesAreSortedElementsAre({
          testing::AllOf(test::OverlayHasResource(child_pass_texture_id),
                         test::OverlayHasClip(gfx::Rect(0, 0, 50, 50)),
                         test::OverlayHasRoundedCorners(rpdq_rounded_corners)),
          testing::AllOf(test::IsRenderPassOverlay(child_pass_id),
                         test::OverlayHasRoundedCorners(rpdq_rounded_corners)),
      }));
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       DamageRemovedFromSurface) {
  const AggregatedRenderPassId child_pass_id{2};
  const gfx::Rect texture_quad_rect = gfx::Rect(5, 5, 50, 50);

  for (int frame = 0; frame < 3; frame++) {
    SCOPED_TRACE(base::StringPrintf("Frame %d", frame));

    const bool is_overlay_candidate_with_damage = frame < 2;

    AggregatedRenderPassList pass_list;
    SurfaceDamageRectList surface_damage_rect_list;

    const gfx::Rect rpdq_quad = gfx::Rect(10, 10, 100, 100);

    auto child_pass = CreateRenderPass(child_pass_id);
    child_pass->transform_to_root_target =
        gfx::Transform::MakeTranslation(rpdq_quad.OffsetFromOrigin());
    child_pass->is_from_surface_root_pass = true;
    child_pass->damage_rect = gfx::Rect();
    auto* texture_quad =
        is_overlay_candidate_with_damage
            ? CreateOverlayQuadWithSurfaceDamageAt(
                  child_pass.get(), surface_damage_rect_list, texture_quad_rect)
            : CreateTextureQuadAt(resource_provider_.get(),
                                  child_resource_provider_.get(),
                                  child_provider_.get(),
                                  child_pass->CreateAndAppendSharedQuadState(),
                                  child_pass.get(), texture_quad_rect,
                                  /*is_overlay_candidate=*/false);
    ResourceId child_pass_texture_id = texture_quad->resource_id();
    pass_list.push_back(std::move(child_pass));

    auto pass = CreateRenderPass();
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               rpdq_quad, child_pass_id);
    pass_list.push_back(std::move(pass));

    switch (frame) {
      case 0:
      case 1:
        EXPECT_EQ(pass_list[0]->damage_rect, texture_quad_rect)
            << "The quad in the child surface contributes damage";
        break;

      case 2:
        EXPECT_EQ(pass_list[0]->damage_rect, gfx::Rect())
            << "No damage on the child surface before overlay processing";
        break;
    }

    auto result = TryProcessForDelegatedOverlays(
        pass_list, std::move(surface_damage_rect_list));
    result.ExpectDelegationSuccess();

    switch (frame) {
      case 0:
        EXPECT_EQ(pass_list[0]->damage_rect, pass_list[0]->output_rect)
            << "Full damage is forced on the first frame";
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(child_pass_id),
                        test::OverlayHasResource(child_pass_texture_id),
                    }));
        break;

      case 1:
        EXPECT_EQ(pass_list[0]->damage_rect, gfx::Rect())
            << "Damage is removed when only from overlays";
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(child_pass_id),
                        test::OverlayHasResource(child_pass_texture_id),
                    }));
        break;

      case 2:
        EXPECT_EQ(pass_list[0]->damage_rect, texture_quad_rect)
            << "Damage removed in frame 1 is re-added";
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(child_pass_id),
                    }));
        break;
    }
  }
}

TEST_F(OverlayProcessorWinPartiallyDelegatedCompositingTest,
       DamageRemovedFromMultipleSurfaces) {
  const AggregatedRenderPassId left_child_pass_id{2};
  const AggregatedRenderPassId right_child_pass_id{3};
  const gfx::Rect left_texture_quad_rect = gfx::Rect(5, 5, 50, 50);
  const gfx::Rect right_texture_quad_rect = gfx::Rect(10, 5, 50, 50);

  for (int frame = 0; frame < 4; frame++) {
    SCOPED_TRACE(base::StringPrintf("Frame %d", frame));

    AggregatedRenderPassList pass_list;
    SurfaceDamageRectList surface_damage_rect_list;

    const bool is_left_overlay_candidate_with_damage = frame < 3;
    const bool is_right_overlay_candidate_with_damage = frame < 2;

    auto pass = CreateRenderPass();
    gfx::Rect left_rpdq_quad;
    gfx::Rect right_rpdq_quad;
    pass->output_rect.SplitVertically(left_rpdq_quad, right_rpdq_quad);
    // Ensure the RPDQs aren't touching so their damages will be disjoint.
    left_rpdq_quad.Inset(5);
    right_rpdq_quad.Inset(5);

    auto left_child_pass = CreateRenderPass(left_child_pass_id);
    left_child_pass->transform_to_root_target =
        gfx::Transform::MakeTranslation(left_rpdq_quad.OffsetFromOrigin());
    left_child_pass->is_from_surface_root_pass = true;
    left_child_pass->output_rect.set_size(left_rpdq_quad.size());
    left_child_pass->damage_rect = gfx::Rect();
    auto* left_texture_quad =
        is_left_overlay_candidate_with_damage
            ? CreateOverlayQuadWithSurfaceDamageAt(left_child_pass.get(),
                                                   surface_damage_rect_list,
                                                   left_texture_quad_rect)
            : CreateTextureQuadAt(
                  resource_provider_.get(), child_resource_provider_.get(),
                  child_provider_.get(),
                  left_child_pass->CreateAndAppendSharedQuadState(),
                  left_child_pass.get(), left_texture_quad_rect,
                  /*is_overlay_candidate=*/false);
    ResourceId left_child_pass_texture_id = left_texture_quad->resource_id();
    pass_list.push_back(std::move(left_child_pass));

    auto right_child_pass = CreateRenderPass(right_child_pass_id);
    right_child_pass->transform_to_root_target =
        gfx::Transform::MakeTranslation(right_rpdq_quad.OffsetFromOrigin());
    right_child_pass->is_from_surface_root_pass = true;
    right_child_pass->output_rect.set_size(right_rpdq_quad.size());
    right_child_pass->damage_rect = gfx::Rect();
    auto* right_texture_quad =
        is_right_overlay_candidate_with_damage
            ? CreateOverlayQuadWithSurfaceDamageAt(right_child_pass.get(),
                                                   surface_damage_rect_list,
                                                   right_texture_quad_rect)
            : CreateTextureQuadAt(
                  resource_provider_.get(), child_resource_provider_.get(),
                  child_provider_.get(),
                  right_child_pass->CreateAndAppendSharedQuadState(),
                  right_child_pass.get(), right_texture_quad_rect,
                  /*is_overlay_candidate=*/false);
    ResourceId right_child_pass_texture_id = right_texture_quad->resource_id();
    pass_list.push_back(std::move(right_child_pass));

    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               left_rpdq_quad, left_child_pass_id);
    CreateRenderPassDrawQuadAt(pass.get(),
                               pass->CreateAndAppendSharedQuadState(),
                               right_rpdq_quad, right_child_pass_id);
    pass_list.push_back(std::move(pass));

    switch (frame) {
      case 0:
      case 1:
        EXPECT_EQ(pass_list[0]->damage_rect, left_texture_quad_rect)
            << "The quad in the child surface contributes damage";
        EXPECT_EQ(pass_list[1]->damage_rect, right_texture_quad_rect)
            << "The quad in the child surface contributes damage";
        break;

      case 2:
        EXPECT_EQ(pass_list[0]->damage_rect, left_texture_quad_rect)
            << "The quad in the child surface contributes damage";
        EXPECT_EQ(pass_list[1]->damage_rect, gfx::Rect())
            << "No damage on the child surface before overlay processing";
        break;

      case 3:
        EXPECT_EQ(pass_list[0]->damage_rect, gfx::Rect())
            << "No damage on the child surface before overlay processing";
        EXPECT_EQ(pass_list[1]->damage_rect, gfx::Rect())
            << "No damage on the child surface before overlay processing";
        break;
    }

    auto result = TryProcessForDelegatedOverlays(
        pass_list, std::move(surface_damage_rect_list));
    result.ExpectDelegationSuccess();

    switch (frame) {
      case 0:
        EXPECT_EQ(pass_list[0]->damage_rect, pass_list[0]->output_rect)
            << "Full damage is forced on the first frame";
        EXPECT_EQ(pass_list[1]->damage_rect, pass_list[1]->output_rect)
            << "Full damage is forced on the first frame";
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(right_child_pass_id),
                        test::OverlayHasResource(right_child_pass_texture_id),
                        test::IsRenderPassOverlay(left_child_pass_id),
                        test::OverlayHasResource(left_child_pass_texture_id),
                    }));
        break;

      case 1:
        EXPECT_EQ(pass_list[0]->damage_rect, gfx::Rect())
            << "Damage is removed when only from overlays";
        EXPECT_EQ(pass_list[1]->damage_rect, gfx::Rect())
            << "Damage is removed when only from overlays";
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(right_child_pass_id),
                        test::OverlayHasResource(right_child_pass_texture_id),
                        test::IsRenderPassOverlay(left_child_pass_id),
                        test::OverlayHasResource(left_child_pass_texture_id),
                    }));
        break;

      case 2:
        EXPECT_EQ(pass_list[0]->damage_rect, gfx::Rect())
            << "Damage is removed when only from overlays";
        EXPECT_EQ(pass_list[1]->damage_rect, right_texture_quad_rect)
            << "Damage removed in frame 1 is re-added";
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(right_child_pass_id),
                        test::IsRenderPassOverlay(left_child_pass_id),
                        test::OverlayHasResource(left_child_pass_texture_id),
                    }));
        break;

      case 3:
        EXPECT_EQ(pass_list[0]->damage_rect, left_texture_quad_rect)
            << "Damage removed in frame 2 is re-added";
        EXPECT_EQ(pass_list[1]->damage_rect, gfx::Rect());
        EXPECT_THAT(result.candidates(),
                    WhenCandidatesAreSortedElementsAre({
                        test::IsRenderPassOverlay(right_child_pass_id),
                        test::IsRenderPassOverlay(left_child_pass_id),
                    }));
        break;
    }
  }
}

}  // namespace
}  // namespace viz
