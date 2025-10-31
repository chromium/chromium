// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_REGION_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

// This wrapper view primarily serves to hold the MainContainer and
// ToolbarHeightSidePanel.
class MainRegionView : public views::View {
  METADATA_HEADER(MainRegionView, views::View)

 public:
  explicit MainRegionView(BrowserView& browser_view);
  MainRegionView(const MainRegionView&) = delete;
  MainRegionView& operator=(const MainRegionView&) = delete;
  ~MainRegionView() override;

  void OnPaint(gfx::Canvas* canvas) override;

 private:
  raw_ref<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_REGION_VIEW_H_
