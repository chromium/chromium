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
#include "build/build_config.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_mac.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/latency/latency_info.h"

using testing::_;
using testing::Mock;

namespace viz {
namespace {

const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);
const gfx::Rect kRenderPassOutputRect(0, 0, 256, 256);

class OverlayOutputSurface : public OutputSurface {
 public:
  explicit OverlayOutputSurface(
      scoped_refptr<TestContextProvider> context_provider)
      : OutputSurface(std::move(context_provider)) {}

  // OutputSurface implementation.
  void BindToClient(OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override { bind_framebuffer_count_ += 1; }
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

class CATestOverlayProcessor : public OverlayProcessorMac {
 public:
  CATestOverlayProcessor()
      : OverlayProcessorMac(true /* could_overlay */,
                            true /* enable_ca_overlay */) {}
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

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::ProtectedVideoType protected_video_type) {
  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  gfx::Size resource_size_in_pixels = rect.size();
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, needs_blending,
                       resource_id, premultiplied_alpha, kUVTopLeft,
                       kUVBottomRight, SK_ColorTRANSPARENT, vertex_opacity,
                       flipped, nearest_neighbor, /*secure_output_only=*/false,
                       protected_video_type);
  overlay_quad->set_resource_size_in_pixels(resource_size_in_pixels);

  return overlay_quad;
}

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, rect, gfx::ProtectedVideoType::kClear);
}

TextureDrawQuad* CreateFullscreenCandidateQuad(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, render_pass->output_rect);
}

SkMatrix44 GetIdentityColorMatrix() {
  return SkMatrix44(SkMatrix44::kIdentity_Constructor);
}

class CALayerOverlayTest : public testing::Test {
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

    overlay_processor_ = std::make_unique<CATestOverlayProcessor>();
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
  std::unique_ptr<CATestOverlayProcessor> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

TEST_F(CALayerOverlayTest, AllowNonAxisAlignedTransform) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(45.f);

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &ca_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(gfx::Rect(), damage_rect);
  EXPECT_EQ(1U, ca_layer_list.size());
  gfx::Rect overlay_damage = overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_EQ(kRenderPassOutputRect, overlay_damage);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, ThreeDTransform) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  CALayerOverlayList ca_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &ca_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, ca_layer_list.size());
  gfx::Rect overlay_damage = overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_EQ(kRenderPassOutputRect, overlay_damage);
  gfx::Transform expected_transform;
  expected_transform.RotateAboutXAxis(45.f);
  gfx::Transform actual_transform(ca_layer_list.back().shared_state->transform);
  EXPECT_EQ(expected_transform.ToString(), actual_transform.ToString());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, AllowContainingClip) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayRect;

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &ca_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(gfx::Rect(), damage_rect);
  EXPECT_EQ(1U, ca_layer_list.size());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, NontrivialClip) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = gfx::Rect(64, 64, 128, 128);

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &ca_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(gfx::Rect(), damage_rect);
  EXPECT_EQ(1U, ca_layer_list.size());
  EXPECT_TRUE(ca_layer_list.back().shared_state->is_clipped);
  EXPECT_EQ(gfx::RectF(64, 64, 128, 128),
            ca_layer_list.back().shared_state->clip_rect);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, SkipTransparent) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->opacity = 0;

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &ca_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(gfx::Rect(), damage_rect);
  EXPECT_EQ(0U, ca_layer_list.size());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
}

TEST_F(CALayerOverlayTest, SkipNonVisible) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->quad_list.back()->visible_rect.set_size(gfx::Size());

  gfx::Rect damage_rect;
  CALayerOverlayList ca_layer_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &ca_layer_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(gfx::Rect(), damage_rect);
  EXPECT_EQ(0U, ca_layer_list.size());
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
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
        render_pass_filters_, render_pass_backdrop_filters_, nullptr,
        &ca_layer_list_, &damage_rect_, &content_bounds_);
  }
  AggregatedRenderPassList pass_list_;
  AggregatedRenderPass* pass_;
  AggregatedRenderPassDrawQuad* quad_;
  AggregatedRenderPassId render_pass_id_;
  cc::FilterOperations filters_;
  cc::FilterOperations backdrop_filters_;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters_;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters_;
  CALayerOverlayList ca_layer_list_;
};

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadNoFilters) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();

  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadAllValidFilters) {
  filters_.Append(cc::FilterOperation::CreateGrayscaleFilter(0.1f));
  filters_.Append(cc::FilterOperation::CreateSepiaFilter(0.2f));
  filters_.Append(cc::FilterOperation::CreateSaturateFilter(0.3f));
  filters_.Append(cc::FilterOperation::CreateHueRotateFilter(0.4f));
  filters_.Append(cc::FilterOperation::CreateInvertFilter(0.5f));
  filters_.Append(cc::FilterOperation::CreateBrightnessFilter(0.6f));
  filters_.Append(cc::FilterOperation::CreateContrastFilter(0.7f));
  filters_.Append(cc::FilterOperation::CreateOpacityFilter(0.8f));
  filters_.Append(cc::FilterOperation::CreateBlurFilter(0.9f));
  filters_.Append(cc::FilterOperation::CreateDropShadowFilter(
      gfx::Point(10, 20), 1.0f, SK_ColorGREEN));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();

  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadOpacityFilterScale) {
  filters_.Append(cc::FilterOperation::CreateOpacityFilter(0.8f));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 2), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBlurFilterScale) {
  filters_.Append(cc::FilterOperation::CreateBlurFilter(0.8f));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 2), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadDropShadowFilterScale) {
  filters_.Append(cc::FilterOperation::CreateDropShadowFilter(
      gfx::Point(10, 20), 1.0f, SK_ColorGREEN));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 2), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadBackgroundFilter) {
  backdrop_filters_.Append(cc::FilterOperation::CreateGrayscaleFilter(0.1f));
  render_pass_backdrop_filters_[render_pass_id_] = &backdrop_filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadMask) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 2, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, RenderPassDrawQuadUnsupportedFilter) {
  filters_.Append(cc::FilterOperation::CreateZoomFilter(0.9f, 1));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 0, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, TooManyRenderPassDrawQuads) {
  filters_.Append(cc::FilterOperation::CreateBlurFilter(0.8f));
  int count = 35;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, 2, gfx::RectF(), gfx::Size(),
                gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);
  for (int i = 1; i < count; ++i) {
    auto* quad = pass_->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    quad->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                 kOverlayRect, render_pass_id_, 2, gfx::RectF(), gfx::Size(),
                 gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                 1.0f);
  }

  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}

}  // namespace
}  // namespace viz
