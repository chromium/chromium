// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_PINNED_TAB_CONTAINER_VIEW_HORIZONTAL_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_PINNED_TAB_CONTAINER_VIEW_HORIZONTAL_LAYOUT_H_

#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"

namespace views {
class SizeBounds;
struct ProposedLayout;
}  // namespace views

class PinnedTabContainerViewHorizontalLayout
    : public PinnedTabContainerView::LayoutManager {
 public:
  PinnedTabContainerViewHorizontalLayout();
  PinnedTabContainerViewHorizontalLayout(
      const PinnedTabContainerViewHorizontalLayout&) = delete;
  PinnedTabContainerViewHorizontalLayout& operator=(
      const PinnedTabContainerViewHorizontalLayout&) = delete;
  ~PinnedTabContainerViewHorizontalLayout() override;

  // views::LayoutManagerBase:
  gfx::Size GetMinimumSize(const views::View* host) const override;

 protected:
  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_PINNED_TAB_CONTAINER_VIEW_HORIZONTAL_LAYOUT_H_
