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
#include "build/build_config.h"
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
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
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

const gfx::Size kDisplaySize(256, 256);
const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::Rect kOverlayTopLeftRect(0, 0, 128, 128);
const gfx::Rect kOverlayBottomRightRect(128, 128, 128, 128);
const gfx::Rect kOverlayClipRect(0, 0, 128, 128);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);
const gfx::Transform kNormalTransform =
    gfx::Transform(0.9f, 0, 0, 0.8f, 0.1f, 0.2f);  // x,y -> x,y.
const gfx::Transform kXMirrorTransform =
    gfx::Transform(-0.9f, 0, 0, 0.8f, 1.0f, 0.2f);  // x,y -> 1-x,y.
const gfx::Transform kYMirrorTransform =
    gfx::Transform(0.9f, 0, 0, -0.8f, 0.1f, 1.0f);  // x,y -> x,1-y.
const gfx::Transform kBothMirrorTransform =
    gfx::Transform(-0.9f, 0, 0, -0.8f, 1.0f, 1.0f);  // x,y -> 1-x,1-y.
const gfx::Transform kSwapTransform =
    gfx::Transform(0, 1, 1, 0, 0, 0);  // x,y -> y,x.
const gfx::BufferFormat kDefaultBufferFormat = gfx::BufferFormat::RGBA_8888;

class TestOverlayCandidateValidator : public OverlayCandidateValidator {
 public:
  size_t GetStrategyCount() const { return strategies_.size(); }

  bool AllowCALayerOverlays() const override { return false; }
  bool AllowDCLayerOverlays() const override { return false; }
  bool NeedsSurfaceOccludingDamageRect() const override { return false; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {}
};

class FullscreenOverlayValidator : public TestOverlayCandidateValidator {
 public:
  void InitializeStrategies() override {
    strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
  }
  bool AllowCALayerOverlays() const override { return false; }
  bool AllowDCLayerOverlays() const override { return false; }
  bool NeedsSurfaceOccludingDamageRect() const override { return true; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {
    surfaces->back().overlay_handled = true;
  }
};

class SingleOverlayValidator : public TestOverlayCandidateValidator {
 public:
  SingleOverlayValidator() : expected_rects_(1, gfx::RectF(kOverlayRect)) {}

  void InitializeStrategies() override {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
  }
  bool AllowCALayerOverlays() const override { return false; }
  bool AllowDCLayerOverlays() const override { return false; }
  bool NeedsSurfaceOccludingDamageRect() const override { return true; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {
    // We have one overlay surface to test. The output surface as primary plane
    // is optional, depending on whether this ran
    // through the full renderer and picked up the output surface, or not.
    ASSERT_EQ(1U, surfaces->size());

    OverlayCandidate& candidate = surfaces->back();
    for (const auto& r : expected_rects_) {
      const float kAbsoluteError = 0.01f;
      if (std::abs(r.x() - candidate.display_rect.x()) <= kAbsoluteError &&
          std::abs(r.y() - candidate.display_rect.y()) <= kAbsoluteError &&
          std::abs(r.width() - candidate.display_rect.width()) <=
              kAbsoluteError &&
          std::abs(r.height() - candidate.display_rect.height()) <=
              kAbsoluteError) {
        EXPECT_FLOAT_RECT_EQ(BoundingRect(kUVTopLeft, kUVBottomRight),
                             candidate.uv_rect);
        if (!candidate.clip_rect.IsEmpty()) {
          EXPECT_EQ(true, candidate.is_clipped);
          EXPECT_EQ(kOverlayClipRect, candidate.clip_rect);
        }
        candidate.overlay_handled = true;
        return;
      }
    }
    // We should find one rect in expected_rects_that matches candidate.
    EXPECT_TRUE(false);
  }

  void AddExpectedRect(const gfx::RectF& rect) {
    expected_rects_.push_back(rect);
  }

 private:
  std::vector<gfx::RectF> expected_rects_;
};

class CALayerValidator : public TestOverlayCandidateValidator {
 public:
  bool AllowCALayerOverlays() const override { return true; }
  bool AllowDCLayerOverlays() const override { return false; }
  bool NeedsSurfaceOccludingDamageRect() const override { return false; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {}
};

class DCLayerValidator : public TestOverlayCandidateValidator {
 public:
  bool AllowCALayerOverlays() const override { return false; }
  bool AllowDCLayerOverlays() const override { return true; }
  bool NeedsSurfaceOccludingDamageRect() const override { return true; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {}
};

class SingleOnTopOverlayValidator : public SingleOverlayValidator {
 public:
  void InitializeStrategies() override {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  }
};

class UnderlayOverlayValidator : public SingleOverlayValidator {
 public:
  void InitializeStrategies() override {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
  }
};

class TransparentUnderlayOverlayValidator : public SingleOverlayValidator {
 public:
  void InitializeStrategies() override {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
        this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  }
};

class UnderlayCastOverlayValidator : public SingleOverlayValidator {
 public:
  void InitializeStrategies() override {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlayCast>(this));
  }
};

class DefaultOverlayProcessor : public OverlayProcessor {
 public:
  DefaultOverlayProcessor()
      : OverlayProcessor(std::make_unique<SingleOverlayValidator>()) {}

  size_t GetStrategyCount() const {
    if (auto* validator = overlay_validator_.get()) {
      return static_cast<TestOverlayCandidateValidator*>(validator)
          ->GetStrategyCount();
    }

    return 0;
  }
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

template <typename OverlayCandidateValidatorType>
class TypedOverlayProcessor : public OverlayProcessor {
 public:
  explicit TypedOverlayProcessor(
      std::unique_ptr<OverlayCandidateValidatorType> validator)
      : OverlayProcessor(std::move(validator)) {}

  OverlayCandidateValidatorType* GetTypedOverlayCandidateValidator() {
    const OverlayCandidateValidator* const_base_validator =
        GetOverlayCandidateValidator();
    // This function is used to modify the expectation of the validator, so
    // first need to cast away the const.
    auto* base_validator =
        const_cast<OverlayCandidateValidator*>(const_base_validator);
    // Then cast to the test only types so the add expectation function exists.
    return static_cast<OverlayCandidateValidatorType*>(base_validator);
  }
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

std::unique_ptr<RenderPass> CreateRenderPassWithTransform(
    const gfx::Transform& transform) {
  int render_pass_id = 1;
  gfx::Rect output_rect(0, 0, 256, 256);

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(render_pass_id, output_rect, output_rect, gfx::Transform());

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  shared_state->quad_to_target_transform = transform;
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

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
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
    RenderPass* render_pass,
    const gfx::Rect& rect) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, rect, gfx::ProtectedVideoType::kClear);
}

// For Cast we use VideoHoleDrawQuad, and that's what overlay_processor_
// expects.
VideoHoleDrawQuad* CreateVideoHoleDrawQuadAt(
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
    const gfx::Rect& rect) {
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();
  auto* overlay_quad =
      render_pass->CreateAndAppendDrawQuad<VideoHoleDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, overlay_plane_id);
  return overlay_quad;
}

TextureDrawQuad* CreateTransparentCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass,
    const gfx::Rect& rect) {
  bool needs_blending = true;
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
                       gfx::ProtectedVideoType::kClear);
  overlay_quad->set_resource_size_in_pixels(resource_size_in_pixels);

  return overlay_quad;
}

TextureDrawQuad* CreateFullscreenCandidateQuad(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    RenderPass* render_pass) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, render_pass->output_rect);
}

void CreateOpaqueQuadAt(DisplayResourceProvider* resource_provider,
                        const SharedQuadState* shared_quad_state,
                        RenderPass* render_pass,
                        const gfx::Rect& rect) {
  auto* color_quad = render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
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

void CreateFullscreenOpaqueQuad(DisplayResourceProvider* resource_provider,
                                const SharedQuadState* shared_quad_state,
                                RenderPass* render_pass) {
  CreateOpaqueQuadAt(resource_provider, shared_quad_state, render_pass,
                     render_pass->output_rect);
}

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i].get();
    RenderPass* actual = actual_list[i].get();

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (auto exp_iter = expected->quad_list.cbegin(),
              act_iter = actual->quad_list.cbegin();
         exp_iter != expected->quad_list.cend(); ++exp_iter, ++act_iter) {
      EXPECT_EQ(exp_iter->rect.ToString(), act_iter->rect.ToString());
      EXPECT_EQ(exp_iter->shared_quad_state->quad_layer_rect.ToString(),
                act_iter->shared_quad_state->quad_layer_rect.ToString());
    }
  }
}

SkMatrix44 GetIdentityColorMatrix() {
  return SkMatrix44(SkMatrix44::kIdentity_Constructor);
}

SkMatrix GetNonIdentityColorMatrix() {
  SkMatrix44 matrix = GetIdentityColorMatrix();
  matrix.set(1, 1, 0.5f);
  matrix.set(2, 2, 0.5f);
  return matrix;
}

template <typename OverlayCandidateValidatorType>
class OverlayTest : public testing::Test {
  using OverlayProcessorType =
      TypedOverlayProcessor<OverlayCandidateValidatorType>;

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

    overlay_processor_ = std::make_unique<OverlayProcessorType>(
        std::make_unique<OverlayCandidateValidatorType>());
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

  void AddExpectedRectToOverlayValidator(const gfx::RectF& rect) {
    overlay_processor_->GetTypedOverlayCandidateValidator()->AddExpectedRect(
        rect);
  }

  scoped_refptr<TestContextProvider> provider_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient client_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<OverlayProcessorType> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

using FullscreenOverlayTest = OverlayTest<FullscreenOverlayValidator>;
using SingleOverlayOnTopTest = OverlayTest<SingleOnTopOverlayValidator>;
using UnderlayTest = OverlayTest<UnderlayOverlayValidator>;
using TransparentUnderlayTest =
    OverlayTest<TransparentUnderlayOverlayValidator>;
using UnderlayCastTest = OverlayTest<UnderlayCastOverlayValidator>;
using CALayerOverlayTest = OverlayTest<CALayerValidator>;

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  provider->BindToCurrentThread();
  OverlayOutputSurface output_surface(provider);
  cc::FakeOutputSurfaceClient client;
  output_surface.BindToClient(&client);

  auto shared_bitmap_manager = std::make_unique<TestSharedBitmapManager>();
  std::unique_ptr<DisplayResourceProvider> resource_provider =
      std::make_unique<DisplayResourceProvider>(DisplayResourceProvider::kGpu,
                                                provider.get(),
                                                shared_bitmap_manager.get());

  auto overlay_processor = std::make_unique<DefaultOverlayProcessor>();
  EXPECT_GE(2U, overlay_processor->GetStrategyCount());
}

#if !defined(OS_MACOSX) && !defined(OS_WIN)
TEST_F(FullscreenOverlayTest, SuccessfulOverlay) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  TextureDrawQuad* original_quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  unsigned original_resource_id = original_quad->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  ASSERT_EQ(1U, candidate_list.size());

  // Check that all the quads are gone.
  EXPECT_EQ(0U, main_pass->quad_list.size());
  // Check that we have only one overlay.
  EXPECT_EQ(1U, candidate_list.size());
  // Check that the right resource id got extracted.
  EXPECT_EQ(original_resource_id, candidate_list.front().resource_id);
  gfx::Rect overlay_damage_rect =
      overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_EQ(output_rect, overlay_damage_rect);
}

TEST_F(FullscreenOverlayTest, FailOnOutputColorMatrix) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));

  // This is passing a non-identity color matrix which will result in disabling
  // overlays since color matrices are not supported yet.
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetNonIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the 2 quads are not gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, AlphaFail) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateTransparentCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      pass->output_rect);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  // Check that all the quads are gone.
  EXPECT_EQ(1U, main_pass->quad_list.size());
  // Check that we have only one overlay.
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(FullscreenOverlayTest, SuccessfulResourceSizeInPixels) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* original_quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  original_quad->set_resource_size_in_pixels(gfx::Size(64, 64));

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that the quad is gone.
  EXPECT_EQ(0U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, OnTopFail) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  // Add something in front of it.
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the 2 quads are not gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, NotCoveringFullscreenFail) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  gfx::Rect inset_rect = pass->output_rect;
  inset_rect.Inset(0, 1, 0, 1);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        inset_rect);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the quad is not gone.
  EXPECT_EQ(1U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, RemoveFullscreenQuadFromQuadList) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  // Add something in front of it that is fully transparent.
  pass->shared_quad_state_list.back()->opacity = 0.0f;
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that the fullscreen quad is gone.
  for (const DrawQuad* quad : main_pass->quad_list) {
    EXPECT_NE(main_pass->output_rect, quad->rect);
  }
}

TEST_F(SingleOverlayOnTopTest, SuccessfulOverlay) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* original_quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  unsigned original_resource_id = original_quad->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that the quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  const auto& quad_list = main_pass->quad_list;
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    EXPECT_NE(DrawQuad::Material::kTextureContent, it->material);
  }

  // Check that the right resource id got extracted.
  EXPECT_EQ(original_resource_id, candidate_list.back().resource_id);
}

TEST_F(SingleOverlayOnTopTest, PrioritizeBiggerOne) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  // Add a small quad.
  const auto kSmallCandidateRect = gfx::Rect(0, 0, 16, 16);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kSmallCandidateRect);
  AddExpectedRectToOverlayValidator(gfx::RectF(kSmallCandidateRect));

  // Add a bigger quad below the previous one, but not occluded.
  const auto kBigCandidateRect = gfx::Rect(20, 20, 32, 32);
  TextureDrawQuad* quad_big = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kBigCandidateRect);
  AddExpectedRectToOverlayValidator(gfx::RectF(kBigCandidateRect));

  unsigned resource_big = quad_big->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that one quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  // Check that we have only one overlay.
  EXPECT_EQ(1U, candidate_list.size());
  // Check that the right resource id (bigger quad) got extracted.
  EXPECT_EQ(resource_big, candidate_list.front().resource_id);
}

TEST_F(SingleOverlayOnTopTest, DamageRect) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  damage_rect_ = kOverlayRect;

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;

  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(SingleOverlayOnTopTest, NoCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

TEST_F(SingleOverlayOnTopTest, OccludedCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  RenderPassList original_pass_list;
  RenderPass::CopyAll(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

// Test with multiple render passes.
TEST_F(SingleOverlayOnTopTest, MultipleRenderPasses) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AcceptBlending) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  damage_rect_ = quad->rect;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
  EXPECT_FALSE(damage_rect_.IsEmpty());
  gfx::Rect overlay_damage_rect =
      overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_FALSE(overlay_damage_rect.IsEmpty());
}

TEST_F(SingleOverlayOnTopTest, RejectBackgroundColor) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SK_ColorRED;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AcceptBlackBackgroundColor) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SK_ColorBLACK;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlackBackgroundColorWithBlending) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SK_ColorBLACK;
  quad->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlendMode) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->blend_mode = SkBlendMode::kScreen;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpacity) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->opacity = 0.5f;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectNonAxisAlignedTransform) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowClipped) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, AllowVerticalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  rect.Offset(0, -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      -1.0f);
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL,
            candidate_list.back().transform);
}

TEST_F(UnderlayTest, AllowHorizontalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_height(rect.height() / 2);
  rect.Offset(-rect.width(), 0);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(-1.0f,
                                                                      2.0f);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
            candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowPositiveScaleTransform) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      1.0f);
  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AcceptMirrorYTransform) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(0, -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(1.f,
                                                                      -1.f);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, Allow90DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(0, -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(90.f);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_90, candidate_list.back().transform);
}

TEST_F(UnderlayTest, Allow180DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), -rect.height());
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(180.f);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_180, candidate_list.back().transform);
}

TEST_F(UnderlayTest, Allow270DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), 0);
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(270.f);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_270, candidate_list.back().transform);
}

TEST_F(UnderlayTest, AllowsOpaqueCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, DisallowsTransparentCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());
}

TEST_F(UnderlayTest, DisallowFilteredQuadOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  int render_pass_id = 3;
  RenderPassDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(pass->shared_quad_state_list.back(), kOverlayRect, kOverlayRect,
               render_pass_id, 0, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateBlurFilter(10.f));

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;

  render_pass_backdrop_filters[render_pass_id] = &filters;

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());
}

TEST_F(TransparentUnderlayTest, AllowsOpaqueCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(TransparentUnderlayTest, AllowsTransparentCandidates) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowNotTopIfNotOccluded) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentOnTop) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 0.f;
  CreateSolidColorQuadAt(shared_state, SK_ColorBLACK, pass.get(),
                         kOverlayBottomRightRect);
  shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        shared_state, pass.get(), kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentColorOnTop) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                         SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpaqueColorOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 0.5f;
  CreateSolidColorQuadAt(shared_state, SK_ColorBLACK, pass.get(),
                         kOverlayBottomRightRect);
  shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        shared_state, pass.get(), kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectTransparentColorOnTopWithoutBlending) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  CreateSolidColorQuadAt(shared_state, SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect)
      ->needs_blending = false;
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        shared_state, pass.get(), kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, DoNotPromoteIfContentsDontChange) {
  // Resource ID for the repeated quads. Value should be equivalent to
  // OverlayStrategySingleOnTop::kMaxFrameCandidateWithSameResourceId.
  constexpr size_t kFramesSkippedBeforeNotPromoting = 3;
  ResourceId previous_resource_id;

  for (size_t i = 0; i < 3 + kFramesSkippedBeforeNotPromoting; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    RenderPass* main_pass = pass.get();

    ResourceId resource_id;
    if (i == 0 || i == 1) {
      // Create a unique resource only for the first 2 frames.
      resource_id = CreateResource(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->output_rect.size(),
          true /*is_overlay_candidate*/);
      previous_resource_id = resource_id;
    } else {
      // Starting the 3rd frame, they should have the same resource ID.
      resource_id = previous_resource_id;
    }

    // Create a quad with the resource ID selected above.
    TextureDrawQuad* original_quad =
        main_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    original_quad->SetNew(
        pass->shared_quad_state_list.back(), pass->output_rect,
        pass->output_rect, false /*needs_blending*/, resource_id,
        false /*premultiplied_alpha*/, kUVTopLeft, kUVBottomRight,
        SK_ColorTRANSPARENT, vertex_opacity, false /*flipped*/,
        false /*nearest_neighbor*/, false /*secure_output_only*/,
        gfx::ProtectedVideoType::kClear);
    original_quad->set_resource_size_in_pixels(pass->output_rect.size());

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), main_pass);

    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);

    if (i <= kFramesSkippedBeforeNotPromoting) {
      EXPECT_EQ(1U, candidate_list.size());
      // Check that the right resource id got extracted.
      EXPECT_EQ(resource_id, candidate_list.back().resource_id);
      // Check that the quad is gone.
      EXPECT_EQ(1U, main_pass->quad_list.size());
    } else {
      // Check nothing has been promoted.
      EXPECT_EQ(2U, main_pass->quad_list.size());
    }
  }
}

TEST_F(UnderlayTest, OverlayLayerUnderMainLayer) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(-1, candidate_list[0].plane_z_order);
  EXPECT_EQ(2U, main_pass->quad_list.size());
  // The overlay quad should have changed to a SOLID_COLOR quad.
  EXPECT_EQ(main_pass->quad_list.back()->material,
            DrawQuad::Material::kSolidColor);
  auto* quad = static_cast<SolidColorDrawQuad*>(main_pass->quad_list.back());
  EXPECT_EQ(quad->rect, quad->visible_rect);
  EXPECT_EQ(false, quad->needs_blending);
  EXPECT_EQ(SK_ColorTRANSPARENT, quad->color);
}

TEST_F(UnderlayTest, AllowOnTop) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->CreateAndAppendSharedQuadState()->opacity = 0.5f;
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  RenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(-1, candidate_list[0].plane_z_order);
  // The overlay quad should have changed to a SOLID_COLOR quad.
  EXPECT_EQ(main_pass->quad_list.front()->material,
            DrawQuad::Material::kSolidColor);
  auto* quad = static_cast<SolidColorDrawQuad*>(main_pass->quad_list.front());
  EXPECT_EQ(quad->rect, quad->visible_rect);
  EXPECT_EQ(false, quad->needs_blending);
  EXPECT_EQ(SK_ColorTRANSPARENT, quad->color);
}

// The first time an underlay is scheduled its damage must not be subtracted.
TEST_F(UnderlayTest, InitialUnderlayDamageNotSubtracted) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  damage_rect_ = kOverlayRect;

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

// An identical underlay for two frames in a row means the damage can be
// subtracted the second time.
TEST_F(UnderlayTest, DamageSubtractedForConsecutiveIdenticalUnderlays) {
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    damage_rect_ = kOverlayRect;

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);
  }

  // The second time the same overlay rect is scheduled it will be subtracted
  // from the damage rect.
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

// Underlay damage can only be subtracted if the previous frame's underlay
// was the same rect.
TEST_F(UnderlayTest, DamageNotSubtractedForNonIdenticalConsecutiveUnderlays) {
  gfx::Rect overlay_rects[] = {kOverlayBottomRightRect, kOverlayRect};
  for (int i = 0; i < 2; ++i) {
    AddExpectedRectToOverlayValidator(gfx::RectF(overlay_rects[i]));

    std::unique_ptr<RenderPass> pass = CreateRenderPass();

    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          overlay_rects[i]);

    damage_rect_ = overlay_rects[i];

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);

    EXPECT_EQ(overlay_rects[i], damage_rect_);
  }
}

// Underlay damage can only be subtracted if the previous frame's underlay
// exists.
TEST_F(UnderlayTest, DamageNotSubtractedForNonConsecutiveIdenticalUnderlays) {
  bool has_fullscreen_candidate[] = {true, false, true};

  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();

    if (has_fullscreen_candidate[i]) {
      CreateFullscreenCandidateQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.back(),
          pass.get());
    }

    damage_rect_ = kOverlayRect;

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);
  }

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

// An identical overlay that is occluded should not have damage subtracted until
// it has been unoccluded for more than one frame.
TEST_F(UnderlayTest, DamageSubtractedForOneFrameAfterBecomingUnoccluded) {
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    if (i == 0) {
      // Add an overlapping quad above the candidate for the first frame.
      CreateFullscreenOpaqueQuad(resource_provider_.get(),
                                 pass->shared_quad_state_list.back(),
                                 pass.get());
    }
    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    damage_rect_ = kOverlayRect;

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);

    // The damage rect should not be subtracted if the underlay is occluded
    // (i==0) or it is unoccluded for the first time (i==1).
    if (i < 2)
      EXPECT_FALSE(damage_rect_.IsEmpty());
  }
  // The second time the same overlay rect is scheduled it should be subtracted
  // from the damage rect.
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(UnderlayTest, DamageNotSubtractedWhenQuadsAboveOverlap) {
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    // Add an overlapping quad above the candidate.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());
    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    damage_rect_ = kOverlayRect;

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);
  }

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

TEST_F(UnderlayTest, DamageSubtractedWhenQuadsAboveDontOverlap) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();
    // Add a non-overlapping quad above the candidate.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       pass->shared_quad_state_list.back(), pass.get(),
                       kOverlayTopLeftRect);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          kOverlayBottomRightRect);

    damage_rect_ = kOverlayBottomRightRect;

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);
  }

  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(UnderlayTest, PrimaryPlaneOverlayIsTransparentWithUnderlay) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     output_rect, SK_ColorWHITE);

  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayRect);

  OverlayCandidateList candidate_list;

  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDefaultBufferFormat, gfx::ColorSpace(),
      false /* has_alpha */);
  OverlayProcessor::OutputSurfaceOverlayPlane* primary_plane =
      &output_surface_plane;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, primary_plane,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, candidate_list.size());
  ASSERT_EQ(true, output_surface_plane.enable_blending);
}

TEST_F(UnderlayTest, UpdateDamageWhenChangingUnderlays) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayTopLeftRect));

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();

    if (i == 0) {
      CreateCandidateQuadAt(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.back(),
          pass.get(), kOverlayRect);
    }

    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          kOverlayTopLeftRect);

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);
  }

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

TEST_F(UnderlayTest, UpdateDamageRectWhenNoPromotion) {
  // In the first pass there is an overlay promotion and the expected damage
  // size should be unchanged.
  // In the second pass there is no overlay promotion, but the damage should be
  // the union of the damage_rect with CreateRenderPass's output_rect which is
  // {0, 0, 256, 256}.
  bool has_fullscreen_candidate[] = {true, false};
  gfx::Rect damages[] = {gfx::Rect(0, 0, 32, 32), gfx::Rect(0, 0, 312, 16)};
  gfx::Rect expected_damages[] = {gfx::Rect(0, 0, 32, 32),
                                  gfx::Rect(0, 0, 312, 256)};
  size_t expected_candidate_size[] = {1, 0};

  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();

    if (has_fullscreen_candidate[i]) {
      CreateFullscreenCandidateQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.back(),
          pass.get());
    }

    gfx::Rect damage_rect{damages[i]};

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_background_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_background_filters, nullptr,
        &candidate_list, &damage_rect, &content_bounds_);

    EXPECT_EQ(expected_damages[i], damage_rect);
    ASSERT_EQ(expected_candidate_size[i], candidate_list.size());
  }
}

// Tests that no damage occurs when the quad shared state has no occluding
// damage.
TEST_F(UnderlayTest, CandidateNoDamageWhenQuadSharedStateNoOccludingDamage) {
  for (int i = 0; i < 4; ++i) {
    std::unique_ptr<RenderPass> pass = CreateRenderPass();

    gfx::Rect rect(2, 3);
    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    if (i == 2) {
      auto* sqs = pass->shared_quad_state_list.front();
      sqs->occluding_damage_rect = gfx::Rect();
    } else if (i == 3) {
      auto* sqs = pass->shared_quad_state_list.front();
      sqs->occluding_damage_rect = gfx::Rect(1, 1, 10, 10);
    }

    CreateSolidColorQuadAt(default_damaged_shared_quad_state, SK_ColorBLACK,
                           pass.get(), rect);

    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.front(),
        pass.get());

    damage_rect_ = kOverlayRect;

    OverlayCandidateList candidate_list;
    OverlayProcessor::FilterOperationsMap render_pass_filters;
    OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
    RenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters, nullptr,
        &candidate_list, &damage_rect_, &content_bounds_);

    if (i == 0 || i == 1 || i == 3)
      EXPECT_FALSE(damage_rect_.IsEmpty());
    else if (i == 2)
      EXPECT_TRUE(damage_rect_.IsEmpty());
  }
}

TEST_F(UnderlayCastTest, NoOverlayContentBounds) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, content_bounds_.size());
}

TEST_F(UnderlayCastTest, FullScreenOverlayContentBounds) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_TRUE(content_bounds_[0].IsEmpty());
}

TEST_F(UnderlayCastTest, BlackOutsideOverlayContentBounds) {
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  const gfx::Rect kLeftSide(0, 0, 128, 256);
  const gfx::Rect kTopRight(128, 0, 128, 128);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayBottomRightRect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(), kLeftSide,
                     SK_ColorBLACK);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(), kTopRight,
                     SK_ColorBLACK);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_TRUE(content_bounds_[0].IsEmpty());
}

TEST_F(UnderlayCastTest, OverlayOccludedContentBounds) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(kOverlayTopLeftRect, content_bounds_[0]);
}

TEST_F(UnderlayCastTest, OverlayOccludedUnionContentBounds) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayBottomRightRect);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(kOverlayRect, content_bounds_[0]);
}

TEST_F(UnderlayCastTest, RoundOverlayContentBounds) {
  // Check rounding behaviour on overlay quads.  Be conservative (content
  // potentially visible on boundary).
  const gfx::Rect overlay_rect(1, 1, 8, 8);
  AddExpectedRectToOverlayValidator(gfx::RectF(1.5f, 1.5f, 8, 8));

  gfx::Transform transform;
  transform.Translate(0.5f, 0.5f);

  std::unique_ptr<RenderPass> pass = CreateRenderPassWithTransform(transform);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            overlay_rect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     gfx::Rect(0, 0, 10, 10), SK_ColorWHITE);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(gfx::Rect(0, 0, 11, 11), content_bounds_[0]);
}

TEST_F(UnderlayCastTest, RoundContentBounds) {
  // Check rounding behaviour on content quads (bounds should be enclosing
  // rect).
  gfx::Rect overlay_rect = kOverlayRect;
  overlay_rect.Inset(0, 0, 1, 1);
  AddExpectedRectToOverlayValidator(gfx::RectF(0.5f, 0.5f, 255, 255));

  gfx::Transform transform;
  transform.Translate(0.5f, 0.5f);

  std::unique_ptr<RenderPass> pass = CreateRenderPassWithTransform(transform);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            overlay_rect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     gfx::Rect(0, 0, 255, 255), SK_ColorWHITE);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(kOverlayRect, content_bounds_[0]);
}

TEST_F(UnderlayCastTest, NoOverlayPromotionWithoutProtectedContent) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, nullptr,
      &candidate_list, &damage_rect_, &content_bounds_);

  ASSERT_TRUE(candidate_list.empty());
  EXPECT_TRUE(content_bounds_.empty());
}
#endif

#if defined(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
TEST_F(UnderlayCastTest, PrimaryPlaneOverlayIsAlwaysTransparent) {
  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     output_rect, SK_ColorWHITE);

  OverlayCandidateList candidate_list;

  OverlayProcessor::FilterOperationsMap render_pass_filters;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDefaultBufferFormat, gfx::ColorSpace());

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters, &output_surface_plane,
      &candidate_list, &damage_rect_, &content_bounds_);

  ASSERT_EQ(true, output_surface_plane.enable_blending);
  EXPECT_EQ(0U, content_bounds_.size());
}
#endif

#if defined(USE_OZONE) || defined(OS_ANDROID) || defined(OS_MACOSX)
class OverlayInfoRendererGL : public GLRenderer {
  using OverlayProcessorType = TypedOverlayProcessor<SingleOverlayValidator>;

 public:
  OverlayInfoRendererGL(const RendererSettings* settings,
                        OutputSurface* output_surface,
                        DisplayResourceProvider* resource_provider,
                        bool use_validator)
      : GLRenderer(settings, output_surface, resource_provider, nullptr),
        expect_overlays_(false) {
  }

  MOCK_METHOD2(DoDrawQuad,
               void(const DrawQuad* quad, const gfx::QuadF* draw_region));

  void SetCurrentFrame(const DrawingFrame& frame) {
    SetCurrentFrameForTesting(frame);
  }

  using GLRenderer::BeginDrawingFrame;

  void FinishDrawingFrame() override {
    GLRenderer::FinishDrawingFrame();

    if (!expect_overlays_) {
      EXPECT_EQ(0U, current_frame()->overlay_list.size());
      return;
    }

    ASSERT_TRUE(current_frame()->output_surface_plane.has_value());
    ASSERT_EQ(1U, current_frame()->overlay_list.size());
    EXPECT_GE(current_frame()->overlay_list.back().resource_id, 0U);
  }

  void AddExpectedRectToOverlayValidator(const gfx::RectF& rect) {
    DCHECK(overlay_processor_);
    static_cast<OverlayProcessorType*>(overlay_processor_.get())
        ->GetTypedOverlayCandidateValidator()
        ->AddExpectedRect(rect);
  }

  void set_expect_overlays(bool expect_overlays) {
    expect_overlays_ = expect_overlays;
  }

  void SetOverlayProcessorWithValidator(
      std::unique_ptr<SingleOverlayValidator> validator) {
    overlay_processor_.reset(new OverlayProcessorType(std::move(validator)));
  }

 private:
  bool expect_overlays_;
};

class MockOverlayScheduler {
 public:
  MOCK_METHOD7(Schedule,
               void(int plane_z_order,
                    gfx::OverlayTransform plane_transform,
                    unsigned overlay_texture_id,
                    const gfx::Rect& display_bounds,
                    const gfx::RectF& uv_rect,
                    bool enable_blend,
                    unsigned gpu_fence_id));
};

class GLRendererWithOverlaysTest : public testing::Test {
  using OverlayProcessorType = TypedOverlayProcessor<SingleOverlayValidator>;

 protected:
  GLRendererWithOverlaysTest() {
    provider_ = TestContextProvider::Create();
    provider_->BindToCurrentThread();
    output_surface_ = std::make_unique<OverlayOutputSurface>(provider_);
    output_surface_->BindToClient(&output_surface_client_);
    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        DisplayResourceProvider::kGpu, provider_.get(), nullptr);

    provider_->support()->SetScheduleOverlayPlaneCallback(base::BindRepeating(
        &MockOverlayScheduler::Schedule, base::Unretained(&scheduler_)));

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentThread();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>(true);
  }

  ~GLRendererWithOverlaysTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  void Init(bool use_validator) {
    renderer_ = std::make_unique<OverlayInfoRendererGL>(
        &settings_, output_surface_.get(), resource_provider_.get(),
        use_validator);
    renderer_->Initialize();
    renderer_->SetVisible(true);
    if (use_validator) {
      renderer_->SetOverlayProcessorWithValidator(
          std::make_unique<SingleOverlayValidator>());
    }
  }

  void DrawFrame(RenderPassList* pass_list, const gfx::Size& viewport_size) {
    renderer_->DrawFrame(pass_list, 1.f, viewport_size);
  }
  void SwapBuffers() {
    renderer_->SwapBuffers(std::vector<ui::LatencyInfo>());
    renderer_->SwapBuffersComplete();
  }
  void SwapBuffersWithoutComplete() {
    renderer_->SwapBuffers(std::vector<ui::LatencyInfo>());
  }
  void SwapBuffersComplete() { renderer_->SwapBuffersComplete(); }
  void ReturnResourceInUseQuery(ResourceId id) {
    DisplayResourceProvider::ScopedReadLockGL lock(resource_provider_.get(),
                                                   id);
    gpu::TextureInUseResponse response;
    response.texture = lock.texture_id();
    response.in_use = false;
    gpu::TextureInUseResponses responses;
    responses.push_back(response);
    renderer_->DidReceiveTextureInUseResponses(responses);
  }

  void AddExpectedRectToOverlayValidator(const gfx::RectF& rect) {
    renderer_->AddExpectedRectToOverlayValidator(rect);
  }

  RendererSettings settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<OverlayInfoRendererGL> renderer_;
  scoped_refptr<TestContextProvider> provider_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  MockOverlayScheduler scheduler_;
};

TEST_F(GLRendererWithOverlaysTest, OverlayQuadNotDrawn) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);
  AddExpectedRectToOverlayValidator(gfx::RectF(kOverlayBottomRightRect));

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Candidate pass was taken out and extra skipped pass added,
  // so only draw 2 quads.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(2);
  EXPECT_CALL(scheduler_,
              Schedule(0, gfx::OVERLAY_TRANSFORM_NONE, _,
                       gfx::Rect(kDisplaySize), gfx::RectF(0, 0, 1, 1), _, _))
      .Times(1);
  EXPECT_CALL(
      scheduler_,
      Schedule(1, gfx::OVERLAY_TRANSFORM_NONE, _, kOverlayBottomRightRect,
               BoundingRect(kUVTopLeft, kUVBottomRight), _, _))
      .Times(1);
  DrawFrame(&pass_list, kDisplaySize);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, OccludedQuadInUnderlay) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Candidate quad should fail to be overlaid on top because of occlusion.
  // Expect to be replaced with transparent hole quad and placed in underlay.
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(3);
  EXPECT_CALL(scheduler_,
              Schedule(0, gfx::OVERLAY_TRANSFORM_NONE, _,
                       gfx::Rect(kDisplaySize), gfx::RectF(0, 0, 1, 1), _, _))
      .Times(1);
  EXPECT_CALL(scheduler_,
              Schedule(-1, gfx::OVERLAY_TRANSFORM_NONE, _, kOverlayRect,
                       BoundingRect(kUVTopLeft, kUVBottomRight), _, _))
      .Times(1);
  DrawFrame(&pass_list, kDisplaySize);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());

  SwapBuffers();

  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, NoValidatorNoOverlay) {
  bool use_validator = false;
  Init(use_validator);
  renderer_->set_expect_overlays(false);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Should not see the primary surface's overlay.
  output_surface_->set_is_displayed_as_overlay_plane(false);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(3);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(0);
  DrawFrame(&pass_list, kDisplaySize);
  EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

// GLRenderer skips drawing occluded quads when partial swap is enabled.
TEST_F(GLRendererWithOverlaysTest, OccludedQuadNotDrawnWhenPartialSwapEnabled) {
  provider_->TestContextGL()->set_have_post_sub_buffer(true);
  settings_.partial_swap_enabled = true;
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  output_surface_->set_is_displayed_as_overlay_plane(true);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(0);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  DrawFrame(&pass_list, kDisplaySize);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

// GLRenderer skips drawing occluded quads when empty swap is enabled.
TEST_F(GLRendererWithOverlaysTest, OccludedQuadNotDrawnWhenEmptySwapAllowed) {
  provider_->TestContextGL()->set_have_commit_overlay_planes(true);
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  std::unique_ptr<RenderPass> pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  output_surface_->set_is_displayed_as_overlay_plane(true);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(0);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  DrawFrame(&pass_list, kDisplaySize);
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturnedWithDelay) {
  bool use_validator = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource2 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource3 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);

  // Return the resource map.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource1, resource2, resource3}, resource_provider_.get(),
          child_resource_provider_.get(), child_provider_.get());

  ResourceId mapped_resource1 = resource_map[resource1];
  ResourceId mapped_resource2 = resource_map[resource2];
  ResourceId mapped_resource3 = resource_map[resource3];

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(1);
  frame1.output_surface_plane = OverlayProcessor::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = mapped_resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(1);
  frame2.output_surface_plane = OverlayProcessor::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = mapped_resource2;
  overlay2.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  frame3.overlay_list.resize(1);
  frame3.output_surface_plane = OverlayProcessor::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay3 = frame3.overlay_list.back();
  overlay3.resource_id = mapped_resource3;
  overlay3.plane_z_order = 1;

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame1);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();

  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(resource1));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(resource2));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(resource3));

  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  Mock::VerifyAndClearExpectations(&scheduler_);

  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));

  SwapBuffersComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame2);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  Mock::VerifyAndClearExpectations(&scheduler_);

  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame3);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  // No overlays, release the resource.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(0);
  DirectRenderer::DrawingFrame frame_no_overlays;
  frame_no_overlays.render_passes_in_draw_order = &pass_list;
  renderer_->set_expect_overlays(false);
  renderer_->SetCurrentFrame(frame_no_overlays);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));

  // Use the same buffer twice.
  renderer_->set_expect_overlays(true);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame1);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  SwapBuffersComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame1);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  SwapBuffersComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));

  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(0);
  renderer_->set_expect_overlays(false);
  renderer_->SetCurrentFrame(frame_no_overlays);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturnedAfterGpuQuery) {
  bool use_validator = true;
  settings_.release_overlay_resources_after_gpu_query = true;
  Init(use_validator);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource2 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource3 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);

  // Return the resource map.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource1, resource2, resource3}, resource_provider_.get(),
          child_resource_provider_.get(), child_provider_.get());
  ResourceId mapped_resource1 = resource_map[resource1];
  ResourceId mapped_resource2 = resource_map[resource2];
  ResourceId mapped_resource3 = resource_map[resource3];

  std::unique_ptr<RenderPass> pass = CreateRenderPass();
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(1);
  frame1.output_surface_plane = OverlayProcessor::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = mapped_resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(1);
  frame2.output_surface_plane = OverlayProcessor::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = mapped_resource2;
  overlay2.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  frame3.overlay_list.resize(1);
  frame3.output_surface_plane = OverlayProcessor::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay3 = frame3.overlay_list.back();
  overlay3.resource_id = mapped_resource3;
  overlay3.plane_z_order = 1;

  // First frame, with no swap completion.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame1);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Second frame, with no swap completion.
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame2);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // Third frame, still with no swap completion (where the resources would
  // otherwise have been released).
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  renderer_->SetCurrentFrame(frame3);
  renderer_->BeginDrawingFrame();
  renderer_->FinishDrawingFrame();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  SwapBuffersWithoutComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));
  Mock::VerifyAndClearExpectations(&scheduler_);

  // This completion corresponds to the first frame.
  SwapBuffersComplete();
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));

  // This completion corresponds to the second frame. The first resource is no
  // longer in use.
  ReturnResourceInUseQuery(mapped_resource1);
  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));

  // This completion corresponds to the third frame.
  SwapBuffersComplete();
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource2));
  EXPECT_TRUE(resource_provider_->InUse(mapped_resource3));

  ReturnResourceInUseQuery(mapped_resource2);
  ReturnResourceInUseQuery(mapped_resource3);
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource1));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource2));
  EXPECT_FALSE(resource_provider_->InUse(mapped_resource3));
}
#endif

#if defined(OS_MACOSX)
class CALayerOverlayRPDQTest : public CALayerOverlayTest {
 protected:
  void SetUp() override {
    CALayerOverlayTest::SetUp();
    pass_list_.push_back(CreateRenderPass());
    pass_ = pass_list_.back().get();
    quad_ = pass_->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
    render_pass_id_ = 3;
  }

  void ProcessForOverlays() {
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list_, GetIdentityColorMatrix(),
        render_pass_filters_, render_pass_backdrop_filters_, nullity,
        &ca_layer_list_, &damage_rect_, &content_bounds_);
  }
  RenderPassList pass_list_;
  RenderPass* pass_;
  RenderPassDrawQuad* quad_;
  int render_pass_id_;
  cc::FilterOperations filters_;
  cc::FilterOperations backdrop_filters_;
  OverlayProcessor::FilterOperationsMap render_pass_filters_;
  OverlayProcessor::FilterOperationsMap render_pass_backdrop_filters_;
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

  for (int i = 0; i < count; ++i) {
    auto* quad = pass_->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
    quad->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                 kOverlayRect, render_pass_id_, 2, gfx::RectF(), gfx::Size(),
                 gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false,
                 1.0f);
  }

  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}
#endif

}  // namespace
}  // namespace viz
