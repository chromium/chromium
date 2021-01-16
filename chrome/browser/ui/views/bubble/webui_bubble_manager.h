// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_

#include <memory>
#include <utility>

#include "base/scoped_observation.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_view.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
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
  explicit WebUIBubbleManagerBase(views::View* anchor_view);
  WebUIBubbleManagerBase(const WebUIBubbleManagerBase&) = delete;
  const WebUIBubbleManagerBase& operator=(const WebUIBubbleManagerBase&) =
      delete;
  ~WebUIBubbleManagerBase() override;

  bool ShowBubble();
  void CloseBubble();
  views::Widget* GetBubbleWidget() const;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  bool bubble_using_cached_webview() const {
    return bubble_using_cached_webview_;
  }

  void ResetWebViewForTesting();
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_for_testing() {
    return bubble_view_;
  }

 protected:
  WebUIBubbleView* cached_web_view() { return cached_web_view_.get(); }

 private:
  virtual std::unique_ptr<WebUIBubbleView> CreateWebView() = 0;
  virtual void WebViewHidden() = 0;
  void ResetWebView();

  views::View* anchor_view_;
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  // Tracks whether the current bubble was created by reusing
  // |cached_web_view_|.
  bool bubble_using_cached_webview_ = false;

  // A cached WebView used to make re-triggering the UI faster. This is not set
  // when the bubble is showing. It will only be set when the bubble is
  // not showing. It is only retained for the length of the |cache_timer_|.
  std::unique_ptr<WebUIBubbleView> cached_web_view_;

  // A timer controlling how long the |cached_web_view_| is cached for.
  std::unique_ptr<base::RetainingOneShotTimer> cache_timer_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  // This is necessary to prevent a bug closing the active tab in the bubble.
  // See https://crbug.com/1139028.
  std::unique_ptr<CloseBubbleOnTabActivationHelper> close_bubble_helper_;
};

template <typename T>
class WebUIBubbleManager : public WebUIBubbleManagerBase {
 public:
  WebUIBubbleManager(int task_manager_string_id,
                     views::View* anchor_view,
                     content::BrowserContext* browser_context,
                     const GURL& webui_url,
                     bool enable_extension_apis = false)
      : WebUIBubbleManagerBase(anchor_view),
        task_manager_string_id_(task_manager_string_id),
        browser_context_(browser_context),
        webui_url_(webui_url),
        enable_extension_apis_(enable_extension_apis) {}
  ~WebUIBubbleManager() override = default;

 private:
  std::unique_ptr<WebUIBubbleView> CreateWebView() override {
    auto web_view = std::make_unique<WebUIBubbleView>(browser_context_);
    content::WebContents* web_contents = web_view->GetWebContents();
    if (enable_extension_apis_) {
      // In order for the WebUI in the renderer to use extensions APIs we must
      // add a ChromeExtensionWebContentsObserver to the WebView's WebContents.
      extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
          web_contents);
    }

    task_manager::WebContentsTags::CreateForToolContents(
        web_contents, task_manager_string_id_);
    web_view->template LoadURL<T>(webui_url_);
    return web_view;
  }

  void WebViewHidden() override {
    DCHECK(cached_web_view());
    return cached_web_view()
        ->template GetWebUIController<T>()
        ->EmbedderHidden();
  }

  // Used for tagging the web contents so that a distinctive name shows up in
  // the task manager.
  const int task_manager_string_id_;
  content::BrowserContext* browser_context_;
  const GURL webui_url_;
  const bool enable_extension_apis_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
