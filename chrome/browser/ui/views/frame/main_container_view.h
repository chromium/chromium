// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_CONTAINER_VIEW_H_

#include "base/memory/raw_ref.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"

class BrowserView;

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
  explicit MainContainerView(BrowserView& browser_view);
  MainContainerView(const MainContainerView&) = delete;
  MainContainerView& operator=(const MainContainerView&) = delete;
  ~MainContainerView() override;

  void SetShadowVisiblityAndRoundedCorners(bool visibile);

 private:
  const raw_ref<BrowserView> browser_view_;

  // The shadow and elevation around main_container to visually separate the
  // container from MainRegionBackground when the toolbar_height_side_panel is
  // visible.
  std::unique_ptr<views::ViewShadow> view_shadow_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MAIN_CONTAINER_VIEW_H_
