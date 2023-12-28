// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/dns/mock_host_resolver.h"

// Helper class to kill render processes on creation.
class RenderProcessCrashOnCreationObserver
    : public content::RenderProcessHostCreationObserver {
 public:
  RenderProcessCrashOnCreationObserver() = default;
  RenderProcessCrashOnCreationObserver(
      const RenderProcessCrashOnCreationObserver&) = delete;
  RenderProcessCrashOnCreationObserver& operator=(
      const RenderProcessCrashOnCreationObserver&) = delete;
  ~RenderProcessCrashOnCreationObserver() override = default;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override {
    process_host->Shutdown(content::RESULT_CODE_KILLED);
  }

 private:
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes_;
};

// This browser test class is used to test what happens if a renderer process
// crashes consistently during navigations, to simulate what happens on machines
// that can't launch renderer processes.
class EarlyCrashBrowserTest : public InProcessBrowserTest {
 public:
  EarlyCrashBrowserTest() = default;
  EarlyCrashBrowserTest(const EarlyCrashBrowserTest&) = delete;
  EarlyCrashBrowserTest& operator=(const EarlyCrashBrowserTest&) = delete;
  ~EarlyCrashBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  RenderProcessCrashOnCreationObserver crash_on_creation_observer_;
};

// Ensure that an early renderer process crash will show the sad tab page and it
// will remain visible after the navigation is complete.
IN_PROC_BROWSER_TEST_F(EarlyCrashBrowserTest, SadTabProcessCreationFailure) {
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto* const sad_tab_view = static_cast<const SadTabView*>(
      SadTabHelper::FromWebContents(contents)->sad_tab());
  ASSERT_TRUE(sad_tab_view);
  EXPECT_TRUE(sad_tab_view->GetVisible());
}
