// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_

#include "base/memory/raw_ptr.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view.h"

class TabGroupViews;

// View for tab group underlines in the tab strip, which are markers of group
// members. Underlines are included in the tab
// strip flow and positioned across all tabs in the group, as well as the group
// header. There is one underline for the tabs in the TabContainer, and another
// for any tabs in the group that are being dragged. These merge visually into a
// single underline, but must be separate views so that paint order requirements
// can be met.
class TabGroupUnderline : public views::View {
 public:
  METADATA_HEADER(TabGroupUnderline);

  static constexpr int kStrokeThickness =
      views::FocusRing::kDefaultHaloThickness;

  static int GetStrokeInset();

  TabGroupUnderline(TabGroupViews* tab_group_views,
                    const tab_groups::TabGroupId& group);
  TabGroupUnderline(const TabGroupUnderline&) = delete;
  TabGroupUnderline& operator=(const TabGroupUnderline&) = delete;

  // Updates the bounds of the underline for painting.
  void UpdateBounds(views::View* leading_view, views::View* trailing_view);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Returns the insets from |sibling_view|'s bounds this underline would have
  // if it were underlining only |sibling_view|.
  gfx::Insets GetInsetsForUnderline(views::View* sibling_view) const;

  // The underline is a straight line with half-rounded endcaps. Since this
  // geometry is nontrivial to represent using primitives, it's instead
  // represented using a fill path.
  SkPath GetPath() const;

  const raw_ptr<TabGroupViews> tab_group_views_;
  const tab_groups::TabGroupId group_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
