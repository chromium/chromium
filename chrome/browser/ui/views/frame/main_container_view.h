// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_CONTAINER_VIEW_H_

#include "ui/views/view.h"

// This view is responsible for holding the primary elements of the Browser UI
// other than the tab strip:
// - TopContainerView
//   - ToolbarView
//   - BookmarksBarView
//   - ContentsSeparator
//   - TopContainerLoadingBar
// - InfobarContainerView
// - ContentContainer
// - SidePanel
class MainContainerView : public views::View {
  METADATA_HEADER(MainContainerView, views::View)

 public:
  MainContainerView();
  MainContainerView(const MainContainerView&) = delete;
  MainContainerView& operator=(const MainContainerView&) = delete;
  ~MainContainerView() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_CONTAINER_VIEW_H_
