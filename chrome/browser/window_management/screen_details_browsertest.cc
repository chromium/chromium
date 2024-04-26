// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/browser/screen_details/screen_details_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"

using ScreenDetailsTest = InProcessBrowserTest;

// Tests the basic structure and values of the ScreenDetails API.
// TODO(crbug.com/40145721): Need content_browsertests permission controls.
IN_PROC_BROWSER_TEST_F(ScreenDetailsTest, GetScreenDetailsBasic) {
  auto* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // Auto-accept the permission request.
  permissions::PermissionRequestManager* permission_request_manager_tab =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager_tab->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  ASSERT_TRUE(EvalJs(tab, "'getScreenDetails' in self").ExtractBool());
  content::EvalJsResult result =
      EvalJs(tab, content::test::kGetScreenDetailsScript);
  EXPECT_EQ(content::test::GetExpectedScreenDetails(), result.value);
}

// Tests that ScreenDetailed and window.screen both yield display metrics, not
// viewport dimensions, while the frame is fullscreen. See crbug.com/1367416
IN_PROC_BROWSER_TEST_F(ScreenDetailsTest, FullscreenSize) {
  auto* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  // Auto-accept the permission request.
  permissions::PermissionRequestManager::FromWebContents(tab)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  constexpr char kGetCurrentScreenSizeScript[] = R"JS(
    window.getScreenDetails().then(sD => {
        return `${sD.currentScreen.width}x${sD.currentScreen.height}`;
    });
  )JS";

  // Check initial dimensions before entering fullscreen.
  ASSERT_FALSE(tab->IsFullscreen());
  const std::string display_size =
      tab->GetRenderWidgetHostView()->GetScreenInfo().rect.size().ToString();
  EXPECT_EQ(display_size, EvalJs(tab, "`${screen.width}x${screen.height}`"));
  EXPECT_NE(display_size, EvalJs(tab, "`${innerWidth}x${innerHeight}`"));
  EXPECT_EQ(display_size, EvalJs(tab, kGetCurrentScreenSizeScript));

  // Check dimensions after entering fullscreen yields a `resize`, and after
  // docking DevTools yields a `resize` as the fullscreen content area shrinks.
  constexpr char kEnterFullscreenAndResizeScript[] = R"JS(
    Promise.all([ document.documentElement.requestFullscreen(),
                  new Promise(r => { onresize = r; })
                ]).then(() => {
                  window.nextResize = new Promise(r => { onresize = r; });
                  return !!document.fullscreenElement;
                });
  )JS";
  ASSERT_TRUE(EvalJs(tab, kEnterFullscreenAndResizeScript).ExtractBool());
  DevToolsWindowTesting::OpenDevToolsWindowSync(tab, true);
  ASSERT_TRUE(EvalJs(tab, "window.nextResize").error.empty());
  ASSERT_TRUE(tab->IsFullscreen());
  // `window.screen` dimensions match the display size.
  EXPECT_EQ(display_size, EvalJs(tab, "`${screen.width}x${screen.height}`"));
  EXPECT_NE(EvalJs(tab, "`${screen.width}x${screen.height}`").ExtractString(),
            EvalJs(tab, "`${innerWidth}x${innerHeight}`").ExtractString());
  EXPECT_EQ(display_size, EvalJs(tab, kGetCurrentScreenSizeScript));

  // Check dimensions again after exiting fullscreen yields a `resize`.
  constexpr char kExitFullscreenAndResizeScript[] = R"JS(
    Promise.all([ document.exitFullscreen(),
                  new Promise(r => { onresize = r; })
                ]).then(() => {
                  return !document.fullscreenElement;
                });
  )JS";
  ASSERT_TRUE(EvalJs(tab, kExitFullscreenAndResizeScript).ExtractBool());
  ASSERT_FALSE(tab->IsFullscreen());
  EXPECT_EQ(display_size, EvalJs(tab, "`${screen.width}x${screen.height}`"));
  EXPECT_EQ(display_size, EvalJs(tab, kGetCurrentScreenSizeScript));
}
