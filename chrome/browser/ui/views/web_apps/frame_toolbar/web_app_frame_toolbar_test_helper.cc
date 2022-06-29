// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

WebAppFrameToolbarTestHelper::WebAppFrameToolbarTestHelper() {
  WebAppToolbarButtonContainer::DisableAnimationForTesting();
}

WebAppFrameToolbarTestHelper::~WebAppFrameToolbarTestHelper() = default;

web_app::AppId WebAppFrameToolbarTestHelper::InstallAndLaunchWebApp(
    Browser* browser,
    const GURL& start_url) {
  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = start_url;
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->title = u"A minimal-ui app";
  web_app_info->display_mode = web_app::DisplayMode::kMinimalUi;
  web_app_info->user_display_mode = web_app::UserDisplayMode::kStandalone;

  web_app::AppId app_id =
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
  return app_id;
}

web_app::AppId WebAppFrameToolbarTestHelper::InstallAndLaunchCustomWebApp(
    Browser* browser,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    const GURL& start_url) {
  web_app::AppId app_id =
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
  return app_id;
}

GURL WebAppFrameToolbarTestHelper::
    LoadWindowControlsOverlayTestPageWithDataAndGetURL(
        net::test_server::EmbeddedTestServer* embedded_test_server,
        base::ScopedTempDir* temp_dir) {
  // Write kTestHTML to a temporary file that can be later reached at
  // http://127.0.0.1/test_file_*.html.
  static int s_test_file_number = 1;

  constexpr char kTestHTML[] =
      "<!DOCTYPE html>"
      "<style>"
      "  #draggable {"
      "     app-region: drag;"
      "     position: absolute;"
      "     top: 100px;"
      "     left: 100px;"
      "     height: 10px;"
      "     width: 10px;"
      "  }"
      "  #non-draggable {"
      "     app-region: no-drag;"
      "     position: relative;"
      "     top: 5px;"
      "     left: 5px;"
      "     height: 2px;"
      "     width: 2px;"
      "  }"
      "  #target {"
      "     padding-left: env(titlebar-area-x);"
      "     padding-right: env(titlebar-area-width);"
      "     padding-top: env(titlebar-area-y);"
      "     padding-bottom: env(titlebar-area-height);"
      "  }"
      "</style>"
      "<div id=\"draggable\">"
      "  <div id=\"non-draggable\"></div>"
      "</div>"
      "<div id=\"target\"></div>";

  base::FilePath file_path = temp_dir->GetPath().AppendASCII(
      base::StringPrintf("test_file_%d.html", s_test_file_number++));
  base::ScopedAllowBlockingForTesting allow_temp_file_writing;
  base::WriteFile(file_path, kTestHTML);
  GURL url =
      embedded_test_server->GetURL("/" + file_path.BaseName().AsUTF8Unsafe());
  return url;
}

base::Value::List WebAppFrameToolbarTestHelper::GetXYWidthHeightListValue(
    content::WebContents* web_contents,
    const std::string& rect_value_list,
    const std::string& rect_var_name) {
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), rect_value_list));
  return std::move(EvalJs(web_contents, rect_var_name).ExtractList().GetList());
}

gfx::Rect WebAppFrameToolbarTestHelper::GetXYWidthHeightRect(
    content::WebContents* web_contents,
    const std::string& rect_value_list,
    const std::string& rect_var_name) {
  base::Value::List rect_list =
      GetXYWidthHeightListValue(web_contents, rect_value_list, rect_var_name);
  return gfx::Rect(rect_list[0].GetInt(), rect_list[1].GetInt(),
                   rect_list[2].GetInt(), rect_list[3].GetInt());
}

void WebAppFrameToolbarTestHelper::SetupGeometryChangeCallback(
    content::WebContents* web_contents) {
  EXPECT_TRUE(
      ExecJs(web_contents->GetPrimaryMainFrame(),
             "var geometrychangeCount = 0;"
             "document.title = 'beforegeometrychange';"
             "navigator.windowControlsOverlay.ongeometrychange = (e) => {"
             "  geometrychangeCount++;"
             "  overlay_rect_from_event = e.titlebarAreaRect;"
             "  overlay_visible_from_event = e.visible;"
             "  document.title = 'ongeometrychange';"
             "}"));
}
