// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_VIEW_LAYOUT_H_

#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/views/layout/layout_manager_base.h"

namespace views {
class SizeBounds;
}  // namespace views

class TabStripView;

// Layout manager for `TabStripView`. Manages laying out the pinned and
// unpinned scroll views (regions) and the separator between them for both
// horizontal and vertical orientations.
class TabStripViewLayout : public views::LayoutManagerBase {
 public:
  explicit TabStripViewLayout(TabStripOrientation orientation);
  TabStripViewLayout(const TabStripViewLayout&) = delete;
  TabStripViewLayout& operator=(const TabStripViewLayout&) = delete;
  ~TabStripViewLayout() override;

  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  views::ProposedLayout CalculateHorizontalLayout(
      const TabStripView* tab_strip_view,
      const views::SizeBounds& size_bounds) const;
  views::ProposedLayout CalculateVerticalLayout(
      const TabStripView* tab_strip_view,
      const views::SizeBounds& size_bounds) const;

  const TabStripOrientation orientation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_STRIP_VIEW_LAYOUT_H_
