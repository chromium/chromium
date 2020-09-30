// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_

#include "base/scoped_observer.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui_embedder.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Widget;
class WebView;
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

class TabSearchBubbleView : public views::BubbleDialogDelegateView,
                            public TabSearchUIEmbedder,
                            public views::WidgetObserver {
 public:
  // TODO(tluk): Since the Bubble is shown asynchronously, we shouldn't call
  // this if the Widget is hidden and yet to be revealed.
  static views::Widget* CreateTabSearchBubble(
      content::BrowserContext* browser_context,
      views::View* anchor_view);

  TabSearchBubbleView(content::BrowserContext* browser_context,
                      views::View* anchor_view);
  ~TabSearchBubbleView() override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // TabSearchUIEmbedder:
  void ShowBubble() override;
  void CloseBubble() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  void OnWebViewSizeChanged();

  views::WebView* web_view_for_testing() { return web_view_; }

 private:
  views::WebView* web_view_;

  // Time the Tab Search window has been open.
  base::Optional<base::ElapsedTimer> timer_;

  ScopedObserver<views::Widget, views::WidgetObserver> observed_bubble_widget_{
      this};

  DISALLOW_COPY_AND_ASSIGN(TabSearchBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SEARCH_TAB_SEARCH_BUBBLE_VIEW_H_
