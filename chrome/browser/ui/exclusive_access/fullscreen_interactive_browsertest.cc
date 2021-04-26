// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class FullscreenWebContentsObserver : public content::WebContentsObserver {
 public:
  FullscreenWebContentsObserver(content::WebContents* web_contents,
                                content::RenderFrameHost* wanted_rfh)
      : content::WebContentsObserver(web_contents), wanted_rfh_(wanted_rfh) {}

  // WebContentsObserver override.
  void DidAcquireFullscreen(content::RenderFrameHost* rfh) override {
    EXPECT_EQ(wanted_rfh_, rfh);
    EXPECT_FALSE(found_value_);

    if (rfh == wanted_rfh_) {
      found_value_ = true;
      run_loop_.Quit();
    }
  }

  void Wait() {
    if (!found_value_)
      run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  bool found_value_ = false;
  content::RenderFrameHost* wanted_rfh_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenWebContentsObserver);
};

}  // namespace

class FullscreenInteractiveBrowserTest : public InProcessBrowserTest {
 public:
  FullscreenInteractiveBrowserTest() {}
  ~FullscreenInteractiveBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenInteractiveBrowserTest);
};

// TODO(jonross): Investigate the flakiness on Linux and Mac. Sheriff if this
// fails update (https://crbug.com/1087875).
IN_PROC_BROWSER_TEST_F(FullscreenInteractiveBrowserTest,
                       NotifyFullscreenAcquired) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b{allowfullscreen})");
  ui_test_utils::NavigateToURL(browser(), url);
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(
        ExecuteScript(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecuteScript(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  // Exit fullscreen on the child frame.
  // This will not work with --site-per-process until crbug.com/617369
  // is fixed.
  if (!content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    {
      FullscreenWebContentsObserver observer(web_contents, main_frame);
      EXPECT_TRUE(
          ExecuteScript(child_frame, "document.webkitExitFullscreen();"));
      observer.Wait();
    }
  }
}

IN_PROC_BROWSER_TEST_F(FullscreenInteractiveBrowserTest,
                       NotifyFullscreenAcquired_SameOrigin) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a{allowfullscreen})");
  ui_test_utils::NavigateToURL(browser(), url);
  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(
        ExecuteScript(main_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(
        ExecuteScript(child_frame, "document.body.webkitRequestFullscreen();"));
    observer.Wait();
  }

  // Exit fullscreen on the child frame.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecuteScript(child_frame, "document.webkitExitFullscreen();"));
    observer.Wait();
  }
}
