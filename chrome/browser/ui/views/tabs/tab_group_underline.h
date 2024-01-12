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
class TabGroupStyle;

// View for tab group underlines in the tab strip, which are markers of group
// members. Underlines are included in the tab
// strip flow and positioned across all tabs in the group, as well as the group
// header. There is one underline for the tabs in the TabContainer, and another
// for any tabs in the group that are being dragged. These merge visually into a
// single underline, but must be separate views so that paint order requirements
// can be met.
class TabGroupUnderline : public views::View {
  METADATA_HEADER(TabGroupUnderline, views::View)

 public:
  static constexpr int kStrokeThickness =
      views::FocusRing::kDefaultHaloThickness;

  static int GetStrokeInset();

  TabGroupUnderline(TabGroupViews* tab_group_views,
                    const tab_groups::TabGroupId& group,
                    const TabGroupStyle& style);
  TabGroupUnderline(const TabGroupUnderline&) = delete;
  TabGroupUnderline& operator=(const TabGroupUnderline&) = delete;

  // Updates the bounds of the underline for painting.
  void UpdateBounds(const views::View* leading_view,
                    const views::View* trailing_view);
  // Checks if the `TabGroupUnderline` should be hidden before
  // setting the visibility.
  void MaybeSetVisible(bool visible);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Returns the insets from `sibling_view`'s bounds this underline would have
  // if it were underlining only `sibling_view`.
  gfx::Insets GetInsetsForUnderline(const views::View* sibling_view) const;
  // Returns the tab group underline bounds based on a `leading_view` and a
  // `trailing_view`.
  gfx::Rect CalculateTabGroupUnderlineBounds(
      const views::View* underline_view,
      const views::View* leading_view,
      const views::View* trailing_view) const;

  const raw_ptr<TabGroupViews> tab_group_views_;
  const tab_groups::TabGroupId group_;
  const raw_ref<const TabGroupStyle> style_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
