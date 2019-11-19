// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_background.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/theme_provider.h"

TopContainerBackground::TopContainerBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {}

void TopContainerBackground::Paint(gfx::Canvas* canvas,
                                   views::View* view) const {
  const ui::ThemeProvider* const theme_provider = view->GetThemeProvider();
  if (theme_provider->HasCustomImage(IDR_THEME_TOOLBAR)) {
    // This is a recapitulation of the logic originally used to place the
    // background image in the bookmarks bar. It is used to ensure backwards-
    // compatibility with existing themes, even though it is not technically
    // correct in all cases.
    gfx::Point pos = view->GetMirroredPosition() +
                     browser_view_->GetMirroredPosition().OffsetFromOrigin();
    pos.Offset(browser_view_->frame()->GetThemeBackgroundXInset(),
               -browser_view_->frame()->GetTopInset());
    const gfx::Rect bounds = view->GetLocalBounds();

    canvas->TileImageInt(*theme_provider->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                         pos.x(), pos.y(), bounds.x(), bounds.y(),
                         bounds.width(), bounds.height());
  } else {
    canvas->DrawColor(theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR));
  }
}
