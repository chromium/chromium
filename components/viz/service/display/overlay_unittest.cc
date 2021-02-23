// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/stl_util.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider_gl.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/latency/latency_info.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#endif

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

class TestOverlayProcessor : public OverlayProcessorUsingStrategy {
 public:
  using PrimaryPlane = OverlayProcessorInterface::OutputSurfaceOverlayPlane;
  TestOverlayProcessor() {
    // By default the prioritization thresholding features are disabled for all
    // tests.
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
  }
  ~TestOverlayProcessor() override = default;

  bool IsOverlaySupported() const override { return true; }
  bool NeedsSurfaceDamageRectList() const override { return false; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {}
  size_t GetStrategyCount() const { return strategies_.size(); }
};

class FullscreenOverlayProcessor : public TestOverlayProcessor {
 public:
  FullscreenOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
  }
  bool NeedsSurfaceDamageRectList() const override { return true; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {
    surfaces->back().overlay_handled = true;
  }
};

// Gets the minimum scaling amount used by either dimension for the src relative
// to the dst.
float GetMinScaleFactor(const OverlayCandidate& candidate) {
  if (candidate.resource_size_in_pixels.IsEmpty() ||
      candidate.uv_rect.IsEmpty()) {
    return 1.0f;
  }
  return std::min(candidate.display_rect.width() /
                      (candidate.uv_rect.width() *
                       candidate.resource_size_in_pixels.width()),
                  candidate.display_rect.height() /
                      (candidate.uv_rect.height() *
                       candidate.resource_size_in_pixels.height()));
}

class DefaultOverlayProcessor : public TestOverlayProcessor {
 public:
  DefaultOverlayProcessor() : expected_rects_(1, gfx::RectF(kOverlayRect)) {}

  bool NeedsSurfaceDamageRectList() const override { return true; }
  void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                           OverlayCandidateList* surfaces) override {
    // We have one overlay surface to test. The output surface as primary plane
    // is optional, depending on whether this ran
    // through the full renderer and picked up the output surface, or not.
    ASSERT_EQ(1U, surfaces->size());
    OverlayCandidate& candidate = surfaces->back();
    const float kAbsoluteError = 0.01f;
    // If we are testing fallback for protected overlay scaling, then check that
    // first.
    if (!scaling_sequence_.empty()) {
      float scaling = GetMinScaleFactor(candidate);
      EXPECT_LE(std::abs(scaling - scaling_sequence_.front().first),
                kAbsoluteError);
      candidate.overlay_handled = scaling_sequence_.front().second;
      scaling_sequence_.erase(scaling_sequence_.begin());
      return;
    }

    for (const auto& r : expected_rects_) {
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

  void AddScalingSequence(float scaling, bool uses_overlay) {
    scaling_sequence_.emplace_back(scaling, uses_overlay);
  }

 private:
  std::vector<gfx::RectF> expected_rects_;
  std::vector<std::pair<float, bool>> scaling_sequence_;
};

class SingleOverlayProcessor : public DefaultOverlayProcessor {
 public:
  SingleOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
  }
};

class SingleOnTopOverlayProcessor : public DefaultOverlayProcessor {
 public:
  SingleOnTopOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    // To reduce the complexity of this test for the prioritization feature the
    // |damage_rate_threshold| is set to zero to make all opaque power positive.
    tracker_config_.damage_rate_threshold = 0.f;
  }
};

class UnderlayOverlayProcessor : public DefaultOverlayProcessor {
 public:
  UnderlayOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
    prioritization_config_.power_gain_sort = false;
  }
};

class TransitionOverlayProcessor : public DefaultOverlayProcessor {
 public:
  TransitionOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
    prioritization_config_.power_gain_sort = false;
  }
};

class TransparentUnderlayOverlayProcessor : public DefaultOverlayProcessor {
 public:
  TransparentUnderlayOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
        this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  }
};

class UnderlayCastOverlayProcessor : public DefaultOverlayProcessor {
 public:
  UnderlayCastOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlayCast>(this));
  }
};

class ChangeThresholdOnTopOverlayProcessor : public DefaultOverlayProcessor {
 public:
  ChangeThresholdOnTopOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    prioritization_config_.damage_rate_threshold = false;
    prioritization_config_.changing_threshold = true;
  }

  // To keep this test consistent we need to expose the config for how long it
  // takes for the system to threshold a unchanging candidate.
  const OverlayCandidateTemporalTracker::Config& TrackerConfigAccessor() {
    return tracker_config_;
  }
};

class FullThresholdUnderlayOverlayProcessor : public DefaultOverlayProcessor {
 public:
  FullThresholdUnderlayOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
    // Disable this feature as it is tested in
    // |ChangeThresholdOnTopOverlayProcessor|.
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = true;
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
  bool IsDisplayedAsOverlayPlane() const override {
    return is_displayed_as_overlay_plane_;
  }
  unsigned GetOverlayTextureId() const override { return 10000; }
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
  void clear_bind_framebuffer_count() { bind_framebuffer_count_ = 0; }

 private:
  bool is_displayed_as_overlay_plane_;
  unsigned bind_framebuffer_count_ = 0;
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

std::unique_ptr<AggregatedRenderPass> CreateRenderPassWithTransform(
    const gfx::Transform& transform) {
  AggregatedRenderPassId render_pass_id{1};
  gfx::Rect output_rect(0, 0, 256, 256);

  auto pass = std::make_unique<AggregatedRenderPass>();
  pass->SetNew(render_pass_id, output_rect, output_rect, gfx::Transform());

  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->opacity = 1.f;
  shared_state->quad_to_target_transform = transform;
  return pass;
}

static ResourceId CreateResourceInLayerTree(
    ClientResourceProvider* child_resource_provider,
    const gfx::Size& size,
    bool is_overlay_candidate,
    ResourceFormat resource_format) {
  auto resource = TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken(),
      size, is_overlay_candidate);
  resource.format = resource_format;
  auto release_callback = SingleReleaseCallback::Create(
      base::BindRepeating([](const gpu::SyncToken&, bool) {}));

  ResourceId resource_id = child_resource_provider->ImportResource(
      resource, std::move(release_callback));

  return resource_id;
}

static ResourceId CreateResourceInLayerTree(
    ClientResourceProvider* child_resource_provider,
    const gfx::Size& size,
    bool is_overlay_candidate) {
  return CreateResourceInLayerTree(child_resource_provider, size,
                                   is_overlay_candidate, RGBA_8888);
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          ContextProvider* child_context_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate,
                          ResourceFormat resource_format) {
  ResourceId resource_id = CreateResourceInLayerTree(
      child_resource_provider, size, is_overlay_candidate, resource_format);

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
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      parent_resource_provider->GetChildToParentMap(child_id);
  return resource_map[list[0].id];
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          ContextProvider* child_context_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate) {
  return CreateResource(parent_resource_provider, child_resource_provider,
                        child_context_provider, size, is_overlay_candidate,
                        RGBA_8888);
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

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::ProtectedVideoType protected_video_type,
    ResourceFormat resource_format,
    const gfx::Size& resource_size_in_pixels) {
  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate, resource_format);

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
    const gfx::Rect& rect,
    gfx::ProtectedVideoType protected_video_type,
    ResourceFormat resource_format) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, rect, protected_video_type,
      resource_format, rect.size());
}

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect) {
  return CreateCandidateQuadAt(parent_resource_provider,
                               child_resource_provider, child_context_provider,
                               shared_quad_state, render_pass, rect,
                               gfx::ProtectedVideoType::kClear, RGBA_8888);
}

// For Cast we use VideoHoleDrawQuad, and that's what overlay_processor_
// expects.
VideoHoleDrawQuad* CreateVideoHoleDrawQuadAt(
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
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
    AggregatedRenderPass* render_pass,
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
    AggregatedRenderPass* render_pass) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, render_pass->output_rect);
}

void CreateOpaqueQuadAt(DisplayResourceProvider* resource_provider,
                        const SharedQuadState* shared_quad_state,
                        AggregatedRenderPass* render_pass,
                        const gfx::Rect& rect) {
  auto* color_quad = render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_quad_state, rect, rect, SK_ColorBLACK, false);
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

void CreateFullscreenOpaqueQuad(DisplayResourceProvider* resource_provider,
                                const SharedQuadState* shared_quad_state,
                                AggregatedRenderPass* render_pass) {
  CreateOpaqueQuadAt(resource_provider, shared_quad_state, render_pass,
                     render_pass->output_rect);
}

static void CompareRenderPassLists(
    const AggregatedRenderPassList& expected_list,
    const AggregatedRenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    AggregatedRenderPass* expected = expected_list[i].get();
    AggregatedRenderPass* actual = actual_list[i].get();

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
  return SkMatrix(matrix);
}

template <typename OverlayProcessorType>
class OverlayTest : public testing::Test {
 protected:
  void SetUp() override {
    provider_ = TestContextProvider::Create();
    provider_->BindToCurrentThread();
    output_surface_ = std::make_unique<OverlayOutputSurface>(provider_);
    output_surface_->BindToClient(&client_);

    resource_provider_ =
        std::make_unique<DisplayResourceProviderGL>(provider_.get());

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentThread();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();

    overlay_processor_ = std::make_unique<OverlayProcessorType>();
  }

  void TearDown() override {
    overlay_processor_ = nullptr;
    child_resource_provider_->ShutdownAndReleaseAllResources();
    child_resource_provider_ = nullptr;
    child_provider_ = nullptr;
    resource_provider_ = nullptr;
    output_surface_ = nullptr;
    provider_ = nullptr;
  }

  void AddExpectedRectToOverlayProcessor(const gfx::RectF& rect) {
    overlay_processor_->AddExpectedRect(rect);
  }

  void AddScalingSequenceToOverlayProcessor(float scaling, bool uses_overlay) {
    overlay_processor_->AddScalingSequence(scaling, uses_overlay);
  }

  scoped_refptr<TestContextProvider> provider_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient client_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<OverlayProcessorType> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

using FullscreenOverlayTest = OverlayTest<FullscreenOverlayProcessor>;
using SingleOverlayOnTopTest = OverlayTest<SingleOnTopOverlayProcessor>;
using ChangeSingleOnTopTest = OverlayTest<ChangeThresholdOnTopOverlayProcessor>;
using FullThresholdTest = OverlayTest<FullThresholdUnderlayOverlayProcessor>;
using TransitionOverlayTypeTest = OverlayTest<TransitionOverlayProcessor>;

using UnderlayTest = OverlayTest<UnderlayOverlayProcessor>;
using TransparentUnderlayTest =
    OverlayTest<TransparentUnderlayOverlayProcessor>;
using UnderlayCastTest = OverlayTest<UnderlayCastOverlayProcessor>;

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  scoped_refptr<TestContextProvider> provider = TestContextProvider::Create();
  provider->BindToCurrentThread();
  OverlayOutputSurface output_surface(provider);
  cc::FakeOutputSurfaceClient client;
  output_surface.BindToClient(&client);

  auto resource_provider =
      std::make_unique<DisplayResourceProviderGL>(provider.get());

  auto overlay_processor = std::make_unique<TestOverlayProcessor>();
  EXPECT_GE(2U, overlay_processor->GetStrategyCount());
}

#if !defined(OS_APPLE) && !defined(OS_WIN)
TEST_F(FullscreenOverlayTest, SuccessfulOverlay) {
  auto pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  TextureDrawQuad* original_quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  ResourceId original_resource_id = original_quad->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

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
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  // This is passing a non-identity color matrix which will result in disabling
  // overlays since color matrices are not supported yet.
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetNonIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the 2 quads are not gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, AlphaFail) {
  auto pass = CreateRenderPass();
  CreateTransparentCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      pass->output_rect);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  // Check that all the quads are gone.
  EXPECT_EQ(1U, main_pass->quad_list.size());
  // Check that we have only one overlay.
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(FullscreenOverlayTest, SuccessfulResourceSizeInPixels) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* original_quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  original_quad->set_resource_size_in_pixels(gfx::Size(64, 64));

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that the quad is gone.
  EXPECT_EQ(0U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, OnTopFail) {
  auto pass = CreateRenderPass();

  // Add something in front of it.
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the 2 quads are not gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, NotCoveringFullscreenFail) {
  auto pass = CreateRenderPass();
  gfx::Rect inset_rect = pass->output_rect;
  inset_rect.Inset(0, 1, 0, 1);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        inset_rect);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the quad is not gone.
  EXPECT_EQ(1U, main_pass->quad_list.size());
}

TEST_F(FullscreenOverlayTest, RemoveFullscreenQuadFromQuadList) {
  auto pass = CreateRenderPass();

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
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that the fullscreen quad is gone.
  for (const DrawQuad* quad : main_pass->quad_list) {
    EXPECT_NE(main_pass->output_rect, quad->rect);
  }
}

TEST_F(SingleOverlayOnTopTest, SuccessfulOverlay) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* original_quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  ResourceId original_resource_id = original_quad->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
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
  auto pass = CreateRenderPass();
  // Add a small quad.
  const auto kSmallCandidateRect = gfx::Rect(0, 0, 16, 16);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kSmallCandidateRect);
  AddExpectedRectToOverlayProcessor(gfx::RectF(kSmallCandidateRect));

  // Add a bigger quad below the previous one, but not occluded.
  const auto kBigCandidateRect = gfx::Rect(20, 20, 32, 32);
  TextureDrawQuad* quad_big = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kBigCandidateRect);
  AddExpectedRectToOverlayProcessor(gfx::RectF(kBigCandidateRect));

  ResourceId resource_big = quad_big->resource_id();

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->SetFrameSequenceNumber(1);
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that one quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  // Check that we have only one overlay.
  EXPECT_EQ(1U, candidate_list.size());
  // Check that the right resource id (bigger quad) got extracted.
  EXPECT_EQ(resource_big, candidate_list.front().resource_id);
}

TEST_F(SingleOverlayOnTopTest, OpaqueOverlayDamageSubtract) {
  // This tests a specific damage optimization where an opaque pure overlay can
  // subtract damage from the primary plane even if the overlay does not have a
  // |shared_quad_state| surface damage index.
  constexpr int kCandidateSmall = 64;
  const gfx::Rect kOverlayDisplayRect = {10, 10, kCandidateSmall,
                                         kCandidateSmall};
  const gfx::Rect kDamageRect[] = {
      gfx::Rect(10, 10, kCandidateSmall, kCandidateSmall),
      gfx::Rect(0, 10, kCandidateSmall, kCandidateSmall),
      gfx::Rect(6, 6, kCandidateSmall, kCandidateSmall)};

  const gfx::Rect kExpectedDamage[] = {
      gfx::Rect(), gfx::Rect(0, 10, 10, kCandidateSmall),
      gfx::Rect(6, 6, kCandidateSmall, kCandidateSmall)};

  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayDisplayRect));
  for (size_t i = 0; i < base::size(kDamageRect); ++i) {
    auto pass = CreateRenderPass();
    SharedQuadState* damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    damaged_shared_quad_state->no_damage = false;

    auto* quad = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), damaged_shared_quad_state, pass.get(),
        kOverlayDisplayRect);

    quad->needs_blending = false;
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    SurfaceDamageRectList surface_damage_rect_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;

    pass_list.push_back(std::move(pass));
    damage_rect_ = kDamageRect[i];

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    EXPECT_RECT_EQ(damage_rect_, kExpectedDamage[i]);
  }
}

TEST_F(SingleOverlayOnTopTest, DamageRect) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  damage_rect_ = kOverlayRect;

  SurfaceDamageRectList surface_damage_rect_list;

  SharedQuadState* default_damaged_shared_quad_state =
      pass->shared_quad_state_list.AllocateAndCopyFrom(
          pass->shared_quad_state_list.back());

  auto* sqs = pass->shared_quad_state_list.front();
  surface_damage_rect_list.emplace_back(damage_rect_);
  sqs->overlay_damage_index = 0;

  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             default_damaged_shared_quad_state, pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.front(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(SingleOverlayOnTopTest, NoCandidates) {
  auto pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  AggregatedRenderPassList original_pass_list;
  AggregatedRenderPass::CopyAllForTest(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

TEST_F(SingleOverlayOnTopTest, OccludedCandidates) {
  auto pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  AggregatedRenderPassList original_pass_list;
  AggregatedRenderPass::CopyAllForTest(pass_list, &original_pass_list);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
  // There should be nothing new here.
  CompareRenderPassLists(pass_list, original_pass_list);
}

// Test with multiple render passes.
TEST_F(SingleOverlayOnTopTest, MultipleRenderPasses) {
  auto pass = CreateRenderPass();
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
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AcceptBlending) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  damage_rect_ = quad->rect;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
  EXPECT_FALSE(damage_rect_.IsEmpty());
  gfx::Rect overlay_damage_rect =
      overlay_processor_->GetAndResetOverlayDamage();
  EXPECT_FALSE(overlay_damage_rect.IsEmpty());
}

TEST_F(SingleOverlayOnTopTest, RejectBackgroundColor) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SK_ColorRED;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AcceptBlackBackgroundColor) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SK_ColorBLACK;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlackBackgroundColorWithBlending) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SK_ColorBLACK;
  quad->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectBlendMode) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->blend_mode = SkBlendMode::kScreen;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpacity) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->opacity = 0.5f;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectNearestNeighbor) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  static_cast<TextureDrawQuad*>(pass->quad_list.back())->nearest_neighbor =
      true;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectNonAxisAlignedTransform) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutXAxis(45.f);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowClipped) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, AllowVerticalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  rect.Offset(0, -rect.height());
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      -1.0f);
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL,
            candidate_list.back().transform);
}

TEST_F(UnderlayTest, AllowHorizontalFlip) {
  gfx::Rect rect = kOverlayRect;
  rect.set_height(rect.height() / 2);
  rect.Offset(-rect.width(), 0);
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(-1.0f,
                                                                      2.0f);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
            candidate_list.back().transform);
}

TEST_F(SingleOverlayOnTopTest, AllowPositiveScaleTransform) {
  gfx::Rect rect = kOverlayRect;
  rect.set_width(rect.width() / 2);
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(2.0f,
                                                                      1.0f);
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AcceptMirrorYTransform) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(0, -rect.height());
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()->quad_to_target_transform.Scale(1.f,
                                                                      -1.f);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, Allow90DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(0, -rect.height());
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(90.f);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_90, candidate_list.back().transform);
}

TEST_F(UnderlayTest, Allow180DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), -rect.height());
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(180.f);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_180, candidate_list.back().transform);
}

TEST_F(UnderlayTest, Allow270DegreeRotation) {
  gfx::Rect rect = kOverlayRect;
  rect.Offset(-rect.width(), 0);
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(), rect);
  pass->shared_quad_state_list.back()
      ->quad_to_target_transform.RotateAboutZAxis(270.f);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_270, candidate_list.back().transform);
}

TEST_F(UnderlayTest, AllowsOpaqueCandidates) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(UnderlayTest, DisallowsTransparentCandidates) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());
}

TEST_F(UnderlayTest, DisallowFilteredQuadOnTop) {
  auto pass = CreateRenderPass();

  AggregatedRenderPassId render_pass_id{3};
  AggregatedRenderPassDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(pass->shared_quad_state_list.back(), kOverlayRect, kOverlayRect,
               render_pass_id, kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateBlurFilter(10.f));

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

  render_pass_backdrop_filters[render_pass_id] = &filters;

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(0U, candidate_list.size());
}

TEST_F(UnderlayTest, AllowFilteredQuadOnTopForProtectedVideo) {
  // This is only used in the overlay prioritization path.
  if (!features::IsOverlayPrioritizationEnabled())
    return;

  auto pass = CreateRenderPass();

  AggregatedRenderPassId render_pass_id{3};
  AggregatedRenderPassDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(pass->shared_quad_state_list.back(), kOverlayRect, kOverlayRect,
               render_pass_id, kInvalidResourceId, gfx::RectF(), gfx::Size(),
               gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(), false, 1.0f);

  CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      pass->output_rect, gfx::ProtectedVideoType::kHardwareProtected,
      YUV_420_BIPLANAR)
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateBlurFilter(10.f));

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

  render_pass_backdrop_filters[render_pass_id] = &filters;

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(TransparentUnderlayTest, AllowsOpaqueCandidates) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = false;
  pass->shared_quad_state_list.front()->opacity = 1.0;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(TransparentUnderlayTest, AllowsTransparentCandidates) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get())
      ->needs_blending = true;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowNotTopIfNotOccluded) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  auto pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentOnTop) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  auto pass = CreateRenderPass();
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
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, AllowTransparentColorOnTop) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  auto pass = CreateRenderPass();
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                         SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(1U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectOpaqueColorOnTop) {
  auto pass = CreateRenderPass();
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
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(SingleOverlayOnTopTest, RejectTransparentColorOnTopWithoutBlending) {
  auto pass = CreateRenderPass();
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  CreateSolidColorQuadAt(shared_state, SK_ColorTRANSPARENT, pass.get(),
                         kOverlayBottomRightRect)
      ->needs_blending = false;
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        shared_state, pass.get(), kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, candidate_list.size());
}

TEST_F(ChangeSingleOnTopTest, DoNotPromoteIfContentsDontChange) {
  // Resource ID for the repeated quads. Value should be equivalent to
  // OverlayStrategySingleOnTop::kMaxFrameCandidateWithSameResourceId.
  size_t kFramesSkippedBeforeNotPromoting = 3;

  // The overlay prioritization feature supports overlay demotion for unchanging
  // overlays however the timing is slightly different as prioritization is
  // frame counter based.
  if (features::IsOverlayPrioritizationEnabled()) {
    kFramesSkippedBeforeNotPromoting =
        overlay_processor_->TrackerConfigAccessor().max_frames_inactive;
  }

  ResourceId previous_resource_id;
  int64_t frame_counter = 0;
  for (size_t i = 0; i < 3 + kFramesSkippedBeforeNotPromoting; ++i) {
    auto pass = CreateRenderPass();
    AggregatedRenderPass* main_pass = pass.get();

    ResourceId resource_id;
    if (i == 0 || i == 1) {
      // Create a unique resource only for the first 2 frames.
      resource_id = CreateResource(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->output_rect.size(),
          true /*is_overlay_candidate*/);
      previous_resource_id = resource_id;
      pass->shared_quad_state_list.back()->no_damage = false;
    } else {
      // Starting the 3rd frame, they should have the same resource ID.
      resource_id = previous_resource_id;
      pass->shared_quad_state_list.back()->no_damage = true;
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
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    overlay_processor_->SetFrameSequenceNumber(frame_counter);
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    frame_counter++;
    if (i <= kFramesSkippedBeforeNotPromoting) {
      EXPECT_EQ(1U, candidate_list.size());
      if (candidate_list.size()) {
        // Check that the right resource id got extracted.
        EXPECT_EQ(resource_id, candidate_list.back().resource_id);
      }
      // Check that the quad is gone.
      EXPECT_EQ(1U, main_pass->quad_list.size());
    } else {
      // Check nothing has been promoted.
      EXPECT_EQ(2U, main_pass->quad_list.size());
    }
  }
}

TEST_F(FullThresholdTest, ThresholdTestForPrioritization) {
  // This test is specific to the prioritized version of the overlay strategies.
  // The thresholds of damage and frame rate are only features of
  // prioritization.
  if (!features::IsOverlayPrioritizationEnabled()) {
    return;
  }

  int64_t frame_counter = 0;
  // This is a helper function to simulate framerates.

  auto wait_1_frame = [&]() { frame_counter++; };

  auto wait_4_frames = [&]() { frame_counter += 4; };

  // This test uses many iterations to test prioritization threshold features
  // due to frame averaging over samples.
  constexpr size_t kDamageFrameTestStart =
      OverlayCandidateTemporalTracker::kNumRecords;
  constexpr size_t kDamageFrameTestEnd =
      kDamageFrameTestStart + OverlayCandidateTemporalTracker::kNumRecords;
  constexpr size_t kSlowFrameTestStart =
      kDamageFrameTestEnd + OverlayCandidateTemporalTracker::kNumRecords;
  constexpr size_t kSlowFrameTestEnd =
      kSlowFrameTestStart + OverlayCandidateTemporalTracker::kNumRecords;

  // This quad is used to occlude the damage of the overlay candidate to the
  // point that the damage is no longer considered significant.
  auto nearly_occluding_quad = kOverlayRect;
  nearly_occluding_quad.Inset(1, 1);

  for (size_t i = 0; i < kSlowFrameTestEnd; ++i) {
    if (i >= kSlowFrameTestStart && i < kSlowFrameTestEnd) {
      wait_4_frames();
    } else {
      wait_1_frame();
    }

    auto pass = CreateRenderPass();
    AggregatedRenderPass* main_pass = pass.get();

    bool nearly_occluded =
        i >= kDamageFrameTestStart && i < kDamageFrameTestEnd;
    CreateSolidColorQuadAt(
        pass->shared_quad_state_list.back(), SK_ColorBLACK, pass.get(),
        nearly_occluded ? nearly_occluding_quad : kOverlayTopLeftRect);

    // Create a quad with the resource ID selected above.
    TextureDrawQuad* quad_candidate = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        kOverlayRect);

    quad_candidate->set_resource_size_in_pixels(pass->output_rect.size());

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), main_pass);

    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;
    surface_damage_rect_list.push_back(kOverlayRect);
    overlay_processor_->SetFrameSequenceNumber(frame_counter);
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(3u, main_pass->quad_list.size());

    if (i == kDamageFrameTestStart - 1 || i == kSlowFrameTestStart - 1) {
      // Test to make sure an overlay was promoted.
      EXPECT_EQ(1U, candidate_list.size());
    } else if (i == kDamageFrameTestEnd - 1 || i == kSlowFrameTestEnd - 1) {
      // Test to make sure no overlay was promoted
      EXPECT_EQ(0u, candidate_list.size());
    }
  }
}

TEST_F(UnderlayTest, OverlayLayerUnderMainLayer) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  auto pass = CreateRenderPass();
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
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
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  pass->CreateAndAppendSharedQuadState()->opacity = 0.5f;
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  AggregatedRenderPass* main_pass = pass.get();
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
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

// Pure overlays have a very specific optimization that does not produce damage
// on promotion because it is not required. However the same rect overlay
// transitions to an underlay the entire |display_rect| must damage the primary
// plane.This allows for the transparent black window to be drawn allowing the
// underlay to show through.
TEST_F(TransitionOverlayTypeTest, DamageChangeOnTransistionOverlayType) {
  static const int kOverlayFrameStart = 3;
  static const int kOverlayFrameEnd = 5;
  for (int i = 0; i < 8; ++i) {
    auto pass = CreateRenderPass();
    damage_rect_ = kOverlayTopLeftRect;

    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

    SurfaceDamageRectList surface_damage_rect_list;
    auto* sqs = pass->shared_quad_state_list.front();
    sqs->overlay_damage_index = 0;
    surface_damage_rect_list.emplace_back(damage_rect_);

    // A partially occluding quad is used to force an underlay rather than pure
    // overlay.
    if (i >= kOverlayFrameStart && i < kOverlayFrameEnd) {
      CreateSolidColorQuadAt(pass->shared_quad_state_list.back(), SK_ColorBLACK,
                             pass.get(), kOverlayTopLeftRect);
    }

    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.front(),
        pass.get());

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               default_damaged_shared_quad_state, pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(candidate_list.size(), 1U);

    if (i < kOverlayFrameStart) {
      // A pure overlay does not produce damage on promotion and all associated
      // damage with this quad is excluded.
      EXPECT_GE(candidate_list[0].plane_z_order, 0);
      EXPECT_TRUE(damage_rect_.IsEmpty());
    } else if (i == kOverlayFrameStart) {
      // An underlay must draw a transparent black window to the primary plane
      // to show through.
      EXPECT_LT(candidate_list[0].plane_z_order, 0);
      EXPECT_EQ(damage_rect_, kOverlayRect);
    } else if (i < kOverlayFrameEnd) {
      EXPECT_LT(candidate_list[0].plane_z_order, 0);
      // Empty damage is expect for an underlay for all frames after promotion.
      EXPECT_TRUE(damage_rect_.IsEmpty());
    } else if (i >= kOverlayFrameEnd) {
      EXPECT_GE(candidate_list[0].plane_z_order, 0);
      // Underlay to pure overlay transition should not produce any damage.
      EXPECT_TRUE(damage_rect_.IsEmpty());
    }
  }
}

// The first time an underlay is scheduled its damage must not be excluded.
TEST_F(UnderlayTest, InitialUnderlayDamageNotExcluded) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  damage_rect_ = kOverlayRect;

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

// An identical underlay for two frames in a row means the damage can be
// excluded the second time. On demotion the frame damage will be the display
// rect of the underlay. After the demotion there will be no exclusion of
// damage.
TEST_F(UnderlayTest, DamageExcludedForConsecutiveIdenticalUnderlays) {
  static const int kDemotionFrame = 3;
  for (int i = 0; i < 5; ++i) {
    auto pass = CreateRenderPass();
    damage_rect_ = kOverlayTopLeftRect;

    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

    SurfaceDamageRectList surface_damage_rect_list;
    if (i < kDemotionFrame) {
      auto* sqs = pass->shared_quad_state_list.front();
      sqs->overlay_damage_index = 0;
      surface_damage_rect_list.emplace_back(damage_rect_);

      CreateFullscreenCandidateQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.front(),
          pass.get());
    }
    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               default_damaged_shared_quad_state, pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    if (i == 0) {
      // A promoted underlay needs to damage the primary plane on the first
      // frame of promotion.
      EXPECT_EQ(kOverlayRect, damage_rect_);
    } else if (i < kDemotionFrame) {
      EXPECT_TRUE(damage_rect_.IsEmpty());
    } else if (i == kDemotionFrame) {
      // A demoted underlay needs to damage the primary plane on the frame of
      // demotion.
      EXPECT_EQ(kOverlayRect, damage_rect_);
    } else if (i > kDemotionFrame) {
      // No overlay candidates so the damage will be whatever root damage was
      // input to the overlay processsor.
      EXPECT_EQ(damage_rect_, kOverlayTopLeftRect);
    }
  }
}

// Underlay damage can only be excluded if the previous frame's underlay
// was the same rect.
TEST_F(UnderlayTest, DamageNotExcludedForNonIdenticalConsecutiveUnderlays) {
  gfx::Rect kSmallTestRect = gfx::Rect(5, 5, 20, 20);
  gfx::Rect overlay_rects[] = {kOverlayBottomRightRect, kSmallTestRect,
                               kOverlayTopLeftRect, kOverlayRect};
  gfx::Rect prev_rect;
  for (auto&& curr_rect : overlay_rects) {
    AddExpectedRectToOverlayProcessor(gfx::RectF(curr_rect));

    auto pass = CreateRenderPass();
    SurfaceDamageRectList surface_damage_rect_list;
    auto* sqs = pass->shared_quad_state_list.front();
    sqs->overlay_damage_index = 0;
    damage_rect_ = gfx::Rect(10, 10, 10, 10);
    surface_damage_rect_list.emplace_back(damage_rect_);

    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          curr_rect);

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    // This is a union as the demoted underlay display rect is added as damage
    // as well as the newly promoted underlay display rect (updating the primary
    // plane for underlays).
    gfx::Rect temp_union = prev_rect;
    temp_union.Union(curr_rect);
    EXPECT_EQ(temp_union, damage_rect_);
    prev_rect = curr_rect;
  }
}

// Underlay damage can only be excluded if the previous frame's underlay exists.
TEST_F(UnderlayTest, DamageNotExcludedForNonConsecutiveIdenticalUnderlays) {
  bool has_fullscreen_candidate[] = {true, false, true, true, true, false};

  for (int i = 0; i < 3; ++i) {
    auto pass = CreateRenderPass();
    damage_rect_ = kOverlayRect;

    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

    SurfaceDamageRectList surface_damage_rect_list;
    auto* sqs = pass->shared_quad_state_list.front();
    sqs->overlay_damage_index = 0;
    surface_damage_rect_list.emplace_back(damage_rect_);

    if (has_fullscreen_candidate[i]) {
      CreateFullscreenCandidateQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.front(),
          pass.get());
    }

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               default_damaged_shared_quad_state, pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    if (i == 0) {
      EXPECT_FALSE(damage_rect_.IsEmpty());
    } else {
      EXPECT_EQ(damage_rect_.IsEmpty(),
                has_fullscreen_candidate[i] && has_fullscreen_candidate[i - 1]);
    }
  }
}

TEST_F(UnderlayTest, DamageExcludedForCandidateAndThoseOccluded) {
  for (int i = 0; i < 3; ++i) {
    auto pass = CreateRenderPass();
    damage_rect_ = kOverlayRect;
    SurfaceDamageRectList surface_damage_rect_list;

    if (i == 1) {
      // Create damages in front which should not be excluded.
      surface_damage_rect_list.emplace_back(kOverlayTopLeftRect);
      auto* sqs = pass->shared_quad_state_list.front();
      sqs->overlay_damage_index = 1;
      surface_damage_rect_list.emplace_back(damage_rect_);
    } else {
      auto* sqs = pass->shared_quad_state_list.front();
      sqs->overlay_damage_index = 0;
      surface_damage_rect_list.emplace_back(damage_rect_);
    }

    if (i > 1) {
      // This damage is after our overlay will be excluded from our final
      // damage.
      surface_damage_rect_list.emplace_back(kOverlayTopLeftRect);
    }

    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.front(),
        pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    // The damage rect should not be excluded if the underlay has been promoted
    // this frame.
    if (i == 0) {
      EXPECT_FALSE(damage_rect_.IsEmpty());
    } else if (i == 1) {
      // Additional damage at i == 1 should also not be excluded as it comes in
      // front of our underlay.
      EXPECT_EQ(damage_rect_, kOverlayTopLeftRect);
    }
  }
  // The second time the same overlay rect is scheduled it should be excluded
  // from the damage rect.
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(UnderlayTest, DamageExtractedWhenQuadsAboveDontOverlap) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  for (int i = 0; i < 2; ++i) {
    auto pass = CreateRenderPass();

    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

    SurfaceDamageRectList surface_damage_rect_list;
    default_damaged_shared_quad_state->overlay_damage_index = 0;
    surface_damage_rect_list.emplace_back(kOverlayTopLeftRect);

    auto* sqs = pass->shared_quad_state_list.front();
    sqs->overlay_damage_index = 1;
    surface_damage_rect_list.emplace_back(kOverlayBottomRightRect);

    // Add a non-overlapping quad above the candidate.
    CreateOpaqueQuadAt(resource_provider_.get(),
                       default_damaged_shared_quad_state, pass.get(),
                       kOverlayTopLeftRect);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.front(), pass.get(),
                          kOverlayBottomRightRect);

    damage_rect_ = kOverlayBottomRightRect;
    damage_rect_.Union(kOverlayTopLeftRect);

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
  }

  EXPECT_EQ(damage_rect_, kOverlayTopLeftRect);
}

TEST_F(UnderlayTest, PrimaryPlaneOverlayIsTransparentWithUnderlay) {
  auto pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     output_rect, SK_ColorWHITE);

  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayRect);

  OverlayCandidateList candidate_list;

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDefaultBufferFormat, gfx::ColorSpace(),
      false /* has_alpha */, gpu::Mailbox());
  OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane =
      &output_surface_plane;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), primary_plane, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, candidate_list.size());
  ASSERT_EQ(true, output_surface_plane.enable_blending);
}

TEST_F(UnderlayTest, UpdateDamageWhenChangingUnderlays) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayTopLeftRect));

  for (size_t i = 0; i < 2; ++i) {
    auto pass = CreateRenderPass();

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
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
  }

  EXPECT_EQ(kOverlayRect, damage_rect_);
}

TEST_F(UnderlayTest, OverlayCandidateTemporalTracker) {
  ResourceIdGenerator id_generator;
  uint64_t frame_counter = 0;

  // Test the default configuration.
  OverlayCandidateTemporalTracker::Config config;
  float kDamageEpsilon = config.damage_rate_hysteresis_range;
  float kBelowLowDamage = config.damage_rate_threshold - kDamageEpsilon;
  float kAboveHighDamage = config.damage_rate_threshold + kDamageEpsilon;
  float kFullDamage = 1.0f;
  // This is a helper function to simulate framerates.
  auto wait_1_frame = [&]() { frame_counter++; };

  auto wait_inactive_frames = [&]() {
    frame_counter += config.max_frames_inactive + 1;
  };

  OverlayCandidateTemporalTracker tracker;
  int fake_display_area = 256 * 256;
  // We test internal hysteresis state by running this test twice.
  for (int j = 0; j < 2; j++) {
    // First setup a 60fps high damage candidate.
    for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
      wait_1_frame();
      tracker.AddRecord(frame_counter, kFullDamage,
                        id_generator.GenerateNextId(), config);
    }

    EXPECT_TRUE(tracker.IsActivelyChanging(frame_counter, config));
    auto opaque_power_gain_60_full =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);

    EXPECT_FLOAT_EQ(tracker.MeanFrameRatioRate(config), 1.0f);
    EXPECT_GT(opaque_power_gain_60_full, 0);

    // Test our hysteresis categorization of power by ensuring a single frame
    // drop does not change the end power categorization.
    wait_1_frame();
    wait_1_frame();
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    auto opaque_power_gain_60_stutter =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);

    // A single frame drop even at 60fps should not change our power
    // categorization.
    ASSERT_EQ(opaque_power_gain_60_full, opaque_power_gain_60_stutter);

    wait_inactive_frames();
    EXPECT_FALSE(tracker.IsActivelyChanging(frame_counter, config));
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    auto opaque_power_gain_60_inactive =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);
    // Simple test to make sure that power categorization is not completely
    // invalidated when candidate becomes inactive.
    EXPECT_GT(opaque_power_gain_60_inactive, 0);

    // Now simulate a overlay candidate with 30fps full damage.
    for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
      wait_1_frame();
      wait_1_frame();
      tracker.AddRecord(frame_counter, kFullDamage,
                        id_generator.GenerateNextId(), config);
    }

    auto opaque_power_gain_30_full =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);

    EXPECT_FLOAT_EQ(tracker.MeanFrameRatioRate(config), 0.5f);
    EXPECT_GT(opaque_power_gain_30_full, 0);
    EXPECT_GT(opaque_power_gain_60_full, opaque_power_gain_30_full);

    // Test the hysteresis by checking that a stuttering frame will not change
    // power categorization.
    wait_1_frame();
    wait_1_frame();
    wait_1_frame();
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    EXPECT_TRUE(tracker.IsActivelyChanging(frame_counter, config));
    auto opaque_power_gain_30_stutter =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);

    EXPECT_TRUE(opaque_power_gain_30_stutter == opaque_power_gain_30_full);

    wait_inactive_frames();
    EXPECT_FALSE(tracker.IsActivelyChanging(frame_counter, config));
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    auto opaque_power_gain_30_inactive =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);
    // Simple test to make sure that power categorization is not completely
    // invalidated when candidate becomes inactive.
    EXPECT_GT(opaque_power_gain_30_inactive, 0);

    // Test low and high damage thresholds.
    for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
      wait_1_frame();
      tracker.AddRecord(frame_counter, kAboveHighDamage,
                        id_generator.GenerateNextId(), config);
    }

    auto opaque_power_gain_high_damage =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);

    EXPECT_GT(opaque_power_gain_high_damage, 0);
    EXPECT_GE(opaque_power_gain_60_full, opaque_power_gain_high_damage);

    for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
      wait_1_frame();
      tracker.AddRecord(frame_counter, kBelowLowDamage,
                        id_generator.GenerateNextId(), config);
    }

    auto opaque_power_gain_low_damage =
        tracker.GetModeledPowerGain(frame_counter, config, fake_display_area);
    EXPECT_LT(opaque_power_gain_low_damage, 0);

    // Test our mean damage ratio computations for our tacker.
    float expected_mean = 0.0f;
    for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
      wait_1_frame();
      // Please note that this first iter frame damage will not get included in
      // the mean as the mean is computed between timing intervals. This means 6
      // timing intervals only gives 5 values.
      float dynamic_damage_ratio = static_cast<float>(i);
      expected_mean += dynamic_damage_ratio;
      tracker.AddRecord(frame_counter, dynamic_damage_ratio,
                        id_generator.GenerateNextId(), config);
    }

    expected_mean =
        expected_mean / (OverlayCandidateTemporalTracker::kNumRecords - 1);

    EXPECT_FLOAT_EQ(expected_mean, tracker.MeanFrameRatioRate(config));
  }

  EXPECT_FALSE(tracker.IsAbsent());
  // After a many absent calls the 'IncAbsent()' function should eventually
  // return true; indicating this tracker is no longer active.
  EXPECT_TRUE(tracker.IsAbsent());

  wait_1_frame();
  tracker.AddRecord(frame_counter, 0.0f, id_generator.GenerateNextId(), config);
  EXPECT_FALSE(tracker.IsAbsent());

  // Tracker forced updates were added to support quads that change content but
  // not resource ids (example here is low latency ink surface). Here we test
  // this small feature by keeping the resource id constant but passing in true
  // to the force update param.
  static const float kDamageRatio = 0.7f;
  static const ResourceId kFakeConstantResourceId(13);
  for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
    wait_1_frame();
    tracker.AddRecord(frame_counter, kDamageRatio, kFakeConstantResourceId,
                      config, true);
  }
  EXPECT_FLOAT_EQ(kDamageRatio, tracker.MeanFrameRatioRate(config));

  // Now test the false case for the force update param.
  for (int i = 0; i < OverlayCandidateTemporalTracker::kNumRecords; i++) {
    wait_1_frame();
    tracker.AddRecord(frame_counter, 0.9f, kFakeConstantResourceId, config,
                      false);
  }
  // The damage should remain unchanged.
  EXPECT_FLOAT_EQ(kDamageRatio, tracker.MeanFrameRatioRate(config));
}

TEST_F(UnderlayTest, UpdateDamageRectWhenNoPromotion) {
  // In the first pass there is an overlay promotion and the expected damage is
  // a union of the hole made for the underlay and the incoming damage. In the
  // second pass there is no occluding damage so the incoming damage is
  // attributed to the overlay candidate and the final output damage is zero. In
  // the third pass there is no overlay promotion, but the damage should be the
  // union of the damage_rect with CreateRenderPass's output_rect which is {0,
  // 0, 256, 256}. This is due to the demotion of the current overlay.
  bool has_fullscreen_candidate[] = {true, true, false};
  gfx::Rect damages[] = {gfx::Rect(0, 0, 32, 32), gfx::Rect(0, 0, 32, 32),
                         gfx::Rect(0, 0, 312, 16)};
  gfx::Rect expected_damages[] = {gfx::Rect(0, 0, 256, 256),
                                  gfx::Rect(0, 0, 0, 0),
                                  gfx::Rect(0, 0, 312, 256)};
  size_t expected_candidate_size[] = {1, 1, 0};

  for (size_t i = 0; i < base::size(expected_damages); ++i) {
    auto pass = CreateRenderPass();
    SurfaceDamageRectList surface_damage_rect_list;
    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    default_damaged_shared_quad_state->overlay_damage_index = 0;
    surface_damage_rect_list.emplace_back(gfx::Rect());

    if (has_fullscreen_candidate[i]) {
      auto* sqs = pass->shared_quad_state_list.front();
      surface_damage_rect_list.emplace_back(damages[i]);
      sqs->overlay_damage_index = 1;
      CreateFullscreenCandidateQuad(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), pass->shared_quad_state_list.front(),
          pass.get());
    }

    gfx::Rect damage_rect{damages[i]};

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap
        render_pass_background_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_background_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect, &content_bounds_);

    EXPECT_EQ(expected_damages[i], damage_rect);
    ASSERT_EQ(expected_candidate_size[i], candidate_list.size());
  }
}

// Tests that no damage occurs when the quad shared state has no occluding
// damage.
TEST_F(UnderlayTest, CandidateNoDamageWhenQuadSharedStateNoOccludingDamage) {
  for (int i = 0; i < 4; ++i) {
    auto pass = CreateRenderPass();
    SurfaceDamageRectList surface_damage_rect_list;

    gfx::Rect rect(2, 3);
    gfx::Rect kSmallDamageRect(1, 1, 10, 10);
    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    if (i == 2) {
      auto* sqs = pass->shared_quad_state_list.front();
      sqs->overlay_damage_index = 0;
      surface_damage_rect_list.emplace_back(0, 0, 20, 20);
    } else if (i == 3) {
      auto* sqs = pass->shared_quad_state_list.front();
      // For the solid color quad kSmallDamageRect.
      surface_damage_rect_list.emplace_back(kSmallDamageRect);
      // For the overlay quad gfx::Rect(0, 0, 20, 20).
      surface_damage_rect_list.emplace_back(0, 0, 20, 20);
      sqs->overlay_damage_index = 1;
    }

    CreateSolidColorQuadAt(default_damaged_shared_quad_state, SK_ColorBLACK,
                           pass.get(), rect);

    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.front(),
        pass.get());

    damage_rect_ = kOverlayRect;

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    if (i == 0 || i == 1) {
      EXPECT_EQ(candidate_list[0].overlay_damage_index,
                OverlayCandidate::kInvalidDamageIndex);
      EXPECT_FALSE(damage_rect_.IsEmpty());
    } else if (i == 3) {
      EXPECT_EQ(candidate_list[0].overlay_damage_index, 1U);
      EXPECT_EQ(damage_rect_, kSmallDamageRect);
    } else if (i == 2) {
      EXPECT_EQ(candidate_list[0].overlay_damage_index, 0U);
      EXPECT_TRUE(damage_rect_.IsEmpty());
    }
  }
}

TEST_F(UnderlayCastTest, NoOverlayContentBounds) {
  auto pass = CreateRenderPass();

  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0U, content_bounds_.size());
}

TEST_F(UnderlayCastTest, FullScreenOverlayContentBounds) {
  auto pass = CreateRenderPass();
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_TRUE(content_bounds_[0].IsEmpty());
}

TEST_F(UnderlayCastTest, BlackOutsideOverlayContentBounds) {
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  const gfx::Rect kLeftSide(0, 0, 128, 256);
  const gfx::Rect kTopRight(128, 0, 128, 128);

  auto pass = CreateRenderPass();
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayBottomRightRect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(), kLeftSide,
                     SK_ColorBLACK);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(), kTopRight,
                     SK_ColorBLACK);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_TRUE(content_bounds_[0].IsEmpty());
}

TEST_F(UnderlayCastTest, OverlayOccludedContentBounds) {
  auto pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(kOverlayTopLeftRect, content_bounds_[0]);
}

TEST_F(UnderlayCastTest, OverlayOccludedUnionContentBounds) {
  auto pass = CreateRenderPass();
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayTopLeftRect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     kOverlayBottomRightRect);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(kOverlayRect, content_bounds_[0]);
}

TEST_F(UnderlayCastTest, RoundOverlayContentBounds) {
  // Check rounding behaviour on overlay quads.  Be conservative (content
  // potentially visible on boundary).
  const gfx::Rect overlay_rect(1, 1, 8, 8);
  AddExpectedRectToOverlayProcessor(gfx::RectF(1.5f, 1.5f, 8, 8));

  gfx::Transform transform;
  transform.Translate(0.5f, 0.5f);

  auto pass = CreateRenderPassWithTransform(transform);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            overlay_rect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     gfx::Rect(0, 0, 10, 10), SK_ColorWHITE);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(gfx::Rect(0, 0, 11, 11), content_bounds_[0]);
}

TEST_F(UnderlayCastTest, RoundContentBounds) {
  // Check rounding behaviour on content quads (bounds should be enclosing
  // rect).
  gfx::Rect overlay_rect = kOverlayRect;
  overlay_rect.Inset(0, 0, 1, 1);
  AddExpectedRectToOverlayProcessor(gfx::RectF(0.5f, 0.5f, 255, 255));

  gfx::Transform transform;
  transform.Translate(0.5f, 0.5f);

  auto pass = CreateRenderPassWithTransform(transform);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            overlay_rect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     gfx::Rect(0, 0, 255, 255), SK_ColorWHITE);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, content_bounds_.size());
  EXPECT_EQ(kOverlayRect, content_bounds_[0]);
}

TEST_F(UnderlayCastTest, NoOverlayPromotionWithoutProtectedContent) {
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayRect);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  ASSERT_TRUE(candidate_list.empty());
  EXPECT_TRUE(content_bounds_.empty());
}
#endif

#if defined(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
TEST_F(UnderlayCastTest, PrimaryPlaneOverlayIsAlwaysTransparent) {
  auto pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     output_rect, SK_ColorWHITE);

  OverlayCandidateList candidate_list;

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDefaultBufferFormat, gfx::ColorSpace(), gpu::Mailbox());

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), &output_surface_plane,
      &candidate_list, &damage_rect_, &content_bounds_);

  ASSERT_EQ(true, output_surface_plane.enable_blending);
  EXPECT_EQ(0U, content_bounds_.size());
}
#endif

#if defined(USE_OZONE) || defined(OS_ANDROID)
class OverlayInfoRendererGL : public GLRenderer {
 public:
  OverlayInfoRendererGL(const RendererSettings* settings,
                        const DebugRendererSettings* debug_settings,
                        OutputSurface* output_surface,
                        DisplayResourceProviderGL* resource_provider,
                        SingleOverlayProcessor* overlay_processor)
      : GLRenderer(settings,
                   debug_settings,
                   output_surface,
                   resource_provider,
                   overlay_processor,
                   nullptr),
        expect_overlays_(false) {}

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
    EXPECT_GE(current_frame()->overlay_list.back().resource_id,
              kInvalidResourceId);
  }

  void AddExpectedRectToOverlayProcessor(const gfx::RectF& rect) {
    DCHECK(overlay_processor_);
    static_cast<SingleOverlayProcessor*>(overlay_processor_)
        ->AddExpectedRect(rect);
  }

  void set_expect_overlays(bool expect_overlays) {
    expect_overlays_ = expect_overlays;
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
 protected:
  GLRendererWithOverlaysTest() {
    provider_ = TestContextProvider::Create();
    provider_->BindToCurrentThread();
    output_surface_ = std::make_unique<OverlayOutputSurface>(provider_);
    output_surface_->BindToClient(&output_surface_client_);
    resource_provider_ =
        std::make_unique<DisplayResourceProviderGL>(provider_.get());

    provider_->support()->SetScheduleOverlayPlaneCallback(base::BindRepeating(
        &MockOverlayScheduler::Schedule, base::Unretained(&scheduler_)));

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentThread();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();
  }

  ~GLRendererWithOverlaysTest() override {
    child_resource_provider_->ShutdownAndReleaseAllResources();
  }

  void SetUp() override {
#if defined(USE_X11)
    // TODO(1096425): non-Ozone/X11 doesn't seem to support overlays.
    if (!features::IsUsingOzonePlatform())
      GTEST_SKIP();
#endif
  }

  void Init(bool use_overlay_processor) {
    if (use_overlay_processor)
      owned_overlay_processor_ = std::make_unique<SingleOverlayProcessor>();
    renderer_ = std::make_unique<OverlayInfoRendererGL>(
        &settings_, &debug_settings_, output_surface_.get(),
        resource_provider_.get(), owned_overlay_processor_.get());
    renderer_->Initialize();
    renderer_->SetVisible(true);
  }

  void DrawFrame(AggregatedRenderPassList* pass_list,
                 const gfx::Size& viewport_size,
                 SurfaceDamageRectList surface_damage_rect_list =
                     SurfaceDamageRectList()) {
    renderer_->DrawFrame(pass_list, 1.f, viewport_size,
                         gfx::DisplayColorSpaces(),
                         std::move(surface_damage_rect_list));
  }
  void SwapBuffers() {
    renderer_->SwapBuffers({});
    renderer_->SwapBuffersComplete();
  }
  void SwapBuffersWithoutComplete() { renderer_->SwapBuffers({}); }
  void SwapBuffersComplete() { renderer_->SwapBuffersComplete(); }
  void ReturnResourceInUseQuery(ResourceId id) {
    DisplayResourceProviderGL::ScopedReadLockGL lock(resource_provider_.get(),
                                                     id);
    gpu::TextureInUseResponse response;
    response.texture = lock.texture_id();
    response.in_use = false;
    gpu::TextureInUseResponses responses;
    responses.push_back(response);
    renderer_->DidReceiveTextureInUseResponses(responses);
  }

  void AddExpectedRectToOverlayProcessor(const gfx::RectF& rect) {
    renderer_->AddExpectedRectToOverlayProcessor(rect);
  }

  void DrawEmptyFrame() {
    // Draw one frame to make sure output surface is reshaped before tests.
    auto pass = CreateRenderPass();

    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
    DrawFrame(&pass_list, kDisplaySize);
    EXPECT_EQ(1U, output_surface_->bind_framebuffer_count());
    output_surface_->clear_bind_framebuffer_count();
    SwapBuffers();
    Mock::VerifyAndClearExpectations(renderer_.get());
    Mock::VerifyAndClearExpectations(&scheduler_);
  }

  RendererSettings settings_;
  DebugRendererSettings debug_settings_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<OverlayOutputSurface> output_surface_;
  std::unique_ptr<DisplayResourceProviderGL> resource_provider_;
  std::unique_ptr<SingleOverlayProcessor> owned_overlay_processor_;
  std::unique_ptr<OverlayInfoRendererGL> renderer_;
  scoped_refptr<TestContextProvider> provider_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  MockOverlayScheduler scheduler_;
};

TEST_F(GLRendererWithOverlaysTest, OverlayQuadNotDrawn) {
  bool use_overlay_processor = true;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(true);
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayBottomRightRect));

  auto pass = CreateRenderPass();

  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayBottomRightRect);
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  AggregatedRenderPassList pass_list;
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
  bool use_overlay_processor = true;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(true);

  auto pass = CreateRenderPass();

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  AggregatedRenderPassList pass_list;
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
  bool use_overlay_processor = false;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(false);

  auto pass = CreateRenderPass();

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             pass->shared_quad_state_list.back(), pass.get());

  AggregatedRenderPassList pass_list;
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
  bool use_overlay_processor = true;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(true);

  // Draw one frame to make sure output surface is reshaped before tests.
  DrawEmptyFrame();

  auto pass = CreateRenderPass();

  SurfaceDamageRectList surface_damage_rect_list;
  SharedQuadState* default_damaged_shared_quad_state =
      pass->shared_quad_state_list.AllocateAndCopyFrom(
          pass->shared_quad_state_list.back());
  auto* sqs = pass->shared_quad_state_list.front();
  surface_damage_rect_list.emplace_back(pass->output_rect);
  sqs->overlay_damage_index = 0;

  surface_damage_rect_list.emplace_back(pass->output_rect);
  default_damaged_shared_quad_state->overlay_damage_index = 1;

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.front(), pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             default_damaged_shared_quad_state, pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             default_damaged_shared_quad_state, pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(0);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  DrawFrame(&pass_list, kDisplaySize, std::move(surface_damage_rect_list));
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

// GLRenderer skips drawing occluded quads when empty swap is enabled.
TEST_F(GLRendererWithOverlaysTest, OccludedQuadNotDrawnWhenEmptySwapAllowed) {
  provider_->TestContextGL()->set_have_commit_overlay_planes(true);
  bool use_overlay_processor = true;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(true);

  // Draw one frame to make sure output surface is reshaped before tests.
  DrawEmptyFrame();

  auto pass = CreateRenderPass();

  SurfaceDamageRectList surface_damage_rect_list;
  SharedQuadState* default_damaged_shared_quad_state =
      pass->shared_quad_state_list.AllocateAndCopyFrom(
          pass->shared_quad_state_list.back());
  auto* sqs = pass->shared_quad_state_list.front();
  surface_damage_rect_list.emplace_back(pass->output_rect);
  sqs->overlay_damage_index = 0;

  surface_damage_rect_list.emplace_back(pass->output_rect);
  default_damaged_shared_quad_state->overlay_damage_index = 1;

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                child_resource_provider_.get(),
                                child_provider_.get(), sqs, pass.get());

  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             default_damaged_shared_quad_state, pass.get());
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             default_damaged_shared_quad_state, pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  output_surface_->set_is_displayed_as_overlay_plane(true);
  EXPECT_CALL(*renderer_, DoDrawQuad(_, _)).Times(0);
  EXPECT_CALL(scheduler_, Schedule(_, _, _, _, _, _, _)).Times(2);
  DrawFrame(&pass_list, kDisplaySize, std::move(surface_damage_rect_list));
  EXPECT_EQ(0U, output_surface_->bind_framebuffer_count());
  SwapBuffers();
  Mock::VerifyAndClearExpectations(renderer_.get());
  Mock::VerifyAndClearExpectations(&scheduler_);
}

TEST_F(GLRendererWithOverlaysTest, ResourcesExportedAndReturnedWithDelay) {
  bool use_overlay_processor = true;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource2 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource3 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);

  // Return the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource1, resource2, resource3}, resource_provider_.get(),
          child_resource_provider_.get(), child_provider_.get());

  ResourceId mapped_resource1 = resource_map[resource1];
  ResourceId mapped_resource2 = resource_map[resource2];
  ResourceId mapped_resource3 = resource_map[resource3];

  auto pass = CreateRenderPass();
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(1);
  frame1.output_surface_plane =
      OverlayProcessorInterface::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = mapped_resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(1);
  frame2.output_surface_plane =
      OverlayProcessorInterface::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = mapped_resource2;
  overlay2.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  frame3.overlay_list.resize(1);
  frame3.output_surface_plane =
      OverlayProcessorInterface::OutputSurfaceOverlayPlane();
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
  bool use_overlay_processor = true;
  settings_.release_overlay_resources_after_gpu_query = true;
  Init(use_overlay_processor);
  renderer_->set_expect_overlays(true);

  ResourceId resource1 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource2 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);
  ResourceId resource3 = CreateResourceInLayerTree(
      child_resource_provider_.get(), gfx::Size(32, 32), true);

  // Return the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource1, resource2, resource3}, resource_provider_.get(),
          child_resource_provider_.get(), child_provider_.get());
  ResourceId mapped_resource1 = resource_map[resource1];
  ResourceId mapped_resource2 = resource_map[resource2];
  ResourceId mapped_resource3 = resource_map[resource3];

  auto pass = CreateRenderPass();
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  DirectRenderer::DrawingFrame frame1;
  frame1.render_passes_in_draw_order = &pass_list;
  frame1.overlay_list.resize(1);
  frame1.output_surface_plane =
      OverlayProcessorInterface::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay1 = frame1.overlay_list.back();
  overlay1.resource_id = mapped_resource1;
  overlay1.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame2;
  frame2.render_passes_in_draw_order = &pass_list;
  frame2.overlay_list.resize(1);
  frame2.output_surface_plane =
      OverlayProcessorInterface::OutputSurfaceOverlayPlane();
  OverlayCandidate& overlay2 = frame2.overlay_list.back();
  overlay2.resource_id = mapped_resource2;
  overlay2.plane_z_order = 1;

  DirectRenderer::DrawingFrame frame3;
  frame3.render_passes_in_draw_order = &pass_list;
  frame3.overlay_list.resize(1);
  frame3.output_surface_plane =
      OverlayProcessorInterface::OutputSurfaceOverlayPlane();
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

#if defined(OS_APPLE)
class CALayerOverlayRPDQTest : public CALayerOverlayTest {
 protected:
  void SetUp() override {
    CALayerOverlayTest::SetUp();
    pass_list_.push_back(CreateRenderPass());
    pass_ = pass_list_.back().get();
    quad_ = pass_->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    render_pass_id_ = 3;
  }

  void ProcessForOverlays() {
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list_, GetIdentityColorMatrix(),
        render_pass_filters_, render_pass_backdrop_filters_,
        std::move(surface_damage_rect_list_), nullity, &ca_layer_list_,
        &damage_rect_, &content_bounds_);
  }
  AggregatedRenderPassList pass_list_;
  AggregatedRenderPass* pass_;
  AggregatedRenderPassDrawQuad* quad_;
  int render_pass_id_;
  cc::FilterOperations filters_;
  cc::FilterOperations backdrop_filters_;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters_;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters_;
  CALayerOverlayList ca_layer_list_;
  SurfaceDamageRectList surface_damage_rect_list_;
};

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadNoFilters) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();

  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadAllValidFilters) {
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
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();

  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadOpacityFilterScale) {
  filters_.Append(cc::FilterOperation::CreateOpacityFilter(0.8f));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 2), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadBlurFilterScale) {
  filters_.Append(cc::FilterOperation::CreateBlurFilter(0.8f));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 2), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest,
       AggregatedRenderPassDrawQuadDropShadowFilterScale) {
  filters_.Append(cc::FilterOperation::CreateDropShadowFilter(
      gfx::Point(10, 20), 1.0f, SK_ColorGREEN));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 2), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadBackgroundFilter) {
  backdrop_filters_.Append(cc::FilterOperation::CreateGrayscaleFilter(0.1f));
  render_pass_backdrop_filters_[render_pass_id_] = &backdrop_filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadMask) {
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, ResourceId(2), gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(1U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, AggregatedRenderPassDrawQuadUnsupportedFilter) {
  filters_.Append(cc::FilterOperation::CreateZoomFilter(0.9f, 1));
  render_pass_filters_[render_pass_id_] = &filters_;
  quad_->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                kOverlayRect, render_pass_id_, kInvalidResourceId, gfx::RectF(),
                gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                false, 1.0f);
  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}

TEST_F(CALayerOverlayRPDQTest, TooManyRenderPassDrawQuads) {
  filters_.Append(cc::FilterOperation::CreateBlurFilter(0.8f));
  int count = 35;

  for (int i = 0; i < count; ++i) {
    auto* quad = pass_->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    quad->SetNew(pass_->shared_quad_state_list.back(), kOverlayRect,
                 kOverlayRect, render_pass_id_, ResourceId(2), gfx::RectF(),
                 gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                 false, 1.0f);
  }

  ProcessForOverlays();
  EXPECT_EQ(0U, ca_layer_list_.size());
}
#endif

void AddQuad(gfx::Rect quad_rect,
             const gfx::Transform& quad_to_target_transform,
             AggregatedRenderPass* render_pass) {
  SharedQuadState* quad_state = render_pass->CreateAndAppendSharedQuadState();

  quad_state->SetAll(
      /*quad_layer_rect=*/quad_to_target_transform, quad_rect,
      /*visible_quad_layer_rect=*/quad_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(), /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false,
      /*are contents opaque=*/true,
      /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, quad_rect, quad_rect, SK_ColorBLACK,
                     false /* force_anti_aliasing_off */);
}

OverlayCandidate CreateCandidate(float left,
                                 float top,
                                 float right,
                                 float bottom) {
  OverlayCandidate candidate;
  candidate.display_rect.SetRect(left, top, right - left, bottom - top);
  return candidate;
}

using OverlayCandidateTest = testing::Test;

TEST_F(OverlayCandidateTest, IsOccluded) {
  std::unique_ptr<AggregatedRenderPass> render_pass =
      std::make_unique<AggregatedRenderPass>();
  gfx::Transform identity;
  identity.MakeIdentity();

  // Create overlapping quads around 1,1 - 10,10.
  AddQuad(gfx::Rect(0, 0, 1, 10), identity, render_pass.get());
  AddQuad(gfx::Rect(0, 0, 10, 1), identity, render_pass.get());
  AddQuad(gfx::Rect(10, 0, 1, 10), identity, render_pass.get());
  AddQuad(gfx::Rect(0, 10, 10, 1), identity, render_pass.get());

  EXPECT_FALSE(OverlayCandidate::IsOccluded(
      CreateCandidate(0.5f, 0.5f, 10.49f, 10.49f),
      render_pass->quad_list.begin(), render_pass->quad_list.end()));

  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(0.49f, 0.5f, 10.49f, 10.49f),
      render_pass->quad_list.begin(), render_pass->quad_list.end()));

  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(0.5f, 0.49f, 10.50f, 10.5f),
      render_pass->quad_list.begin(), render_pass->quad_list.end()));
  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(0.5f, 0.5f, 10.5f, 10.49f),
      render_pass->quad_list.begin(), render_pass->quad_list.end()));

  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(0.5f, 0.5f, 10.49f, 10.5f),
      render_pass->quad_list.begin(), render_pass->quad_list.end()));
}

TEST_F(OverlayCandidateTest, IsOccludedScaled) {
  std::unique_ptr<AggregatedRenderPass> render_pass =
      std::make_unique<AggregatedRenderPass>();
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Scale(1.6, 1.6);

  // Create overlapping quads around 1.6,2.4 - 14.4,17.6.
  AddQuad(gfx::Rect(0, 0, 1, 10), quad_to_target_transform, render_pass.get());
  AddQuad(gfx::Rect(0, 0, 10, 2), quad_to_target_transform, render_pass.get());
  AddQuad(gfx::Rect(9, 0, 1, 10), quad_to_target_transform, render_pass.get());
  AddQuad(gfx::Rect(0, 11, 10, 1), quad_to_target_transform, render_pass.get());

  EXPECT_FALSE(OverlayCandidate::IsOccluded(
      CreateCandidate(2.f, 3.f, 14.f, 17.f), render_pass->quad_list.begin(),
      render_pass->quad_list.end()));
  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(1.f, 3.f, 14.f, 17.f), render_pass->quad_list.begin(),
      render_pass->quad_list.end()));
  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(2.f, 2.f, 14.f, 17.f), render_pass->quad_list.begin(),
      render_pass->quad_list.end()));
  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(2.f, 3.f, 15.f, 17.f), render_pass->quad_list.begin(),
      render_pass->quad_list.end()));
  EXPECT_TRUE(OverlayCandidate::IsOccluded(
      CreateCandidate(2.f, 3.f, 15.f, 18.f), render_pass->quad_list.begin(),
      render_pass->quad_list.end()));
}

TEST_F(SingleOverlayOnTopTest, IsOverlayRequiredBasic) {
  // Add a small quad.
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(0, 0, 16, 16);
  auto* new_quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  SurfaceDamageRectList surface_damage_rect_list;
  SkMatrix44 default_color = GetIdentityColorMatrix();
  OverlayCandidate candidate;
  OverlayCandidate::FromDrawQuad(resource_provider_.get(),
                                 &surface_damage_rect_list, default_color,
                                 new_quad, gfx::RectF(), &candidate);

  // Verify that a default candidate is not a required overlay.
  EXPECT_FALSE(candidate.requires_overlay);

  ASSERT_EQ(gfx::ToRoundedRect(candidate.display_rect), kSmallCandidateRect);
}

TEST_F(SingleOverlayOnTopTest, IsOverlayRequiredHwProtectedVideo) {
  // Add a small quad.
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(0, 0, 16, 16);
  auto* new_quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect, gfx::ProtectedVideoType::kHardwareProtected,
      YUV_420_BIPLANAR);
  SurfaceDamageRectList surface_damage_rect_list;
  SkMatrix44 default_color = GetIdentityColorMatrix();
  OverlayCandidate candidate;
  OverlayCandidate::FromDrawQuad(resource_provider_.get(),
                                 &surface_damage_rect_list, default_color,
                                 new_quad, gfx::RectF(), &candidate);

  // Verify that a HW protected video candidate requires overlay.
  EXPECT_TRUE(candidate.requires_overlay);

  ASSERT_EQ(gfx::ToRoundedRect(candidate.display_rect), kSmallCandidateRect);
}

TEST_F(SingleOverlayOnTopTest, RequiredOverlayClippingAndSubsampling) {
  // Add a small quad.
  auto pass = CreateRenderPass();
  const auto kVideoCandidateRect = gfx::Rect(-19, -20, 320, 240);
  auto* new_quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kVideoCandidateRect, gfx::ProtectedVideoType::kHardwareProtected,
      YUV_420_BIPLANAR);
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;
  SurfaceDamageRectList surface_damage_rect_list;
  SkMatrix44 default_color = GetIdentityColorMatrix();
  OverlayCandidate candidate;
  OverlayCandidate::FromDrawQuad(resource_provider_.get(),
                                 &surface_damage_rect_list, default_color,
                                 new_quad, gfx::RectF(), &candidate);

  // Default uv rect is 0.1, 0.2, 1.0, 1.0 which in the 320x240 buffer
  // corresponds to 32, 48, 288x192. That maps to |kVideoCandidateRect| in the
  // destination space. After clipping by |kOverlayClipRect| that clips the src
  // rect to be 49.1, 64, 115.2x102.4. After rounding to the nearest subsample
  // (2x), the result is 48, 64, 114x102.
  const auto kTargetSrcRect = gfx::Rect(48, 64, 114, 102);
  EXPECT_EQ(kTargetSrcRect,
            gfx::ToRoundedRect(gfx::ScaleRect(
                candidate.uv_rect, candidate.resource_size_in_pixels.width(),
                candidate.resource_size_in_pixels.height())));
  EXPECT_TRUE(candidate.requires_overlay);
  EXPECT_FALSE(candidate.is_clipped);
  EXPECT_EQ(gfx::ToRoundedRect(candidate.display_rect), kOverlayClipRect);
}

TEST_F(SingleOverlayOnTopTest,
       RequiredOverlayClippingAndSubsamplingWithPrimary) {
  // Add a small quad.
  auto pass = CreateRenderPass();
  const auto kVideoCandidateRect = gfx::Rect(-19, -20, 320, 240);
  auto* new_quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kVideoCandidateRect, gfx::ProtectedVideoType::kHardwareProtected,
      YUV_420_BIPLANAR);
  pass->shared_quad_state_list.back()->is_clipped = true;
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;
  SurfaceDamageRectList surface_damage_rect_list;
  SkMatrix44 default_color = GetIdentityColorMatrix();
  gfx::RectF primary_rect(0, 0, 100, 120);
  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane;
  OverlayCandidate candidate;
  OverlayCandidate::FromDrawQuad(resource_provider_.get(),
                                 &surface_damage_rect_list, default_color,
                                 new_quad, primary_rect, &candidate);

  // Default uv rect is 0.1, 0.2, 1.0, 1.0 which in the 320x240 buffer
  // corresponds to 32, 48, 288x192. That maps to |kVideoCandidateRect| in the
  // destination space. After clipping by |kOverlayClipRect| and the
  // |primary_rect| that clips the src rect to be 49.1, 64, 90x96. After
  // rounding to the nearest subsample (2x), the result is 48, 64, 90x96.
  const auto kTargetSrcRect = gfx::Rect(48, 64, 90, 96);
  EXPECT_EQ(kTargetSrcRect,
            gfx::ToRoundedRect(gfx::ScaleRect(
                candidate.uv_rect, candidate.resource_size_in_pixels.width(),
                candidate.resource_size_in_pixels.height())));
  EXPECT_TRUE(candidate.requires_overlay);
  EXPECT_FALSE(candidate.is_clipped);
  EXPECT_EQ(candidate.display_rect, primary_rect);
}

TEST_F(UnderlayTest, EstimateOccludedDamage) {
  // A visual depiction of how this test works.
  //   * - Candidate
  //   # - Occluder
  //
  //   The first candidate has no quad occlusions.
  ///
  //   ***********
  //   *         *
  //   *         *
  //   *         *
  //   ***********
  //
  //     The second candidate has only one quad occlusion.
  //                      ######*****************
  //                      #    #    *           *
  //                      ######    *           *
  //                      *         *           *
  //                      **********######      *
  //                      *         #    #      *
  //                      *         ######      *
  //                      *                     *
  //                      *                     *
  //                      ***********************
  // Finally the third larger candidate is occluded by both quads.
  // The |damage_area_estimate| reflects this damage occlusion when
  // 'EstimateOccludedDamage' is called

  auto pass = CreateRenderPass();
  gfx::Transform identity;
  identity.MakeIdentity();

  // These quads will server to occlude some of our test overlay candidates.
  const int kOccluderWidth = 10;
  AddQuad(gfx::Rect(100, 100, kOccluderWidth, kOccluderWidth), identity,
          pass.get());
  AddQuad(gfx::Rect(150, 150, kOccluderWidth, kOccluderWidth), identity,
          pass.get());

  const int kCandidateSmall = 50;
  const int kCandidateLarge = 100;
  const gfx::Rect kCandidateRects[] = {
      gfx::Rect(0, 0, kCandidateSmall, kCandidateSmall),
      gfx::Rect(100, 100, kCandidateSmall, kCandidateSmall),
      gfx::Rect(100, 100, kCandidateLarge, kCandidateLarge), kOverlayRect};

  const bool kCandidateUseSurfaceIndex[] = {true, true, true, false};

  const int kExpectedDamages[] = {
      kCandidateSmall * kCandidateSmall,
      kCandidateSmall * kCandidateSmall - kOccluderWidth * kOccluderWidth,
      kCandidateLarge * kCandidateLarge - kOccluderWidth * kOccluderWidth * 2,
      kCandidateLarge * kCandidateLarge * 4 -
          kOccluderWidth * kOccluderWidth * 2};

  static_assert(
      base::size(kCandidateRects) == base::size(kCandidateUseSurfaceIndex),
      "Number of elements in each list should be the identical.");
  static_assert(base::size(kCandidateRects) == base::size(kExpectedDamages),
                "Number of elements in each list should be the identical.");

  QuadList& quad_list = pass->quad_list;
  auto occluder_iter_count = quad_list.size();

  SurfaceDamageRectList surface_damage_rect_list;
  for (size_t i = 0; i < base::size(kCandidateRects); ++i) {
    // Create fake surface damage for this candidate.
    SharedQuadState* damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    // We want to test what happens when an overlay candidate does not have an
    // associated damage index. An estimate damage for this candidate will still
    // be computed but it will be derived from a union of all surface damages.
    // TODO(petermcneeley): Update this code when surface damage is made more
    // reliable.
    if (kCandidateUseSurfaceIndex[i]) {
      damaged_shared_quad_state->overlay_damage_index =
          surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kCandidateRects[i]);
    } else {
      damaged_shared_quad_state->overlay_damage_index.reset();
    }

    auto* quad_candidate = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), damaged_shared_quad_state, pass.get(),
        kCandidateRects[i]);

    SkMatrix44 default_color = GetIdentityColorMatrix();
    OverlayCandidate candidate;
    OverlayCandidate::FromDrawQuad(resource_provider_.get(),
                                   &surface_damage_rect_list, default_color,
                                   quad_candidate, gfx::RectF(), &candidate);

    // Before the 'EstimateOccludedDamage' function is called the damage area
    // will just be whatever comes from the |surface_damage_rect_list|.
    if (kCandidateUseSurfaceIndex[i]) {
      ASSERT_EQ(kCandidateRects[i].size().GetArea(),
                candidate.damage_area_estimate);
      ASSERT_EQ(candidate.overlay_damage_index, i);
    } else {
      // In the special case where we have no surface damage index the candidate
      // area will not simply be the |damage_area_estimate|.
      ASSERT_FALSE(
          quad_candidate->shared_quad_state->overlay_damage_index.has_value());
    }

    // We have to find the occluder end of our list as it changes each
    // iteration.
    auto iter_occluder_end = quad_list.begin();
    for (size_t j = 0; j < occluder_iter_count; j++) {
      iter_occluder_end++;
    }

    // Now we test the opaque occlusion provided by 'EstimateOccludedDamage'
    // function.
    candidate.damage_area_estimate = OverlayCandidate::EstimateVisibleDamage(
        quad_candidate, &surface_damage_rect_list, quad_list.begin(),
        iter_occluder_end);

    ASSERT_EQ(kExpectedDamages[i], candidate.damage_area_estimate);
  }
}

TEST_F(UnderlayTest, ProtectedVideoOverlayScaling) {
  // This test verifies the algorithm used when adjusting the scaling for
  // protected content due to HW overlay scaling limitations where we resort
  // to clipping when we need to downscale beyond the HW's limits.

  // This is only used in the overlay prioritization path.
  if (!features::IsOverlayPrioritizationEnabled())
    return;

  // 0.5 should fail, and then it'll increase it by 0.5 each try until it
  // succeeds. Have it succeed at 0.65f.
  AddScalingSequenceToOverlayProcessor(0.5f, false);
  AddScalingSequenceToOverlayProcessor(0.55f, false);
  AddScalingSequenceToOverlayProcessor(0.6f, false);
  AddScalingSequenceToOverlayProcessor(0.65f, true);
  // Then send one in at 0.625, which we let pass, that'll establish a high
  // end working bound of 0.626.
  AddScalingSequenceToOverlayProcessor(0.625f, true);
  // Send another one in at 0.5f, which fails and then it should try 0.626 since
  // it knows 0.6 and below fails and 0.625 worked (and the 0.001 is to keep the
  // bounds separated).
  AddScalingSequenceToOverlayProcessor(0.5f, false);
  AddScalingSequenceToOverlayProcessor(0.626f, true);

  float initial_scalings[] = {0.5f, 0.625f, 0.5f};
  for (auto initial_scaling : initial_scalings) {
    auto pass = CreateRenderPass();

    AggregatedRenderPassId render_pass_id{3};
    AggregatedRenderPassDrawQuad* quad =
        pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
    quad->SetNew(pass->shared_quad_state_list.back(), kOverlayRect,
                 kOverlayRect, render_pass_id, kInvalidResourceId, gfx::RectF(),
                 gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
                 false, 1.0f);

    // First, we want the overlay to be scaled by 0.5 and have it rejected.
    float res_scale = 1.0f / (initial_scaling * (1.0f - kUVTopLeft.x()));
    CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        pass->output_rect, gfx::ProtectedVideoType::kHardwareProtected,
        YUV_420_BIPLANAR, gfx::ScaleToRoundedSize(kDisplaySize, res_scale))
        ->needs_blending = false;
    pass->shared_quad_state_list.front()->opacity = 1.0;

    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));
    SurfaceDamageRectList surface_damage_rect_list;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    ASSERT_EQ(1U, candidate_list.size());
  }
}

}  // namespace
}  // namespace viz
