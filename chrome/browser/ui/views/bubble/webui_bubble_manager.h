// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_

#include <memory>
#include <utility>

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

class GURL;
class WebUIBubbleDialogView;

// WebUIBubbleManagerBase handles the creation / destruction of the WebUI bubble
// and caching of the WebView.
class WebUIBubbleManagerBase : public views::WidgetObserver {
 public:
  WebUIBubbleManagerBase(views::View* anchor_view,
                         content::BrowserContext* browser_context,
                         const GURL& webui_url);
  WebUIBubbleManagerBase(const WebUIBubbleManagerBase&) = delete;
  const WebUIBubbleManagerBase& operator=(const WebUIBubbleManagerBase&) =
      delete;
  ~WebUIBubbleManagerBase() override;

  bool ShowBubble();
  void CloseBubble();
  views::Widget* GetBubbleWidget() const;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  content::BrowserContext* browser_context() { return browser_context_; }
  const GURL& webui_url() const { return webui_url_; }

 private:
  virtual std::unique_ptr<WebUIBubbleView> CreateWebView() = 0;
  void ResetWebView();

  views::View* anchor_view_;
  content::BrowserContext* browser_context_;
  GURL webui_url_;
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  // A cached WebView used to make re-triggering the UI faster. This is not set
  // when the bubble is showing. It will only be set when the bubble is
  // not showing. It is only retained for the length of the |cache_timer_|.
  std::unique_ptr<WebUIBubbleView> cached_web_view_;

  // A timer controlling how long the |cached_web_view_| is cached for.
  std::unique_ptr<base::RetainingOneShotTimer> cache_timer_;

  ScopedObserver<views::Widget, views::WidgetObserver> observed_bubble_widget_{
      this};
};

template <typename T>
class WebUIBubbleManager : public WebUIBubbleManagerBase {
 public:
  using WebUIBubbleManagerBase::WebUIBubbleManagerBase;

 private:
  std::unique_ptr<WebUIBubbleView> CreateWebView() override {
    auto web_view = std::make_unique<WebUIBubbleView>(browser_context());
    web_view->template LoadURL<T>(webui_url());
    return web_view;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
