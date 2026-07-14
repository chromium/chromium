// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_VIEW_LAYOUT_H_

#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

class TabGroupView;

// Layout manager responsible for calculating the proposed layouts for
// both vertical and horizontal orientations of TabGroupView.
class TabGroupViewLayout : public views::LayoutManagerBase {
 public:
  explicit TabGroupViewLayout(TabStripOrientation orientation);
  TabGroupViewLayout(const TabGroupViewLayout&) = delete;
  TabGroupViewLayout& operator=(const TabGroupViewLayout&) = delete;
  ~TabGroupViewLayout() override;

  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
  gfx::Size GetMinimumSize(const views::View* host) const override;

 private:
  views::ProposedLayout CalculateHorizontalLayout(
      const TabGroupView* tab_group_view,
      const views::SizeBounds& size_bounds) const;
  views::ProposedLayout CalculateVerticalLayout(
      const TabGroupView* tab_group_view,
      const views::SizeBounds& size_bounds) const;

  gfx::Size CalculateHorizontalMinimumSize(
      const TabGroupView* tab_group_view) const;
  gfx::Size CalculateVerticalMinimumSize(
      const TabGroupView* tab_group_view) const;

  const TabStripOrientation orientation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_VIEW_LAYOUT_H_
