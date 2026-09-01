// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical_tab_style_views.h"

#include <optional>
#include <string>

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

VerticalTabStyleViews::VerticalTabStyleViews(
    std::unique_ptr<TabStyleViewDelegate> delegate)
    : delegate_(std::move(delegate)) {
  CHECK(delegate_);
}

VerticalTabStyleViews::~VerticalTabStyleViews() = default;

const TabStyleViewDelegate* VerticalTabStyleViews::delegate() const {
  return delegate_.get();
}

double VerticalTabStyleViews::GetHoverAnimationValue() const {
  return delegate_->GetHoverAnimationValue();
}

GlowHoverController* VerticalTabStyleViews::GetHoverControllerForTesting() {
  return delegate_->GetHoverControllerForTesting();  // IN-TEST
}

TabStyle::SeparatorOpacities
VerticalTabStyleViews::GetSeparatorOpacitiesForTesting() const {  // IN-TEST
  return {0.0f, 0.0f};
}

SkPath VerticalTabStyleViews::GetPath(TabStyle::PathType path_type,
                                      float scale,
                                      const TabPathFlags& flags) const {
  const SkScalar corner_radius = GetCornerRadius();
  gfx::RectF bounds(delegate_->GetView()->GetLocalBounds());

  if (flags.render_units == TabStyle::RenderUnits::kPixels) {
    bounds.Scale(scale);
  }
  const SkScalar scaled_corner_radius =
      flags.render_units == TabStyle::RenderUnits::kPixels
          ? corner_radius * scale
          : corner_radius;

  return SkPath::RRect(SkRRect::MakeRectXY(
      gfx::RectFToSkRect(bounds), scaled_corner_radius, scaled_corner_radius));
}

std::optional<SkPath> VerticalTabStyleViews::GetChildClipPath(
    float paint_recording_scale) const {
  // Vertical tabs do not need to specifically clip their children, instead we
  // have a global clip path for the entire tab view.
  return std::nullopt;
}

void VerticalTabStyleViews::PaintTab(gfx::Canvas* canvas) const {
  std::optional<int> active_tab_fill_id;
  const ui::ThemeProvider* const theme_provider =
      delegate_->GetView()->GetThemeProvider();
  if (theme_provider && theme_provider->HasCustomImage(IDR_THEME_TOOLBAR)) {
    active_tab_fill_id = IDR_THEME_TOOLBAR;
  }
  BrowserFrameView* const browser_frame_view = delegate_->GetBrowserFrameView();
  const std::optional<int> inactive_tab_fill_id =
      browser_frame_view ? browser_frame_view->GetCustomBackgroundId(
                               BrowserFrameActiveState::kUseCurrent)
                         : std::nullopt;

  if (active_tab_fill_id.has_value() || inactive_tab_fill_id.has_value()) {
    PaintTabBackgroundWithImages(canvas, active_tab_fill_id,
                                 inactive_tab_fill_id);
  } else {
    PaintTabBackgroundFill(canvas, GetSelectionState(),
                           delegate_->IsHoverAnimationActive(), std::nullopt);
  }
}

void VerticalTabStyleViews::PaintTabBackgroundWithImages(
    gfx::Canvas* canvas,
    std::optional<int> active_tab_fill_id,
    std::optional<int> inactive_tab_fill_id) const {
  const TabStyle::TabSelectionState current_state = GetSelectionState();

  if (current_state == TabStyle::TabSelectionState::kActive) {
    PaintTabBackgroundFill(canvas, TabStyle::TabSelectionState::kActive,
                           /*hovered=*/false, active_tab_fill_id);
  } else {
    PaintTabBackgroundFill(canvas, TabStyle::TabSelectionState::kInactive,
                           /*hovered=*/false, inactive_tab_fill_id);

    const float opacity = GetCurrentActiveOpacity();
    if (opacity > 0) {
      canvas->SaveLayerAlpha(base::ClampRound<uint8_t>(opacity * 0xff),
                             delegate_->GetView()->GetLocalBounds());
      PaintTabBackgroundFill(canvas, TabStyle::TabSelectionState::kActive,
                             /*hovered=*/false, active_tab_fill_id);
      canvas->Restore();
    }
  }
}

float VerticalTabStyleViews::GetCurrentActiveOpacity() const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return 1.0f;
  }
  const float base_opacity =
      selection_state == TabStyle::TabSelectionState::kSelected
          ? tab_style()->GetSelectedTabOpacity()
          : 0.0f;
  if (!delegate_->IsHoverAnimationActive()) {
    return base_opacity;
  }
  return std::lerp(base_opacity, delegate_->GetHoverOpacity(),
                   delegate_->GetHoverAnimationValue());
}

void VerticalTabStyleViews::PaintTabBackgroundFill(
    gfx::Canvas* canvas,
    TabStyle::TabSelectionState selection_state,
    bool hovered,
    std::optional<int> fill_id) const {
  const SkPath fill_path =
      GetPath(TabStyle::PathType::kHighlight, canvas->image_scale(), {});

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);

  if (ShouldPaintTabBackgroundColor(selection_state, fill_id.has_value(),
                                    hovered)) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetCurrentTabBackgroundColor(selection_state));
    canvas->DrawRect(gfx::ScaleToEnclosingRect(
                         delegate_->GetView()->GetLocalBounds(), scale),
                     flags);
  }

  if (fill_id.has_value()) {
    const ui::ThemeProvider* const theme_provider =
        delegate_->GetView()->GetThemeProvider();
    if (theme_provider) {
      gfx::ImageSkia* image =
          theme_provider->GetImageSkiaNamed(fill_id.value());

      canvas->TileImageInt(*image, 0, 0, 0, 0,
                           delegate_->GetView()->width() * scale,
                           delegate_->GetView()->height() * scale);
    }
  }
}

bool VerticalTabStyleViews::ShouldPaintTabBackgroundColor(
    TabStyle::TabSelectionState selection_state,
    bool has_custom_background,
    bool hovered) const {
  if (selection_state == TabStyle::TabSelectionState::kActive ||
      selection_state == TabStyle::TabSelectionState::kSelected) {
    return true;
  }

  if (has_custom_background) {
    return false;
  }

  if (hovered) {
    return true;
  }

  return delegate_->ShouldPaintTabBackgroundColor();
}

SkColor VerticalTabStyleViews::GetCurrentTabBackgroundColor(
    TabStyle::TabSelectionState selection_state) const {
  const bool frame_glass = delegate_->IsGlassFrame();
  return tab_style()->GetCurrentTabBackgroundColor(
      selection_state, delegate_->IsHoverAnimationActive(),
      delegate_->GetHoverAnimationValue(),
      delegate_->GetView()->GetWidget()
          ? delegate_->GetView()->GetWidget()->ShouldPaintAsActive()
          : true,
      frame_glass, delegate_->GetView()->GetColorProvider());
}

TabStyle::TabSelectionState VerticalTabStyleViews::GetSelectionState() const {
  if (delegate_->IsActive()) {
    return TabStyle::TabSelectionState::kActive;
  }

  if (delegate_->IsSelected()) {
    return TabStyle::TabSelectionState::kSelected;
  }

  return TabStyle::TabSelectionState::kInactive;
}

SkScalar VerticalTabStyleViews::GetCornerRadius() const {
  return SkIntToScalar(
      GetLayoutConstant(LayoutConstant::kVerticalTabCornerRadius) +
      (delegate_->IsSplit() ? delegate_->GetView()->GetInsets().height() : 0));
}

gfx::Insets VerticalTabStyleViews::GetContentsInsets() const {
  return gfx::Insets::VH(
      GetLayoutConstant(LayoutConstant::kTabVerticalPadding),
      GetLayoutConstant(LayoutConstant::kTabHorizontalPadding));
}

int VerticalTabStyleViews::GetStrokeThickness() const {
  return delegate_->GetStrokeThickness();
}

SkPath VerticalTabStyleViews::GetOverlinePath(float scale) const {
  return SkPath();
}

bool VerticalTabStyleViews::IsApparentlyActive() const {
  const TabStyle::TabSelectionState selection_state = GetSelectionState();
  if (selection_state == TabStyle::TabSelectionState::kActive) {
    return true;
  }
  if (delegate_->IsHovering()) {
    return delegate_->GetHoverOpacity() > 0.5f;
  }
  return selection_state == TabStyle::TabSelectionState::kSelected;
}

TabStyle::TabColors VerticalTabStyleViews::CalculateTargetColors() const {
  return tab_style()->CalculateTargetColors(
      GetSelectionState(), IsApparentlyActive(), delegate_->IsHovering(),
      delegate_->GetView()->GetWidget()
          ? delegate_->GetView()->GetWidget()->ShouldPaintAsActive()
          : true,
      delegate_->GetView()->GetColorProvider());
}
