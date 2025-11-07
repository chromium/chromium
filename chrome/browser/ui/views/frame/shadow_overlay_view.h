// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_

#include "ui/views/view.h"

// This view is responsible for framing the primary elements of the UI when
// toolbar height side panel is showing, providing a nice drop shadow.
class ShadowOverlayView : public views::View {
  METADATA_HEADER(ShadowOverlayView, views::View)

 public:
  ShadowOverlayView();
  ~ShadowOverlayView() override;

  class ShadowBox;

 private:
  // views::View:
  void VisibilityChanged(View* starting_from, bool visible) override;
  void OnPaint(gfx::Canvas* canvas) override;

  raw_ptr<ShadowBox> shadow_box_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_OVERLAY_VIEW_H_
