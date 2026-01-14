// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_BACKGROUND_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_BACKGROUND_REGION_VIEW_H_

#include "ui/views/view.h"

// This view primarily serves to paint and style the background of the browser
// when visible (e.g. toolbar height side panel is active).
class MainBackgroundRegionView : public views::View {
  METADATA_HEADER(MainBackgroundRegionView, views::View)

 public:
  explicit MainBackgroundRegionView(BrowserView& browser_view);
  ~MainBackgroundRegionView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_BACKGROUND_REGION_VIEW_H_
