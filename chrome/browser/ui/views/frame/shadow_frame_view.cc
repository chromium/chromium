// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/shadow_frame_view.h"

#include <memory>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/view_shadow.h"

BEGIN_METADATA(ShadowFrameView)
END_METADATA

ShadowFrameView::ShadowFrameView(int elevation, ShadowAlpha alpha)
    : shadow_elevation_(elevation), shadow_alpha_(alpha) {
  SetCanProcessEventsWithinSubtree(false);
}

ShadowFrameView::~ShadowFrameView() = default;

void ShadowFrameView::SetShadowVisible(bool visible) {
  // No-op if visible set is the same as current state.
  if (visible == !!layer()) {
    return;
  }

  if (visible) {
    view_shadow_ = std::make_unique<views::ViewShadow>(this, shadow_elevation_);
    view_shadow_->SetRoundedCornerRadius(corner_radius_);
    view_shadow_->shadow()->shadow_layer()->SetOpacity(shadow_opacity_);
    UpdateShadowColors();
  } else {
    view_shadow_.reset();
    DestroyLayer();
    was_dark_.reset();
    SchedulePaint();
  }
}

void ShadowFrameView::SetShadowOpacity(double opacity) {
  if (shadow_opacity_ == opacity) {
    return;
  }
  shadow_opacity_ = opacity;

  if (view_shadow_) {
    view_shadow_->shadow()->shadow_layer()->SetOpacity(opacity);
    SchedulePaint();
  }
}

void ShadowFrameView::SetShadowCornerRadius(int corner_radius) {
  if (corner_radius_ == corner_radius) {
    return;
  }
  corner_radius_ = corner_radius;

  if (view_shadow_) {
    view_shadow_->SetRoundedCornerRadius(corner_radius_);
    SchedulePaint();
  }
}

void ShadowFrameView::OnThemeChanged() {
  View::OnThemeChanged();
  if (view_shadow_) {
    UpdateShadowColors();
  }
}

void ShadowFrameView::UpdateShadowColors() {
  const bool is_dark =
      color_utils::IsDark(GetColorProvider()->GetColor(kColorToolbar));
  if (was_dark_ == is_dark) {
    return;
  }
  was_dark_ = is_dark;

  const std::pair<SkColor, SkColor> shadow_colors =
      is_dark
          ? std::make_pair(
                SkColorSetARGB(base::ClampRound(255.0 * shadow_alpha_.dark_key),
                               0, 0, 0),
                SkColorSetARGB(
                    base::ClampRound(255.0 * shadow_alpha_.dark_ambient), 0, 0,
                    0))
          : std::make_pair(
                SkColorSetARGB(
                    base::ClampRound(255.0 * shadow_alpha_.light_key), 0, 0, 0),
                SkColorSetARGB(
                    base::ClampRound(255.0 * shadow_alpha_.light_ambient), 0, 0,
                    0));
  const ui::Shadow::ElevationToColorsMap map{
      {shadow_elevation_, shadow_colors}};
  view_shadow_->shadow()->SetElevationToColorsMap(map);
  SchedulePaint();
}
