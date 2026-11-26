// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/hdr_video_passthrough.h"

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/full_screen_video_util.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rect_f.h"

namespace viz {

const DrawQuad* FindHdrVideoPassthroughQuad(
    DisplayResourceProvider* resource_provider,
    const AggregatedRenderPassList& render_passes,
    const gfx::ColorSpace& output_color_space,
    bool is_page_fullscreen) {
  if (!is_page_fullscreen || render_passes.empty() ||
      output_color_space.GetTransferID() != gfx::ColorSpace::TransferID::PQ) {
    return nullptr;
  }

  const AggregatedRenderPass* root_pass = render_passes.back().get();
  if (root_pass->content_color_usage != gfx::ContentColorUsage::kHDR) {
    return nullptr;
  }

  // HDR content inside a non-root render pass would be tone mapped by the
  // system compositor along with the video, so don't pass through.
  for (const auto& pass : render_passes) {
    if (pass.get() != root_pass &&
        pass->content_color_usage == gfx::ContentColorUsage::kHDR) {
      return nullptr;
    }
  }

  const DrawQuad* candidate = nullptr;
  const DrawQuad* quad_below_candidate = nullptr;
  for (auto it = root_pass->quad_list.begin(); it != root_pass->quad_list.end();
       ++it) {
    const DrawQuad* quad = *it;
    if (quad->resource_id == kInvalidResourceId) {
      continue;
    }
    const gfx::ColorSpace color_space =
        resource_provider->GetColorSpace(quad->resource_id);
    if (!color_space.IsHDR()) {
      continue;
    }
    // Only one HDR quad is allowed.
    if (candidate) {
      return nullptr;
    }
    if (quad->material != DrawQuad::Material::kTextureContent ||
        color_space.GetTransferID() != gfx::ColorSpace::TransferID::PQ ||
        resource_provider->GetHDRMetadata(quad->resource_id).HasAgtm() ||
        quad->ShouldDrawWithBlendingForReasonOtherThanMaskFilter()) {
      return nullptr;
    }
    // Content that requested a limited dynamic range must be tone mapped by
    // the renderer.
    const auto& dynamic_range_limit =
        TextureDrawQuad::MaterialCast(quad)->dynamic_range_limit;
    if (dynamic_range_limit.standard_mix > 0.f ||
        dynamic_range_limit.constrained_high_mix > 0.f) {
      return nullptr;
    }
    candidate = quad;
    // `quad_list` is ordered front to back, so the next quad is below.
    auto below_it = it;
    ++below_it;
    quad_below_candidate =
        below_it != root_pass->quad_list.end() ? *below_it : nullptr;
  }
  if (!candidate) {
    return nullptr;
  }

  // Detect a full screen video like the Windows full screen video
  // optimization does (OverlayProcessorWin::TryPromoteFullScreenVideo).
  const gfx::Rect& output_rect = root_pass->output_rect;
  if (!IsPossibleFullScreenLetterboxing(quad_below_candidate, output_rect)) {
    return nullptr;
  }
  const SharedQuadState* sqs = candidate->shared_quad_state;
  if (!sqs->quad_to_target_transform.IsPositiveScaleOrTranslation()) {
    return nullptr;
  }
  const gfx::RectF target_rect =
      sqs->quad_to_target_transform.MapRect(gfx::RectF(candidate->rect));
  if (!GetFullScreenVideoRect(target_rect, sqs->clip_rect, output_rect,
                              kFullScreenVideoTightTolerance)) {
    return nullptr;
  }
  return candidate;
}

gfx::HDRMetadata ComputeHdrVideoPassthroughMetadata(
    const gfx::ColorSpace& content_color_space,
    const gfx::HDRMetadata& content_hdr_metadata,
    float output_sdr_white_level) {
  const float content_sdr_white_level =
      gfx::HDRMetadata::GetWaylandReferenceLuminance(content_color_space,
                                                     content_hdr_metadata);
  const float scale = content_sdr_white_level > 0.f
                          ? output_sdr_white_level / content_sdr_white_level
                          : 1.f;

  gfx::HDRMetadata result =
      gfx::HDRMetadata::PopulateUnspecifiedWithDefaults(content_hdr_metadata);
  skhdr::MasteringDisplayColorVolume mdcv = result.GetMDCV();
  mdcv.fMaximumDisplayMasteringLuminance *= scale;
  mdcv.fMinimumDisplayMasteringLuminance *= scale;
  result.SetMDCV(mdcv);
  if (result.HasCLLI()) {
    skhdr::ContentLightLevelInformation clli = result.GetCLLI();
    clli.fMaxCLL *= scale;
    clli.fMaxFALL *= scale;
    result.SetCLLI(clli);
  }
  return result;
}

}  // namespace viz
