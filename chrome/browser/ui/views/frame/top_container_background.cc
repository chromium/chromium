// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_background.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"

TopContainerBackground::TopContainerBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {}

void TopContainerBackground::Paint(gfx::Canvas* canvas,
                                   views::View* view) const {
  PaintBackground(canvas, view, browser_view_,
                  /*translate_view_coordinates=*/false);
}

bool TopContainerBackground::PaintThemeCustomImage(
    gfx::Canvas* canvas,
    const views::View* view,
    const BrowserView* browser_view,
    bool translate_view_coordinates) {
  const ui::ThemeProvider* const theme_provider = view->GetThemeProvider();
  if (!theme_provider->HasCustomImage(IDR_THEME_TOOLBAR)) {
    return false;
  }

  // This is a recapitulation of the logic originally used to place the
  // background image in the bookmarks bar. It is used to ensure backwards-
  // compatibility with existing themes, even though it is not technically
  // correct in all cases.
  gfx::Point view_offset = view->GetMirroredPosition();
  // TODO(pbos): See if we can figure out how to translate correctly
  // unconditionally from this bool.
  if (translate_view_coordinates) {
    views::View::ConvertPointToTarget(view, browser_view, &view_offset);
  }
  gfx::Point pos =
      view_offset + browser_view->GetMirroredPosition().OffsetFromOrigin();
  pos.Offset(browser_view->frame()->GetThemeBackgroundXInset(),
             -browser_view->tabstrip()->GetStrokeThickness() -
                 browser_view->frame()->GetTopInset());
  const gfx::Rect bounds = view->GetLocalBounds();

  canvas->TileImageInt(*theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                       pos.x(), pos.y(), bounds.x(), bounds.y(), bounds.width(),
                       bounds.height(), 1.0f, SkTileMode::kRepeat,
                       SkTileMode::kMirror);
  return true;
}

void TopContainerBackground::PaintBackground(gfx::Canvas* canvas,
                                             const views::View* view,
                                             const BrowserView* browser_view,
                                             bool translate_view_coordinates) {
  bool painted = PaintThemeCustomImage(canvas, view, browser_view,
                                       translate_view_coordinates);
  if (!painted) {
    canvas->DrawColor(view->GetColorProvider()->GetColor(kColorToolbar));
  }
}
