// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "components/viz/service/display/overlay_candidate_temporal_tracker.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#include "components/viz/service/display/overlay_proposed_candidate.h"
#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/latency/latency_info.h"

#if BUILDFLAG(IS_OZONE)
#include "components/viz/service/display/overlay_processor_delegated.h"
#include "ui/base/ui_base_features.h"
#endif

#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

using testing::_;
using testing::Mock;

namespace viz {
namespace {

using RoundedDisplayMasksInfo = TextureDrawQuad::RoundedDisplayMasksInfo;

const gfx::Size kDisplaySize(256, 256);
const gfx::Rect kOverlayRect(0, 0, 256, 256);
const gfx::Rect kOverlayTopLeftRect(0, 0, 128, 128);
const gfx::Rect kOverlayTopRightRect(128, 0, 128, 128);
const gfx::Rect kOverlayBottomRightRect(128, 128, 128, 128);
const gfx::Rect kOverlayClipRect(0, 0, 128, 128);
const gfx::PointF kUVTopLeft(0.1f, 0.2f);
const gfx::PointF kUVBottomRight(1.0f, 1.0f);
const SharedImageFormat kDefaultSIFormat = SinglePlaneFormat::kRGBA_8888;
const OverlayCandidateFactory::OverlayContext kTestOverlayContext;

class TimeTicksOverride {
 public:
  static base::TimeTicks Now() { return now_ticks_; }
  static base::TimeTicks now_ticks_;
};

// static
base::TimeTicks TimeTicksOverride::now_ticks_ = base::TimeTicks::Now();

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
  void CheckOverlaySupportImpl(const PrimaryPlane* primary_plane,
                               OverlayCandidateList* surfaces) override {}
  size_t GetStrategyCount() const { return strategies_.size(); }
};

class FullscreenOverlayProcessor : public TestOverlayProcessor {
 public:
  FullscreenOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
  }
  bool NeedsSurfaceDamageRectList() const override { return true; }
  void CheckOverlaySupportImpl(const PrimaryPlane* primary_plane,
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
  void CheckOverlaySupportImpl(const PrimaryPlane* primary_plane,
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
      SCOPED_TRACE(r.ToString());

      if (std::abs(r.x() - candidate.display_rect.x()) <= kAbsoluteError &&
          std::abs(r.y() - candidate.display_rect.y()) <= kAbsoluteError &&
          std::abs(r.width() - candidate.display_rect.width()) <=
              kAbsoluteError &&
          std::abs(r.height() - candidate.display_rect.height()) <=
              kAbsoluteError) {
        EXPECT_RECTF_EQ(BoundingRect(kUVTopLeft, kUVBottomRight),
                        candidate.uv_rect);
        if (candidate.clip_rect) {
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

class MultiOverlayProcessorBase : public TestOverlayProcessor {
 public:
  MultiOverlayProcessorBase() {
    // Don't wait for hardware support in these tests.
    max_overlays_considered_ = max_overlays_config_;
  }

  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) override {
    // Clear the combination cache every frame so results are more predictable
    // in these tests.
    ClearOverlayCombinationCache();
    // Parameters unchanged.
    OverlayProcessorUsingStrategy::ProcessForOverlays(
        resource_provider, render_passes, output_color_matrix,
        render_pass_filters, render_pass_backdrop_filters,
        surface_damage_rect_list, output_surface_plane, overlay_candidates,
        damage_rect, content_bounds);
  }

  void CheckOverlaySupportImpl(const PrimaryPlane* primary_plane,
                               OverlayCandidateList* surfaces) override {
    EXPECT_EQ(expected_rects_.size(), surfaces->size());

    for (size_t i = 0; i < surfaces->size(); ++i) {
      OverlayCandidate& candidate = (*surfaces)[i];
      EXPECT_EQ(gfx::ToEnclosedRect(candidate.display_rect),
                expected_rects_[i]);
      candidate.overlay_handled = responses_[i];
    }
  }

  // Sort required overlay candidates first, followed by candidates with
  // rounded_display masks and then just by input order.
  void SortProposedOverlayCandidates(
      std::vector<OverlayProposedCandidate>* proposed_candidates) override {
    // We want the power gains to be assigned for the OverlayCombinationCache.
    size_t order = proposed_candidates->size();
    for (auto& proposed_candidate : *proposed_candidates) {
      proposed_candidate.relative_power_gain = order--;
    }
    std::stable_sort(
        proposed_candidates->begin(), proposed_candidates->end(),
        [](const auto& a, const auto& b) {
          if (a.candidate.requires_overlay != b.candidate.requires_overlay) {
            return a.candidate.requires_overlay > b.candidate.requires_overlay;
          }

          if (a.candidate.has_rounded_display_masks !=
              b.candidate.has_rounded_display_masks) {
            return a.candidate.has_rounded_display_masks >
                   b.candidate.has_rounded_display_masks;
          }

          return a.relative_power_gain > b.relative_power_gain;
        });
  }

  void AddExpectedRect(const gfx::Rect& rect, bool response) {
    expected_rects_.push_back(rect);
    responses_.push_back(response);
  }

  void ClearExpectedRects() {
    expected_rects_.clear();
    responses_.clear();
  }

  // Overrides the maximum number of candidates that the OverlayProcessor can
  // consider each frame.
  void SetMaximumOverlaysConsidered(int overlays_considered) {
    max_overlays_considered_ = overlays_considered;
  }

 private:
  std::vector<gfx::Rect> expected_rects_;
  std::vector<bool> responses_;
};

class MultiOverlayProcessor : public MultiOverlayProcessorBase {
 public:
  MultiOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
  }
};

class MultiSingleOnTopProcessor : public MultiOverlayProcessorBase {
 public:
  MultiSingleOnTopProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  }
};

class SizeSortedMultiSingleOnTopProcessor : public MultiOverlayProcessorBase {
 public:
  SizeSortedMultiSingleOnTopProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  }

  // Sort candidates only by their display_rect area.
  void SortProposedOverlayCandidates(
      std::vector<OverlayProposedCandidate>* proposed_candidates) override {
    // We want the power gains to be assigned for the OverlayCombinationCache.
    for (auto& proposed_candidate : *proposed_candidates) {
      proposed_candidate.relative_power_gain =
          proposed_candidate.candidate.display_rect.size().GetArea();
    }
    std::sort(proposed_candidates->begin(), proposed_candidates->end(),
              [](const auto& a, const auto& b) {
                return a.relative_power_gain > b.relative_power_gain;
              });
  }

  void CheckOverlaySupportImpl(const PrimaryPlane* primary_plane,
                               OverlayCandidateList* surfaces) override {
    for (auto& candidate : *surfaces) {
      candidate.overlay_handled = true;
    }
  }
};

class MultiUnderlayProcessor : public MultiOverlayProcessorBase {
 public:
  MultiUnderlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
  }
};

class SizeSortedMultiOverlayProcessorBase : public MultiOverlayProcessorBase {
 public:
  // Sort candidates only by their display_rect area.
  void SortProposedOverlayCandidates(
      std::vector<OverlayProposedCandidate>* proposed_candidates) override {
    // We want the power gains to be assigned for the OverlayCombinationCache.
    for (auto& proposed_candidate : *proposed_candidates) {
      proposed_candidate.relative_power_gain =
          proposed_candidate.candidate.display_rect.size().GetArea();
    }
    std::sort(proposed_candidates->begin(), proposed_candidates->end(),
              [](const auto& a, const auto& b) {
                return a.relative_power_gain > b.relative_power_gain;
              });
  }
};

class SizeSortedMultiOverlayProcessor
    : public SizeSortedMultiOverlayProcessorBase {
 public:
  SizeSortedMultiOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  }
};

class SizeSortedMultiUnderlayProcessor
    : public SizeSortedMultiOverlayProcessorBase {
 public:
  SizeSortedMultiUnderlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
  }
};

class TypeAndSizeSortedMultiOverlayProcessor
    : public MultiOverlayProcessorBase {
 public:
  TypeAndSizeSortedMultiOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  }

  void SortProposedOverlayCandidates(
      std::vector<OverlayProposedCandidate>* proposed_candidates) override {
    // We want the power gains to be assigned for the OverlayCombinationCache.
    for (auto& proposed_candidate : *proposed_candidates) {
      proposed_candidate.relative_power_gain =
          proposed_candidate.candidate.display_rect.size().GetArea();
    }
    std::sort(
        proposed_candidates->begin(), proposed_candidates->end(),
        [](const auto& a, const auto& b) {
          if (a.candidate.requires_overlay || b.candidate.requires_overlay) {
            return a.candidate.requires_overlay &&
                   !b.candidate.requires_overlay;
          }

          if (a.candidate.has_rounded_display_masks ||
              b.candidate.has_rounded_display_masks) {
            return a.candidate.has_rounded_display_masks &&
                   !b.candidate.has_rounded_display_masks;
          }

          return a.relative_power_gain > b.relative_power_gain;
        });
  }
};

// This processor only allows only candidates with rounded-display masks after
// sorting them.
class AllowCandidateWithMasksSortedMultiOverlayProcessor
    : public MultiOverlayProcessorBase {
 public:
  AllowCandidateWithMasksSortedMultiOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
  }

  void SortProposedOverlayCandidates(
      std::vector<OverlayProposedCandidate>* proposed_candidates) override {
    // After sort we should only be left with candidates with rounded-display
    // masks.
    std::erase_if(*proposed_candidates, [](OverlayProposedCandidate& cand) {
      return !cand.candidate.has_rounded_display_masks;
    });

    // We want the power gains to be assigned for the OverlayCombinationCache.
    for (auto& proposed_candidate : *proposed_candidates) {
      proposed_candidate.relative_power_gain =
          proposed_candidate.candidate.display_rect.size().GetArea();
    }
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
    strategies_.push_back(std::make_unique<OverlayStrategyFullscreen>(this));
    strategies_.push_back(std::make_unique<OverlayStrategySingleOnTop>(this));
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
    prioritization_config_.changing_threshold = false;
    prioritization_config_.damage_rate_threshold = false;
    prioritization_config_.power_gain_sort = true;
  }
};

class TransparentUnderlayOverlayProcessor : public DefaultOverlayProcessor {
 public:
  TransparentUnderlayOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
        this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  }
};

#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
class UnderlayCastOverlayProcessor : public DefaultOverlayProcessor {
 public:
  UnderlayCastOverlayProcessor() : DefaultOverlayProcessor() {
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlayCast>(this));
  }
};
#endif

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
    bool is_overlay_candidate,
    SharedImageFormat format) {
  auto resource = TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D, gpu::SyncToken(), size, format,
      is_overlay_candidate);

  ResourceId resource_id =
      child_resource_provider->ImportResource(resource, base::DoNothing());

  return resource_id;
}

ResourceId CreateResource(DisplayResourceProvider* parent_resource_provider,
                          ClientResourceProvider* child_resource_provider,
                          RasterContextProvider* child_context_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate,
                          SharedImageFormat format,
                          SurfaceId test_surface_id = SurfaceId()) {
  ResourceId resource_id = CreateResourceInLayerTree(
      child_resource_provider, size, is_overlay_candidate, format);

  int child_id =
      parent_resource_provider->CreateChild(base::DoNothing(), test_surface_id);

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
                          RasterContextProvider* child_context_provider,
                          const gfx::Size& size,
                          bool is_overlay_candidate) {
  return CreateResource(parent_resource_provider, child_resource_provider,
                        child_context_provider, size, is_overlay_candidate,
                        SinglePlaneFormat::kRGBA_8888);
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

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::ProtectedVideoType protected_video_type,
    SharedImageFormat format,
    const gfx::Size& resource_size_in_pixels,
    SurfaceId test_surface_id = SurfaceId()) {
  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate, format, test_surface_id);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, needs_blending,
                       resource_id, premultiplied_alpha, kUVTopLeft,
                       kUVBottomRight, SkColors::kTransparent, flipped,
                       nearest_neighbor, /*secure_output_only=*/false,
                       protected_video_type);
  overlay_quad->set_resource_size_in_pixels(resource_size_in_pixels);

  return overlay_quad;
}

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    gfx::ProtectedVideoType protected_video_type,
    SharedImageFormat format,
    SurfaceId test_surface_id = SurfaceId()) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, rect, protected_video_type, format,
      rect.size(), test_surface_id);
}

TextureDrawQuad* CreateCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect,
    SurfaceId test_surface_id = SurfaceId()) {
  return CreateCandidateQuadAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, rect, gfx::ProtectedVideoType::kClear,
      SinglePlaneFormat::kRGBA_8888, test_surface_id);
}

#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
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
#endif

TextureDrawQuad* CreateTransparentCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect) {
  bool needs_blending = true;
  bool premultiplied_alpha = false;
  bool flipped = false;
  bool nearest_neighbor = false;
  gfx::Size resource_size_in_pixels = rect.size();
  bool is_overlay_candidate = true;
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, needs_blending,
                       resource_id, premultiplied_alpha, kUVTopLeft,
                       kUVBottomRight, SkColors::kTransparent, flipped,
                       nearest_neighbor, /*secure_output_only=*/false,
                       gfx::ProtectedVideoType::kClear);
  overlay_quad->set_resource_size_in_pixels(resource_size_in_pixels);

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
      shared_quad_state, render_pass, render_pass->output_rect);
}

TextureDrawQuad* CreateQuadWithRoundedDisplayMasksAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    bool is_overlay_candidate,
    const gfx::Rect& rect,
    const RoundedDisplayMasksInfo& rounded_display_masks_info) {
  bool needs_blending = true;
  bool premultiplied_alpha = true;
  bool flipped = false;
  bool nearest_neighbor = false;
  gfx::Size resource_size_in_pixels = rect.size();
  ResourceId resource_id = CreateResource(
      parent_resource_provider, child_resource_provider, child_context_provider,
      resource_size_in_pixels, is_overlay_candidate);

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  overlay_quad->SetNew(
      shared_quad_state, rect, rect, needs_blending, resource_id,
      premultiplied_alpha, kUVTopLeft, kUVBottomRight, SkColors::kTransparent,
      flipped, nearest_neighbor,
      /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
  overlay_quad->rounded_display_masks_info = rounded_display_masks_info;

  return overlay_quad;
}

TextureDrawQuad* CreateFullscreenQuadWithRoundedDisplayMasks(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    bool is_overlay_candidate,
    const RoundedDisplayMasksInfo& rounded_display_masks_info) {
  return CreateQuadWithRoundedDisplayMasksAt(
      parent_resource_provider, child_resource_provider, child_context_provider,
      shared_quad_state, render_pass, is_overlay_candidate,
      render_pass->output_rect, rounded_display_masks_info);
}

void CreateOpaqueQuadAt(DisplayResourceProvider* resource_provider,
                        const SharedQuadState* shared_quad_state,
                        AggregatedRenderPass* render_pass,
                        const gfx::Rect& rect) {
  auto* color_quad = render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_quad_state, rect, rect, SkColors::kRed, false);
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
    SCOPED_TRACE(i);

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

SkM44 GetIdentityColorMatrix() {
  return SkM44();
}

SkM44 GetNonIdentityColorMatrix() {
  SkM44 matrix = GetIdentityColorMatrix();
  matrix.setRC(1, 1, 0.5f);
  matrix.setRC(2, 2, 0.5f);
  return matrix;
}

template <typename OverlayProcessorType>
class OverlayTest : public testing::Test {
 protected:
  void SetUp() override {
    output_surface_ = FakeSkiaOutputSurface::Create3d();
    output_surface_->BindToClient(&output_surface_client_);

    resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();
    lock_set_for_external_use_.emplace(resource_provider_.get(),
                                       output_surface_.get());

    child_provider_ = TestContextProvider::Create();
    child_provider_->BindToCurrentSequence();
    child_resource_provider_ = std::make_unique<ClientResourceProvider>();

    overlay_processor_ = std::make_unique<OverlayProcessorType>();
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

  void AddExpectedRectToOverlayProcessor(const gfx::RectF& rect) {
    overlay_processor_->AddExpectedRect(rect);
  }

  void AddScalingSequenceToOverlayProcessor(float scaling, bool uses_overlay) {
    overlay_processor_->AddScalingSequence(scaling, uses_overlay);
  }

  std::unique_ptr<SkiaOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<DisplayResourceProviderSkia> resource_provider_;
  std::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;
  scoped_refptr<TestContextProvider> child_provider_;
  std::unique_ptr<ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<OverlayProcessorType> overlay_processor_;
  gfx::Rect damage_rect_;
  std::vector<gfx::Rect> content_bounds_;
};

template <typename OverlayProcessorType>
class UseMultipleOverlaysTest : public OverlayTest<OverlayProcessorType> {
 public:
  UseMultipleOverlaysTest() {
    // To use more than one overlay, we need to enable some features.
    const std::vector<base::test::FeatureRefAndParams> featureAndParamsList = {
        {features::kUseMultipleOverlays, {{features::kMaxOverlaysParam, "4"}}}};
    scoped_features.InitWithFeaturesAndParameters(featureAndParamsList, {});
  }

 private:
  base::test::ScopedFeatureList scoped_features;
};

using FullscreenOverlayTest = OverlayTest<FullscreenOverlayProcessor>;
using SingleOverlayOnTopTest = OverlayTest<SingleOnTopOverlayProcessor>;
using ChangeSingleOnTopTest = OverlayTest<ChangeThresholdOnTopOverlayProcessor>;
using FullThresholdTest = OverlayTest<FullThresholdUnderlayOverlayProcessor>;
using TransitionOverlayTypeTest = OverlayTest<TransitionOverlayProcessor>;

using UnderlayTest = OverlayTest<UnderlayOverlayProcessor>;
using TransparentUnderlayTest =
    OverlayTest<TransparentUnderlayOverlayProcessor>;
#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
using UnderlayCastTest = OverlayTest<UnderlayCastOverlayProcessor>;
#endif
using MultiOverlayTest = UseMultipleOverlaysTest<MultiOverlayProcessor>;
using MultiUnderlayTest = UseMultipleOverlaysTest<MultiUnderlayProcessor>;
using MultiSingleOnTopOverlayTest =
    UseMultipleOverlaysTest<MultiSingleOnTopProcessor>;
using SizeSortedMultiSingeOnTopOverlayTest =
    UseMultipleOverlaysTest<SizeSortedMultiSingleOnTopProcessor>;

using SizeSortedMultiUnderlayOverlayTest =
    UseMultipleOverlaysTest<SizeSortedMultiUnderlayProcessor>;
using SizeSortedMultiOverlayTest =
    UseMultipleOverlaysTest<SizeSortedMultiOverlayProcessor>;
using TypeAndSizeSortedMultiOverlayTest =
    UseMultipleOverlaysTest<TypeAndSizeSortedMultiOverlayProcessor>;
using AllowCandidateWithMasksSortedMultiOverlayTest =
    UseMultipleOverlaysTest<AllowCandidateWithMasksSortedMultiOverlayProcessor>;

TEST(OverlayTest, OverlaysProcessorHasStrategy) {
  auto overlay_processor = std::make_unique<TestOverlayProcessor>();
  EXPECT_GE(2U, overlay_processor->GetStrategyCount());
}

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN)

TEST_F(FullscreenOverlayTest, DRMDefaultBlackOptimization) {
  auto pass = CreateRenderPass();
  auto sub_fullscreen = pass->output_rect;
  sub_fullscreen.Inset(16);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        sub_fullscreen);

  // Add a black solid color behind it.
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(), SkColors::kBlack,
                         pass.get(), pass->output_rect);

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

  if (base::FeatureList::IsEnabled(
          features::kUseDrmBlackFullscreenOptimization)) {
    // Check that all the quads are gone.
    EXPECT_EQ(0U, main_pass->quad_list.size());
    // Check that we have only one overlay.
    EXPECT_EQ(1U, candidate_list.size());
    // Check that the candidate has replaced the primary plane.
    EXPECT_EQ(candidate_list[0].plane_z_order, 0);
  } else {
    // No fullscreen promotion is possible.
    EXPECT_EQ(0U, candidate_list.size());
  }
}

TEST_F(FullscreenOverlayTest,
       DRMDefaultBlackOptimizationWithRoundedDisplayMaskTextures) {
  auto pass = CreateRenderPass();
  auto sub_fullscreen = pass->output_rect;
  sub_fullscreen.Inset(32);

  const int display_width = kDisplaySize.width();
  const int display_height = kDisplaySize.height();
  const int radius = 16;

  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, gfx::Rect(0, 0, radius, display_height),
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
          radius, radius, /*is_horizontally_positioned=*/false));

  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true,
      gfx::Rect(display_width - radius, 0, radius, display_height),
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(
          radius, radius, /*is_horizontally_positioned=*/false));

  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        sub_fullscreen);

  // Add a black solid color behind it.
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(), SkColors::kBlack,
                         pass.get(), pass->output_rect);

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

  if (base::FeatureList::IsEnabled(
          features::kUseDrmBlackFullscreenOptimization)) {
    // Check that all the quads are gone.
    EXPECT_EQ(0U, main_pass->quad_list.size());
    // Check that we have only one overlay.
    EXPECT_EQ(1U, candidate_list.size());
    // Check that the candidate has replaced the primary plane.
    EXPECT_EQ(candidate_list[0].plane_z_order, 0);
  } else {
    // No fullscreen promotion is possible.
    EXPECT_EQ(0U, candidate_list.size());
  }
}

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

TEST_F(FullscreenOverlayTest, FailIfFullscreenQuadHasRoundedDisplayMasks) {
  gfx::Rect rect = kOverlayRect;

  auto pass = CreateRenderPass();

  // Create a full-screen quad.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, rect,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

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
  ASSERT_EQ(0U, candidate_list.size());

  // Check that the 2 quads are not gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
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
  inset_rect.Inset(gfx::Insets::VH(1, 0));
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
  if (base::FeatureList::IsEnabled(
          features::kUseDrmBlackFullscreenOptimization)) {
    ASSERT_EQ(1U, candidate_list.size());
    // Check that the quad is gone.
    EXPECT_EQ(0U, main_pass->quad_list.size());
  } else {
    ASSERT_EQ(0U, candidate_list.size());
    // Check that the quad is not gone.
    EXPECT_EQ(1U, main_pass->quad_list.size());
  }
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

TEST_F(MultiSingleOnTopOverlayTest,
       SuccessfulOverlay_OccludedByOverlayRoundedDisplayMaskCandidate) {
  auto pass = CreateRenderPass();

  // Add a quad with rounded-display masks.
  CreateFullscreenQuadWithRoundedDisplayMasks(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));
  overlay_processor_->AddExpectedRect(kOverlayRect, /*response=*/true);

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  overlay_processor_->AddExpectedRect(kOverlayRect, /*response=*/true);

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

  // Since the rounded-display mask quad is an overlay candidate, it will not
  // occlude the other potential single-on-top candidate. Both the
  // rounded-display mask quad and other single-on-top candidate quad will get
  // promoted.
  ASSERT_EQ(2U, candidate_list.size());

  // Check that the two quads are gone.
  EXPECT_EQ(1U, main_pass->quad_list.size());
}

TEST_F(MultiSingleOnTopOverlayTest, RoundedDisplayMaskCandidateNotDrawnOnTop) {
  auto pass = CreateRenderPass();

  const auto kSmallCandidateRect = gfx::Rect(16, 0, 16, 16);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kSmallCandidateRect);
  overlay_processor_->AddExpectedRect(kSmallCandidateRect, /*response=*/true);

  // Add a quad with rounded-display masks that is an overlay candidate. It will
  // not be promoted since it is not drawn on top.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, gfx::Rect(0, 0, 16, 100),
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

  const auto kMediumCandidateRect = gfx::Rect(16, 16, 32, 32);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kMediumCandidateRect);
  overlay_processor_->AddExpectedRect(kMediumCandidateRect, /*response=*/true);

  // This quad is occluded by the quad with mask candidate, so will not be
  // promoted.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        gfx::Rect(0, 16, 16, 16));

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

  // The first two candidates are promoted as they are not occluded.
  ASSERT_EQ(2U, candidate_list.size());

  // Candidate with mask candidate should be composited since it is not drawn
  // on top.
  for (const auto& candidate : candidate_list) {
    EXPECT_FALSE(candidate.has_rounded_display_masks);
  }

  EXPECT_EQ(2U, main_pass->quad_list.size());
}

using DeathMultiSingleOnTopOverlayTest = MultiSingleOnTopOverlayTest;
TEST_F(DeathMultiSingleOnTopOverlayTest,
       RoundedDisplayMaskCandidatesOverlapsEachOther) {
  auto pass = CreateRenderPass();

  // Add a quad with rounded-display masks.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kOverlayTopLeftRect,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

  // Add a quad with rounded-display masks.
  CreateFullscreenQuadWithRoundedDisplayMasks(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  EXPECT_DCHECK_DEATH(overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_));
}

TEST_F(DeathMultiSingleOnTopOverlayTest,
       RoundedDisplayMaskCandidateIsNotOverlayCandidate) {
  auto pass = CreateRenderPass();

  // Add a quad with rounded-display masks that is not an overlay candidate.
  CreateFullscreenQuadWithRoundedDisplayMasks(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/false,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  SurfaceDamageRectList surface_damage_rect_list;

  EXPECT_DCHECK_DEATH(overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_));
}

TEST_F(MultiSingleOnTopOverlayTest,
       PromoteRoundedDisplayMaskCandidateIfOccludeOtherCandidates) {
  // Given different rect sizes, the candidate will be out of draw order once
  // sorted.
  auto pass = CreateRenderPass();

  constexpr auto kCandidateRect1 = gfx::Rect(0, 0, 256, 10);

  // Add a quad with rounded-display masks that is an overlay candidate.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kCandidateRect1,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(0, 10));

  constexpr gfx::Rect kCandidateRect2 = {0, 0, 64, 64};

  // This candidate is not occluded by quad with rounded-display masks was it
  // does not intersect any of the mask rect.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kCandidateRect2);

  // This candidate occludes quad with rounded-display masks.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayTopRightRect);

  overlay_processor_->AddExpectedRect(kCandidateRect2, true);
  overlay_processor_->AddExpectedRect(kOverlayTopRightRect, true);

  // Candidates with masks are appended at the end of the surfaces in
  // `CheckOverlaySupportImpl()` in their respective draw order.
  overlay_processor_->AddExpectedRect(kCandidateRect1, true);

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

  // All the quads will get promoted.
  ASSERT_EQ(3U, candidate_list.size());
  EXPECT_EQ(0U, main_pass->quad_list.size());
}

TEST_F(
    MultiSingleOnTopOverlayTest,
    FailToPromoteRoundedDisplayMaskCandidatesIfTheyDontOccludeOtherCandidates) {
  // Given different rect sizes, the candidate will be out of draw order once
  // sorted.
  auto pass = CreateRenderPass();

  // Add a quad with rounded-display masks that is an overlay candidate.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, gfx::Rect(0, 0, 256, 10),
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(0, 10));

  // Add a quad with rounded-display masks that is an overlay candidate.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, gfx::Rect(0, 20, 256, 10),
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(0, 10));

  // This candidate does not occludes any of the quads with rounded-display
  // masks.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayTopLeftRect);
  overlay_processor_->AddExpectedRect(kOverlayTopLeftRect, true);

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

  // Only the candidate without rounded-display masks will be promoted.
  ASSERT_EQ(1U, candidate_list.size());
  // Confirm that candidates with rounded-display masks are not promoted.
  EXPECT_FALSE(candidate_list.back().has_rounded_display_masks);
  EXPECT_EQ(2U, main_pass->quad_list.size());
}

TEST_F(MultiSingleOnTopOverlayTest,
       OcclusionOptimizationForRoundedDisplayMaskCandidate) {
  auto pass = CreateRenderPass();

  // Add a quad with rounded-display masks that is an overlay candidate. This
  // will not get promoted since it does not occlude any other candidate.
  const auto* not_promoted_candidate = CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, gfx::Rect(0, 0, 100, 10),
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 10));

  constexpr gfx::Rect kCandidateWithMaskRect2 = gfx::Rect(0, 100, 100, 10);

  // Add a quad with rounded-display masks that is an overlay candidate. This
  // will get promoted since it occlude candidate at bounds `kCandidateRect2`.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kCandidateWithMaskRect2,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 10));

  constexpr gfx::Rect kCandidateRect1 = gfx::Rect(10, 0, 50, 50);

  // Even though it intersects the display_rect of quad with rounded-corners, it
  // is not occluded since it does not intersects one of the mask rects.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kCandidateRect1);

  constexpr gfx::Rect kCandidateRect2 = gfx::Rect(0, 90, 50, 50);

  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kCandidateRect2);

  overlay_processor_->AddExpectedRect(kCandidateRect1, /*response=*/true);

  overlay_processor_->AddExpectedRect(kCandidateRect2, /*response=*/true);

  // Candidates with masks are appended at the end of the surfaces in
  // `CheckOverlaySupportImpl()` in their respective draw order.
  overlay_processor_->AddExpectedRect(kCandidateWithMaskRect2,
                                      /*response=*/true);

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

  ASSERT_EQ(3U, candidate_list.size());

  // Check that the top quad is gone.
  EXPECT_EQ(1U, main_pass->quad_list.size());

  for (const auto& candidate : candidate_list) {
    EXPECT_NE(candidate.resource_id, not_promoted_candidate->resource_id());
  }
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

// It is possible (but unlikely) that the candidate tracking ids are not unique.
// This might result in some mis-prioritization but should not result in any
// significant error. Here we test that if we have two candidates with same
// tracking id the first candidate in the root is selected for overlay.
TEST_F(SingleOverlayOnTopTest, CandidateIdCollision) {
  auto pass = CreateRenderPass();
  const auto kCandidateRect = gfx::Rect(0, 0, 16, 16);
  TextureDrawQuad* quad_a = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kCandidateRect);
  AddExpectedRectToOverlayProcessor(gfx::RectF(kCandidateRect));
  ResourceId resource_a = quad_a->resource_id();

  TextureDrawQuad* quad_b = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kCandidateRect);
  AddExpectedRectToOverlayProcessor(gfx::RectF(kCandidateRect));

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

  // Code to make sure the 'unique' tracking ids are actually identical.
  OverlayCandidate candidate_a;
  auto color_mat = GetIdentityColorMatrix();
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters,
      kTestOverlayContext);
  auto ret_a = candidate_factory.FromDrawQuad(quad_a, candidate_a);
  OverlayCandidate candidate_b;
  auto ret_b = candidate_factory.FromDrawQuad(quad_b, candidate_b);
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kSuccess, ret_a);
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kSuccess, ret_b);

  // This line confirms that these two quads have the same tracking id.
  EXPECT_EQ(candidate_a.tracking_id, candidate_b.tracking_id);

  pass_list.push_back(std::move(pass));
  overlay_processor_->SetFrameSequenceNumber(1);
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, color_mat, render_pass_filters,
      render_pass_backdrop_filters, std::move(surface_damage_rect_list),
      nullptr, &candidate_list, &damage_rect_, &content_bounds_);
  ASSERT_EQ(1U, candidate_list.size());

  // Check that one quad is gone.
  EXPECT_EQ(2U, main_pass->quad_list.size());
  // Check that we have only one overlay.
  EXPECT_EQ(1U, candidate_list.size());
  // Check that the right resource id (the first one) got extracted.
  EXPECT_EQ(resource_a, candidate_list.front().resource_id);
}

// Tests to make sure that quads from different surfaces have different
// candidate tracking ids.
TEST_F(SingleOverlayOnTopTest, CandidateTrackIdUniqueSurface) {
  auto pass = CreateRenderPass();
  const auto kCandidateRect = gfx::Rect(0, 0, 16, 16);
  TextureDrawQuad* quad_a = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kCandidateRect, SurfaceId(FrameSinkId(1, 1), LocalSurfaceId()));
  TextureDrawQuad* quad_b = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kCandidateRect, SurfaceId(FrameSinkId(2, 2), LocalSurfaceId()));
  // Code to make sure the 'unique' tracking ids are actually different.
  OverlayCandidate candidate_a;
  SurfaceDamageRectList surface_damage_rect_list;
  auto color_mat = GetIdentityColorMatrix();
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters,
      kTestOverlayContext);
  auto ret_a = candidate_factory.FromDrawQuad(quad_a, candidate_a);
  OverlayCandidate candidate_b;
  auto ret_b = candidate_factory.FromDrawQuad(quad_b, candidate_b);
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kSuccess, ret_a);
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kSuccess, ret_b);

  // This line confirms that these two quads have different tracking ids.
  EXPECT_NE(candidate_a.tracking_id, candidate_b.tracking_id);
}

// This test makes sure that the prioritization choices remain stable over a
// series of many frames. The example here would be two similar sized unoccluded
// videos running at 30fps. It is possible (observed on android) for these
// frames to get staggered such that each video frame updates on alternating
// frames of the 60fps vsync. Under specific damage conditions this will lead
// prioritization to be very indecisive and flip priorities every frame. The
// root cause for this issue has been resolved.
TEST_F(SingleOverlayOnTopTest, StablePrioritizeIntervalFrame) {
  const auto kCandidateRectA = gfx::Rect(0, 0, 16, 16);
  // Add a bigger quad below the previous one, but not occluded.
  const auto kCandidateRectB = gfx::Rect(20, 20, 16, 16);
  ResourceId resource_id_a =
      CreateResource(resource_provider_.get(), child_resource_provider_.get(),
                     child_provider_.get(), kCandidateRectA.size(),
                     true /*is_overlay_candidate*/);
  ResourceId resource_id_b =
      CreateResource(resource_provider_.get(), child_resource_provider_.get(),
                     child_provider_.get(), kCandidateRectB.size(),
                     true /*is_overlay_candidate*/);
  ResourceId prev_resource = kInvalidResourceId;
  int num_overlay_swaps = 0;
  // The number of frames here is very high to simulate two videos or animations
  // playing each at 30fps.
  constexpr int kNumFrames = 300;
  for (int i = 1; i < kNumFrames; i++) {
    auto pass = CreateRenderPass();
    SharedQuadState* shared_quad_state_a =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    shared_quad_state_a->overlay_damage_index = 0;
    TextureDrawQuad* quad_small =
        pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    quad_small->SetNew(
        shared_quad_state_a, kCandidateRectA, kCandidateRectA,
        false /*needs_blending*/, resource_id_a, false /*premultiplied_alpha*/,
        kUVTopLeft, kUVBottomRight, SkColors::kTransparent, false /*flipped*/,
        false /*nearest_neighbor*/, false /*secure_output_only*/,
        gfx::ProtectedVideoType::kClear);
    quad_small->set_resource_size_in_pixels(kCandidateRectA.size());
    AddExpectedRectToOverlayProcessor(gfx::RectF(kCandidateRectA));

    SharedQuadState* shared_quad_state_b =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    TextureDrawQuad* quad_big =
        pass->CreateAndAppendDrawQuad<TextureDrawQuad>();

    quad_big->SetNew(shared_quad_state_b, kCandidateRectB, kCandidateRectB,
                     false /*needs_blending*/, resource_id_b,
                     false /*premultiplied_alpha*/, kUVTopLeft, kUVBottomRight,
                     SkColors::kTransparent, false /*flipped*/,
                     false /*nearest_neighbor*/, false /*secure_output_only*/,
                     gfx::ProtectedVideoType::kClear);
    quad_big->set_resource_size_in_pixels(kCandidateRectB.size());

    shared_quad_state_b->overlay_damage_index = 1;
    AddExpectedRectToOverlayProcessor(gfx::RectF(kCandidateRectB));

    // Add something behind it.
    SharedQuadState* default_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               default_shared_quad_state, pass.get());

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    OverlayCandidateList candidate_list;
    SurfaceDamageRectList surface_damage_rect_list;
    // Alternatively add damage to each potential overlay.
    surface_damage_rect_list.push_back((i % 2) == 0 ? kCandidateRectA
                                                    : gfx::Rect());
    surface_damage_rect_list.push_back((i % 2) == 0 ? gfx::Rect()
                                                    : kCandidateRectB);

    overlay_processor_->SetFrameSequenceNumber(i);
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    ASSERT_EQ(1U, candidate_list.size());

    if (prev_resource != candidate_list.front().resource_id) {
      if (prev_resource != kInvalidResourceId) {
        num_overlay_swaps++;
      }
      prev_resource = candidate_list.front().resource_id;
    }
  }
  // Note the value of |kMaxNumSwaps| is not simply 2 or 3 due to some possible
  // additional swaps that can occur as part of initial overlay tracking.
  constexpr int kMaxNumSwaps = 10;
  EXPECT_LE(num_overlay_swaps, kMaxNumSwaps);
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
  for (size_t i = 0; i < std::size(kDamageRect); ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    SharedQuadState* damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

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

    EXPECT_EQ(damage_rect_, kExpectedDamage[i]);
  }
}

TEST_F(SingleOverlayOnTopTest, NonOpaquePureOverlayFirstFrameDamage) {
  // This test makes sure that non-opaque pure overlays fully damage the overlay
  // rect region. This allows for a final update of the primary plane on the
  // frame of promotion. The example for where this is necessary is the laser
  // pointing stylus effect. The laser is regularly updated and is non-opaque
  // overlay. On the frame of promotion the old laser position can be cleared
  // with a new one now updated in what will be the overlay. We must also damage
  // the primary plane in this case to clear the previously composited laser
  // stylus effect.
  constexpr int kCandidateSmall = 64;
  const gfx::Rect kOverlayDisplayRect(10, 10, kCandidateSmall, kCandidateSmall);

  const gfx::Rect kExpectedDamage[] = {
      kOverlayDisplayRect,
      gfx::Rect(),
      gfx::Rect(),
  };

  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayDisplayRect));
  for (size_t i = 0; i < std::size(kExpectedDamage); ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();

    auto* quad = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
        kOverlayDisplayRect);

    // We are intentionally testing a non-opaque overlay .
    quad->needs_blending = true;

    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;

    pass_list.push_back(std::move(pass));
    damage_rect_ = gfx::Rect();
    SurfaceDamageRectList surface_damage_rect_list;
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(damage_rect_, kExpectedDamage[i]);
  }
}

TEST_F(SingleOverlayOnTopTest, NonOpaquePureOverlayNonOccludingDamage) {
  // This tests a specific damage optimization where a pure overlay (aka not an
  // underlay) which is non opaque removes the damage associated with the
  // overlay quad but occludes no other surface damage. This is in contrast to
  // opaque overlays which can occlude damage beneath them.
  constexpr int kCandidateSmall = 64;
  const gfx::Rect kOverlayDisplayRect = {10, 10, kCandidateSmall,
                                         kCandidateSmall};
  const gfx::Rect kInFrontDamage = {0, 0, 16, 16};
  const gfx::Rect kBehindOverlayDamage = {10, 10, 32, 32};

  const gfx::Rect kExpectedDamage[] = {
      kInFrontDamage, kInFrontDamage,
      // As the overlay transitions to transparent it must contribute damage.
      gfx::UnionRects(kInFrontDamage, kOverlayDisplayRect),
      // After the transition, the overlay itself doesn't contribute damage.
      gfx::UnionRects(kInFrontDamage, kBehindOverlayDamage)};

  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayDisplayRect));
  for (size_t i = 0; i < std::size(kExpectedDamage); ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    SharedQuadState* damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

    // Create surface damages corresponding to the in front damage, the overlay
    // damage, and finally the behind overlay damage.
    SurfaceDamageRectList surface_damage_rect_list;
    surface_damage_rect_list.push_back(kInFrontDamage);
    surface_damage_rect_list.push_back(kOverlayDisplayRect);
    damaged_shared_quad_state->overlay_damage_index = 1;
    surface_damage_rect_list.push_back(kBehindOverlayDamage);

    auto* quad = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), damaged_shared_quad_state, pass.get(),
        kOverlayDisplayRect);

    // After the first two frames we test non opaque overlays.
    quad->needs_blending = i >= 2;

    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;

    pass_list.push_back(std::move(pass));
    damage_rect_ = kOverlayRect;

    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    EXPECT_EQ(damage_rect_, kExpectedDamage[i]);
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

TEST_F(SingleOverlayOnTopTest, DamageWithMutipleSurfaceDamage) {
  // This test makes sure that damage is not unnecessarily expanded when there
  // is |SurfaceDamageRectList| that is outside the original damage bounds. See
  // https://crbug.com/1197609 for context.
  auto pass = CreateRenderPass();
  damage_rect_ = kOverlayTopLeftRect;
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayTopLeftRect));
  SurfaceDamageRectList surface_damage_rect_list;

  SharedQuadState* default_damaged_shared_quad_state =
      pass->shared_quad_state_list.AllocateAndCopyFrom(
          pass->shared_quad_state_list.back());

  auto* sqs = pass->shared_quad_state_list.front();
  surface_damage_rect_list.emplace_back(damage_rect_);
  // This line adds the unnecessarily damage.
  surface_damage_rect_list.emplace_back(kOverlayRect);
  sqs->overlay_damage_index = 0;

  auto* quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), sqs, pass.get(), kOverlayTopLeftRect);
  quad->needs_blending = false;
  // Add something behind it.
  CreateFullscreenOpaqueQuad(resource_provider_.get(),
                             default_damaged_shared_quad_state, pass.get());

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
  quad->background_color = SkColors::kRed;

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
  quad->background_color = SkColors::kBlack;

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

// MaskFilters are only supported by underlay strategy.
TEST_F(UnderlayTest, AcceptBlackBackgroundColorWithRoundedCorners) {
  auto pass = CreateRenderPass();

  auto* sqs = pass->shared_quad_state_list.back();
  sqs->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RectF(kOverlayRect), gfx::RoundedCornersF(5.0f),
                          gfx::LinearGradient::GetEmpty());

  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), sqs, pass.get());
  quad->background_color = SkColors::kBlack;

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

// MaskFilters are only supported by underlay strategy.
TEST_F(UnderlayTest, RejectBlackBackgroundColorWithGradient) {
  auto pass = CreateRenderPass();

  gfx::LinearGradient gradient;
  gradient.AddStep(0.5, 9);

  auto* sqs = pass->shared_quad_state_list.back();
  sqs->mask_filter_info = gfx::MaskFilterInfo(gfx::RectF(kOverlayRect),
                                              gfx::RoundedCornersF(), gradient);

  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), sqs, pass.get());
  quad->background_color = SkColors::kBlack;

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

TEST_F(SingleOverlayOnTopTest, RejectBlackBackgroundColorWithBlending) {
  auto pass = CreateRenderPass();
  TextureDrawQuad* quad = CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());
  quad->background_color = SkColors::kBlack;
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

TEST_F(UnderlayTest, ReplacementQuad) {
  auto pass = CreateRenderPass();
  CreateFullscreenCandidateQuad(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get());

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
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(1U, pass_list.front()->quad_list.size());
  EXPECT_EQ(SkColors::kTransparent, static_cast<SolidColorDrawQuad*>(
                                        pass_list.front()->quad_list.front())
                                        ->color);
  EXPECT_FALSE(pass_list.front()->quad_list.front()->ShouldDrawWithBlending());
  EXPECT_FALSE(pass_list.front()
                   ->quad_list.front()
                   ->shared_quad_state->are_contents_opaque);
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
            absl::get<gfx::OverlayTransform>(candidate_list.back().transform));
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
            absl::get<gfx::OverlayTransform>(candidate_list.back().transform));
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
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
            absl::get<gfx::OverlayTransform>(candidate_list.back().transform));
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
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
            absl::get<gfx::OverlayTransform>(candidate_list.back().transform));
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
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
            absl::get<gfx::OverlayTransform>(candidate_list.back().transform));
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

TEST_F(UnderlayTest, DisallowsQuadsWithRoundedDisplayMasks) {
  gfx::Rect rect = kOverlayRect;

  auto pass = CreateRenderPass();
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, rect,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

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
      MultiPlaneFormat::kNV12)
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
  CreateSolidColorQuadAt(shared_state, SkColors::kBlack, pass.get(),
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
                         SkColors::kTransparent, pass.get(),
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
  CreateSolidColorQuadAt(shared_state, SkColors::kBlack, pass.get(),
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
  CreateSolidColorQuadAt(shared_state, SkColors::kTransparent, pass.get(),
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

// Test makes sure promotion hint (|overlay_priority_hint| in |TextureDrawQuad|)
// feature functions. The (current) expectation is that |kLow| will not promote
// and that |kRequired| will be promoted in preference to |kRegular| candidates.
TEST_F(SingleOverlayOnTopTest, CheckPromotionHintBasic) {
  // Test has two passes:
  // Pass 1 checks kLow and kRegular values.
  constexpr size_t kTestRegularAtFrame = 3;
  // Pass 2 checks kRequired against kRegular values.
  constexpr size_t kTestRequiredAtFrame = 6;
  AddExpectedRectToOverlayProcessor(gfx::RectF(kOverlayTopLeftRect));
  for (size_t i = 0; i <= kTestRequiredAtFrame; ++i) {
    auto pass = CreateRenderPass();
    SurfaceDamageRectList surface_damage_rect_list;
    SharedQuadState* sqs_partial =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    auto* tex_quad_partial = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), sqs_partial, pass.get(), kOverlayTopLeftRect);
    auto inset_rect_cpy = kOverlayTopLeftRect;
    // This 'Inset' makes sure the damage is a partial fraction of the
    // |display_rect|.
    inset_rect_cpy.Inset(8);

    sqs_partial->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.push_back(inset_rect_cpy);
    tex_quad_partial->overlay_priority_hint = i > kTestRegularAtFrame
                                                  ? OverlayPriority::kRequired
                                                  : OverlayPriority::kRegular;

    // Full damaged quad with a different rect; specifically
    // |kOverlayBottomRightRect|.
    SharedQuadState* sqs_full =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());
    auto* tex_quad_full = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), sqs_full, pass.get(), kOverlayBottomRightRect);
    sqs_partial->overlay_damage_index = surface_damage_rect_list.size();
    tex_quad_full->overlay_priority_hint = i > kTestRegularAtFrame
                                               ? OverlayPriority::kRegular
                                               : OverlayPriority::kLow;
    // Damage is 100% of |display_rect|.
    surface_damage_rect_list.push_back(kOverlayBottomRightRect);

    // Add something behind it.
    CreateFullscreenOpaqueQuad(resource_provider_.get(),
                               pass->shared_quad_state_list.back(), pass.get());

    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    overlay_processor_->SetFrameSequenceNumber(static_cast<int64_t>(i));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    if (i == kTestRegularAtFrame) {
      EXPECT_EQ(1U, candidate_list.size());
      if (!candidate_list.empty()) {
        // Check that it was the partial damaged one that got promoted.
        EXPECT_EQ(kOverlayTopLeftRect,
                  gfx::ToRoundedRect(candidate_list.back().display_rect));
      }
    } else if (i == kTestRequiredAtFrame) {
      EXPECT_EQ(1U, candidate_list.size());
      if (!candidate_list.empty()) {
        // Check that it was the partial damaged one that got promoted.
        EXPECT_EQ(kOverlayTopLeftRect,
                  gfx::ToRoundedRect(candidate_list.back().display_rect));
        // Also check that the required flag is set.
        EXPECT_TRUE(candidate_list.back().requires_overlay);
      }
    }
  }
}

TEST_F(ChangeSingleOnTopTest, DoNotPromoteIfContentsDontChange) {
  // Overlay demotion for unchanging overlays is frame counter based because of
  // overlay prioritization.
  size_t kFramesSkippedBeforeNotPromoting =
      overlay_processor_->TrackerConfigAccessor().max_num_frames_avg;

  ResourceId previous_resource_id;
  int64_t frame_counter = 0;
  for (size_t i = 0; i < 3 + kFramesSkippedBeforeNotPromoting; ++i) {
    SCOPED_TRACE(i);

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
    } else {
      // Starting the 3rd frame, they should have the same resource ID.
      resource_id = previous_resource_id;
    }

    // Create a quad with the resource ID selected above.
    TextureDrawQuad* original_quad =
        main_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    original_quad->SetNew(
        pass->shared_quad_state_list.back(), pass->output_rect,
        pass->output_rect, false /*needs_blending*/, resource_id,
        false /*premultiplied_alpha*/, kUVTopLeft, kUVBottomRight,
        SkColors::kTransparent, false /*flipped*/, false /*nearest_neighbor*/,
        false /*secure_output_only*/, gfx::ProtectedVideoType::kClear);
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
  int64_t frame_counter = 0;
  // This is a helper function to simulate framerates.

  // Damage rate here should be 1.0f
  auto wait_1_frame = [&]() { frame_counter++; };

  // Damage rate here should be 0.25 which should be less than
  // |damage_rate_threshold| for this test to work
  auto wait_4_frames = [&]() { frame_counter += 4; };

  // This test uses many iterations to test prioritization threshold features
  // due to frame averaging over samples.
  OverlayCandidateTemporalTracker::Config config;

  // Computes the number of frames to clear out the existing temporal value
  // using exponential smoothing. The computation looks like
  // clear_threashold=1.0*(exp_rate)^num_frames_to_clear. In our case
  // clear_threashold=1/num_frames_to_clear and
  // exp_rate=(num_frames_to_clear-1)/num_frames_to_clear. We take the log to
  // find num_frames_to_clear
  const int kNumFramesClear =
      static_cast<int>(std::ceil(std::log(1.0f / config.max_num_frames_avg) /
                                 std::log((config.max_num_frames_avg - 1.0f) /
                                          config.max_num_frames_avg)));

  size_t kDamageFrameTestStart = kNumFramesClear;
  size_t kDamageFrameTestEnd = kDamageFrameTestStart + kNumFramesClear;
  size_t kSlowFrameTestStart = kDamageFrameTestEnd + kNumFramesClear;
  size_t kSlowFrameTestEnd = kSlowFrameTestStart + kNumFramesClear;

  // This quad is used to occlude the damage of the overlay candidate to the
  // point that the damage is no longer considered significant.
  auto nearly_occluding_quad = kOverlayRect;
  nearly_occluding_quad.Inset(1);

  for (size_t i = 0; i < kSlowFrameTestEnd; ++i) {
    SCOPED_TRACE(i);

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
        pass->shared_quad_state_list.back(), SkColors::kBlack, pass.get(),
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
  EXPECT_EQ(SkColors::kTransparent, quad->color);
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
  EXPECT_EQ(SkColors::kTransparent, quad->color);
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
    SCOPED_TRACE(i);

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
      CreateSolidColorQuadAt(pass->shared_quad_state_list.back(),
                             SkColors::kBlack, pass.get(), kOverlayTopLeftRect);
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

    ASSERT_EQ(candidate_list.size(), 1U);

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

TEST_F(TransitionOverlayTypeTest, DamageWhenOverlayBecomesTransparent) {
  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  // constexpr gfx::Rect kOccludesTopLeft(64, 64, 128, 128);

  static const int kTransparentFrameStart = 3;
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kTopLeft;

    SurfaceDamageRectList surface_damage_rect_list;

    if (i < kTransparentFrameStart) {
      // Create opaque candidate in top left.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopLeft);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopLeft);
    } else {
      // Create transparent candidate in top left.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopLeft);
      CreateTransparentCandidateQuadAt(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), sqs, pass.get(), kTopLeft);
    }
    overlay_processor_->AddExpectedRect(gfx::RectF(kTopLeft));

    {
      // Add something behind it.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
    }

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

    ASSERT_EQ(candidate_list.size(), 1U);

    if (i < kTransparentFrameStart) {
      // A pure overlay does not produce damage on promotion and all associated
      // damage with this quad is excluded.
      EXPECT_EQ(candidate_list[0].plane_z_order, 1);
      EXPECT_TRUE(damage_rect_.IsEmpty());
    } else if (i == kTransparentFrameStart) {
      // When an opaque overlay becomes transparent it must contribute damage to
      // update any damage we may have occluded while it was opaque.
      EXPECT_EQ(candidate_list[0].plane_z_order, 1);
      EXPECT_EQ(damage_rect_, kTopLeft);
    } else if (i > kTransparentFrameStart) {
      // After the overlay is transparent it doesn't need to contribute damage.
      EXPECT_EQ(candidate_list[0].plane_z_order, 1);
      EXPECT_TRUE(damage_rect_.IsEmpty());
    }
  }
}

// A candidate with a mask filter must go to underlay, and not single on top.
// Also, the quad must be replaced by a black quad with |SkBlendMode::kDstOut|.
TEST_F(TransitionOverlayTypeTest, MaskFilterBringsUnderlay) {
  auto pass = CreateRenderPass();
  damage_rect_ = kOverlayRect;

  SharedQuadState* default_damaged_shared_quad_state =
      pass->shared_quad_state_list.AllocateAndCopyFrom(
          pass->shared_quad_state_list.back());

  SurfaceDamageRectList surface_damage_rect_list;
  auto* sqs = pass->shared_quad_state_list.front();
  sqs->overlay_damage_index = 0;
  surface_damage_rect_list.emplace_back(damage_rect_);
  sqs->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RectF(kOverlayRect), gfx::RoundedCornersF(1.f),
                          gfx::LinearGradient::GetEmpty());
  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                child_resource_provider_.get(),
                                child_provider_.get(), sqs, pass.get());
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

  ASSERT_EQ(candidate_list.size(), 1U);
  EXPECT_LT(candidate_list.front().plane_z_order, 0);

  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(2U, pass_list.front()->quad_list.size());
  EXPECT_EQ(SkColors::kBlack, static_cast<SolidColorDrawQuad*>(
                                  pass_list.front()->quad_list.front())
                                  ->color);
  EXPECT_FALSE(pass_list.front()
                   ->quad_list.front()
                   ->shared_quad_state->are_contents_opaque);
  EXPECT_EQ(
      SkBlendMode::kDstOut,
      pass_list.front()->quad_list.front()->shared_quad_state->blend_mode);
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
    SCOPED_TRACE(i);

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
    SCOPED_TRACE(curr_rect.ToString());

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
    SCOPED_TRACE(i);

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

// Underlay damage cannot be excluded if the underlay has a mask filter in the
// current frame but did not in the previous frame or vice versa.
TEST_F(
    UnderlayTest,
    DamageNotExcludedForConsecutiveUnderlaysIfOneHasMaskFilterAndOtherDoesNot) {
  constexpr bool kHasMaskFilter[] = {true, false, true,  false, true,
                                     true, true,  false, false, false};

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kOverlayRect;

    SharedQuadState* default_damaged_shared_quad_state =
        pass->shared_quad_state_list.AllocateAndCopyFrom(
            pass->shared_quad_state_list.back());

    SurfaceDamageRectList surface_damage_rect_list;
    auto* sqs = pass->shared_quad_state_list.front();
    sqs->overlay_damage_index = 0;
    surface_damage_rect_list.emplace_back(damage_rect_);
    if (kHasMaskFilter[i]) {
      sqs->mask_filter_info = gfx::MaskFilterInfo(
          gfx::RectF(kOverlayRect), gfx::RoundedCornersF(1.f),
          gfx::LinearGradient::GetEmpty());
    }
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kOverlayRect);
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
                kHasMaskFilter[i] == kHasMaskFilter[i - 1]);
    }
  }
}

TEST_F(UnderlayTest, DamageExcludedForCandidateAndThoseOccluded) {
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);

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
                     output_rect, SkColors::kWhite);

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
      kDisplaySize, kDisplaySize, kDefaultSIFormat, gfx::ColorSpace(),
      false /* has_alpha */, 1.0f /* opacity */, gpu::Mailbox());
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

  constexpr bool kIsFullscreen = false;
  // Test the default configuration.
  OverlayCandidateTemporalTracker::Config config;
  float kDamageEpsilon = 1.0f / config.max_num_frames_avg;
  float kBelowLowDamage = config.damage_rate_threshold - kDamageEpsilon;
  float kAboveHighDamage = config.damage_rate_threshold + kDamageEpsilon;
  float kFullDamage = 1.0f;
  // This is a helper function to simulate framerates.
  auto wait_1_frame = [&]() { frame_counter++; };

  auto wait_inactive_frames = [&]() {
    frame_counter += config.max_num_frames_avg + 1;
  };

  // Computes the number of frames to clear out the existing temporal value
  // using exponential smoothing. The computation looks like
  // clear_threashold=1.0*(exp_rate)^num_frames_to_clear. In our case
  // clear_threashold=1/num_frames_to_clear and
  // exp_rate=(num_frames_to_clear-1)/num_frames_to_clear. We take the log to
  // find num_frames_to_clear
  const int kNumFramesClear =
      static_cast<int>(std::ceil(std::log(1.0f / config.max_num_frames_avg) /
                                 std::log((config.max_num_frames_avg - 1.0f) /
                                          config.max_num_frames_avg)));

  OverlayCandidateTemporalTracker tracker;
  int fake_display_area = 256 * 256;
  // We test internal hysteresis state by running this test twice.
  for (int j = 0; j < 2; j++) {
    SCOPED_TRACE(j);
    tracker.Reset();
    // First setup a 60fps high damage candidate.
    for (int i = 0; i < kNumFramesClear; i++) {
      wait_1_frame();
      tracker.AddRecord(frame_counter, kFullDamage,
                        id_generator.GenerateNextId(), config);
    }

    EXPECT_TRUE(tracker.IsActivelyChanging(frame_counter, config));
    auto opaque_power_gain_60_full = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);

    EXPECT_NEAR(tracker.MeanFrameRatioRate(config), 1.0f, kDamageEpsilon);
    EXPECT_GT(opaque_power_gain_60_full, 0);

    // Test our hysteresis categorization of power by ensuring a single frame
    // drop does not change the end power categorization.
    wait_1_frame();
    wait_1_frame();
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    auto opaque_power_gain_60_stutter = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);

    // A single frame drop even at 60fps should not change our power
    // categorization.
    EXPECT_EQ(opaque_power_gain_60_full, opaque_power_gain_60_stutter);

    wait_inactive_frames();
    EXPECT_FALSE(tracker.IsActivelyChanging(frame_counter, config));
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    auto opaque_power_gain_60_inactive = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);
    // Simple test to make sure that power categorization is not completely
    // invalidated when candidate becomes inactive.
    EXPECT_GT(opaque_power_gain_60_inactive, 0);

    // Now simulate a overlay candidate with 30fps full damage.
    for (int i = 0; i < kNumFramesClear; i++) {
      wait_1_frame();
      wait_1_frame();
      tracker.AddRecord(frame_counter, kFullDamage,
                        id_generator.GenerateNextId(), config);
    }

    // Insert single stutter frame here to avoid hysteresis boundary.
    wait_1_frame();
    wait_1_frame();
    wait_1_frame();
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    for (int i = 0; i < kNumFramesClear; i++) {
      wait_1_frame();
      wait_1_frame();
      tracker.AddRecord(frame_counter, kFullDamage,
                        id_generator.GenerateNextId(), config);
    }

    auto opaque_power_gain_30_full = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);

    EXPECT_NEAR(tracker.MeanFrameRatioRate(config), 0.5f, kDamageEpsilon);
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
    auto opaque_power_gain_30_stutter = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);

    EXPECT_EQ(opaque_power_gain_30_stutter, opaque_power_gain_30_full);

    wait_inactive_frames();
    EXPECT_FALSE(tracker.IsActivelyChanging(frame_counter, config));
    tracker.AddRecord(frame_counter, kFullDamage, id_generator.GenerateNextId(),
                      config);

    auto opaque_power_gain_30_inactive = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);
    // Simple test to make sure that power categorization is not completely
    // invalidated when candidate becomes inactive.
    EXPECT_GT(opaque_power_gain_30_inactive, 0);

    tracker.Reset();
    // Test low and high damage thresholds.
    for (int i = 0; i < kNumFramesClear; i++) {
      wait_1_frame();
      tracker.AddRecord(frame_counter, kAboveHighDamage,
                        id_generator.GenerateNextId(), config);
    }

    auto opaque_power_gain_high_damage = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);

    EXPECT_GT(opaque_power_gain_high_damage, 0);
    EXPECT_GE(opaque_power_gain_60_full, opaque_power_gain_high_damage);

    for (int i = 0; i < kNumFramesClear; i++) {
      wait_1_frame();
      tracker.AddRecord(frame_counter, kBelowLowDamage,
                        id_generator.GenerateNextId(), config);
    }

    auto opaque_power_gain_low_damage = tracker.GetModeledPowerGain(
        frame_counter, config, fake_display_area, kIsFullscreen);
    EXPECT_LT(opaque_power_gain_low_damage, 0);

    // Test our mean damage ratio computations for our tracker.
    int avg_range_tracker = config.max_num_frames_avg - 1;
    float expected_mean = 0.0f;
    tracker.Reset();
    for (int i = 0; i < avg_range_tracker; i++) {
      wait_1_frame();
      float dynamic_damage_ratio = static_cast<float>(i) / avg_range_tracker;
      expected_mean += dynamic_damage_ratio;
      tracker.AddRecord(frame_counter, dynamic_damage_ratio,
                        id_generator.GenerateNextId(), config);
    }

    expected_mean = expected_mean / avg_range_tracker;

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
  tracker.Reset();
  static const float kDamageRatio = 0.7f;
  static const ResourceId kFakeConstantResourceId(13);
  for (int i = 0; i < config.max_num_frames_avg; i++) {
    wait_1_frame();
    tracker.AddRecord(frame_counter, kDamageRatio, kFakeConstantResourceId,
                      config, true);
  }
  EXPECT_FLOAT_EQ(kDamageRatio, tracker.MeanFrameRatioRate(config));

  // Now test the false case for the force update param.
  for (int i = 0; i < config.max_num_frames_avg; i++) {
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

  for (size_t i = 0; i < std::size(expected_damages); ++i) {
    SCOPED_TRACE(i);

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
    SCOPED_TRACE(i);

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

    CreateSolidColorQuadAt(default_damaged_shared_quad_state, SkColors::kBlack,
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
#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
TEST_F(UnderlayCastTest, ReplacementQuad) {
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
  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(1U, pass_list.front()->quad_list.size());
  EXPECT_EQ(SkColors::kTransparent, static_cast<SolidColorDrawQuad*>(
                                        pass_list.front()->quad_list.front())
                                        ->color);
  EXPECT_FALSE(pass_list.front()->quad_list.front()->ShouldDrawWithBlending());
  EXPECT_FALSE(pass_list.front()
                   ->quad_list.front()
                   ->shared_quad_state->are_contents_opaque);
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
                     SkColors::kBlack);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(), kTopRight,
                     SkColors::kBlack);

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
                     gfx::Rect(0, 0, 10, 10), SkColors::kWhite);

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
  overlay_rect.Inset(gfx::Insets::TLBR(0, 0, 1, 1));
  AddExpectedRectToOverlayProcessor(gfx::RectF(0.5f, 0.5f, 255, 255));

  gfx::Transform transform;
  transform.Translate(0.5f, 0.5f);

  auto pass = CreateRenderPassWithTransform(transform);
  CreateVideoHoleDrawQuadAt(pass->shared_quad_state_list.back(), pass.get(),
                            overlay_rect);
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     gfx::Rect(0, 0, 255, 255), SkColors::kWhite);

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

TEST_F(UnderlayCastTest, OverlayPromotionWithMaskFilter) {
  auto pass = CreateRenderPass();

  SurfaceDamageRectList surface_damage_rect_list;
  auto* sqs = pass->shared_quad_state_list.front();
  sqs->overlay_damage_index = 0;
  surface_damage_rect_list.emplace_back(damage_rect_);
  sqs->mask_filter_info =
      gfx::MaskFilterInfo(gfx::RectF(kOverlayRect), gfx::RoundedCornersF(1.f),
                          gfx::LinearGradient::GetEmpty());
  CreateVideoHoleDrawQuadAt(sqs, pass.get(), kOverlayRect);

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

  ASSERT_EQ(1U, content_bounds_.size());
  EXPECT_TRUE(content_bounds_.front().IsEmpty());

  ASSERT_EQ(1U, pass_list.size());
  ASSERT_EQ(1U, pass_list.front()->quad_list.size());
  EXPECT_EQ(SkColors::kBlack, static_cast<SolidColorDrawQuad*>(
                                  pass_list.front()->quad_list.front())
                                  ->color);
  EXPECT_FALSE(pass_list.front()
                   ->quad_list.front()
                   ->shared_quad_state->are_contents_opaque);
  EXPECT_EQ(
      SkBlendMode::kDstOut,
      pass_list.front()->quad_list.front()->shared_quad_state->blend_mode);
}
#endif  // BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)

#if BUILDFLAG(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
TEST_F(UnderlayCastTest, PrimaryPlaneOverlayIsAlwaysTransparent) {
  auto pass = CreateRenderPass();
  gfx::Rect output_rect = pass->output_rect;
  CreateOpaqueQuadAt(resource_provider_.get(),
                     pass->shared_quad_state_list.back(), pass.get(),
                     output_rect, SkColors::kWhite);

  OverlayCandidateList candidate_list;

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDisplaySize, kDefaultSIFormat, gfx::ColorSpace(),
      false /* has_alpha */, 1.0f /* opacity */, gpu::Mailbox());

  SurfaceDamageRectList surface_damage_rect_list;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), &output_surface_plane,
      &candidate_list, &damage_rect_, &content_bounds_);

  ASSERT_EQ(true, output_surface_plane.enable_blending);
  EXPECT_EQ(0U, content_bounds_.size());
}
#endif  // BUILDFLAG(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)

#if BUILDFLAG(IS_APPLE)
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
      /*quad_to_target_transform=*/quad_to_target_transform, quad_rect,
      /*visible_layer_rect=*/quad_rect,
      /*mask_filter_info=*/gfx::MaskFilterInfo(),
      /*clip_rect=*/std::nullopt,
      /*are contents opaque=*/true,
      /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context=*/0,
      /*layer_id=*/0u, /*fast_rounded_corner=*/false);

  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, quad_rect, quad_rect, SkColors::kBlack,
                     false /* force_anti_aliasing_off */);
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
  OverlayCandidate candidate;
  auto color_mat = GetIdentityColorMatrix();
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters,
      kTestOverlayContext);
  candidate_factory.FromDrawQuad(new_quad, candidate);

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
      MultiPlaneFormat::kNV12);
  SurfaceDamageRectList surface_damage_rect_list;
  OverlayCandidate candidate;
  auto color_mat = GetIdentityColorMatrix();
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters,
      kTestOverlayContext);
  candidate_factory.FromDrawQuad(new_quad, candidate);

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
      MultiPlaneFormat::kNV12);
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;
  SurfaceDamageRectList surface_damage_rect_list;
  OverlayCandidate candidate;
  auto color_mat = GetIdentityColorMatrix();
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters,
      kTestOverlayContext);
  candidate_factory.FromDrawQuad(new_quad, candidate);

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
  EXPECT_FALSE(candidate.clip_rect);
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
      MultiPlaneFormat::kNV12);
  pass->shared_quad_state_list.back()->clip_rect = kOverlayClipRect;
  SurfaceDamageRectList surface_damage_rect_list;
  gfx::RectF primary_rect(0, 0, 100, 120);
  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane;
  OverlayCandidate candidate;
  auto color_mat = GetIdentityColorMatrix();
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, primary_rect, &render_pass_filters, kTestOverlayContext);
  candidate_factory.FromDrawQuad(new_quad, candidate);

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
  EXPECT_FALSE(candidate.clip_rect);
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
      kOverlayRect.width() * kOverlayRect.height() -
          kOccluderWidth * kOccluderWidth * 2};

  static_assert(
      std::size(kCandidateRects) == std::size(kCandidateUseSurfaceIndex),
      "Number of elements in each list should be the identical.");
  static_assert(std::size(kCandidateRects) == std::size(kExpectedDamages),
                "Number of elements in each list should be the identical.");

  QuadList& quad_list = pass->quad_list;
  auto occluder_iter_count = quad_list.size();

  SurfaceDamageRectList surface_damage_rect_list;
  for (size_t i = 0; i < std::size(kCandidateRects); ++i) {
    SCOPED_TRACE(i);

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
    } else {
      damaged_shared_quad_state->overlay_damage_index.reset();
    }
    surface_damage_rect_list.emplace_back(kCandidateRects[i]);

    auto* quad_candidate = CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), damaged_shared_quad_state, pass.get(),
        kCandidateRects[i]);

    OverlayCandidate candidate;
    auto color_mat = GetIdentityColorMatrix();
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    auto candidate_factory = OverlayCandidateFactory(
        pass.get(), resource_provider_.get(), &surface_damage_rect_list,
        &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters,
        kTestOverlayContext);
    candidate_factory.FromDrawQuad(quad_candidate, candidate);

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

    // Now we test the opaque occlusion provided by 'EstimateOccludedDamage'
    // function.
    candidate.damage_area_estimate = candidate_factory.EstimateVisibleDamage(
        quad_candidate, candidate, quad_list.begin(),
        std::next(quad_list.begin(), occluder_iter_count));

    ASSERT_EQ(kExpectedDamages[i], candidate.damage_area_estimate);
  }
}

TEST_F(UnderlayTest, ProtectedVideoOverlayScaling) {
  // This test verifies the algorithm used when adjusting the scaling for
  // protected content due to HW overlay scaling limitations where we resort
  // to clipping when we need to downscale beyond the HW's limits.

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
    SCOPED_TRACE(initial_scaling);

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
        MultiPlaneFormat::kNV12,
        gfx::ScaleToRoundedSize(kDisplaySize, res_scale))
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

TEST_F(UnderlayTest, DisableOverlayWithRootCopies) {
  auto root_pass = CreateRenderPass();

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList render_pass_list;
  SurfaceDamageRectList surface_damage_rect_list;

  render_pass_list.push_back(std::move(root_pass));
  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDisplaySize, kDefaultSIFormat, gfx::ColorSpace(),
      false /* has_alpha */, 1.0f /* opacity */, gpu::Mailbox());
  OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane =
      &output_surface_plane;

  // Choose 5 here for testing purpose, this value will not change
  constexpr int kDisableOverlayTestVectorSize =
      OverlayProcessorUsingStrategy::kCopyRequestSkipOverlayFrames + 5;

  std::vector<bool> copy_frames = {true, false, true};
  copy_frames.resize(kDisableOverlayTestVectorSize, false);

  // The last 3 elements in |expected_overlays| should always be true
  // for testing purpose
  std::vector<bool> expected_overlays(kDisableOverlayTestVectorSize, false);
  for (int i = kDisableOverlayTestVectorSize - 3;
       i < kDisableOverlayTestVectorSize; i++) {
    expected_overlays[i] = true;
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kTemporalSkipOverlaysWithRootCopyOutputRequests);

  for (size_t i = 0; i < copy_frames.size(); ++i) {
    SCOPED_TRACE(i);

    render_pass_list[0]->copy_requests.clear();
    render_pass_list[0]->quad_list.clear();

    CreateOpaqueQuadAt(resource_provider_.get(),
                       render_pass_list[0]->shared_quad_state_list.back(),
                       render_pass_list[0].get(),
                       render_pass_list[0]->output_rect, SkColors::kWhite);

    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          render_pass_list[0]->shared_quad_state_list.back(),
                          render_pass_list[0].get(), kOverlayRect);

    // Add copy output request to root render pass on certain frames.
    if (copy_frames[i]) {
      render_pass_list[0]->copy_requests.push_back(
          CopyOutputRequest::CreateStubForTesting());
    }

    OverlayCandidateList candidate_list;
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &render_pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        surface_damage_rect_list, primary_plane, &candidate_list, &damage_rect_,
        &content_bounds_);

    EXPECT_EQ(expected_overlays[i], candidate_list.size() > 0);
  }
}

#if BUILDFLAG(IS_OZONE)

TileDrawQuad* CreateTileCandidateQuadAt(
    DisplayResourceProvider* parent_resource_provider,
    ClientResourceProvider* child_resource_provider,
    RasterContextProvider* child_context_provider,
    const SharedQuadState* shared_quad_state,
    AggregatedRenderPass* render_pass,
    const gfx::Rect& rect) {
  bool needs_blending = false;
  bool premultiplied_alpha = false;
  bool force_anti_aliasing_off = false;
  bool nearest_neighbor = false;
  bool is_overlay_candidate = true;
  ResourceId resource_id =
      CreateResource(parent_resource_provider, child_resource_provider,
                     child_context_provider, rect.size(), is_overlay_candidate,
                     SinglePlaneFormat::kRGBA_8888, SurfaceId());

  auto* overlay_quad = render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  overlay_quad->SetNew(shared_quad_state, rect, rect, needs_blending,
                       resource_id, gfx::RectF(0, 0, 1, 1), rect.size(),
                       premultiplied_alpha, nearest_neighbor,
                       force_anti_aliasing_off);

  return overlay_quad;
}

class TestDelegatedOverlayProcessor : public OverlayProcessorDelegated {
 public:
  using PrimaryPlane = OverlayProcessorInterface::OutputSurfaceOverlayPlane;
  TestDelegatedOverlayProcessor()
      : OverlayProcessorDelegated(nullptr, {}, nullptr) {}
  ~TestDelegatedOverlayProcessor() override = default;

  OverlayProcessorInterface::OutputSurfaceOverlayPlane*
  GetDefaultPrimaryPlane() {
    primary_plane_ = ProcessOutputSurfaceAsOverlay(
        kDisplaySize, kDisplaySize, kDefaultSIFormat, gfx::ColorSpace(),
        false /* has_alpha */, 1.0f /* opacity */, gpu::Mailbox());
    return &primary_plane_;
  }

  bool IsOverlaySupported() const override { return true; }
  bool NeedsSurfaceDamageRectList() const override { return false; }
  void CheckOverlaySupportImpl(const PrimaryPlane* primary_plane,
                               OverlayCandidateList* surfaces) override {
    for (auto&& each_candidate : *surfaces) {
      each_candidate.overlay_handled = true;
    }
  }
  size_t GetStrategyCount() const { return strategies_.size(); }
  void AddExpectedRect(const gfx::RectF& rect) {}

  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane_;
};

class DelegatedTest : public OverlayTest<TestDelegatedOverlayProcessor> {
 public:
  DelegatedTest() {
    scoped_features.InitAndEnableFeature(features::kDelegatedCompositing);
  }

 private:
  base::test::ScopedFeatureList scoped_features;
};

gfx::Transform MakePerspectiveTransform() {
  gfx::Transform transform;
  transform.ApplyPerspectiveDepth(100.f);
  transform.RotateAbout(gfx::Vector3dF(1.f, 1.f, 1.f), 30);
  return transform;
}

gfx::Transform MakeShearTransform() {
  gfx::Transform transform;
  transform.Skew(0, 30);
  return transform;
}

gfx::Transform MakeRotationTransform() {
  gfx::Transform transform;
  transform.RotateAboutZAxis(30);
  return transform;
}

TEST_F(DelegatedTest, ForwardMultipleBasic) {
  auto pass = CreateRenderPass();
  constexpr size_t kNumTestQuads = 5;
  for (size_t i = 0; i < kNumTestQuads; i++) {
    const auto kSmallCandidateRect = gfx::Rect(0, 0, 16 * (i + 1), 16);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          kSmallCandidateRect);
  }

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(kNumTestQuads, candidate_list.size());
  for (size_t i = 0; i < kNumTestQuads; i++) {
    const auto kSmallCandidateRect = gfx::RectF(0, 0, 16 * (i + 1), 16);
    EXPECT_RECTF_EQ(kSmallCandidateRect, candidate_list[i].display_rect);
  }
}

// Transparent colors are important for delegating overlays. Overlays that have
// an alpha channel but are required to be drawn as opaque will have solid black
// placed behind them. This solid black can interfer with overlay
// promotion/blend optimizations.
TEST_F(DelegatedTest, ForwardBackgroundColor) {
  auto pass = CreateRenderPass();

  auto* quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kOverlayRect);
  quad->background_color = SkColors::kTransparent;
  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_RECTF_EQ(gfx::RectF(kOverlayRect), candidate_list[0].display_rect);
  EXPECT_EQ(SkColors::kTransparent, candidate_list[0].color.value());
}

TEST_F(DelegatedTest, DoNotDelegateCopyRequest) {
  auto pass = CreateRenderPass();
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kOverlayTopLeftRect);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);
  EXPECT_EQ(0u, candidate_list.size());
}

TEST_F(DelegatedTest, BlockDelegationWithNonRootCopies) {
  auto child_pass = CreateRenderPass();
  AggregatedRenderPassId child_id{3};
  child_pass->id = child_id;
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        child_pass->shared_quad_state_list.back(),
                        child_pass.get(), kOverlayTopLeftRect);

  auto root_pass = CreateRenderPass();
  AggregatedRenderPassDrawQuad* quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(root_pass->shared_quad_state_list.back(), kOverlayRect,
               kOverlayRect, child_id, kInvalidResourceId, gfx::RectF(),
               gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(), gfx::RectF(),
               false, 1.0f);

  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(kOverlayRect);
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // The second copy frame will reset the counter. Three frames after that
  // delegation can resume.
  std::vector<bool> copy_frames = {true,  false, true, false,
                                   false, false, false};
  std::vector<size_t> expected_overlays = {0, 0, 0, 0, 0, 1, 1};
  ASSERT_EQ(copy_frames.size(), expected_overlays.size());

  for (size_t i = 0; i < copy_frames.size(); ++i) {
    SCOPED_TRACE(i);

    pass_list[0]->copy_requests.clear();
    // Add copy request to child pass on certain frames.
    if (copy_frames[i]) {
      pass_list[0]->copy_requests.push_back(
          CopyOutputRequest::CreateStubForTesting());
    }

    OverlayCandidateList candidate_list;
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        surface_damage_rect_list, overlay_processor_->GetDefaultPrimaryPlane(),
        &candidate_list, &damage_rect_, &content_bounds_);
    EXPECT_EQ(expected_overlays[i], candidate_list.size());
  }
}

TEST_F(DelegatedTest, TestClipHandCrafted) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(0, 0, 100, 100);
  const auto kTestClip = gfx::Rect(0, 50, 50, 50);
  auto* tex_rect = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  tex_rect->uv_bottom_right = gfx::PointF(1, 1);
  tex_rect->uv_top_left = gfx::PointF(0, 0);
  pass->shared_quad_state_list.back()->clip_rect = kTestClip;
  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  const auto uv_rect = gfx::RectF(0, 0.5f, 0.5f, 0.5f);
  EXPECT_EQ(1U, candidate_list.size());
  EXPECT_RECTF_NEAR(gfx::RectF(kTestClip), candidate_list[0].display_rect,
                    0.01f);
  EXPECT_RECTF_NEAR(uv_rect, candidate_list[0].uv_rect, 0.01f);
}

TEST_F(DelegatedTest, TestVisibleRectClip) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(0, 0, 100, 100);
  const auto kTestClip = gfx::Rect(0, 50, 50, 50);
  auto* tex_rect = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  tex_rect->uv_bottom_right = gfx::PointF(1, 1);
  tex_rect->uv_top_left = gfx::PointF(0, 0);
  tex_rect->visible_rect = kTestClip;
  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  const auto uv_rect = gfx::RectF(0, 0.5f, 0.5f, 0.5f);
  EXPECT_EQ(1U, candidate_list.size());
  EXPECT_RECTF_NEAR(gfx::RectF(kTestClip), candidate_list[0].display_rect,
                    0.01f);
  EXPECT_RECTF_NEAR(uv_rect, candidate_list[0].uv_rect, 0.01f);
}

TEST_F(DelegatedTest, TestClipComputed) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(5, 10, 128, 64);
  const auto kTestClip = gfx::Rect(0, 15, 70, 64);
  auto* tex_rect = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);

  pass->shared_quad_state_list.back()->clip_rect = kTestClip;
  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, candidate_list.size());
  auto expected_rect = kTestClip;
  expected_rect.Intersect(kSmallCandidateRect);
  gfx::RectF uv_rect = cc::MathUtil::ScaleRectProportional(
      BoundingRect(tex_rect->uv_top_left, tex_rect->uv_bottom_right),
      gfx::RectF(kSmallCandidateRect), gfx::RectF(expected_rect));
  EXPECT_RECTF_NEAR(gfx::RectF(expected_rect), candidate_list[0].display_rect,
                    0.01f);
  EXPECT_RECTF_NEAR(uv_rect, candidate_list[0].uv_rect, 0.01f);
}

TEST_F(DelegatedTest, TestClipAggregateRenderPass) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(5, 10, 128, 64);
  const auto kTestClip = gfx::Rect(0, 15, 70, 64);

  AggregatedRenderPassId render_pass_id{3};
  AggregatedRenderPassDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(pass->shared_quad_state_list.back(), kSmallCandidateRect,
               kSmallCandidateRect, render_pass_id, kInvalidResourceId,
               gfx::RectF(), gfx::Size(), gfx::Vector2dF(1, 1), gfx::PointF(),
               gfx::RectF(), false, 1.0f);

  pass->shared_quad_state_list.back()->clip_rect = kTestClip;
  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(1U, candidate_list.size());
  // Render pass clip rect gets applied because clip delegation is not enabled.
  EXPECT_RECTF_NEAR(gfx::RectF(5, 15, 65, 59), candidate_list[0].display_rect,
                    0.01f);
  EXPECT_FALSE(candidate_list[0].clip_rect.has_value());
}

TEST_F(DelegatedTest, TestClipWithPrimary) {
  auto pass = CreateRenderPass();
  // This is a quad with a rect that is twice is large as the primary plane and
  // will be clipped in this test.
  const auto kOversizedCandidateRect =
      gfx::Rect(kDisplaySize.height() * 2, kDisplaySize.width() * 2);
  auto* tex_rect = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kOversizedCandidateRect);
  tex_rect->uv_bottom_right = gfx::PointF(1, 1);
  tex_rect->uv_top_left = gfx::PointF(0, 0);
  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  // AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(pass->output_rect);
  pass_list.push_back(std::move(pass));
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  const auto uv_rect = gfx::RectF(0, 0.0f, 0.5f, 0.5f);
  EXPECT_EQ(1U, candidate_list.size());
  // We clip all rects to the primary display to ensure delegated and composited
  // paths have identical results.
  EXPECT_RECTF_NEAR(gfx::RectF(gfx::Rect(kDisplaySize)),
                    candidate_list[0].display_rect, 0.01f);
  EXPECT_RECTF_NEAR(uv_rect, candidate_list[0].uv_rect, 0.01f);
}

TEST_F(DelegatedTest, ScaledBufferDamage) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(15, 10, 32, 64);
  const auto kResourceSize = gfx::Size(16, 16);
  const auto kDisplayDamage = gfx::Rect(25, 0, 5, 32);

  // Specify a resource size to be much smaller than the display size of this
  // quad.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kSmallCandidateRect, gfx::ProtectedVideoType::kClear,
                        SinglePlaneFormat::kRGBA_8888, kResourceSize);

  // Here resource size and rect size on screen will match 1:1.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kSmallCandidateRect);

  // Check for potential candidates.
  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;

  AggregatedRenderPass* main_pass = pass.get();
  SurfaceDamageRectList surface_damage_rect_list;
  // Simplify by adding full root damage.
  surface_damage_rect_list.push_back(kDisplayDamage);
  pass_list.push_back(std::move(pass));
  damage_rect_ = kDisplayDamage;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  // Damage is not assigned to specific candidates.
  EXPECT_EQ(main_pass->quad_list.size(), candidate_list.size());
  EXPECT_TRUE(damage_rect_.IsEmpty());
  EXPECT_TRUE(candidate_list[0].damage_rect.IsEmpty());
  EXPECT_TRUE(candidate_list[1].damage_rect.IsEmpty());
  EXPECT_RECTF_NEAR(overlay_processor_->GetUnassignedDamage(),
                    gfx::RectF(kDisplayDamage), 0.0001f);
}

TEST_F(DelegatedTest, QuadTypes) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(5, 10, 57, 64);
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kSmallCandidateRect);
  CreateTileCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  CreateSolidColorQuadAt(pass->shared_quad_state_list.back(), SkColors::kDkGray,
                         pass.get(), kOverlayBottomRightRect);
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
  damage_rect_ = kOverlayRect;
  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list),
      overlay_processor_->GetDefaultPrimaryPlane(), &candidate_list,
      &damage_rect_, &content_bounds_);

  EXPECT_EQ(main_pass->quad_list.size(), candidate_list.size());
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(DelegatedTest, NonAxisAlignedCandidateStatus) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(5, 10, 57, 64);
  auto* quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  SurfaceDamageRectList surface_damage_rect_list;
  OverlayCandidate candidate;
  auto color_mat = GetIdentityColorMatrix();
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  auto candidate_factory = OverlayCandidateFactory(
      pass.get(), resource_provider_.get(), &surface_damage_rect_list,
      &color_mat, gfx::RectF(pass->output_rect), &render_pass_filters, context);

  pass->shared_quad_state_list.back()->quad_to_target_transform =
      MakePerspectiveTransform();
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kFailNotAxisAligned3dTransform,
            candidate_factory.FromDrawQuad(quad, candidate));

  pass->shared_quad_state_list.back()->quad_to_target_transform =
      MakeShearTransform();
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dShear,
            candidate_factory.FromDrawQuad(quad, candidate));

  pass->shared_quad_state_list.back()->quad_to_target_transform =
      MakeRotationTransform();
  EXPECT_EQ(OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dRotation,
            candidate_factory.FromDrawQuad(quad, candidate));
}

// These tests check to make sure that candidate quads that should fail (aka
// non-delegatable) do fail. These tests might need to be changed if we have
// improved delegation support.
class DelegatedTestNonDelegated : public DelegatedTest {
 public:
  void TestExpectCandidateFailure(std::unique_ptr<AggregatedRenderPass> pass) {
    // Check for potential candidates.
    OverlayCandidateList candidate_list;
    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
    AggregatedRenderPassList pass_list;
    SurfaceDamageRectList surface_damage_rect_list;
    // Simplify by adding full root damage.
    surface_damage_rect_list.push_back(pass->output_rect);
    pass_list.push_back(std::move(pass));
    damage_rect_ = kOverlayRect;
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);

    EXPECT_EQ(0U, candidate_list.size());
    EXPECT_EQ(damage_rect_, kOverlayRect);
  }
};

TEST_F(DelegatedTestNonDelegated, TextureQuadNearest) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(5, 10, 57, 64);
  auto* texture_quad = CreateCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  texture_quad->nearest_neighbor = true;
  TestExpectCandidateFailure(std::move(pass));
}

TEST_F(DelegatedTestNonDelegated, TileQuadNearest) {
  auto pass = CreateRenderPass();
  const auto kSmallCandidateRect = gfx::Rect(5, 10, 57, 64);
  auto* tile_quad = CreateTileCandidateQuadAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      kSmallCandidateRect);
  tile_quad->nearest_neighbor = true;
  TestExpectCandidateFailure(std::move(pass));
}

#endif  // BUILDFLAG(IS_OZONE)

TEST_F(MultiUnderlayTest, DamageWhenDemotingTwoUnderlays) {
  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kTopHalf(0, 0, 256, 128);

  constexpr int kDemotionFrame = 3;
  for (int i = 0; i < 5; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kTopHalf;
    SurfaceDamageRectList surface_damage_rect_list;
    overlay_processor_->ClearExpectedRects();

    // Stop promoting the candidates on this frame.
    bool promoted = i < kDemotionFrame;

    {
      // Create a candidate in the top left.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopLeft);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopLeft);
      // This candidate will always be promoted.
      overlay_processor_->AddExpectedRect(kTopLeft, promoted);
    }
    {
      // Create a candidate in the top right.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopRight);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopRight);
      // The second candidate won't get promoted after kDemotionFrame.
      overlay_processor_->AddExpectedRect(kTopRight, promoted);
    }
    {
      // Add something behind it.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
    }

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
      // The promoted underlays need to damage the primary plane on the first
      // frame of promotion.
      EXPECT_EQ(damage_rect_, kTopHalf);
    } else if (i < kDemotionFrame) {
      // No damage after they're promoted.
      EXPECT_TRUE(damage_rect_.IsEmpty());
    } else if (i >= kDemotionFrame) {
      // The demoted underlays need to damage the primary plane.
      EXPECT_EQ(damage_rect_, kTopHalf);
    }
  }
}

TEST_F(MultiUnderlayTest, DamageWhenDemotingOneUnderlay) {
  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kTopHalf(0, 0, 256, 128);

  constexpr int kDemotionFrame = 3;
  for (int i = 0; i < 5; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kTopHalf;
    SurfaceDamageRectList surface_damage_rect_list;
    overlay_processor_->ClearExpectedRects();

    {
      // Create a candidate in the top left.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopLeft);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopLeft);
      // This candidate will always be promoted.
      overlay_processor_->AddExpectedRect(kTopLeft, true);
    }
    {
      // Create a candidate in the top right.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopRight);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopRight);
      // Stop promoting this candidate on kDemotionFrame.
      bool promoted = i < kDemotionFrame;
      overlay_processor_->AddExpectedRect(kTopRight, promoted);
    }
    {
      // Add something behind it.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
    }

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
      // The promoted underlays need to damage the primary plane on the first
      // frame of promotion.
      EXPECT_EQ(damage_rect_, kTopHalf);
    } else if (i < kDemotionFrame) {
      // No damage after they're promoted.
      EXPECT_TRUE(damage_rect_.IsEmpty());
    } else if (i >= kDemotionFrame) {
      // Only the demoted underlay needs to damage the primary plane. The top
      // left candidate is still in an underlay, so it doesn't need damage the
      // primary plane.
      EXPECT_EQ(damage_rect_, kTopRight);
    }
  }
}

TEST_F(MultiOverlayTest, DamageOnlyForNewUnderlays) {
  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kMidRight(192, 64, 64, 128);
  constexpr gfx::Rect kTopHalf(0, 0, 256, 128);

  constexpr int kPromotionFrame = 2;
  for (int i = 0; i < 5; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kTopHalf;
    SurfaceDamageRectList surface_damage_rect_list;
    overlay_processor_->ClearExpectedRects();

    bool promoted = i >= kPromotionFrame;

    {
      // Create quad partially covering up top right candidate, forcing it to
      // be an underlay.
      CreateOpaqueQuadAt(resource_provider_.get(),
                         pass->shared_quad_state_list.back(), pass.get(),
                         kMidRight);
    }
    {
      // Create a candidate in the top left.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopLeft);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopLeft);
      overlay_processor_->AddExpectedRect(kTopLeft, promoted);
    }
    {
      // Create a candidate in the top right.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopRight);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopRight);
      overlay_processor_->AddExpectedRect(kTopRight, promoted);
    }
    {
      // Add something behind it.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
    }

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

    if (i < kPromotionFrame) {
      // Full damage before promotion.
      EXPECT_EQ(damage_rect_, kTopHalf);
    } else if (i == kPromotionFrame) {
      // Only the underlay needs damage on the promotion frame.
      EXPECT_EQ(damage_rect_, kTopRight);
    } else if (i > kPromotionFrame) {
      // No damage after both are promoted.
      EXPECT_TRUE(damage_rect_.IsEmpty());
    }
  }
}

TEST_F(MultiOverlayTest, DamageMaskFilterChange) {
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kBottomLeft(0, 128, 128, 128);
  constexpr gfx::Rect kMidRight(192, 64, 64, 128);
  constexpr gfx::Rect kFullRect(0, 0, 256, 256);

  constexpr int kMaskFilterStartFrame = 2;
  constexpr int kMaskFilterEndFrame = 4;
  for (int i = 0; i < 7; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kFullRect;
    SurfaceDamageRectList surface_damage_rect_list;
    overlay_processor_->ClearExpectedRects();

    bool mask_filter = i >= kMaskFilterStartFrame && i < kMaskFilterEndFrame;

    {
      // Create quad partially covering up top right candidate, forcing it to
      // be an underlay.
      CreateOpaqueQuadAt(resource_provider_.get(),
                         pass->shared_quad_state_list.back(), pass.get(),
                         kMidRight);
    }
    {
      // Create a candidate in the bottom left that won't be promoted.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      if (mask_filter) {
        sqs->mask_filter_info = gfx::MaskFilterInfo(
            gfx::RectF(kOverlayRect), gfx::RoundedCornersF(1.f),
            gfx::LinearGradient::GetEmpty());
      }
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kBottomLeft);
      CreateCandidateQuadAt(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), sqs, pass.get(), kBottomLeft);
      overlay_processor_->AddExpectedRect(kBottomLeft, false);
    }
    {
      // Create an underlay candidate in the top right.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      if (mask_filter) {
        sqs->mask_filter_info = gfx::MaskFilterInfo(
            gfx::RectF(kOverlayRect), gfx::RoundedCornersF(1.f),
            gfx::LinearGradient::GetEmpty());
      }
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopRight);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopRight);
      overlay_processor_->AddExpectedRect(kTopRight, true);
    }
    {
      // Add something behind it.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
    }

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

    ASSERT_EQ(candidate_list.size(), 1u);

    if (i == 0) {
      // Damage for both candidates
      EXPECT_EQ(damage_rect_, kFullRect);
    } else if (i < kMaskFilterStartFrame) {
      EXPECT_EQ(damage_rect_, kBottomLeft);
    } else if (i == kMaskFilterStartFrame || i == kMaskFilterEndFrame) {
      // Damage added for underlay candidate when mask filter changes.
      EXPECT_EQ(damage_rect_, kFullRect);
    } else {
      // Otherwise damage for just the unpromoted candidate.
      EXPECT_EQ(damage_rect_, kBottomLeft);
    }
  }
}

TEST_F(MultiOverlayTest, DamageOccluded) {
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kUnderTopRight(129, 1, 64, 64);
  constexpr gfx::Rect kBottomLeft(0, 128, 128, 128);
  constexpr gfx::Rect kUnderBottomLeft(1, 129, 64, 64);
  constexpr gfx::Rect kMidRight(192, 64, 64, 128);
  constexpr gfx::Rect kFullRect(0, 0, 256, 256);

  constexpr int kTopRightGone = 3;

  for (int i = 0; i < 6; ++i) {
    SCOPED_TRACE(i);

    auto pass = CreateRenderPass();
    damage_rect_ = kFullRect;
    SurfaceDamageRectList surface_damage_rect_list;
    overlay_processor_->ClearExpectedRects();

    {
      // Create quad partially covering up top right candidate, forcing it to
      // be an underlay.
      CreateOpaqueQuadAt(resource_provider_.get(),
                         pass->shared_quad_state_list.back(), pass.get(),
                         kMidRight);
    }
    {
      // Create a transparent candidate in the bottom left.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kBottomLeft);
      CreateTransparentCandidateQuadAt(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), sqs, pass.get(), kBottomLeft);
      overlay_processor_->AddExpectedRect(kBottomLeft, true);
    }
    {
      // Create unpromoted quad that would be underneath bottom left quad
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kUnderBottomLeft);
      CreateCandidateQuadAt(
          resource_provider_.get(), child_resource_provider_.get(),
          child_provider_.get(), sqs, pass.get(), kUnderBottomLeft);
      overlay_processor_->AddExpectedRect(kUnderBottomLeft, false);
    }
    if (i < kTopRightGone) {
      // Create an underlay candidate in the top right.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kTopRight);
      CreateCandidateQuadAt(resource_provider_.get(),
                            child_resource_provider_.get(),
                            child_provider_.get(), sqs, pass.get(), kTopRight);
      overlay_processor_->AddExpectedRect(kTopRight, true);
    }
    {
      // Create an opaque damaging quad under the top right candidate.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      sqs->overlay_damage_index = surface_damage_rect_list.size();
      surface_damage_rect_list.emplace_back(kUnderTopRight);
      CreateOpaqueQuadAt(resource_provider_.get(), sqs, pass.get(),
                         kUnderTopRight);
    }
    {
      // Add something behind it.
      auto* sqs = pass->CreateAndAppendSharedQuadState();
      CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
    }

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

    if (i < kTopRightGone) {
      ASSERT_EQ(candidate_list.size(), 2u);
      // Transparent overlay
      EXPECT_GT(candidate_list[0].plane_z_order, 0);
      EXPECT_FALSE(candidate_list[0].is_opaque);
      // Opaque underlay
      EXPECT_LT(candidate_list[1].plane_z_order, 0);
      EXPECT_TRUE(candidate_list[1].is_opaque);
    } else {
      ASSERT_EQ(candidate_list.size(), 1u);
      // Transparent overlay
      EXPECT_GT(candidate_list[0].plane_z_order, 0);
      EXPECT_FALSE(candidate_list[0].is_opaque);
    }

    if (i == 0) {
      // Damage for both candidates
      EXPECT_EQ(damage_rect_, kFullRect);
    } else if (i < kTopRightGone) {
      // This quad isn't occluded because it's under a transparent overlay, so
      // its damage persists.
      EXPECT_EQ(damage_rect_, kUnderBottomLeft);
    } else if (i == kTopRightGone) {
      // The top right candidate is demoted, so
      // damage = kUnderBottomLeft union kTopRight
      EXPECT_EQ(damage_rect_, gfx::Rect(1, 0, 255, 193));
    } else if (i >= kTopRightGone) {
      // The quad under top right is now visible, so
      // damage = kUnderBottomLeft union kUnderTopRight
      EXPECT_EQ(damage_rect_, gfx::Rect(1, 1, 192, 192));
    }
  }
}

TEST_F(MultiOverlayTest, FullscreenOnly) {
  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kFullRect(0, 0, 256, 256);

  auto pass = CreateRenderPass();
  damage_rect_ = kFullRect;
  SurfaceDamageRectList surface_damage_rect_list;

  {
    // Create a fullscreen candidate.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kFullRect);
    CreateFullscreenCandidateQuad(resource_provider_.get(),
                                  child_resource_provider_.get(),
                                  child_provider_.get(), sqs, pass.get());
    overlay_processor_->AddExpectedRect(kFullRect, true);
  }
  {
    // Create a candidate in the top left.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kTopLeft);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kTopLeft);
  }
  {
    // Create a candidate in the top right.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kTopRight);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kTopRight);
  }
  {
    // Add something behind it.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

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

  ASSERT_EQ(candidate_list.size(), 1u);
  // Fullscreen overlay
  EXPECT_EQ(candidate_list[0].plane_z_order, 0);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kFullRect);
  // No damage required for a fullscreen overlay.
  EXPECT_TRUE(damage_rect_.IsEmpty());
}

TEST_F(MultiOverlayTest, RequiredOverlayOnly) {
  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kTopHalf(0, 0, 256, 128);
  constexpr gfx::Rect kBottomLeft(0, 128, 128, 128);
  constexpr gfx::Rect kFullRect(0, 0, 256, 256);

  auto pass = CreateRenderPass();
  damage_rect_ = kFullRect;
  SurfaceDamageRectList surface_damage_rect_list;

  {
    // Create a candidate in the top left.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kTopLeft);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kTopLeft);
  }
  {
    // Create a candidate in the top right.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kTopRight);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kTopRight);
  }
  {
    // Create an overlay required candidate.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kBottomLeft);
    CreateCandidateQuadAt(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), sqs, pass.get(), kBottomLeft,
        gfx::ProtectedVideoType::kHardwareProtected, MultiPlaneFormat::kNV12);
    overlay_processor_->AddExpectedRect(kBottomLeft, true);
  }
  {
    // Add something behind it.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

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

  ASSERT_EQ(candidate_list.size(), 1u);
  // Only the required overlay is promoted.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kBottomLeft);
  EXPECT_TRUE(candidate_list[0].requires_overlay);
  // The two unpromoted candidates still have damage.
  EXPECT_EQ(damage_rect_, kTopHalf);
}

TEST_F(MultiOverlayTest, CappedAtMaxOverlays) {
  constexpr gfx::Rect kCandRects[]{{0, 0, 64, 64},   {64, 0, 64, 64},
                                   {0, 64, 64, 64},  {64, 64, 64, 64},
                                   {0, 128, 64, 64}, {64, 128, 64, 64}};
  constexpr gfx::Rect kBottomTwo(0, 128, 128, 64);

  damage_rect_ = gfx::Rect(0, 0, 128, 192);
  auto pass = CreateRenderPass();
  SurfaceDamageRectList surface_damage_rect_list;

  constexpr int kMaxOverlays = 4;

  for (int i = 0; i < 6; ++i) {
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kCandRects[i]);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kCandRects[i]);
    // Only the first 4 overlays should be attempted.
    if (i < kMaxOverlays) {
      overlay_processor_->AddExpectedRect(kCandRects[i], true);
    }
  }

  {
    // Add something behind it.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

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

  ASSERT_EQ(candidate_list.size(), 4u);
  // Expect the first four candidates promoted to on top overlays.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kCandRects[0]);
  EXPECT_EQ(candidate_list[0].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[1].display_rect), kCandRects[1]);
  EXPECT_EQ(candidate_list[1].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[2].display_rect), kCandRects[2]);
  EXPECT_EQ(candidate_list[2].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[3].display_rect), kCandRects[3]);
  EXPECT_EQ(candidate_list[3].plane_z_order, 1);
  // Only the bottom two candidates still have damage.
  EXPECT_EQ(damage_rect_, kBottomTwo);
}

TEST_F(MultiOverlayTest, RoundedDisplayMaskCandidateFailsToPromote) {
  overlay_processor_->SetMaximumOverlaysConsidered(6);

  const gfx::Rect kRoundedDisplayMaskLeftRect(0, 0, 32, 100);
  const gfx::Rect kRoundedDisplayMaskRightRect(224, 0, 32, 100);
  const gfx::Rect kNormalLeft(kOverlayTopLeftRect);
  const gfx::Rect kNormalRight(kOverlayTopRightRect);

  auto pass = CreateRenderPass();
  SurfaceDamageRectList surface_damage_rect_list;
  const auto kRoundedDisplayMaskInfo =
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(16, 16);

  // Create a candidate with rounded display masks in top left.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kRoundedDisplayMaskLeftRect,
      kRoundedDisplayMaskInfo);

  // Create a candidate with rounded display masks in top right.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kRoundedDisplayMaskRightRect,
      kRoundedDisplayMaskInfo);

  // This candidate is occluded by top left candidate with rounded display
  // masks.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kNormalLeft);

  // This candidate is occluded by top right candidate with rounded display
  // masks.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kNormalRight);

  // Create a quad that will the next quad to become an underlay.
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                child_resource_provider_.get(),
                                child_provider_.get(), sqs, pass.get());

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  overlay_processor_->AddExpectedRect(kNormalLeft, true);
  overlay_processor_->AddExpectedRect(kNormalRight, true);
  overlay_processor_->AddExpectedRect(kOverlayRect, true);

  // Candidates with masks are appended at the end of the surfaces in
  // `CheckOverlaySupportImpl()`
  overlay_processor_->AddExpectedRect(kRoundedDisplayMaskLeftRect, true);
  overlay_processor_->AddExpectedRect(kRoundedDisplayMaskRightRect, false);

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  // Since the mask candidate in top right will fail to promote, we will also
  // composite the SingleOnTop candidate occlude by this failing candidate. The
  // underlay candidate will promoted normally since it is not effected by the
  // failing mask candidate (even though it is occluded).
  ASSERT_EQ(candidate_list.size(), 3u);

  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kNormalLeft);
  EXPECT_EQ(candidate_list[0].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[1].display_rect), kOverlayRect);
  EXPECT_EQ(candidate_list[1].plane_z_order, -1);

  // Candidates with masks are appended at the end of the `candidate_list` in
  // draw order.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[2].display_rect),
            kRoundedDisplayMaskLeftRect);
  EXPECT_EQ(candidate_list[2].plane_z_order, 2);
}

TEST_F(MultiOverlayTest,
       DontPromoteCandidatesWithMasksIfAreOnlyOverlayCandidatesNotRejected) {
  overlay_processor_->SetMaximumOverlaysConsidered(6);

  const gfx::Rect kRoundedDisplayMaskLeftRect(0, 0, 32, 100);
  const gfx::Rect kRoundedDisplayMaskRightRect(224, 0, 32, 100);
  const gfx::Rect kNormalLeft(kOverlayTopLeftRect);
  const gfx::Rect kNormalRight(kOverlayTopRightRect);

  auto pass = CreateRenderPass();
  SurfaceDamageRectList surface_damage_rect_list;
  const auto kRoundedDisplayMaskInfo =
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(16, 16);

  // Create a candidate with rounded display masks in top left.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kRoundedDisplayMaskLeftRect,
      kRoundedDisplayMaskInfo);

  // Create a candidate with rounded display masks in top right.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kRoundedDisplayMaskRightRect,
      kRoundedDisplayMaskInfo);

  // This candidate is occluded by top left candidate with rounded display
  // masks.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kNormalLeft);

  // This candidate is occluded by top right candidate with rounded display
  // masks.
  CreateCandidateQuadAt(resource_provider_.get(),
                        child_resource_provider_.get(), child_provider_.get(),
                        pass->shared_quad_state_list.back(), pass.get(),
                        kNormalRight);

  // Create a quad that will the next quad to become an underlay.
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());

  CreateFullscreenCandidateQuad(resource_provider_.get(),
                                child_resource_provider_.get(),
                                child_provider_.get(), sqs, pass.get());

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  overlay_processor_->AddExpectedRect(kNormalLeft, false);
  overlay_processor_->AddExpectedRect(kNormalRight, false);
  overlay_processor_->AddExpectedRect(kOverlayRect, true);

  // Candidates with masks are appended at the end of the surfaces in
  // `CheckOverlaySupportImpl()`
  overlay_processor_->AddExpectedRect(kRoundedDisplayMaskLeftRect, true);
  overlay_processor_->AddExpectedRect(kRoundedDisplayMaskRightRect, true);

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  // Since both the overlay (SingleOnTop) candidates without masks failed to
  // promote, we end up composting both candidates with masks as well. Only the
  // underlay candidate is promoted.
  ASSERT_EQ(candidate_list.size(), 1u);

  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kOverlayRect);
  EXPECT_EQ(candidate_list[0].plane_z_order, -1);
}

// Since AllowCandidateWithMasksSortedMultiOverlayProcessor will only allow
// candidates with masks after sorting in `SortProposedOverlayCandidates()`.
TEST_F(AllowCandidateWithMasksSortedMultiOverlayTest,
       DontPromoteCandidatesWithMasksIfAreOnlySingleOnTopCandidates) {
  auto pass = CreateRenderPass();

  // Add a quad with rounded-display masks.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kOverlayTopLeftRect,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

  // Add a quad with rounded-display masks.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kOverlayTopRightRect,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(10, 0));

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

  // None of the candidates will be promoted.
  // Since AllowCandidateWithMasksSortedMultiOverlayProcessor will only allow
  // candidates with rounded-display masks after sorting(i.e only candidates to
  // pass sorting hubristic), we can skip promoting these candidates.
  ASSERT_EQ(0U, candidate_list.size());
  EXPECT_EQ(3U, main_pass->quad_list.size());
}

TEST_F(TypeAndSizeSortedMultiOverlayTest,
       PrioritizationWithRoundedDisplayMasks) {
  enum OverlayCandidateType { kNormal, kHasRoundedDisplayMasks };

  constexpr gfx::Rect kRoundedDisplayMaskRectSmallest(0, 0, 32, 32);
  constexpr gfx::Rect kRoundedDisplayMaskRectSmall(64, 0, 64, 64);
  constexpr gfx::Rect kNormalBiggest(128, 0, 256, 256);
  constexpr gfx::Rect kNormalBig(0, 0, 128, 128);
  constexpr gfx::Rect kNormalSmallest(128, 0, 32, 32);
  // Intersects with both mask rects.
  constexpr gfx::Rect kNormalSmall(128, 64, 64, 64);

  // The draw order of these candidates is scrambled, so we can verify that the
  // plane_z_orders are are based on draw quad order.
  constexpr std::pair<gfx::Rect, OverlayCandidateType> kCandidatesInfo[]{
      {kRoundedDisplayMaskRectSmall,
       OverlayCandidateType::kHasRoundedDisplayMasks},
      {kRoundedDisplayMaskRectSmallest,
       OverlayCandidateType::kHasRoundedDisplayMasks},
      {kNormalBiggest, OverlayCandidateType::kNormal},
      {kNormalSmallest, OverlayCandidateType::kNormal},
      {kNormalBig, OverlayCandidateType::kNormal},
      {kNormalSmall, OverlayCandidateType::kNormal}};

  constexpr gfx::Rect kAllCands(0, 0, 128, 128);
  const auto kRoundedDisplayMaskInfo =
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(16, 16);

  damage_rect_ = kAllCands;
  auto pass = CreateRenderPass();

  for (auto info : kCandidatesInfo) {
    switch (info.second) {
      case kNormal:
        CreateCandidateQuadAt(
            resource_provider_.get(), child_resource_provider_.get(),
            child_provider_.get(), pass->shared_quad_state_list.back(),
            pass.get(), info.first);
        break;
      case kHasRoundedDisplayMasks:
        CreateQuadWithRoundedDisplayMasksAt(
            resource_provider_.get(), child_resource_provider_.get(),
            child_provider_.get(), pass->shared_quad_state_list.back(),
            pass.get(),
            /*is_overlay_candidate=*/true, info.first, kRoundedDisplayMaskInfo);
        break;
    }
  }

  {
    // Add something behind them.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

  overlay_processor_->AddExpectedRect(kNormalBiggest, true);
  overlay_processor_->AddExpectedRect(kNormalBig, true);

  // Candidates with masks are appended at the end of the surfaces in
  // `CheckOverlaySupportImpl()`
  overlay_processor_->AddExpectedRect(kRoundedDisplayMaskRectSmall, true);
  overlay_processor_->AddExpectedRect(kRoundedDisplayMaskRectSmallest, true);

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  SurfaceDamageRectList surface_damage_rect_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  ASSERT_EQ(candidate_list.size(), 4u);

  // We expect the two rounded masks to get promoted regardless of their surface
  // area. Candidates with rounded masks are followed by candidate with largest
  // surface area.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kNormalBiggest);
  EXPECT_EQ(candidate_list[0].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[1].display_rect), kNormalBig);
  EXPECT_EQ(candidate_list[1].plane_z_order, 1);

  // Candidates with masks are appended at the end of the `candidate_list` in
  // draw order.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[2].display_rect),
            kRoundedDisplayMaskRectSmall);
  EXPECT_EQ(candidate_list[2].plane_z_order, 2);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[3].display_rect),
            kRoundedDisplayMaskRectSmallest);
  EXPECT_EQ(candidate_list[3].plane_z_order, 2);
}

// Test that we favor fullscreen even if other strategies have a higher damage
// rate were previously active.
TEST_F(TransitionOverlayTypeTest, FullscreenFavored) {
  constexpr int kLastIter = 10;

  constexpr int kMakeHiddenOccluderIter = 5;
  for (int i = 0; i <= kLastIter; i++) {
    auto pass = CreateRenderPass();
    constexpr gfx::Rect kSmall(66, 128, 32, 32);

    auto* small_quad_sqs = pass->CreateAndAppendSharedQuadState();

    CreateSolidColorQuadAt(small_quad_sqs, SkColors::kWhite, pass.get(),
                           kSmall);

    auto* fullscreen_sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenCandidateQuad(
        resource_provider_.get(), child_resource_provider_.get(),
        child_provider_.get(), fullscreen_sqs, pass.get());

    fullscreen_sqs->overlay_damage_index = 0;

    OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
    OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;

    auto damage_rect_surface = pass->output_rect;
    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    OverlayCandidateList candidate_list;
    SurfaceDamageRectList surface_damage_rect_list;
    if (i >= kMakeHiddenOccluderIter) {
      damage_rect_surface.Inset(32);
      small_quad_sqs->opacity = 0.f;
    }

    surface_damage_rect_list.push_back(damage_rect_surface);
    overlay_processor_->SetFrameSequenceNumber(static_cast<int64_t>(i));
    overlay_processor_->ProcessForOverlays(
        resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
        render_pass_filters, render_pass_backdrop_filters,
        std::move(surface_damage_rect_list), nullptr, &candidate_list,
        &damage_rect_, &content_bounds_);
    ASSERT_EQ(candidate_list.size(), 1u);
    if (i == 0) {
      EXPECT_EQ(candidate_list[0].plane_z_order, -1);
    }
    if (i == kLastIter) {
      EXPECT_EQ(candidate_list[0].plane_z_order, 0);
    }
  }
}

TEST_F(SizeSortedMultiOverlayTest, OverlaysAreSorted) {
  constexpr gfx::Rect kBiggest(0, 0, 128, 128);
  constexpr gfx::Rect kBig(128, 28, 100, 100);
  // kSmall rect intersects with kSmallest rect.
  constexpr gfx::Rect kSmall(66, 128, 64, 64);
  constexpr gfx::Rect kSmallest(128, 128, 32, 32);
  // The draw order of these candidates is scrambled, so we can verify that the
  // plane_z_orders are are based on draw quad order.
  constexpr gfx::Rect kCandRects[]{kBiggest, kSmallest, kBig};
  constexpr gfx::Rect kAllCands(0, 0, 228, 192);

  damage_rect_ = kAllCands;
  auto pass = CreateRenderPass();

  // Quad with rounded_display mask occluded candidate at `kSmallest`.
  CreateQuadWithRoundedDisplayMasksAt(
      resource_provider_.get(), child_resource_provider_.get(),
      child_provider_.get(), pass->shared_quad_state_list.back(), pass.get(),
      /*is_overlay_candidate=*/true, kSmall,
      RoundedDisplayMasksInfo::CreateRoundedDisplayMasksInfo(16, 16));

  for (auto& cand_rect : kCandRects) {
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          pass->shared_quad_state_list.back(), pass.get(),
                          cand_rect);
  }
  // Candidates will be sorted by surface area. All will be promoted.
  overlay_processor_->AddExpectedRect(kBiggest, true);
  overlay_processor_->AddExpectedRect(kBig, true);
  overlay_processor_->AddExpectedRect(kSmall, true);
  overlay_processor_->AddExpectedRect(kSmallest, true);

  {
    // Add something behind them.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  SurfaceDamageRectList surface_damage_rect_list;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), nullptr, &candidate_list,
      &damage_rect_, &content_bounds_);

  ASSERT_EQ(candidate_list.size(), 4u);
  // Expect all four are promoted to overlays, and their plane_z_order is based
  // on draw order.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kBiggest);
  EXPECT_EQ(candidate_list[0].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[1].display_rect), kBig);
  EXPECT_EQ(candidate_list[1].plane_z_order, 1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[2].display_rect), kSmall);
  EXPECT_EQ(candidate_list[2].plane_z_order, 2);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[3].display_rect), kSmallest);
  EXPECT_EQ(candidate_list[3].plane_z_order, 1);
}

TEST_F(SizeSortedMultiUnderlayOverlayTest, UnderlaysAreSorted) {
  constexpr gfx::Rect kBiggest(0, 0, 128, 128);
  constexpr gfx::Rect kBig(128, 28, 100, 100);
  constexpr gfx::Rect kSmall(64, 128, 64, 64);
  constexpr gfx::Rect kSmallest(128, 128, 32, 32);
  // The draw order of these candidates is scrambled, so we can verify that the
  // plane_z_orders are are based on draw quad order.
  constexpr gfx::Rect kCandRects[]{kSmall, kBiggest, kSmallest, kBig};
  constexpr gfx::Rect kSmallCenter(112, 112, 32, 32);
  constexpr gfx::Rect kAllCands(0, 0, 228, 192);

  damage_rect_ = kAllCands;
  auto pass = CreateRenderPass();
  SurfaceDamageRectList surface_damage_rect_list;

  {
    // Create a quad partially covering up all candidates, forcing them to all
    // be underlays.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateOpaqueQuadAt(resource_provider_.get(), sqs, pass.get(), kSmallCenter);
  }

  for (auto& cand_rect : kCandRects) {
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(cand_rect);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), cand_rect);
  }
  // Candidates will be sorted by surface area. All will be promoted.
  overlay_processor_->AddExpectedRect(kBiggest, true);
  overlay_processor_->AddExpectedRect(kBig, true);
  overlay_processor_->AddExpectedRect(kSmall, true);
  overlay_processor_->AddExpectedRect(kSmallest, true);

  {
    // Add something behind them.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

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

  ASSERT_EQ(candidate_list.size(), 4u);
  // Expect all four are promoted to underlay, and their plane_z_order is based
  // on draw order.
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[0].display_rect), kBiggest);
  EXPECT_EQ(candidate_list[0].plane_z_order, -2);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[1].display_rect), kBig);
  EXPECT_EQ(candidate_list[1].plane_z_order, -4);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[2].display_rect), kSmall);
  EXPECT_EQ(candidate_list[2].plane_z_order, -1);
  EXPECT_EQ(gfx::ToRoundedRect(candidate_list[3].display_rect), kSmallest);
  EXPECT_EQ(candidate_list[3].plane_z_order, -3);
  // Full damage because these are underlays on their first frame of promotion.
  EXPECT_EQ(damage_rect_, kAllCands);
}

class MultiUnderlayPromotedTest : public MultiUnderlayTest,
                                  public testing::WithParamInterface<bool> {};

TEST_P(MultiUnderlayPromotedTest, UnderlaysBlendPrimaryPlane) {
  bool promoted = GetParam();

  constexpr gfx::Rect kTopLeft(0, 0, 128, 128);
  constexpr gfx::Rect kTopRight(128, 0, 128, 128);
  constexpr gfx::Rect kTopHalf(0, 0, 256, 128);

  auto pass = CreateRenderPass();
  damage_rect_ = kTopHalf;
  SurfaceDamageRectList surface_damage_rect_list;

  {
    // Create a candidate in the top left.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kTopLeft);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kTopLeft);
    overlay_processor_->AddExpectedRect(kTopLeft, promoted);
  }
  {
    // Create a candidate in the top right.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->overlay_damage_index = surface_damage_rect_list.size();
    surface_damage_rect_list.emplace_back(kTopRight);
    CreateCandidateQuadAt(resource_provider_.get(),
                          child_resource_provider_.get(), child_provider_.get(),
                          sqs, pass.get(), kTopRight);
    overlay_processor_->AddExpectedRect(kTopRight, promoted);
  }
  {
    // Add something behind it.
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    CreateFullscreenOpaqueQuad(resource_provider_.get(), sqs, pass.get());
  }

  OverlayCandidateList candidate_list;
  OverlayProcessorInterface::FilterOperationsMap render_pass_filters;
  OverlayProcessorInterface::FilterOperationsMap render_pass_backdrop_filters;
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));
  auto output_surface_plane = overlay_processor_->ProcessOutputSurfaceAsOverlay(
      kDisplaySize, kDisplaySize, kDefaultSIFormat, gfx::ColorSpace(),
      false /* has_alpha */, 1.0f /* opacity */, gpu::Mailbox());
  OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane =
      &output_surface_plane;

  overlay_processor_->ProcessForOverlays(
      resource_provider_.get(), &pass_list, GetIdentityColorMatrix(),
      render_pass_filters, render_pass_backdrop_filters,
      std::move(surface_damage_rect_list), primary_plane, &candidate_list,
      &damage_rect_, &content_bounds_);

  if (promoted) {
    // Both candidates are promoted.
    ASSERT_EQ(candidate_list.size(), 2u);
    // Blending enabled on primary plane.
    EXPECT_TRUE(primary_plane->enable_blending);
  } else {
    // No candidates are promoted.
    EXPECT_TRUE(candidate_list.empty());
    // Blending not enabled on primary plane.
    EXPECT_FALSE(primary_plane->enable_blending);
  }
}

INSTANTIATE_TEST_SUITE_P(PromotedTrueFalse,
                         MultiUnderlayPromotedTest,
                         testing::Bool());

}  // namespace
}  // namespace viz
