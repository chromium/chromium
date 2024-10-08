// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_origin_text.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

WebAppFrameToolbarTestHelper::WebAppFrameToolbarTestHelper() {
  WebAppToolbarButtonContainer::DisableAnimationForTesting(true);
}

WebAppFrameToolbarTestHelper::~WebAppFrameToolbarTestHelper() = default;

webapps::AppId WebAppFrameToolbarTestHelper::InstallWebApp(
    Profile* profile,
    const GURL& start_url) {
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->title = u"A minimal-ui app";
  web_app_info->display_mode = web_app::DisplayMode::kMinimalUi;
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

webapps::AppId WebAppFrameToolbarTestHelper::InstallAndLaunchWebApp(
    Profile* profile,
    const GURL& start_url) {
  webapps::AppId app_id = InstallWebApp(profile, start_url);
  content::TestNavigationObserver navigation_observer(start_url);
  navigation_observer.StartWatchingNewWebContents();
  app_browser_ = web_app::LaunchWebAppBrowser(profile, app_id);
  navigation_observer.WaitForNavigationFinished();

  browser_view_ = BrowserView::GetBrowserViewForBrowser(app_browser_);
  views::NonClientFrameView* frame_view =
      browser_view_->GetWidget()->non_client_view()->frame_view();
  frame_view_ = static_cast<BrowserNonClientFrameView*>(frame_view);
  root_view_ = browser_view_->GetWidget()->GetRootView();

  web_app_frame_toolbar_ = browser_view_->web_app_frame_toolbar_for_testing();
  DCHECK(web_app_frame_toolbar_);
  DCHECK(web_app_frame_toolbar_->GetVisible());
  return app_id;
}

webapps::AppId WebAppFrameToolbarTestHelper::InstallAndLaunchWebApp(
    Browser* browser,
    const GURL& start_url) {
  return InstallAndLaunchWebApp(browser->profile(), start_url);
}

webapps::AppId WebAppFrameToolbarTestHelper::InstallAndLaunchCustomWebApp(
    Browser* browser,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    const GURL& start_url) {
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
  root_view_ = browser_view_->GetWidget()->GetRootView();

  web_app_frame_toolbar_ = browser_view_->web_app_frame_toolbar_for_testing();
  DCHECK(web_app_frame_toolbar_);
  DCHECK(web_app_frame_toolbar_->GetVisible());
  return app_id;
}

GURL WebAppFrameToolbarTestHelper::
    LoadWindowControlsOverlayTestPageWithDataAndGetURL(
        net::test_server::EmbeddedTestServer* embedded_test_server,
        base::ScopedTempDir* temp_dir) {
  return LoadTestPageWithDataAndGetURL(embedded_test_server, temp_dir, R"(
    <!DOCTYPE html>
    <style>
    body {
      background: blue;
    }
    @media (display-mode: window-controls-overlay) {
      body {
        background: red;
      }
    }
    #draggable {
      app-region: drag;
      position: absolute;
      top: 100px;
      left: 100px;
      height: 10px;
      width: 10px;
    }
    #non-draggable {
      app-region: no-drag;
      position: relative;
      top: 5px;
      left: 5px;
      height: 2px;
      width: 2px;
    }
    #target {
      padding-left: env(titlebar-area-x);
      padding-right: env(titlebar-area-width);
      padding-top: env(titlebar-area-y);
      padding-bottom: env(titlebar-area-height);
    }
    </style>
    <div id='draggable'>
      <div id='non-draggable'></div>
    </div>
    <div id='target'></div>
    )");
}

GURL WebAppFrameToolbarTestHelper::
    LoadWholeAppIsDraggableTestPageWithDataAndGetURL(
        net::test_server::EmbeddedTestServer* embedded_test_server,
        base::ScopedTempDir* temp_dir) {
  return LoadTestPageWithDataAndGetURL(embedded_test_server, temp_dir, R"(
    <!DOCTYPE html>
    <style>
      div {
        app-region: drag;
        width: 100%;
        height: 100%;
        padding: 0px;
        margin: 0px;
        position: absolute;
      }
      body {
        padding: 0px;
        margin: 0px;
      }
    </style>
    <div>Hello draggable world</div>
  )");
}

GURL WebAppFrameToolbarTestHelper::LoadTestPageWithDataAndGetURL(
    net::test_server::EmbeddedTestServer* embedded_test_server,
    base::ScopedTempDir* temp_dir,
    std::string_view test_html) {
  // Write kTestHTML to a temporary file that can be later reached at
  // http://127.0.0.1/test_file_*.html.
  static int s_test_file_number = 1;
  base::FilePath file_path = temp_dir->GetPath().AppendASCII(
      base::StringPrintf("test_file_%d.html", s_test_file_number++));
  base::ScopedAllowBlockingForTesting allow_temp_file_writing;
  base::WriteFile(file_path, test_html);
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
  EXPECT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(), R"(
    var geometrychangeCount = 0;
    document.title = 'beforegeometrychange';
    navigator.windowControlsOverlay.ongeometrychange = (e) => {
      geometrychangeCount++;
      overlay_rect_from_event = e.titlebarAreaRect;
      overlay_visible_from_event = e.visible;
      document.title = 'ongeometrychange';
    }
  )"));
}

// TODO(crbug.com/40809857): Flaky.
void WebAppFrameToolbarTestHelper::TestDraggableRegions() {
  views::NonClientFrameView* frame_view =
      browser_view()->GetWidget()->non_client_view()->frame_view();

  // Draggable regions take some time to initialize after opening and tests fail
  // if not exhausting the run loop before checking the value.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // Validate that a point marked "app-region: drag" is draggable. The draggable
  // region is defined in the kTestHTML of WebAppFrameToolbarTestHelper's
  // LoadWindowControlsOverlayTestPageWithDataAndGetURL.
  gfx::Point draggable_point(100, 100);
  views::View::ConvertPointToTarget(browser_view()->contents_web_view(),
                                    frame_view, &draggable_point);

  EXPECT_EQ(frame_view->NonClientHitTest(draggable_point), HTCAPTION);

  EXPECT_FALSE(browser_view()->ShouldDescendIntoChildForEventHandling(
      browser_view()->GetWidget()->GetNativeView(), draggable_point));

  // Validate that a point marked "app-region: no-drag" within a draggable
  // region is not draggable.
  gfx::Point non_draggable_point(106, 106);
  views::View::ConvertPointToTarget(browser_view()->contents_web_view(),
                                    frame_view, &non_draggable_point);

  EXPECT_EQ(frame_view->NonClientHitTest(non_draggable_point), HTCLIENT);

  EXPECT_TRUE(browser_view()->ShouldDescendIntoChildForEventHandling(
      browser_view()->GetWidget()->GetNativeView(), non_draggable_point));

  // Validate that a point at the border that does not intersect with the
  // overlays is not marked as draggable.
  constexpr gfx::Point kBorderPoint(100, 1);
  EXPECT_NE(frame_view->NonClientHitTest(kBorderPoint), HTCAPTION);
  EXPECT_TRUE(browser_view()->ShouldDescendIntoChildForEventHandling(
      browser_view()->GetWidget()->GetNativeView(), kBorderPoint));

  // Validate that draggable region does not clear after history.replaceState is
  // invoked.
  std::string kHistoryReplaceState =
      "history.replaceState({ test: 'test' }, null);";
  EXPECT_TRUE(
      ExecJs(browser_view()->GetActiveWebContents(), kHistoryReplaceState));
  EXPECT_FALSE(browser_view()->ShouldDescendIntoChildForEventHandling(
      browser_view()->GetWidget()->GetNativeView(), draggable_point));

  // Validate that the draggable region is reset on navigation and the point is
  // no longer draggable.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_view()->browser(),
                                           GURL("http://example.test/")));
  EXPECT_NE(frame_view->NonClientHitTest(draggable_point), HTCAPTION);
  EXPECT_TRUE(browser_view()->ShouldDescendIntoChildForEventHandling(
      browser_view()->GetWidget()->GetNativeView(), draggable_point));
}

BrowserView* WebAppFrameToolbarTestHelper::OpenPopup(
    const std::string& window_open_script) {
  content::ExecuteScriptAsync(browser_view_->GetActiveWebContents(),
                              window_open_script);
  Browser* popup = ui_test_utils::WaitForBrowserToOpen();
  EXPECT_NE(app_browser_, popup);
  EXPECT_TRUE(popup);

  BrowserView* popup_browser_view =
      BrowserView::GetBrowserViewForBrowser(popup);
  EXPECT_TRUE(content::WaitForRenderFrameReady(
      popup_browser_view->GetActiveWebContents()->GetPrimaryMainFrame()));

  return popup_browser_view;
}

void WebAppFrameToolbarTestHelper::GrantWindowManagementPermission(
    content::WebContents* web_contents) {
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(ExecJs(web_contents, "window.getScreenDetails();"));
  content::WaitForLoadStop(web_contents);

  constexpr std::string_view permission_query_script = R"(
      navigator.permissions.query({
        name: 'window-management'
      }).then(res => res.state)
    )";
  ASSERT_EQ("granted", EvalJs(web_contents, permission_query_script));
}

void WebAppFrameToolbarTestHelper::GrantWindowManagementPermission() {
  return GrantWindowManagementPermission(
      browser_view()->GetActiveWebContents());
}

WebAppOriginText* WebAppFrameToolbarTestHelper::origin_text_view() {
  return static_cast<WebAppOriginText*>(
      web_app_frame_toolbar()->GetViewByID(VIEW_ID_WEB_APP_ORIGIN_TEXT));
}

void WebAppFrameToolbarTestHelper::SetOriginTextLabelForTesting(
    const std::u16string& label_text) {
  origin_text_view()->label_->SetText(label_text);
}
