// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_win.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_switches.h"
#include "ui/latency/latency_info.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::Rect kOverlayBottomRightRect(128, 128, 128, 128);

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(
      scoped_refptr<TestContextProvider> context_provider)
      : OutputSurface(std::move(context_provider)) {
    capabilities_.supports_dc_layers = true;
  }

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override { bind_framebuffer_count_ += 1; }
  void SetDrawRectangle(const gfx::Rect& rect) override {}
  MOCK_METHOD1(SetEnableDCLayers, void(bool));
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               bool use_stencil) override {}
  void SwapBuffers(OutputSurfaceFrame frame) override {}
  uint32_t GetFramebufferCopyTextureFormat() override {
    // TestContextProvider has no real framebuffer, just use RGB.
    return GL_RGB;
  }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 10000; }
  unsigned UpdateGpuFence() override { return 0; }
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OVERLAY_TRANSFORM_NONE;
  }
  scoped_refptr<gpu::GpuTaskSchedulerHelper> GetGpuTaskSchedulerHelper()
      override {
    return nullptr;
  }
  gpu::MemoryTracker* GetMemoryTracker() override { return nullptr; }

  unsigned bind_framebuffer_count() const { return bind_framebuffer_count_; }

 private:
  unsigned bind_framebuffer_count_ = 0;
};

class DCTestOverlayProcessor : public OverlayProcessorWin {
 public:
  explicit DCTestOverlayProcessor(OutputSurface* output_surface)
      : OverlayProcessorWin(
            output_surface,
            std::make_unique<DCLayerOverlayProcessor>(&debug_settings_, true)) {
  }
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
  auto resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      size, is_overlay_candidate);
  auto release_callback = SingleReleaseCallback::Create(
      base::BindRepeating([](const gpu::SyncToken&, bool) {}));

  ResourceId resource_id = child_resource_provider->ImportResource(
      resource, std::move(release_callback));

  return resource_id;
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          ContextProvider* child_context_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate) {
  ResourceId resource_id = CreateResourceInLayerTree(
      child_resource_provider, size, is_overlay_candidate);

  int child_id = parent_resource_provider->CreateChild(base::DoNothing());

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
  std::unordered_map<ResourceId, ResourceId> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  return resource_map[list[0].id];
}

SolidColorDrawQuad* CreateSolidColorQuadAt(
    const SharedQuadState* shared_quad_state,
    SkColor color,
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
                        SkColor color) {
  DCHECK_EQ(255u, SkColorGetA(color));
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
  gfx::RectF tex_coord_rect(0, 0, 1, 1);
  gfx::Rect rect = render_pass->output_rect;
  gfx::Size resource_size_in_pixels = rect.size();
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, needs_blending,
                       tex_coord_rect, tex_coord_rect, resource_size_in_pixels,
                       resource_size_in_pixels, resource_id, resource_id,
                       resource_id, resource_id,
                       gfx::ColorSpace::CreateREC601(), 0, 1.0, 8);

  return overlay_quad;
}

SkMatrix44 GetIdentityColorMatrix() {
  return SkMatrix44(SkMatrix44::kIdentity_Constructor);
}

class DCLayerOverlayTest : public testing::Test {
 protected:
  void SetUp() override {
    provider_ = TestContextProvider::Create();
    provider_->BindToCurrentThread();
    output_surface_ = std::make_unique<OverlayOutputSurface>(provider_);
    output_surface_->BindToClient(&client_);

    shared_bitmap_manager_ = std::make_unique<TestSharedBitmapManager>();
    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        DisplayResourceProvider::kGpu, provider_.get(),
        shared_bitmap_manager_.get());

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentThread();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();

    overlay_processor_ =
        std::make_unique<DCTestOverlayProcessor>(output_surface_.get());
    overlay_processor_->set_using_dc_layers_for_testing(true);
    EXPECT_TRUE(overlay_processor_->IsOverlaySupported());
  }

  void TearDown() override {
    overlay_processor_ = nullptr;
    child_resource_provider_->ShutdownAndReleaseAllResources();
    child_resource_provider_ = nullptr;
    child_provider_ = nullptr;
    resource_provider_ = nullptr;
    shared_bitmap_manager_ = nullptr;
    output_surface_ = nullptr;
    provider_ = nullptr;
  }

  scoped_refptr<TestContextProvider> provider_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient client_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<OverlayProcessorWin> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

TEST_F(DCLayerOverlayTest, Occluded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);
  {
    auto pass = CreateRenderPass();
    SharedQuadState* first_shared_state = pass->shared_quad_state_list.back();
    first_shared_state->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SK_ColorWHITE);
    auto* first_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force DCLayerOverlay to use hw overlay
    first_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;

    SharedQuadState* second_shared_state =
        pass->CreateAndAppendSharedQuadState();
    second_shared_state->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    auto* second_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force DCLayerOverlay to use hw overlay
    second_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
    second_video_quad->rect.set_origin(gfx::Point(2, 2));
    second_video_quad->visible_rect.set_origin(gfx::Point(2, 2));

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);

    EXPECT_EQ(2U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.front().z_order);
    EXPECT_EQ(-2, dc_layer_list.back().z_order);
    // Entire underlay rect must be redrawn.
    EXPECT_EQ(gfx::Rect(0, 0, 256, 256), damage_rect_);
  }
  {
    auto pass = CreateRenderPass();
    SharedQuadState* first_shared_state = pass->shared_quad_state_list.back();
    first_shared_state->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(3, 3, 100, 100), SK_ColorWHITE);
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    // Set the protected video flag will force DCLayerOverlay to use hw overlay
    video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;

    SharedQuadState* second_shared_state =
        pass->CreateAndAppendSharedQuadState();
    second_shared_state->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    auto* second_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    second_video_quad->protected_video_type =
        gfx::ProtectedVideoType::kHardwareProtected;
    second_video_quad->rect.set_origin(gfx::Point(2, 2));
    second_video_quad->visible_rect.set_origin(gfx::Point(2, 2));

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);

    EXPECT_EQ(2U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.front().z_order);
    EXPECT_EQ(-2, dc_layer_list.back().z_order);
    // The underlay rectangle is the same, so the damage for first video quad is
    // contained within the combined occluding rects for this and the last
    // frame. Second video quad also adds its damage.

    // This is calculated by carving out the underlay rect size from the
    // damage_rect, adding back the quads on top and then the overlay/underlay
    // rects from the previous frame. The damage rect carried over from  the
    // revious frame with multiple overlays cannot be skipped.
    EXPECT_EQ(gfx::Rect(0, 0, 256, 256), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, DamageRectWithoutVideoDamage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);
  {
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state = pass->shared_quad_state_list.back();
    shared_quad_state->occluding_damage_rect = gfx::Rect(210, 210, 20, 20);
    // Occluding quad fully contained in video rect.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SK_ColorWHITE);
    // Non-occluding quad fully outside video rect
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(210, 210, 20, 20), SK_ColorWHITE);
    // Underlay video quad
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 210, 20, 20);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.back().z_order);
    // All rects must be redrawn at the first frame.
    EXPECT_EQ(gfx::Rect(0, 0, 230, 230), damage_rect_);
  }
  {
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state = pass->shared_quad_state_list.back();
    shared_quad_state->occluding_damage_rect = gfx::Rect(210, 210, 20, 20);
    // Occluding quad fully contained in video rect.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SK_ColorWHITE);
    // Non-occluding quad fully outside video rect
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(210, 210, 20, 20), SK_ColorWHITE);
    // Underlay video quad
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 210, 20, 20);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.back().z_order);
    // Only the non-overlay damaged rect need to be drawn by the gl compositor
    EXPECT_EQ(gfx::Rect(210, 210, 20, 20), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, DamageRect) {
  for (int i = 0; i < 2; i++) {
    auto pass = CreateRenderPass();
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    gfx::Rect damage_rect;
    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(gfx::Rect(), damage_rect);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(1, dc_layer_list.back().z_order);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);

  // Process twice. The second time through the overlay list shouldn't change,
  // which will allow the damage rect to reflect just the changes in that
  // frame.
  for (size_t i = 0; i < 2; ++i) {
    auto pass = CreateRenderPass();
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 2, 100, 100), SK_ColorWHITE);
    pass->shared_quad_state_list.back()->is_clipped = true;
    pass->shared_quad_state_list.back()->clip_rect = gfx::Rect(0, 3, 100, 100);
    SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->opacity = 1.f;
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), shared_state, pass.get());
    shared_state->is_clipped = true;
    // Clipped rect shouldn't be overlapped by clipped opaque quad rect.
    shared_state->clip_rect = gfx::Rect(0, 0, 100, 3);

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    // Because of clip rects the overlay isn't occluded and shouldn't be an
    // underlay.
    EXPECT_EQ(1, dc_layer_list.back().z_order);
    EXPECT_TRUE(dc_layer_list.back().is_clipped);
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
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    pass->shared_quad_state_list.back()->opacity = 0.5f;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(1, dc_layer_list.back().z_order);
    // Quad isn't opaque, so underlying damage must remain the same.
    EXPECT_EQ(gfx::Rect(1, 1, 10, 10), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, UnderlayDamageRectWithQuadOnTopUnchanged) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);

  for (int i = 0; i < 3; i++) {
    auto pass = CreateRenderPass();
    // Add a solid color quad on top
    SharedQuadState* shared_state_on_top = pass->shared_quad_state_list.back();
    CreateSolidColorQuadAt(shared_state_on_top, SK_ColorRED, pass.get(),
                           kOverlayBottomRightRect);

    SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->opacity = 1.f;
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), shared_state, pass.get());

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    gfx::Rect damage_rect_ = kOverlayRect;

    // The quad on top does not give damage on the third frame
    if (i == 2)
      shared_state->occluding_damage_rect = gfx::Rect();
    else
      shared_state->occluding_damage_rect = kOverlayBottomRightRect;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.back().z_order);
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

// If there are multiple yuv overlay quad candidates, no overlay will be
// promoted to save power.
TEST_F(DCLayerOverlayTest, MultipleYUVOverlay) {
  base::test::ScopedFeatureList feature_list;
  {
    auto pass = CreateRenderPass();
    pass->shared_quad_state_list.back();
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 0, 256, 256), SK_ColorWHITE);

    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect rect(10, 10, 80, 80);
    video_quad->rect = rect;
    video_quad->visible_rect = rect;

    auto* second_video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    gfx::Rect second_rect(100, 100, 120, 120);
    second_video_quad->rect = second_rect;
    second_video_quad->visible_rect = second_rect;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(0, 0, 220, 220);
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);

    // Skip overlays.
    EXPECT_EQ(0U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
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

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
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
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);

    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(1, dc_layer_list.back().z_order);
    EXPECT_EQ(damage_rect_, expected_damage);

    Mock::VerifyAndClearExpectations(output_surface_.get());
  }

  // Draw 65 frames without overlays.
  for (int i = 0; i < 65; i++) {
    auto pass = CreateRenderPass();

    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    auto* quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    quad->SetNew(pass->CreateAndAppendSharedQuadState(), damage_rect_,
                 damage_rect_, SK_ColorRED, false);

    DCLayerOverlayList dc_layer_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

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
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);

    EXPECT_EQ(0u, dc_layer_list.size());
    EXPECT_EQ(0u, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(damage_rect_, expected_damage);

    Mock::VerifyAndClearExpectations(output_surface_.get());
  }
}

}  // namespace
}  // namespace viz
