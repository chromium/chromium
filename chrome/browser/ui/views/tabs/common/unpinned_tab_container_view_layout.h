// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_LAYOUT_H_

#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

class UnpinnedTabContainerView;

// Layout manager responsible for calculating the proposed layouts for
// both vertical and horizontal orientations of UnpinnedTabContainerView.
class UnpinnedTabContainerViewLayout : public views::LayoutManagerBase {
 public:
  explicit UnpinnedTabContainerViewLayout(TabStripOrientation orientation);
  UnpinnedTabContainerViewLayout(const UnpinnedTabContainerViewLayout&) =
      delete;
  UnpinnedTabContainerViewLayout& operator=(
      const UnpinnedTabContainerViewLayout&) = delete;
  ~UnpinnedTabContainerViewLayout() override;

  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
  gfx::Size GetMinimumSize(const views::View* host) const override;

 private:
  views::ProposedLayout CalculateHorizontalLayout(
      const UnpinnedTabContainerView* host,
      const views::SizeBounds& size_bounds) const;
  views::ProposedLayout CalculateVerticalLayout(
      const UnpinnedTabContainerView* host,
      const views::SizeBounds& size_bounds) const;

  gfx::Size CalculateHorizontalMinimumSize(
      const UnpinnedTabContainerView* host) const;
  gfx::Size CalculateVerticalMinimumSize(
      const UnpinnedTabContainerView* host) const;
  const TabStripOrientation orientation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_LAYOUT_H_
