// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

// Owns the webview and navigates to a google search URL when requested. Its
// owned by the side panel registry.
class AboutThisSiteSidePanelView
    : public views::FlexLayoutView,
      public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public base::SupportsWeakPtr<AboutThisSiteSidePanelView> {
 public:
  explicit AboutThisSiteSidePanelView(BrowserView* browser_view);
  AboutThisSiteSidePanelView(const AboutThisSiteSidePanelView&) = delete;
  AboutThisSiteSidePanelView& operator=(const AboutThisSiteSidePanelView&) =
      delete;
  ~AboutThisSiteSidePanelView() override;

  void OpenUrl(const content::OpenURLParams& params);

 private:
  // Opens a URL in a regular tab.
  void OpenUrlInBrowser(const content::OpenURLParams& params);

  // Shows / hides the page and the loading view to avoid showing
  // loading artifacts.
  void SetContentVisible(bool visible);

  // content::WebContentsObserver:
  void LoadProgressChanged(double progress) override;

  // content::WebContentsDelegate:
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  content::WebContentsDelegate* outer_delegate();

  raw_ptr<BrowserView> browser_view_;
  raw_ptr<views::WebView> loading_indicator_web_view_;
  raw_ptr<views::WebView> web_view_;
};

#endif // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ABOUT_THIS_SITE_SIDE_PANEL_VIEW_H_
