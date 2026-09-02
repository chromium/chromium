// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/widget_activation_waiter.h"

namespace {

class WebUIBrowserInteractiveTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWebium,
                              features::kAttachUnownedInnerWebContents},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebUIBrowserInteractiveTest,
                       TabPointerLockEnterAndExit) {
  auto* window = WebUIBrowserWindow::FromBrowser(browser());
  ASSERT_TRUE(window);

  // Set bounds to ensure non-empty container bounds.
  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 800, 600));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Add a second tab.
  GURL url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
  EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(second_tab);
  ASSERT_NE(web_contents, second_tab);

  second_tab->Focus();
  browser()->GetWindow()->Activate();
  views::test::WaitForWidgetActive(window->widget(), /*active=*/true);
  content::SimulateMouseClick(second_tab, 0,
                              blink::WebMouseEvent::Button::kLeft);

  ASSERT_TRUE(base::test::RunUntil([web_contents, second_tab]() {
    return second_tab->GetRenderWidgetHostView() &&
           second_tab->GetRenderWidgetHostView()->HasFocus() &&
           web_contents->GetVisibility() == content::Visibility::HIDDEN;
  }));

  auto* pointer_lock_controller =
      ExclusiveAccessManager::From(browser())->pointer_lock_controller();

  const char kRequestPointerLockJS[] = R"(
    new Promise((resolve) => {
      const timeout = setTimeout(() => {
        document.removeEventListener('pointerlockchange', onLock);
        document.removeEventListener('pointerlockerror', onError);
        resolve('timeout');
      }, 5000);
      const onLock = () => {
        clearTimeout(timeout);
        document.removeEventListener('pointerlockerror', onError);
        const isLocked = document.pointerLockElement === document.body;
        resolve(isLocked ? 'success' : 'failure');
      };
      const onError = () => {
        clearTimeout(timeout);
        document.removeEventListener('pointerlockchange', onLock);
        resolve('error');
      };
      document.addEventListener('pointerlockchange', onLock, {once: true});
      document.addEventListener('pointerlockerror', onError, {once: true});
      document.body.requestPointerLock();
    });
  )";

  content::EvalJsResult result = EvalJs(second_tab, kRequestPointerLockJS);
  ASSERT_EQ("success", result.ExtractString());
  EXPECT_TRUE(pointer_lock_controller->IsPointerLocked());

  // Switch to first tab. This should unlock.
  browser()->tab_strip_model()->ActivateTabAt(0);

  EXPECT_TRUE(base::test::RunUntil([pointer_lock_controller]() {
    return !pointer_lock_controller->IsPointerLocked();
  }));
  EXPECT_FALSE(pointer_lock_controller->IsPointerLocked());
}
