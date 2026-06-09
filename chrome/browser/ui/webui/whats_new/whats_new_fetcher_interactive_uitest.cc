// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/views/test/mock_activation_controller.h"  // nogncheck
#endif

class WhatsNewFetcherActiveStateTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    whats_new::DisableRemoteContentForTests();
  }

#if BUILDFLAG(IS_LINUX)
  // On linux, the activation is asynchronous, which makes it impossible to
  // reliably wait for the posted task after activation change. Use mocked
  // activation logic which is synchronous.
  views::test::MockActivationController activation_controller_;
#endif
};

IN_PROC_BROWSER_TEST_F(WhatsNewFetcherActiveStateTest,
                       DoesNotOpenWhenInactive) {
#if BUILDFLAG(IS_OZONE)
  // Ozone/wayland doesn't support window activaation from apps.
  if (ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP();
  }
#endif

  Browser* new_browser = CreateBrowser(browser()->profile());
  ui_test_utils::BrowserActivationWaiter(new_browser).WaitForActivation();
  EXPECT_TRUE(new_browser->IsActive());

  int initial_tab_count = new_browser->tab_strip_model()->count();

  // Start the fetch. It posts a task to open the tab.
  whats_new::StartWhatsNewFetch(new_browser);

  // Immediately change the activation so that it will not show the what's new
  // tab.
  ui_test_utils::BrowserActivationWaiter waiter(browser());
  browser()->GetWindow()->Activate();
  waiter.WaitForActivation();

  // There is no events to wait, so just use RunUntilIdle to give the posted
  // task a chance to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(browser()->IsActive());
  EXPECT_FALSE(new_browser->IsActive());

  // Because the new browser was made inactive, the fetcher should NOT
  // open the What's New tab.
  EXPECT_EQ(initial_tab_count, new_browser->tab_strip_model()->count());
}
