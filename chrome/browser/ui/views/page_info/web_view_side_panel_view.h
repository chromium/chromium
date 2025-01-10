// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_WEB_VIEW_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_WEB_VIEW_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/page_info/web_view_side_panel_throttle.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/layout/flex_layout_view.h"
#include "url/gurl.h"

class BrowserView;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// Owns the webview and navigates to a URL when requested. It's
// owned by the side panel registry.
class WebViewSidePanelView final
    : public views::FlexLayoutView,
      public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public WebViewSidePanelWebContentsUserData::Delegate {
 public:
  explicit WebViewSidePanelView(
      content::WebContents* parent_web_contents,
      const std::string& loading_screen_url,
      const std::optional<std::string>& param_name_to_cleanup);
  WebViewSidePanelView(const WebViewSidePanelView&) = delete;
  WebViewSidePanelView& operator=(const WebViewSidePanelView&) = delete;
  ~WebViewSidePanelView() override;

  void OpenUrl(const content::OpenURLParams& params);

  base::WeakPtr<WebViewSidePanelView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  GURL GetLastUrlForTesting();

 private:
  // Remove parameters that shouldn't be passed to the main browser.
  GURL CleanUpQueryParams(const GURL& url);

  // Shows / hides the page to avoid showing loading artifacts.
  void SetContentVisible(bool visible);

  // WebViewSidePanelWebContentsUserData::Delegate
  void OpenUrlInBrowser(const content::OpenURLParams& params) override;

  // content::WebContentsObserver:
  void LoadProgressChanged(double progress) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  BrowserView* outer_browser_view();
  content::WebContentsDelegate* outer_delegate();

  GURL last_url_;
  base::WeakPtr<content::WebContents> parent_web_contents_;
  // If set, the parameter will be removed from the URL before navigating to a
  // new tab.
  std::optional<std::string> param_name_to_cleanup_;
  raw_ptr<views::WebView> loading_indicator_web_view_;
  raw_ptr<views::WebView> web_view_;
  base::WeakPtrFactory<WebViewSidePanelView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_WEB_VIEW_SIDE_PANEL_VIEW_H_
