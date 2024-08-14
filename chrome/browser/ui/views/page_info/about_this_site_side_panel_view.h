// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/page_info/about_this_site_side_panel_throttle.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/layout/flex_layout_view.h"

class BrowserView;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

// Owns the webview and navigates to a google search URL when requested. It's
// owned by the side panel registry.
class AboutThisSiteSidePanelView final
    : public views::FlexLayoutView,
      public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public AboutThisSiteWebContentsUserData::Delegate {
 public:
  explicit AboutThisSiteSidePanelView(
      content::WebContents* parent_web_contents);
  AboutThisSiteSidePanelView(const AboutThisSiteSidePanelView&) = delete;
  AboutThisSiteSidePanelView& operator=(const AboutThisSiteSidePanelView&) =
      delete;
  ~AboutThisSiteSidePanelView() override;

  void OpenUrl(const content::OpenURLParams& params);

  base::WeakPtr<AboutThisSiteSidePanelView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Remove parameters that shouldn't be passed to the main browser.
  GURL CleanUpQueryParams(const GURL& url);

  // Shows / hides the page and the loading view to avoid showing
  // loading artifacts.
  void SetContentVisible(bool visible);

  // AboutThisSiteWebContentsUserData::Delegate
  void OpenUrlInBrowser(const content::OpenURLParams& params) override;
  bool IsNavigationAllowed(const GURL& new_url, const GURL& old_url) override;

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
  raw_ptr<views::WebView> loading_indicator_web_view_;
  raw_ptr<views::WebView> web_view_;
  base::WeakPtrFactory<AboutThisSiteSidePanelView> weak_ptr_factory_{this};
};

#endif // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_
