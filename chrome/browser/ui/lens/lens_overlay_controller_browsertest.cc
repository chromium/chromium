// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"

namespace {

using State = LensOverlayController::State;

class LensOverlayControllerBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CaptureScreenshot) {
  // State should start in off.
  auto* controller =
      browser()->tab_strip_model()->GetActiveTab()->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CreateAndLoadWebUI) {
  // State should start in off.
  auto* controller =
      browser()->tab_strip_model()->GetActiveTab()->lens_overlay_controller();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Assert that the web view was created and loaded WebUI.
  GURL webui_url(chrome::kChromeUILensUntrustedURL);
  raw_ptr<views::WebView> overlay_web_view = views::AsViewClass<views::WebView>(
      controller->GetOverlayWidgetForTesting()
          ->GetContentsView()
          ->children()[0]);
  ASSERT_TRUE(content::WaitForLoadStop(overlay_web_view->GetWebContents()));
  ASSERT_EQ(overlay_web_view->GetWebContents()->GetLastCommittedURL(),
            webui_url);
}

}  // namespace
