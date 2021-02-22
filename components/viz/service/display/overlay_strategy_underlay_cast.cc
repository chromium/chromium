// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_underlay_cast.h"

#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/unguessable_token.h"
#include "build/chromecast_buildflags.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if BUILDFLAG(IS_CHROMECAST)
#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace viz {
namespace {

base::LazyInstance<OverlayStrategyUnderlayCast::OverlayCompositedCallback>::
    DestructorAtExit g_overlay_composited_callback = LAZY_INSTANCE_INITIALIZER;

#if BUILDFLAG(IS_CHROMECAST)
// This persistent mojo::Remote is bound then used by all the instances
// of OverlayStrategyUnderlayCast.
mojo::Remote<chromecast::media::mojom::VideoGeometrySetter>&
GetVideoGeometrySetter() {
  static base::NoDestructor<
      mojo::Remote<chromecast::media::mojom::VideoGeometrySetter>>
      g_video_geometry_setter;
  return *g_video_geometry_setter;
}
#endif

}  // namespace

OverlayStrategyUnderlayCast::OverlayStrategyUnderlayCast(
    OverlayProcessorUsingStrategy* capability_checker)
    : OverlayStrategyUnderlay(capability_checker) {}

OverlayStrategyUnderlayCast::~OverlayStrategyUnderlayCast() {}

bool OverlayStrategyUnderlayCast::Attempt(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  // Before we attempt an overlay strategy, the candidate list should be empty.
  DCHECK(candidate_list->empty());
  auto* render_pass = render_pass_list->back().get();
  QuadList& quad_list = render_pass->quad_list;
  bool found_underlay = false;
  gfx::Rect content_rect;
  for (const auto* quad : base::Reversed(quad_list)) {
    if (OverlayCandidate::IsInvisibleQuad(quad))
      continue;

    const auto& transform = quad->shared_quad_state->quad_to_target_transform;
    gfx::RectF quad_rect = gfx::RectF(quad->rect);
    transform.TransformRect(&quad_rect);

    bool is_underlay = false;
    if (!found_underlay) {
      OverlayCandidate candidate;
      // Look for quads that are overlayable and require an overlay. Chromecast
      // only supports a video underlay so this can't promote all quads that are
      // overlayable, it needs to ensure that the quad requires overlays since
      // that quad is side-channeled through a secure path into an overlay
      // sitting underneath the primary plane. This is only looking at where the
      // quad is supposed to be to replace it with a transparent quad to allow
      // the underlay to be visible.
      // VIDEO_HOLE implies it requires overlay.
      is_underlay =
          quad->material == DrawQuad::Material::kVideoHole &&
          OverlayCandidate::FromDrawQuad(
              resource_provider, surface_damage_rect_list, output_color_matrix,
              quad, GetPrimaryPlaneDisplayRect(primary_plane), &candidate);
      found_underlay = is_underlay;
    }

    if (!found_underlay && quad->material == DrawQuad::Material::kSolidColor) {
      const SolidColorDrawQuad* solid = SolidColorDrawQuad::MaterialCast(quad);
      if (solid->color == SK_ColorBLACK)
        continue;
    }

    if (is_underlay) {
      content_rect.Subtract(gfx::ToEnclosedRect(quad_rect));
    } else {
      content_rect.Union(gfx::ToEnclosingRect(quad_rect));
    }
  }

  if (is_using_overlay_ != found_underlay) {
    is_using_overlay_ = found_underlay;
    VLOG(1) << (found_underlay ? "Overlay activated" : "Overlay deactivated");
  }

  if (found_underlay) {
    for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
      OverlayCandidate candidate;
      if (it->material != DrawQuad::Material::kVideoHole ||
          !OverlayCandidate::FromDrawQuad(
              resource_provider, surface_damage_rect_list, output_color_matrix,
              *it, GetPrimaryPlaneDisplayRect(primary_plane), &candidate)) {
        continue;
      }

      // TODO(guohuideng): when migration to GPU process complete, remove
      // the code that's for the browser process compositor.
#if BUILDFLAG(IS_CHROMECAST)
      if (g_overlay_composited_callback.Get().is_null()) {
        DCHECK(GetVideoGeometrySetter());
        GetVideoGeometrySetter()->SetVideoGeometry(
            candidate.display_rect, candidate.transform,
            VideoHoleDrawQuad::MaterialCast(*it)->overlay_plane_id);
      } else {
        g_overlay_composited_callback.Get().Run(candidate.display_rect,
                                                candidate.transform);
      }
#endif

      render_pass->ReplaceExistingQuadWithOpaqueTransparentSolidColor(it);

      break;
    }
  }

  DCHECK(content_bounds && content_bounds->empty());
  if (found_underlay) {
    content_bounds->push_back(content_rect);
  }
  return found_underlay;
}

void OverlayStrategyUnderlayCast::ProposePrioritized(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    OverlayProposedCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  auto* render_pass = render_pass_list->back().get();
  QuadList& quad_list = render_pass->quad_list;
  OverlayCandidate candidate;
  auto overlay_iter = quad_list.end();
  // Original code did reverse iteration.
  // Here we do forward but find the last one. which should be the same thing.
  for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
    if (OverlayCandidate::IsInvisibleQuad(*it))
      continue;

    // Look for quads that are overlayable and require an overlay. Chromecast
    // only supports a video underlay so this can't promote all quads that are
    // overlayable, it needs to ensure that the quad requires overlays since
    // that quad is side-channeled through a secure path into an overlay
    // sitting underneath the primary plane. This is only looking at where the
    // quad is supposed to be to replace it with a transparent quad to allow
    // the underlay to be visible.
    // VIDEO_HOLE implies it requires overlay.
    if (it->material == DrawQuad::Material::kVideoHole &&
        OverlayCandidate::FromDrawQuad(
            resource_provider, surface_damage_rect_list, output_color_matrix,
            *it, GetPrimaryPlaneDisplayRect(primary_plane), &candidate)) {
      overlay_iter = it;
    }
  }

  if (overlay_iter != quad_list.end()) {
    candidates->push_back({overlay_iter, candidate, this});
  }
}

bool OverlayStrategyUnderlayCast::AttemptPrioritized(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds,
    OverlayProposedCandidate* proposed_candidate) {
  // Before we attempt an overlay strategy, the candidate list should be empty.
  DCHECK(candidate_list->empty());
  auto* render_pass = render_pass_list->back().get();
  QuadList& quad_list = render_pass->quad_list;
  bool found_underlay = false;
  gfx::Rect content_rect;
  for (const auto* quad : base::Reversed(quad_list)) {
    if (OverlayCandidate::IsInvisibleQuad(quad))
      continue;

    const auto& transform = quad->shared_quad_state->quad_to_target_transform;
    gfx::RectF quad_rect = gfx::RectF(quad->rect);
    transform.TransformRect(&quad_rect);

    bool is_underlay = false;
    if (!found_underlay) {
      OverlayCandidate candidate;
      // Look for quads that are overlayable and require an overlay. Chromecast
      // only supports a video underlay so this can't promote all quads that are
      // overlayable, it needs to ensure that the quad requires overlays since
      // that quad is side-channeled through a secure path into an overlay
      // sitting underneath the primary plane. This is only looking at where the
      // quad is supposed to be to replace it with a transparent quad to allow
      // the underlay to be visible.
      // VIDEO_HOLE implies it requires overlay.
      is_underlay =
          quad->material == DrawQuad::Material::kVideoHole &&
          OverlayCandidate::FromDrawQuad(
              resource_provider, surface_damage_rect_list, output_color_matrix,
              quad, GetPrimaryPlaneDisplayRect(primary_plane), &candidate);
      found_underlay = is_underlay;
    }

    if (!found_underlay && quad->material == DrawQuad::Material::kSolidColor) {
      const SolidColorDrawQuad* solid = SolidColorDrawQuad::MaterialCast(quad);
      if (solid->color == SK_ColorBLACK)
        continue;
    }

    if (is_underlay) {
      content_rect.Subtract(gfx::ToEnclosedRect(quad_rect));
    } else {
      content_rect.Union(gfx::ToEnclosingRect(quad_rect));
    }
  }

  if (is_using_overlay_ != found_underlay) {
    is_using_overlay_ = found_underlay;
    VLOG(1) << (found_underlay ? "Overlay activated" : "Overlay deactivated");
  }

  if (found_underlay) {
    for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
      OverlayCandidate candidate;
      if (it->material != DrawQuad::Material::kVideoHole ||
          !OverlayCandidate::FromDrawQuad(
              resource_provider, surface_damage_rect_list, output_color_matrix,
              *it, GetPrimaryPlaneDisplayRect(primary_plane), &candidate)) {
        continue;
      }

      // TODO(guohuideng): when migration to GPU process complete, remove
      // the code that's for the browser process compositor.
#if BUILDFLAG(IS_CHROMECAST)
      if (g_overlay_composited_callback.Get().is_null()) {
        DCHECK(GetVideoGeometrySetter());
        GetVideoGeometrySetter()->SetVideoGeometry(
            candidate.display_rect, candidate.transform,
            VideoHoleDrawQuad::MaterialCast(*it)->overlay_plane_id);
      } else {
        g_overlay_composited_callback.Get().Run(candidate.display_rect,
                                                candidate.transform);
      }
#endif

      render_pass->ReplaceExistingQuadWithOpaqueTransparentSolidColor(it);

      break;
    }
  }

  DCHECK(content_bounds && content_bounds->empty());
  if (found_underlay) {
    content_bounds->push_back(content_rect);
  }
  return found_underlay;
}

OverlayStrategy OverlayStrategyUnderlayCast::GetUMAEnum() const {
  return OverlayStrategy::kUnderlayCast;
}

// static
void OverlayStrategyUnderlayCast::SetOverlayCompositedCallback(
    const OverlayCompositedCallback& cb) {
  g_overlay_composited_callback.Get() = cb;
}

#if BUILDFLAG(IS_CHROMECAST)
// static
void OverlayStrategyUnderlayCast::ConnectVideoGeometrySetter(
    mojo::PendingRemote<chromecast::media::mojom::VideoGeometrySetter>
        video_geometry_setter) {
  GetVideoGeometrySetter().Bind(std::move(video_geometry_setter));
}
#endif

}  // namespace viz
