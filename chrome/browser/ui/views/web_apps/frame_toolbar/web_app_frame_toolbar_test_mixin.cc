// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_mixin.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

WebAppFrameToolbarTestMixin::WebAppFrameToolbarTestMixin() {
  WebAppToolbarButtonContainer::DisableAnimationForTesting(true);
}

void WebAppFrameToolbarTestMixin::InstallAndLaunchWebApp(
    Browser* browser,
    const GURL& start_url) {
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->title = u"A minimal-ui app";
  web_app_info->display_mode = web_app::DisplayMode::kMinimalUi;
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  webapps::AppId app_id =
      web_app::test::InstallWebApp(browser->profile(), std::move(web_app_info));
  content::TestNavigationObserver navigation_observer(start_url);
  navigation_observer.StartWatchingNewWebContents();
  app_browser_ = web_app::LaunchWebAppBrowser(browser->profile(), app_id);
  navigation_observer.WaitForNavigationFinished();

  browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
  views::NonClientFrameView* frame_view =
      browser_view_->GetWidget()->non_client_view()->frame_view();
  frame_view_ = static_cast<BrowserNonClientFrameView*>(frame_view);

  web_app_frame_toolbar_ = frame_view_->web_app_frame_toolbar_for_testing();
  DCHECK(web_app_frame_toolbar_);
  DCHECK(web_app_frame_toolbar_->GetVisible());
}
