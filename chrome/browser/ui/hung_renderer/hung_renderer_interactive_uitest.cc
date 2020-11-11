// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_helpers.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/buildflags.h"

class HungRendererNavigationTest : public InProcessBrowserTest {
 public:
  HungRendererNavigationTest() {}
  ~HungRendererNavigationTest() override {}

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HungRendererNavigationTest);
};

// Verify that a cross-process navigation will dismiss the hung renderer
// dialog so that we do not kill the new (responsive) process.
IN_PROC_BROWSER_TEST_F(HungRendererNavigationTest,
                       HungRendererWithCrossProcessNavigation) {
  EXPECT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html"));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabDialogs::FromWebContents(active_web_contents)
      ->ShowHungRendererDialog(
          active_web_contents->GetMainFrame()->GetRenderViewHost()->GetWidget(),
          base::DoNothing::Repeatedly());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html"));
  // Expect that the dialog has been dismissed.
  EXPECT_FALSE(TabDialogs::FromWebContents(active_web_contents)
                   ->IsShowingHungRendererDialog());
}
