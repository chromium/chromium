// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_background.h"

#include <optional>

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"

namespace {

bool WillPaintCustomImage(const views::View* view, int id) {
  const ui::ThemeProvider* const theme_provider = view->GetThemeProvider();
  return theme_provider->HasCustomImage(id);
}

bool IsFrameActive(const views::View* view) {
  return view->GetWidget() ? view->GetWidget()->ShouldPaintAsActive() : true;
}

std::pair<int, ui::ColorId> GetColorChoiceInfo(
    TopContainerBackground::ColorChoice color_choice,
    const views::View* view) {
  switch (color_choice) {
    case TopContainerBackground::ColorChoice::kToolbarColor:
      return {IDR_THEME_TOOLBAR, kColorToolbar};
    case TopContainerBackground::ColorChoice::kFrameColor:
      if (IsFrameActive(view)) {
        return {IDR_THEME_FRAME, ui::kColorFrameActive};
      } else {
        return {IDR_THEME_FRAME_INACTIVE, ui::kColorFrameInactive};
      }
    default:
      NOTREACHED();
  }
}

}  // namespace

TopContainerBackground::TopContainerBackground(BrowserView* browser_view,
                                               ColorChoice color_choice)
    : browser_view_(browser_view), color_choice_(color_choice) {}

void TopContainerBackground::Paint(gfx::Canvas* canvas,
                                   views::View* view) const {
  PaintBackground(canvas, view, browser_view_, color_choice_);
}

bool TopContainerBackground::PaintThemeCustomImage(
    gfx::Canvas* canvas,
    const views::View* view,
    const BrowserView* browser_view,
    ColorChoice color_choice) {
  int theme_resource_id = GetColorChoiceInfo(color_choice, view).first;

  if (!WillPaintCustomImage(view, theme_resource_id)) {
    return false;
  }

  const ui::ThemeProvider* const theme_provider = view->GetThemeProvider();
  PaintThemeAlignedImage(canvas, view, browser_view,
                         theme_provider->GetImageSkiaNamed(theme_resource_id));

  return true;
}

void TopContainerBackground::PaintThemeAlignedImage(
    gfx::Canvas* canvas,
    const views::View* view,
    const BrowserView* browser_view,
    gfx::ImageSkia* image) {
  // Get the origin of this view and translate it to coordinate system of the
  // BrowserView.
  gfx::Point pos;
  views::View::ConvertPointToTarget(view, browser_view, &pos);

  // Add in the translation to account for positioning of the theme image
  // relative of the origin of BrowserView.
  pos.Offset(0, ThemeProperties::kFrameHeightAboveTabs);

  // Make sure the the background will cover the entire clip path region.
  // TODO(crbug.com/41344902): Remove the clip code and just use local bounds
  // once the pixel canvas is enabled on all aura platforms.
  SkPath clip_path = view->clip_path();
  SkRect rect = clip_path.getBounds();
  const gfx::Rect bounds =
      !clip_path.isEmpty()
          ? gfx::Rect(rect.x(), rect.y(), rect.width(), rect.height())
          : view->GetLocalBounds();
  canvas->TileImageInt(*image, pos.x(), pos.y(), bounds.x(), bounds.y(),
                       bounds.width(), bounds.height(), 1.0f,
                       SkTileMode::kRepeat, SkTileMode::kMirror);
}

void TopContainerBackground::PaintBackground(gfx::Canvas* canvas,
                                             const views::View* view,
                                             const BrowserView* browser_view,
                                             ColorChoice color_choice) {
  bool painted =
      PaintThemeCustomImage(canvas, view, browser_view, color_choice);
  if (!painted) {
    canvas->DrawColor(view->GetColorProvider()->GetColor(
        GetColorChoiceInfo(color_choice, view).second));
  }
}

std::optional<SkColor> TopContainerBackground::GetBackgroundColor(
    const views::View* view,
    const BrowserView* browser_view,
    ColorChoice color_choice) {
  std::pair<int, ui::ColorId> color_choice_info =
      GetColorChoiceInfo(color_choice, view);
  const bool will_be_painted =
      WillPaintCustomImage(view, color_choice_info.first);
  if (!will_be_painted) {
    return view->GetColorProvider()->GetColor(color_choice_info.second);
  }

  return std::nullopt;
}
