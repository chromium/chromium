// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_CONTENT_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

// SidePanelAnimationContentView is a view container that hosts side panel
// content during side panel transition animations.
class SidePanelAnimationContentView : public views::View {
  METADATA_HEADER(SidePanelAnimationContentView, views::View)

 public:
  SidePanelAnimationContentView();
  explicit SidePanelAnimationContentView(views::View* content);
  SidePanelAnimationContentView(const SidePanelAnimationContentView&) = delete;
  SidePanelAnimationContentView& operator=(
      const SidePanelAnimationContentView&) = delete;
  ~SidePanelAnimationContentView() override;

  void UpdateHorizontalTranslation(double translation_x);
  void ClipBounds(const gfx::Rect& bounds);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_CONTENT_VIEW_H_
