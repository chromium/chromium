// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_HORIZONTAL_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_HORIZONTAL_LAYOUT_H_

#include "chrome/browser/ui/views/tabs/common/tab_view.h"

class TabViewHorizontalLayout : public TabView::LayoutManager {
 public:
  TabViewHorizontalLayout();
  TabViewHorizontalLayout(const TabViewHorizontalLayout&) = delete;
  TabViewHorizontalLayout& operator=(const TabViewHorizontalLayout&) = delete;
  ~TabViewHorizontalLayout() override;

  // TabView::LayoutManager:
  void OnTabClosing() override;

 protected:
  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  struct ChildVisibilities {
    bool showing_icon = false;
    bool showing_alert_indicator = false;
    bool showing_close_button = false;
    bool show_title = false;
    bool center_icon = false;
    bool extra_alert_indicator_padding = false;
  };
  ChildVisibilities CalculateChildVisibilities(int width,
                                               int available_width) const;

  // When a horizontal tab starts closing, preserve the initial center_icon_
  // state so that child views' relative positions within the tab stay constant
  // while the tab bounds animate.
  std::optional<bool> center_icon_override_ = std::nullopt;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_HORIZONTAL_LAYOUT_H_
