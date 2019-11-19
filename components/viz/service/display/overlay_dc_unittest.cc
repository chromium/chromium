// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
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
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/display/overlay_processor.h"
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

class DCLayerValidator : public OverlayCandidateValidator {
 public:
  bool AllowCALayerOverlays() const override { return false; }
  bool AllowDCLayerOverlays() const override { return true; }
  bool NeedsSurfaceOccludingDamageRect() const override { return true; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {}
};

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(
      scoped_refptr<TestContextProvider> context_provider)
      : OutputSurface(std::move(context_provider)) {
    is_displayed_as_overlay_plane_ = true;
  }

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override { bind_framebuffer_count_ += 1; }
  void SetDrawRectangle(const gfx::Rect& rect) override {}
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override {}
  void SwapBuffers(OutputSurfaceFrame frame) override {}
  uint32_t GetFramebufferCopyTextureFormat() override {
    // TestContextProvider has no real framebuffer, just use RGB.
    return GL_RGB;
  }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}
  bool IsDisplayedAsOverlayPlane() const override {
    return is_displayed_as_overlay_plane_;
  }
  unsigned GetOverlayTextureId() const override { return 10000; }
  gfx::BufferFormat GetOverlayBufferFormat() const override {
    return gfx::BufferFormat::RGBX_8888;
  }
  unsigned UpdateGpuFence() override { return 0; }
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override {}
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override {
    return gfx::OVERLAY_TRANSFORM_NONE;
  }

  void set_is_displayed_as_overlay_plane(bool value) {
    is_displayed_as_overlay_plane_ = value;
  }

  unsigned bind_framebuffer_count() const { return bind_framebuffer_count_; }

 private:
  bool is_displayed_as_overlay_plane_;
  unsigned bind_framebuffer_count_ = 0;
};

class DCTestOverlayProcessor : public OverlayProcessor {
 public:
  DCTestOverlayProcessor()
      : OverlayProcessor(std::make_unique<DCLayerValidator>()) {}
};

std::unique_ptr<RenderPass> CreateRenderPass() {
  int render_pass_id = 1;
  gfx::Rect output_rect(0, 0, 256, 256);

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
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

  int child_id = parent_resource_provider->CreateChild(
      base::BindRepeating([](const std::vector<ReturnedResource>&) {}), true);

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
    RenderPass* render_pass,
    const gfx::Rect& rect) {
  SolidColorDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, rect, rect, color, false);
  return quad;
}

void CreateOpaqueQuadAt(DisplayResourceProvider* resource_provider,
                        const SharedQuadState* shared_quad_state,
                        RenderPass* render_pass,
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
    RenderPass* render_pass) {
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
    child_resource_provider_ = std::make_unique<ClientResourceProvider>(true);

    overlay_processor_ = std::make_unique<DCTestOverlayProcessor>();
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
  std::unique_ptr<DCTestOverlayProcessor> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

TEST_F(DCLayerOverlayTest, AllowNonAxisAlignedTransform) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDirectCompositionComplexOverlays);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateYUVVideoQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(45.f);

  DCLayerOverlayList dc_layer_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  damage_rect_ = gfx::Rect(1, 1, 10, 10);
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &dc_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, dc_layer_list.size());
  EXPECT_EQ(1, dc_layer_list.back().z_order);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
  EXPECT_EQ(gfx::Rect(1, 1, 10, 10), damage_rect_);
}

TEST_F(DCLayerOverlayTest, AllowRequiredNonAxisAlignedTransform) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDirectCompositionNonrootOverlays);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  YUVVideoDrawQuad* yuv_quad = CreateFullscreenCandidateYUVVideoQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  // Set the protected video flag will force DCLayerOverlay to use hw overlay
  yuv_quad->protected_video_type = gfx::ProtectedVideoType::kHardwareProtected;
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(45.f);

  gfx::Rect damage_rect;
  DCLayerOverlayList dc_layer_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  damage_rect_ = gfx::Rect(1, 1, 10, 10);
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &dc_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(gfx::Rect(), damage_rect);
  ASSERT_EQ(1U, dc_layer_list.size());
  EXPECT_EQ(1, dc_layer_list.back().z_order);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
  EXPECT_EQ(gfx::Rect(1, 1, 10, 10), damage_rect_);
}

TEST_F(DCLayerOverlayTest, Occluded) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);
  {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    SharedQuadState* first_shared_state = pass->shared_quad_state_list.back();
    first_shared_state->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(0, 3, 100, 100), SK_ColorWHITE);
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

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
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    RenderPassList pass_list;
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
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    SharedQuadState* first_shared_state = pass->shared_quad_state_list.back();
    first_shared_state->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       gfx::Rect(3, 3, 100, 100), SK_ColorWHITE);
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

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
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    RenderPassList pass_list;
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
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
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
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 210, 20, 20);
    RenderPassList pass_list;
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
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
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
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 210, 20, 20);
    RenderPassList pass_list;
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

TEST_F(DCLayerOverlayTest, DamageRectWithNonRootOverlay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kDirectCompositionUnderlays,
                                 features::kDirectCompositionNonrootOverlays},
                                {});
  {
    // A root solid quad
    std::unique_ptr<RenderPass> root_pass = CreateRenderPass();
    CreateOpaqueQuadAt(
        resource_provider_.get(), root_pass->shared_quad_state_list.back(),
        root_pass.get(), gfx::Rect(210, 0, 20, 20), SK_ColorWHITE);

    // A non-root video quad
    std::unique_ptr<RenderPass> nonroot_pass = CreateRenderPass();
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), nonroot_pass->shared_quad_state_list.back(),
        nonroot_pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 0, 20, 20);
    RenderPassList pass_list;
    pass_list.push_back(std::move(nonroot_pass));
    pass_list.push_back(std::move(root_pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.back().z_order);
    // damage_rect returned from ProcessForOverlays() is for root render pass
    // only. Non-root damage rect is not included.
    EXPECT_EQ(gfx::Rect(210, 0, 20, 20), damage_rect_);
  }
  {
    // A root solid quad
    std::unique_ptr<RenderPass> root_pass = CreateRenderPass();
    CreateOpaqueQuadAt(
        resource_provider_.get(), root_pass->shared_quad_state_list.back(),
        root_pass.get(), gfx::Rect(210, 0, 20, 20), SK_ColorWHITE);

    // A non-root video quad
    std::unique_ptr<RenderPass> nonroot_pass = CreateRenderPass();
    auto* video_quad = CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), nonroot_pass->shared_quad_state_list.back(),
        nonroot_pass.get());
    video_quad->rect = gfx::Rect(0, 0, 200, 200);
    video_quad->visible_rect = video_quad->rect;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    // Damage rect fully outside video quad
    damage_rect_ = gfx::Rect(210, 0, 20, 20);
    RenderPassList pass_list;
    pass_list.push_back(std::move(nonroot_pass));
    pass_list.push_back(std::move(root_pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &dc_layer_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(1U, dc_layer_list.size());
    EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
    EXPECT_EQ(-1, dc_layer_list.back().z_order);
    // Nonroot damage_rect from the previous frame should be added to this frame
    EXPECT_EQ(gfx::Rect(0, 0, 230, 200), damage_rect_);
  }
}

TEST_F(DCLayerOverlayTest, DamageRect) {
  for (int i = 0; i < 2; i++) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    gfx::Rect damage_rect;
    DCLayerOverlayList dc_layer_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    RenderPassList pass_list;
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

TEST_F(DCLayerOverlayTest, MultiplePassDamageRect) {
  gfx::Transform child_pass1_transform;
  child_pass1_transform.Translate(0, 100);

  RenderPassId child_pass1_id(5);
  std::unique_ptr<RenderPass> child_pass1 = CreateRenderPass();
  ASSERT_EQ(child_pass1->shared_quad_state_list.size(), 1u);
  child_pass1->id = child_pass1_id;
  child_pass1->damage_rect = gfx::Rect();
  child_pass1->transform_to_root_target = child_pass1_transform;
  child_pass1->shared_quad_state_list.back()->opacity = 0.9f;
  child_pass1->shared_quad_state_list.back()->blend_mode =
      SkBlendMode::kSrcOver;

  YUVVideoDrawQuad* yuv_quad_required = CreateFullscreenCandidateYUVVideoQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), child_pass1->shared_quad_state_list.back(),
      child_pass1.get());
  // Set the protected video flag will force DCLayerOverlay to use hw overlay
  yuv_quad_required->protected_video_type =
      gfx::ProtectedVideoType::kHardwareProtected;

  RenderPassId child_pass2_id(6);
  std::unique_ptr<RenderPass> child_pass2 = CreateRenderPass();
  ASSERT_EQ(child_pass2->shared_quad_state_list.size(), 1u);
  child_pass2->id = child_pass2_id;
  child_pass2->damage_rect = gfx::Rect();
  child_pass2->transform_to_root_target = gfx::Transform();
  child_pass2->shared_quad_state_list.back()->opacity = 0.8f;

  YUVVideoDrawQuad* yuv_quad_not_required =
      CreateFullscreenCandidateYUVVideoQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), child_pass2->shared_quad_state_list.back(),
          child_pass2.get());

  std::unique_ptr<RenderPass> root_pass = CreateRenderPass();
  root_pass->CreateAndAppendSharedQuadState();
  ASSERT_EQ(root_pass->shared_quad_state_list.size(), 2u);

  SharedQuadState* child_pass1_sqs =
      root_pass->shared_quad_state_list.ElementAt(0);
  child_pass1_sqs->quad_to_target_transform =
      child_pass1->transform_to_root_target;
  child_pass1_sqs->opacity = 0.7f;

  gfx::Rect unit_rect(0, 0, 1, 1);
  auto* child_pass1_rpdq =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  child_pass1_rpdq->SetNew(child_pass1_sqs, unit_rect, unit_rect,
                           child_pass1_id, 0, gfx::RectF(), gfx::Size(),
                           gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(0, 0, 1, 1), false, 1.0f);

  SharedQuadState* child_pass2_sqs =
      root_pass->shared_quad_state_list.ElementAt(1);
  child_pass2_sqs->quad_to_target_transform =
      child_pass2->transform_to_root_target;
  child_pass2_sqs->opacity = 0.6f;

  auto* child_pass2_rpdq =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  child_pass2_rpdq->SetNew(child_pass2_sqs, unit_rect, unit_rect,
                           child_pass2_id, 0, gfx::RectF(), gfx::Size(),
                           gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(0, 0, 1, 1), false, 1.0f);

  root_pass->damage_rect = gfx::Rect();

  gfx::Rect root_damage_rect;
  DCLayerOverlayList dc_layer_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass1));
  pass_list.push_back(std::move(child_pass2));
  pass_list.push_back(std::move(root_pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &dc_layer_list, &root_damage_rect, &content_bounds_);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());

  // Only the kHardwareProtectedVideo video quad produces damage.
  ASSERT_EQ(1U, dc_layer_list.size());
  EXPECT_EQ(-1, dc_layer_list.back().z_order);
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), pass_list[0]->damage_rect);
  EXPECT_EQ(gfx::Rect(), pass_list[1]->damage_rect);
  EXPECT_EQ(gfx::Rect(0, 100, 256, 156), root_damage_rect);
  // Overlay damage handling is done entirely within DCOverlayProcessor so this
  // is expected to return an empty rect
  gfx::Rect overlay_damage = overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), overlay_damage);

  EXPECT_EQ(1u, pass_list[0]->quad_list.size());
  EXPECT_EQ(DrawQuad::Material::kSolidColor,
            pass_list[0]->quad_list.ElementAt(0)->material);

  // The kHardwareProtectedVideo video quad is put into an underlay, and
  // replaced by a solid color quad.
  auto* yuv_solid_color_quad =
      static_cast<SolidColorDrawQuad*>(pass_list[0]->quad_list.ElementAt(0));
  EXPECT_EQ(SK_ColorBLACK, yuv_solid_color_quad->color);
  EXPECT_EQ(gfx::Rect(0, 0, 256, 256), yuv_solid_color_quad->rect);
  EXPECT_TRUE(yuv_solid_color_quad->shared_quad_state->quad_to_target_transform
                  .IsIdentity());
  EXPECT_EQ(0.9f, yuv_solid_color_quad->shared_quad_state->opacity);
  EXPECT_EQ(SkBlendMode::kDstOut,
            yuv_solid_color_quad->shared_quad_state->blend_mode);

  // The non required video quad is not put into an underlay.
  EXPECT_EQ(1u, pass_list[1]->quad_list.size());
  EXPECT_EQ(yuv_quad_not_required, pass_list[1]->quad_list.ElementAt(0));

  EXPECT_EQ(3u, pass_list[2]->quad_list.size());

  // The RPDQs are not modified.
  EXPECT_EQ(DrawQuad::Material::kRenderPass,
            pass_list[2]->quad_list.ElementAt(0)->material);
  EXPECT_EQ(child_pass1_id, static_cast<RenderPassDrawQuad*>(
                                pass_list[2]->quad_list.ElementAt(0))
                                ->render_pass_id);

  // A solid color quad is put behind the RPDQ containing the video.
  EXPECT_EQ(DrawQuad::Material::kSolidColor,
            pass_list[2]->quad_list.ElementAt(1)->material);
  auto* rpdq_solid_color_quad =
      static_cast<SolidColorDrawQuad*>(pass_list[2]->quad_list.ElementAt(1));
  EXPECT_EQ(SK_ColorTRANSPARENT, rpdq_solid_color_quad->color);
  EXPECT_EQ(child_pass1_transform,
            rpdq_solid_color_quad->shared_quad_state->quad_to_target_transform);
  EXPECT_EQ(1.f, rpdq_solid_color_quad->shared_quad_state->opacity);
  EXPECT_FALSE(rpdq_solid_color_quad->ShouldDrawWithBlending());

  EXPECT_EQ(DrawQuad::Material::kRenderPass,
            pass_list[2]->quad_list.ElementAt(2)->material);
  EXPECT_EQ(child_pass2_id, static_cast<RenderPassDrawQuad*>(
                                pass_list[2]->quad_list.ElementAt(2))
                                ->render_pass_id);
}

TEST_F(DCLayerOverlayTest, ClipRect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDirectCompositionUnderlays);

  // Process twice. The second time through the overlay list shouldn't change,
  // which will allow the damage rect to reflect just the changes in that
  // frame.
  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
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
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
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
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    CreateFullscreenCandidateYUVVideoQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
    pass->shared_quad_state_list.back()->opacity = 0.5f;

    DCLayerOverlayList dc_layer_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    damage_rect_ = gfx::Rect(1, 1, 10, 10);
    RenderPassList pass_list;
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
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
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
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
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

}  // namespace
}  // namespace viz
