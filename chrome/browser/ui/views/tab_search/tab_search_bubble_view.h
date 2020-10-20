// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_

#include "base/scoped_observer.h"
#include "base/timer/elapsed_timer.h"
#include "ui/views/controls/webview/web_bubble_dialog_view.h"

namespace views {
class Widget;
}  // namespace views

namespace content {
class BrowserContext;
}  // namespace content

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabSearchOpenAction {
  kMouseClick = 0,
  kKeyboardNavigation = 1,
  kKeyboardShortcut = 2,
  kTouchGesture = 3,
  kMaxValue = kTouchGesture,
};

class TabSearchBubbleView : public views::WebBubbleDialogView,
                            public views::WidgetObserver {
 public:
  static views::Widget* CreateTabSearchBubble(
      content::BrowserContext* browser_context,
      views::View* anchor_view);

  TabSearchBubbleView(content::BrowserContext* browser_context,
                      views::View* anchor_view);
  TabSearchBubbleView(const TabSearchBubbleView&) = delete;
  TabSearchBubbleView& operator=(const TabSearchBubbleView&) = delete;
  ~TabSearchBubbleView() override;

  // views::WebBubbleDialogView:
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  const base::Optional<base::ElapsedTimer>& timer_for_testing() {
    return timer_;
  }

 private:
  // Time the Tab Search window has been open.
  base::Optional<base::ElapsedTimer> timer_;

  ScopedObserver<views::Widget, views::WidgetObserver> observed_bubble_widget_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
