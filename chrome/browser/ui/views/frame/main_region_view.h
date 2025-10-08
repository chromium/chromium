// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_REGION_VIEW_H_

#include "ui/views/view.h"

// This wrapper view primarily serves to hold the MainContainer and
// ToolbarHeightSidePanel.
class MainRegionView : public views::View {
  METADATA_HEADER(MainRegionView, views::View)

 public:
  MainRegionView();
  MainRegionView(const MainRegionView&) = delete;
  MainRegionView& operator=(const MainRegionView&) = delete;
  ~MainRegionView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_REGION_VIEW_H_
