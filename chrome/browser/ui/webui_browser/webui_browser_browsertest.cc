// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

// Use an anonymous namespace here to avoid colliding with the other
// WebUIBrowserTest defined in chrome/test/base/ash/web_ui_browser_test.h
namespace {

class WebUIBrowserTest : public InProcessBrowserTest {
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

// Ensures that WebUIBrowser does not crash on startup and can shutdown.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, StartupAndShutdown) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
// For now this is disabled on CrOS since BrowserStatusMonitor/
// AppServiceInstanceRegistryHelper aren't happy with our shutdown deletion
// order of native windows vs. Browser and aren't tracking the switch over
// of views on child guest contents properly.
#define MAYBE_NavigatePage DISABLED_NavigatePage
#else
#define MAYBE_NavigatePage NavigatePage
#endif

// Navigation at chrome/ layer, which hits some focus management paths.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_NavigatePage) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Make sure that the web contents actually got converted to a guest before
  // we navigate it again, so that WebContentsViewChildFrame gets involved.
  EXPECT_TRUE(base::test::RunUntil([web_contents]() {
    return web_contents->GetOuterWebContents() != nullptr;
  }));

  GURL url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("Default response given for path: /defaultresponse",
            EvalJs(web_contents, "document.body.textContent"));
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
// For now this is disabled on CrOS since BrowserStatusMonitor/
// AppServiceInstanceRegistryHelper aren't happy with our shutdown deletion
// order of native windows vs. Browser and aren't tracking the switch over
// of views on child guest contents properly.
#define MAYBE_EnumerateDevToolsTargets DISABLED_EnumerateDevToolsTargets
#else
#define MAYBE_EnumerateDevToolsTargets EnumerateDevToolsTargets
#endif
// Verify DevTools targets enumeration for browser UI and tabs.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_EnumerateDevToolsTargets) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Make sure that the web contents actually got converted to a guest and in
  // DOM before enumerate DevTools targets.
  EXPECT_TRUE(base::test::RunUntil([web_contents]() {
    return web_contents->GetOuterWebContents() != nullptr;
  }));

  // Verify DevTools target types.
  auto targets = content::DevToolsAgentHost::GetOrCreateAll();
  int tab_count = 0;
  int page_count = 0;
  int browser_ui_count = 0;
  auto hosts = content::DevToolsAgentHost::GetOrCreateAll();
  for (auto& host : hosts) {
    LOG(INFO) << "Found DevTools target, type: " << host->GetType()
              << ", parent id:" << host->GetParentId()
              << ", url: " << host->GetURL().spec();
    // Only expect top level targets.
    EXPECT_TRUE(host->GetParentId().empty());
    if (host->GetType() == content::DevToolsAgentHost::kTypeTab) {
      ++tab_count;
    } else if (host->GetType() == content::DevToolsAgentHost::kTypePage) {
      ++page_count;
    } else if (host->GetType() == content::DevToolsAgentHost::kTypeBrowserUI) {
      ++browser_ui_count;
    }
  }
  // Expect browser_ui target for browser UI main frame, Tab target for tab
  // WebContents, and Page target for tab main frame.
  EXPECT_EQ(hosts.size(), 3U);
  EXPECT_EQ(browser_ui_count, 1);
  EXPECT_EQ(tab_count, 1);
  EXPECT_EQ(page_count, 1);
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
#define MAYBE_FullscreenEnterAndExit DISABLED_FullscreenEnterAndExit
#else
#define MAYBE_FullscreenEnterAndExit FullscreenEnterAndExit
#endif
// Test entering and exiting fullscreen mode.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_FullscreenEnterAndExit) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Should not be in fullscreen initially.
  EXPECT_FALSE(window->IsFullscreen());

  // Enter fullscreen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(window->IsFullscreen());

  // Exit fullscreen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(window->IsFullscreen());
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
#define MAYBE_TabFullscreenEnterAndExit DISABLED_TabFullscreenEnterAndExit
#else
#define MAYBE_TabFullscreenEnterAndExit TabFullscreenEnterAndExit
#endif
// Test entering and exiting tab fullscreen mode, including tab switching.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_TabFullscreenEnterAndExit) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

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

  auto* fullscreen_controller = browser()
                                    ->GetFeatures()
                                    .exclusive_access_manager()
                                    ->fullscreen_controller();

  // Enter tab fullscreen mode on second tab.
  fullscreen_controller->EnterFullscreenModeForTab(
      second_tab->GetPrimaryMainFrame());

  // Wait for fullscreen state.
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return window->IsFullscreen(); }));
  EXPECT_TRUE(window->IsFullscreen());

  // Exit fullscreen explicitly before switching tabs.
  fullscreen_controller->ExitFullscreenModeForTab(second_tab);
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return !window->IsFullscreen(); }));
  EXPECT_FALSE(window->IsFullscreen());

  // Switch to first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(window->IsFullscreen());

  // Switch back to the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(window->IsFullscreen());

  // Enter fullscreen again on the second tab.
  fullscreen_controller->EnterFullscreenModeForTab(
      second_tab->GetPrimaryMainFrame());
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return window->IsFullscreen(); }));
  EXPECT_TRUE(window->IsFullscreen());

  // Exit fullscreen.
  fullscreen_controller->ExitFullscreenModeForTab(second_tab);
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return !window->IsFullscreen(); }));
  EXPECT_FALSE(window->IsFullscreen());
}
