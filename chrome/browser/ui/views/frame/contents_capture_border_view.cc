// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_capture_border_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/contents_container_outline.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/view_class_properties.h"

ContentsCaptureBorderView::ContentsCaptureBorderView(views::View* mini_toolbar)
    : mini_toolbar_(mini_toolbar) {
  SetProperty(views::kElementIdentifierKey, kContentsCaptureBorder);
  SetBorder(views::CreateSolidBorder(kContentsBorderThickness,
                                     kColorCapturedTabContentsBorder));

  if (mini_toolbar_) {
    view_bounds_observer_.Observe(mini_toolbar_);
  }
}

ContentsCaptureBorderView::~ContentsCaptureBorderView() = default;

void ContentsCaptureBorderView::SetIsInSplit(bool is_in_split) {
  if (is_in_split_ == is_in_split) {
    return;
  }
  is_in_split_ = is_in_split;
  if (!is_in_split_) {
    SetBorder(views::CreateSolidBorder(kContentsBorderThickness,
                                       kColorCapturedTabContentsBorder));
  } else if (GetBorder()) {
    // In split mode, we'll draw the border in OnPaint()
    SetBorder(nullptr);
  }
}

void ContentsCaptureBorderView::OnViewBoundsChanged(
    views::View* observed_view) {
  SchedulePaint();
}

void ContentsCaptureBorderView::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view,
    bool visible) {
  SchedulePaint();
}

void ContentsCaptureBorderView::OnViewIsDeleting(views::View* observed_view) {
  view_bounds_observer_.Reset();
  mini_toolbar_ = nullptr;
}

void ContentsCaptureBorderView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // If not in split mode, the border is drawn by the standard border mechanism.
  if (!is_in_split_) {
    return;
  }

  cc::PaintFlags flags;
  flags.setStrokeWidth(kContentsBorderThickness);
  flags.setColor(GetColorProvider()->GetColor(kColorCapturedTabContentsBorder));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  gfx::RectF local_bounds(GetLocalBounds());
  const float half_thickness = kContentsBorderThickness / 2.0f;
  local_bounds.Inset(half_thickness);
  const float corner_radius =
      ContentsContainerOutline::kCornerRadius - half_thickness;

  // Get the mini toolbar size if it's visible.
  gfx::SizeF mini_toolbar_size;
  if (mini_toolbar_ && mini_toolbar_->GetVisible()) {
    mini_toolbar_size = gfx::SizeF(mini_toolbar_->size());
  }

  SkPath path = ContentsContainerOutline::GetPath(
      local_bounds, corner_radius, ContentsContainerOutline::kCornerRadius,
      mini_toolbar_size);

  canvas->DrawPath(path, flags);
}

BEGIN_METADATA(ContentsCaptureBorderView)
END_METADATA
