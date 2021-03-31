// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_

#include "components/tab_groups/tab_group_id.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabGroupViews;

// View for tab group underlines in the tab strip, which are markers of group
// members. There is one underline for each group, which is included in the tab
// strip flow and positioned across all tabs in the group.
class TabGroupUnderline : public views::View {
 public:
  METADATA_HEADER(TabGroupUnderline);

  static constexpr int kStrokeThickness = 3;
  static int GetStrokeInset();

  TabGroupUnderline(TabGroupViews* tab_group_views,
                    const tab_groups::TabGroupId& group);
  TabGroupUnderline(const TabGroupUnderline&) = delete;
  TabGroupUnderline& operator=(const TabGroupUnderline&) = delete;

  // Updates the bounds of the underline for painting, given the current bounds
  // of the group.
  void UpdateBounds(const gfx::Rect& group_bounds);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // The underline starts at the left edge of the header chip.
  int GetStart(const gfx::Rect& group_bounds) const;

  // The underline ends at the right edge of the last grouped tab's close
  // button. If the last grouped tab is active, the underline ends at the
  // right edge of the active tab border stroke.
  int GetEnd(const gfx::Rect& group_bounds) const;

  // The underline is a straight line with half-rounded endcaps. Since this
  // geometry is nontrivial to represent using primitives, it's instead
  // represented using a fill path.
  SkPath GetPath() const;

  TabGroupViews* const tab_group_views_;
  const tab_groups::TabGroupId group_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
