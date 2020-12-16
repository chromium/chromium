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
  WebUIBubbleManagerBase(int task_manager_string_id,
                         views::View* anchor_view,
                         content::BrowserContext* browser_context,
                         const GURL& webui_url,
                         bool enable_extension_apis = false);
  WebUIBubbleManagerBase(const WebUIBubbleManagerBase&) = delete;
  const WebUIBubbleManagerBase& operator=(const WebUIBubbleManagerBase&) =
      delete;
  ~WebUIBubbleManagerBase() override;

  bool ShowBubble();
  void CloseBubble();
  views::Widget* GetBubbleWidget() const;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  int task_manager_string_id() const { return task_manager_string_id_; }
  content::BrowserContext* browser_context() { return browser_context_; }
  const GURL& webui_url() const { return webui_url_; }
  bool enable_extension_apis() const { return enable_extension_apis_; }
  bool bubble_using_cached_webview() const {
    return bubble_using_cached_webview_;
  }

  void ResetWebViewForTesting();
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_for_testing() {
    return bubble_view_;
  }

 private:
  virtual std::unique_ptr<WebUIBubbleView> CreateWebView() = 0;
  void ResetWebView();

  // Used for tagging the web contents so that a distinctive name shows up in
  // the task manager.
  const int task_manager_string_id_;

  views::View* anchor_view_;
  content::BrowserContext* browser_context_;
  GURL webui_url_;
  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;
  const bool enable_extension_apis_;

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
  using WebUIBubbleManagerBase::WebUIBubbleManagerBase;

 private:
  std::unique_ptr<WebUIBubbleView> CreateWebView() override {
    auto web_view = std::make_unique<WebUIBubbleView>(browser_context());
    content::WebContents* web_contents = web_view->GetWebContents();
    if (enable_extension_apis()) {
      // In order for the WebUI in the renderer to use extensions APIs we must
      // add a ChromeExtensionWebContentsObserver to the WebView's WebContents.
      extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
          web_contents);
    }

    task_manager::WebContentsTags::CreateForToolContents(
        web_contents, task_manager_string_id());

    web_view->template LoadURL<T>(webui_url());
    return web_view;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_MANAGER_H_
