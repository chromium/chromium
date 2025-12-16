// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_like_background.h"

#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"

TabStripLikeBackground::TabStripLikeBackground(BrowserView* browser_view)
    : browser_view_(browser_view) {}

void TabStripLikeBackground::Paint(gfx::Canvas* canvas,
                                   views::View* view) const {
  bool painted = TopContainerBackground::PaintThemeCustomImage(canvas, view,
                                                               browser_view_);

  if (!painted) {
    SkColor frame_color;
    frame_color =
        browser_view_->browser_widget()->GetFrameView()->GetFrameColor(
            BrowserFrameActiveState::kUseCurrent);
    canvas->DrawColor(frame_color);
  }
}
