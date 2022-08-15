// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/overflow_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class ImageButton;
}

class TabStrip;

// Allows the TabStrip to be scrolled back and forth when there are more tabs
// than can be displayed at one time. When the TabStrip is scrollable, displays
// buttons that control the scrolling.
class TabStripScrollContainer : public views::View, views::ViewObserver {
 public:
  METADATA_HEADER(TabStripScrollContainer);
  explicit TabStripScrollContainer(std::unique_ptr<TabStrip> tab_strip);
  TabStripScrollContainer(const TabStripScrollContainer&) = delete;
  TabStripScrollContainer& operator=(const TabStripScrollContainer&) = delete;
  ~TabStripScrollContainer() override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(View* view) override;

  bool IsRectInWindowCaption(const gfx::Rect& rect);

  void OnContentsScrolledCallback();

  raw_ptr<views::ImageButton> GetLeadingScrollButtonForTesting() {
    return leading_scroll_button_;
  }

  raw_ptr<views::ImageButton> GetTrailingScrollButtonForTesting() {
    return trailing_scroll_button_;
  }

 private:
  int GetTabStripAvailableWidth() const;

  // Scrolls the tabstrip towards the first tab in the tabstrip.
  void ScrollTowardsLeadingTab();

  // Scrolls the tabstrip towards the last tab in the tabstrip.
  void ScrollTowardsTrailingTab();

  // Subscription for scrolling of content view
  base::CallbackListSubscription on_contents_scrolled_subscription_;

  void FrameColorsChanged();

  // views::View
  void OnThemeChanged() override;

  // Manages the visibility of the scroll buttons based on whether |tab_strip_|
  // is currently overflowing.
  raw_ptr<OverflowView> overflow_view_;

  // Actually scrolls |tab_strip_|.
  raw_ptr<views::ScrollView> scroll_view_;
  raw_ptr<TabStrip> tab_strip_;

  // The buttons that allow users to manually scroll |tab_strip_|.
  raw_ptr<views::ImageButton> leading_scroll_button_;
  raw_ptr<views::ImageButton> trailing_scroll_button_;

  // The views, owned by |scroll_view_|, that indicate that there are more
  // tabs overflowing to the left or right.
  raw_ptr<views::View> left_overflow_indicator_;
  raw_ptr<views::View> right_overflow_indicator_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_CONTAINER_H_
