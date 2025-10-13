// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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

#if !BUILDFLAG(IS_CHROMEOS)
// Begin security related tests. These tests validate the security
// boundary between a GuestContents and the parent.
class WebUIBrowserSecurityTest : public WebUIBrowserTest {
 public:
  void SetUp() override { WebUIBrowserTest::SetUp(); }

  content::WebContents* GetInnerWebContents() {
    if (!inner_webcontents_) {
      inner_webcontents_ = SetUpEmbeddedWebContents()->GetWeakPtr();
    }
    return inner_webcontents_.get();
  }

  content::WebContents* GetOuterWebContents() {
    return GetInnerWebContents()->GetOuterWebContents();
  }

 private:
  // Helper function to set up embedded web contents for tests.
  // Returns the embedded web contents after it has been converted to a guest.
  content::WebContents* SetUpEmbeddedWebContents() {
    EXPECT_TRUE(browser()->window());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(web_contents);
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));

    // Make sure that the web contents actually got converted to a guest before
    // we navigate it again, so that WebContentsViewChildFrame gets involved.
    EXPECT_TRUE(base::test::RunUntil(
        [web_contents]() { return !!web_contents->GetOuterWebContents(); }));

    GURL url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    return web_contents;
  }

  base::WeakPtr<content::WebContents> inner_webcontents_;
};

// Test that parent history is not affected by embedded navigation.
// The history.length should be independent between inner and outer webcontents.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest, HistoryLengthIndependent) {
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_FALSE(outer_webcontents->GetOuterWebContents());
  EXPECT_TRUE(outer_webcontents);
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.history.length"));

  // Navigate the inner contents another cross origin URL and verify outer
  // remains 1.
  GURL url = embedded_https_test_server().GetURL("b.com", "/defaultresponse");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(1, EvalJs(outer_webcontents, "window.history.length"));
}

// Test that the frame tree isolation between inner and outer webcontents.
// Neither should include the other in their frames collection.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest, FramesIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_EQ(0, EvalJs(inner_webcontents, "window.frames.length"));
  EXPECT_EQ(0, EvalJs(outer_webcontents, "window.frames.length"));
}

// Test that the parent window does not count the embedded content as a frame.
// The outer web contents should have window.length = 0 since the embedded
// content should not be counted in the parent's frame count.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest, WindowLengthIndependent) {
  content::WebContents* outer_webcontents = GetOuterWebContents();

  EXPECT_EQ(0, EvalJs(outer_webcontents, "window.length"));
}

// Test that the embedded content acts as top level.
// window.top in the embedded content should equal window (itself),
// not the actual parent's top-level window.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest, WindowTopIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(EvalJs(inner_webcontents, "window.top === window").ExtractBool());
}

// Test that the embedded content acts as top level.
// window.opener should be null since the embedded content should not
// have access to the parent that "opened" it.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest, WindowOpenerIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(
      EvalJs(inner_webcontents, "window.opener === null").ExtractBool());
}

// Test that the embedded content acts as top level.
// window.parent should equal window (itself) since there should be
// no accessible parent window from the embedded content's perspective.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest, WindowParentIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(
      EvalJs(inner_webcontents, "window.parent === window").ExtractBool());
}

// Test that the embedded content acts as top level.
// window.frameElement should be null since the embedded content should
// not appear to be contained within a frame element.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest,
                       WindowFrameElementIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();

  EXPECT_TRUE(
      EvalJs(inner_webcontents, "window.frameElement === null").ExtractBool());
}

// Test that inner webcontents cannot target outer webcontents
// _parent and _top should all target the inner webcontents itself.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest,
                       WindowOpenTargetingIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // Store current URLs to verify navigation targets
  GURL outer_url = outer_webcontents->GetLastCommittedURL();

  // Test _parent targeting from inner webcontents.
  GURL test_url =
      embedded_https_test_server().GetURL("b.com", "/defaultresponse");
  EXPECT_TRUE(
      ExecJs(inner_webcontents,
             content::JsReplace("window.open($1, '_parent')", test_url)));

  // Verify inner webcontents navigated, outer did not.
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));
  EXPECT_EQ(inner_webcontents->GetLastCommittedURL().host(), test_url.host());
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), outer_url);

  // Test _top
  test_url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
  EXPECT_TRUE(ExecJs(inner_webcontents,
                     content::JsReplace("window.open($1, '_top')", test_url)));

  // Verify inner webcontents navigated, outer did not.
  EXPECT_TRUE(content::WaitForLoadStop(inner_webcontents));
  EXPECT_EQ(inner_webcontents->GetLastCommittedURL().host(), test_url.host());
  EXPECT_EQ(outer_webcontents->GetLastCommittedURL(), outer_url);
}

// Test that cross-context window references are not useful.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSecurityTest,
                       WindowOpenReferenceIndependent) {
  content::WebContents* inner_webcontents = GetInnerWebContents();
  content::WebContents* outer_webcontents = GetOuterWebContents();

  // Part 1. outer makes a window, it's not accessible in inner.
  EXPECT_TRUE(ExecJs(outer_webcontents,
                     "window.testWindow = window.open('about:blank')"));
  EXPECT_TRUE(EvalJs(outer_webcontents, "window.hasOwnProperty('testWindow')")
                  .ExtractBool());
  EXPECT_FALSE(EvalJs(inner_webcontents, "window.hasOwnProperty('testWindow')")
                   .ExtractBool());

  // Part 2. inner makes a window, it's not accessible in outer.
  EXPECT_TRUE(ExecJs(inner_webcontents,
                     "window.innerWindow = window.open('about:blank')"));
  EXPECT_FALSE(EvalJs(outer_webcontents, "window.hasOwnProperty('innerWindow')")
                   .ExtractBool());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)
