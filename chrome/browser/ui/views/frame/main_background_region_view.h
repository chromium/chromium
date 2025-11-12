// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_BACKGROUND_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_BACKGROUND_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

// This wrapper view primarily serves to paint and style the background of the
// browser when visible (e.g. toolbar height side panel is active).
class MainBackgroundRegionView : public views::View {
  METADATA_HEADER(MainBackgroundRegionView, views::View)

 public:
  explicit MainBackgroundRegionView(BrowserView& browser_view);
  MainBackgroundRegionView(const MainBackgroundRegionView&) = delete;
  MainBackgroundRegionView& operator=(const MainBackgroundRegionView&) = delete;
  ~MainBackgroundRegionView() override;

  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  raw_ref<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_BACKGROUND_REGION_VIEW_H_
