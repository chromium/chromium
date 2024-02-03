// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ROUNDED_CORNER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ROUNDED_CORNER_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserView;

namespace gfx {
class Canvas;
}  // namespace gfx

// Draws background akin to the toolbar used for a rounded corner from the
// SidePanel to the page content.
class SidePanelRoundedCorner : public views::View {
  METADATA_HEADER(SidePanelRoundedCorner, views::View)

 public:
  explicit SidePanelRoundedCorner(BrowserView* browser_view);

 private:
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ROUNDED_CORNER_H_
