// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/overflow_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class ImageButton;
}

class TabStrip;
class TabStripScrollingOverflowIndicatorStrategy;

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

  views::ImageButton* GetLeadingScrollButtonForTesting() {
    return leading_scroll_button_;
  }

  views::ImageButton* GetTrailingScrollButtonForTesting() {
    return trailing_scroll_button_;
  }

  // Update the background colors when frame active state changes.
  void FrameColorsChanged();

 private:
  int GetTabStripAvailableWidth() const;

  // Scrolls the tabstrip towards the first tab in the tabstrip.
  void ScrollTowardsLeadingTab();

  // Scrolls the tabstrip towards the last tab in the tabstrip.
  void ScrollTowardsTrailingTab();

  // enable or disable the scroll buttons based on the scroll position
  void MaybeUpdateScrollButtonState();

  // Subscription for scrolling of content view
  base::CallbackListSubscription on_contents_scrolled_subscription_;

  // views::View
  void OnThemeChanged() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // Manages the visibility of the scroll buttons based on whether |tab_strip_|
  // is currently overflowing.
  raw_ptr<OverflowView> overflow_view_;

  // Actually scrolls |tab_strip_|.
  raw_ptr<views::ScrollView> scroll_view_;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_;

  // The buttons that allow users to manually scroll |tab_strip_|.
  raw_ptr<views::ImageButton> leading_scroll_button_;
  raw_ptr<views::ImageButton> trailing_scroll_button_;

  // The class handling the overflow indiciators for the scroll view.
  std::unique_ptr<TabStripScrollingOverflowIndicatorStrategy>
      overflow_indicator_strategy_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_SCROLL_CONTAINER_H_
