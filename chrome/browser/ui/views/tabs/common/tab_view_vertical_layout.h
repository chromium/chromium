// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_VERTICAL_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_VERTICAL_LAYOUT_H_

#include "chrome/browser/ui/views/tabs/common/tab_view.h"

enum class TabStripOrientation;

namespace views {
struct ProposedLayout;
}

class TabViewVerticalLayout : public TabView::LayoutManager {
 public:
  TabViewVerticalLayout();
  TabViewVerticalLayout(const TabViewVerticalLayout&) = delete;
  TabViewVerticalLayout& operator=(const TabViewVerticalLayout&) = delete;
  ~TabViewVerticalLayout() override;

 protected:
  // views::LayoutManagerBase:
  void OnInstalled(views::View* host) override;
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  struct TabChildConfig {
    raw_ptr<views::View> view = nullptr;
    int min_width = 0;
    int padding = 0;
    bool align_leading = false;
    bool expand = false;
    // Some alert indicators need to decorate the close button when the tab
    // strip is collapsed. In that case, center the child and set a size of (0,
    // 0).
    bool decorate_on_collapse = false;
  };

  gfx::Rect GetChildBounds(const gfx::Rect& container,
                           const TabChildConfig& config,
                           const bool center) const;

  // Calculates the visibility of child view based on various states.
  bool IsChildVisible(const views::View* child, const int width) const;

  // Ordered vector of children to be rendered in the tab.
  std::vector<TabChildConfig> tab_children_configs_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_VERTICAL_LAYOUT_H_
