// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_home_control.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/views/toolbar/webui_home_control_test_base.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"

class WebUIHomeControlInteractiveUiTest : public WebUIHomeControlTestBase {};

IN_PROC_BROWSER_TEST_F(WebUIHomeControlInteractiveUiTest, LongPressHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(),
                              DispatchEventScript("#home", "PointerEvent",
                                                  "pointerdown", "button: 0")));

  WebUIHomeControl* home_control = &webui_toolbar_view->home_control_;

  // Wait for the long press timer to trigger and show the menu.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return home_control->menu_runner_ &&
           home_control->menu_runner_->IsRunning();
  }));

  // Clean up
  home_control->menu_runner_->Cancel();
}
