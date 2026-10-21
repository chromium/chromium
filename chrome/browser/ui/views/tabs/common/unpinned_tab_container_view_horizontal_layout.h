// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_HORIZONTAL_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_HORIZONTAL_LAYOUT_H_

#include <optional>

#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"

namespace views {
class SizeBounds;
struct ProposedLayout;
}  // namespace views

class UnpinnedTabContainerViewHorizontalLayout
    : public UnpinnedTabContainerView::LayoutManager {
 public:
  UnpinnedTabContainerViewHorizontalLayout();
  UnpinnedTabContainerViewHorizontalLayout(
      const UnpinnedTabContainerViewHorizontalLayout&) = delete;
  UnpinnedTabContainerViewHorizontalLayout& operator=(
      const UnpinnedTabContainerViewHorizontalLayout&) = delete;
  ~UnpinnedTabContainerViewHorizontalLayout() override;

  // UnpinnedTabContainerView::LayoutManager:
  int GetUnconstrainedPreferredWidth() const override;

  // views::LayoutManagerBase:
  gfx::Size GetMinimumSize(const views::View* host) const override;

 protected:
  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  std::optional<int> GetClosingModeOverrideWidth() const;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_UNPINNED_TAB_CONTAINER_VIEW_HORIZONTAL_LAYOUT_H_
