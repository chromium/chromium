// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_

#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "ui/views/view.h"

class TabStrip;

// View for tab group underlines in the tab strip, which are markers of group
// members. There is one underline for each group, which is included in the tab
// strip flow and positioned across all tabs in the group.
class TabGroupUnderline : public views::View {
 public:
  static constexpr int kStrokeThickness = 3;
  static int GetStrokeInset();

  TabGroupUnderline(TabStrip* tab_strip, TabGroupId group);

  TabGroupId group() const { return group_; }

  // Updates the bounds of the underline for painting.
  void UpdateBounds();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // The underline starts at the left edge of the header chip.
  int GetStart() const;

  // The underline ends at the right edge of the last grouped tab's close
  // button. If the last grouped tab is active, the underline ends at the
  // right edge of the active tab border stroke.
  int GetEnd() const;

  // The underline is a straight line with half-rounded endcaps. Since this
  // geometry is nontrivial to represent using primitives, it's instead
  // represented using a fill path.
  SkPath GetPath() const;

  // The underline color is the group color.
  SkColor GetColor() const;

  TabStrip* const tab_strip_;
  const TabGroupId group_;

  DISALLOW_COPY_AND_ASSIGN(TabGroupUnderline);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
