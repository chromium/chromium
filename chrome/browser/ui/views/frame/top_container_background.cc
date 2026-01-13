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

std::pair<int, ui::ColorId> GetTopChromeInfo(
    TopContainerBackground::TopChromeArea top_chrome_area,
    const views::View* view) {
  switch (top_chrome_area) {
    case TopContainerBackground::TopChromeArea::TOOLBAR:
      return {IDR_THEME_TOOLBAR, kColorToolbar};
    case TopContainerBackground::TopChromeArea::FRAME:
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
                                               TopChromeArea top_chrome_area)
    : browser_view_(browser_view), top_chrome_area_(top_chrome_area) {}

void TopContainerBackground::Paint(gfx::Canvas* canvas,
                                   views::View* view) const {
  PaintBackground(canvas, view, browser_view_, top_chrome_area_);
}

bool TopContainerBackground::PaintThemeCustomImage(
    gfx::Canvas* canvas,
    const views::View* view,
    const BrowserView* browser_view,
    TopChromeArea top_chrome_area) {
  int theme_resource_id = GetTopChromeInfo(top_chrome_area, view).first;

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
                                             TopChromeArea top_chrome_area) {
  bool painted =
      PaintThemeCustomImage(canvas, view, browser_view, top_chrome_area);
  if (!painted) {
    canvas->DrawColor(view->GetColorProvider()->GetColor(
        GetTopChromeInfo(top_chrome_area, view).second));
  }
}

std::optional<SkColor> TopContainerBackground::GetBackgroundColor(
    const views::View* view,
    const BrowserView* browser_view,
    TopChromeArea top_chrome_area) {
  std::pair<int, ui::ColorId> top_chrome_info =
      GetTopChromeInfo(top_chrome_area, view);
  const bool will_be_painted =
      WillPaintCustomImage(view, top_chrome_info.first);
  if (!will_be_painted) {
    return view->GetColorProvider()->GetColor(top_chrome_info.second);
  }

  return std::nullopt;
}
