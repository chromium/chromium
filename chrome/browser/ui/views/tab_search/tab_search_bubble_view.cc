// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

// static.
views::Widget* TabSearchBubbleView::CreateTabSearchBubble(
    content::BrowserContext* browser_context,
    views::View* anchor_view) {
  return views::WebBubbleDialogView::CreateWebBubbleDialog<TabSearchUI>(
      std::make_unique<TabSearchBubbleView>(browser_context, anchor_view),
      GURL(chrome::kChromeUITabSearchURL));
}

TabSearchBubbleView::TabSearchBubbleView(
    content::BrowserContext* browser_context,
    views::View* anchor_view)
    : WebBubbleDialogView(browser_context, anchor_view) {
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_view()->GetWebContents());
}

TabSearchBubbleView::~TabSearchBubbleView() {
  if (timer_.has_value()) {
    UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration2",
                            timer_->Elapsed());
  }
}

void TabSearchBubbleView::AddedToWidget() {
  WebBubbleDialogView::AddedToWidget();
  observed_bubble_widget_.Add(GetWidget());
}

void TabSearchBubbleView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                    bool visible) {
  if (GetWidget() == widget && visible && !timer_.has_value()) {
    timer_ = base::ElapsedTimer();
  }
}
