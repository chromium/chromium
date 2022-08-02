// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/about_this_site_side_panel_view.h"

#include "chrome/browser/page_info/about_this_site_side_panel_throttle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {
std::unique_ptr<views::WebView> CreateWebView(
    views::View* host,
    content::BrowserContext* browser_context) {
  auto web_view = std::make_unique<views::WebView>(browser_context);
  // Set a flex behavior for the WebView to always fill out the extra space in
  // the parent view. In the minimum case, it will scale down to 0.
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Set background of webview to the same background as the toolbar. This is to
  // prevent personal color themes from showing in the side panel when
  // navigating to a new results panel.
  web_view->SetBackground(views::CreateThemedSolidBackground(kColorToolbar));
  return web_view;
}
}  // namespace

// TODO(crbug.com/1318000): Implement loading screen for AboutThisSite.
constexpr char kStaticLoadingScreenURL[] =
    "https://www.gstatic.com/lens/chrome/lens_side_panel_loading.html";

AboutThisSiteSidePanelView::AboutThisSiteSidePanelView(
    BrowserView* browser_view) {
  browser_view_ = browser_view;
  auto* browser_context = browser_view->GetProfile();
  // Align views vertically top to bottom.
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);

  // Stretch views to fill horizontal bounds.
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  loading_indicator_web_view_ =
      AddChildView(CreateWebView(this, browser_context));
  loading_indicator_web_view_->GetWebContents()->GetController().LoadURL(
      GURL(kStaticLoadingScreenURL), content::Referrer(),
      ui::PAGE_TRANSITION_FROM_API, std::string());
  web_view_ = AddChildView(CreateWebView(this, browser_context));

  SetContentVisible(false);
  auto* web_contents = web_view_->GetWebContents();
  web_contents->SetDelegate(this);
  web_contents->SetUserData(
      kAboutThisSiteWebContentsUserDataKey,
      std::make_unique<AboutThisSiteWebContentsUserData>(base::BindRepeating(
          &AboutThisSiteSidePanelView::OpenUrlInBrowser, AsWeakPtr())));
  Observe(web_contents);
}

void AboutThisSiteSidePanelView::LoadProgressChanged(double progress) {
  SetContentVisible(progress == 1.0);
}

void AboutThisSiteSidePanelView::OpenUrl(const content::OpenURLParams& params) {
  web_view_->GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
}

content::WebContents* AboutThisSiteSidePanelView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // Redirect requests to open a new tab to the main browser. These come e.g.
  // from the context menu.
  return outer_delegate()->OpenURLFromTab(source, params);
}

void AboutThisSiteSidePanelView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  // Redirect requests to add a webcontents to the main browser. These come e.g.
  // from middle clicks on links.
  outer_delegate()->AddNewContents(source, std::move(new_contents), target_url,
                                   disposition, initial_rect, user_gesture,
                                   was_blocked);
}

bool AboutThisSiteSidePanelView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Redirect keyboard events to the main browser.
  return outer_delegate()->HandleKeyboardEvent(source, event);
}

content::WebContentsDelegate* AboutThisSiteSidePanelView::outer_delegate() {
  return browser_view_->browser();
}

void AboutThisSiteSidePanelView::OpenUrlInBrowser(
    const content::OpenURLParams& params) {
  browser_view_->browser()->OpenURL(params);
}

void AboutThisSiteSidePanelView::SetContentVisible(bool visible) {
  web_view_->SetVisible(visible);
  loading_indicator_web_view_->SetVisible(!visible);
}

AboutThisSiteSidePanelView::~AboutThisSiteSidePanelView() = default;
