// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_outline.h"

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/ui_features.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/view_class_properties.h"

ContentsContainerOutline::ContentsContainerOutline(views::View* mini_toolbar)
    : mini_toolbar_(mini_toolbar) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetCanProcessEventsWithinSubtree(false);
  SetVisible(false);
  GetViewAccessibility().SetIsInvisible(true);

  view_bounds_observer_.Observe(mini_toolbar);
}

ContentsContainerOutline::~ContentsContainerOutline() = default;

// static
int ContentsContainerOutline::GetThickness(bool is_highlighted) {
  return is_highlighted ? kHighlightThickness : kThickness;
}

// static
ui::ColorId ContentsContainerOutline::GetColor(bool is_active,
                                               bool is_highlighted) {
  if (is_active) {
    return is_highlighted ? kColorMultiContentsViewHighlightContentOutline
                          : kColorMultiContentsViewActiveContentOutline;
  }
  return kColorMultiContentsViewInactiveContentOutline;
}

void ContentsContainerOutline::UpdateState(bool is_active,
                                           bool is_highlighted) {
  is_active_ = is_active;
  is_highlighted_ = is_highlighted;
  SetVisible(true);
  SchedulePaint();
}

void ContentsContainerOutline::OnPaint(gfx::Canvas* canvas) {
  // Draw the bordering stroke.
  cc::PaintFlags flags;
  flags.setStrokeWidth(GetThickness(is_highlighted_));
  flags.setColor(
      GetColorProvider()->GetColor(GetColor(is_active_, is_highlighted_)));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  gfx::RectF local_bounds(GetLocalBounds());
  const float half_thickness = GetThickness(is_highlighted_) / 2.0f;
  local_bounds.Inset(half_thickness);
  const float corner_radius = kCornerRadius - half_thickness;

  // Generate the outline path starting from the left edge right below the
  // rounded rect arc and then conditionally either raws out a rounded rect
  // or a path that overlaps with the mini-toolbar view in clockwise direction.
  SkPathBuilder path;
  path.moveTo(local_bounds.x(), local_bounds.y() + corner_radius);
  path.arcTo(SkVector(corner_radius, corner_radius), 0.0f,
             SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
             SkPoint(local_bounds.x() + corner_radius, local_bounds.y()));
  path.lineTo(local_bounds.right() - corner_radius, local_bounds.y());
  path.arcTo(SkVector(corner_radius, corner_radius), 0.0f,
             SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
             SkPoint(local_bounds.right(), local_bounds.y() + corner_radius));

  if (is_active_ &&
      (is_highlighted_ ||
       features::kSideBySideMiniToolbarActiveConfiguration.Get() ==
           features::MiniToolbarActiveConfiguration::Hide)) {
    // If the mini toolbar is hidden on active view, just draw the rounded rect.
    path.lineTo(local_bounds.right(), local_bounds.bottom() - corner_radius);
    path.arcTo(SkVector(corner_radius, corner_radius), 0.0f,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
               SkPoint(local_bounds.right() - corner_radius,
                       local_bounds.bottom()));
  } else {
    // Draw the path around the mini toolbar. This uses a corner radius which
    // is half thickness greater than the clip path. The outline path needs
    // to match the clip path.
    CHECK(mini_toolbar_.get());
    const gfx::SizeF mini_toolbar_size(mini_toolbar_->size());
    path.lineTo(local_bounds.right(),
                local_bounds.bottom() - mini_toolbar_size.height());
    path.arcTo(SkVector(kCornerRadius, kCornerRadius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
               SkPoint(local_bounds.right() - kCornerRadius,
                       local_bounds.bottom() - mini_toolbar_size.height() +
                           kCornerRadius));
    path.lineTo(
        local_bounds.right() - mini_toolbar_size.width() + kCornerRadius * 2,
        local_bounds.bottom() - mini_toolbar_size.height() + kCornerRadius);
    path.arcTo(SkVector(kCornerRadius, kCornerRadius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
               SkPoint(local_bounds.right() - mini_toolbar_size.width() +
                           kCornerRadius,
                       local_bounds.bottom() - mini_toolbar_size.height() +
                           kCornerRadius * 2));
    path.lineTo(
        local_bounds.right() - mini_toolbar_size.width() + kCornerRadius,
        local_bounds.bottom() - kCornerRadius);
    path.arcTo(SkVector(kCornerRadius, kCornerRadius), 0,
               SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
               SkPoint(local_bounds.right() - mini_toolbar_size.width(),
                       local_bounds.bottom()));
  }

  path.lineTo(local_bounds.x() + corner_radius, local_bounds.bottom());
  path.arcTo(SkVector(corner_radius, corner_radius), 0.0f,
             SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
             SkPoint(local_bounds.x(), local_bounds.bottom() - corner_radius));
  path.close();

  if (base::i18n::IsRTL()) {
    // Mirror the path for outline in case of RTL.
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    SkMatrix flip;
    flip.setScale(-1, 1, center.x(), center.y());
    path.transform(flip);
  }

  canvas->DrawPath(path.detach(), flags);
}

void ContentsContainerOutline::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == mini_toolbar_.get());
  SetClipPath();
  SchedulePaint();
}

void ContentsContainerOutline::OnViewIsDeleting(views::View* observed_view) {
  CHECK(observed_view == mini_toolbar_.get());
  view_bounds_observer_.Reset();
  mini_toolbar_ = nullptr;
}

void ContentsContainerOutline::SetClipPath() {
  // Set clip path on the mini toolbar view, it uses half thickness less corner
  // radius than the outline. This path needs to match the outline path.
  gfx::Rect mini_toolbar_rect(mini_toolbar_->GetLocalBounds());
  const float half_thickness = GetThickness(is_highlighted_) / 2.0f;
  mini_toolbar_rect.Inset(half_thickness);
  const float corner_radius = kCornerRadius - half_thickness;

  SkPathBuilder clip_path;
  clip_path.moveTo(mini_toolbar_rect.right(), mini_toolbar_rect.y());
  clip_path.arcTo(SkVector(corner_radius, corner_radius), 0,
                  SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                  SkPoint(mini_toolbar_rect.right() - corner_radius,
                          mini_toolbar_rect.y() + corner_radius));
  clip_path.lineTo(mini_toolbar_rect.x() + corner_radius * 2,
                   mini_toolbar_rect.y() + corner_radius);
  clip_path.arcTo(SkVector(corner_radius, corner_radius), 0,
                  SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                  SkPoint(mini_toolbar_rect.x() + corner_radius,
                          mini_toolbar_rect.y() + corner_radius * 2));
  clip_path.lineTo(mini_toolbar_rect.x() + corner_radius,
                   mini_toolbar_rect.bottom() - corner_radius);
  clip_path.arcTo(SkVector(corner_radius, corner_radius), 0,
                  SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                  SkPoint(mini_toolbar_rect.x(), mini_toolbar_rect.bottom()));
  clip_path.lineTo(mini_toolbar_rect.right(), mini_toolbar_rect.bottom());
  clip_path.lineTo(mini_toolbar_rect.right(), mini_toolbar_rect.y());

  if (base::i18n::IsRTL()) {
    // Mirror the clip path in case of RTL.
    gfx::PointF center =
        gfx::RectF(mini_toolbar_->GetLocalBounds()).CenterPoint();
    SkMatrix flip;
    flip.setScale(-1, 1, center.x(), center.y());
    clip_path.transform(flip);
  }

  mini_toolbar_->SetClipPath(clip_path.detach());
}

BEGIN_METADATA(ContentsContainerOutline)
END_METADATA
