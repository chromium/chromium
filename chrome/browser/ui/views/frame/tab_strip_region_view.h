// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_

#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/accessible_pane_view.h"

class TabSearchButton;
class TabStrip;

// Container for the tabstrip, new tab button, and reserved grab handle space.
// TODO (https://crbug.com/949660) Under construction.
class TabStripRegionView final : public views::AccessiblePaneView {
 public:
  explicit TabStripRegionView(std::unique_ptr<TabStrip> tab_strip);
  ~TabStripRegionView() override;

  // Returns true if the specified rect intersects the window caption area of
  // the browser window. |rect| is in the local coordinate space
  // of |this|.
  bool IsRectInWindowCaption(const gfx::Rect& rect);

  // A convenience function which calls |IsRectInWindowCaption()| with a rect of
  // size 1x1 and an origin of |point|. |point| is in the local coordinate space
  // of |this|.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the colors of the frame change.
  void FrameColorsChanged();

  TabSearchButton* tab_search_button() { return tab_search_button_; }

  // views::AccessiblePaneView:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  views::View* GetDefaultFocusableChild() override;

  // TODO(958173): Override OnBoundsChanged to cancel tabstrip animations.

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStripRegionView);

  int CalculateTabStripAvailableWidth();

  views::View* tab_strip_container_;
  TabStrip* tab_strip_;
  TabSearchButton* tab_search_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
