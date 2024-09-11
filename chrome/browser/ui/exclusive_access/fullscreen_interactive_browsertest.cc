// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// UMA Key for tracking duration of fullscreen requests.
static constexpr char kFullscreenDurationMetricKeyRequestFullscreen[] =
    "Blink.Element.Fullscreen.DurationUpTo1H.RequestFullscreen";

class FullscreenWebContentsObserver : public content::WebContentsObserver {
 public:
  FullscreenWebContentsObserver(content::WebContents* web_contents,
                                content::RenderFrameHost* wanted_rfh)
      : content::WebContentsObserver(web_contents), wanted_rfh_(wanted_rfh) {}

  FullscreenWebContentsObserver(const FullscreenWebContentsObserver&) = delete;
  FullscreenWebContentsObserver& operator=(
      const FullscreenWebContentsObserver&) = delete;

  // WebContentsObserver overrides.
  void DidAcquireFullscreen(content::RenderFrameHost* rfh) override {
    // Note: This function may be called twice for cross-process child frame
    // fullscreen.
    if (rfh == wanted_rfh_) {
      found_value_ = true;
      run_loop_.Quit();
    }
  }
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    if (!entered_fullscreen) {
      did_exit_ = true;
      run_loop_.Quit();
    }
  }

  void Wait() {
    if (!found_value_)
      run_loop_.Run();
  }
  void WaitForExit() {
    if (!did_exit_) {
      run_loop_.Run();
    }
  }

 private:
  base::RunLoop run_loop_;
  bool found_value_ = false;
  bool did_exit_ = false;
  raw_ptr<content::RenderFrameHost> wanted_rfh_;
};

}  // namespace

class FullscreenInteractiveBrowserTest : public InProcessBrowserTest {
 public:
  FullscreenInteractiveBrowserTest() {}

  FullscreenInteractiveBrowserTest(const FullscreenInteractiveBrowserTest&) =
      delete;
  FullscreenInteractiveBrowserTest& operator=(
      const FullscreenInteractiveBrowserTest&) = delete;

  ~FullscreenInteractiveBrowserTest() override {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// https://crbug.com/1087875: Flaky on Linux, Mac and Windows.
// TODO(crbug.com/40810181): Flaky on Chrome OS.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_WIN)
#define MAYBE_NotifyFullscreenAcquired DISABLED_NotifyFullscreenAcquired
#else
#define MAYBE_NotifyFullscreenAcquired NotifyFullscreenAcquired
#endif
IN_PROC_BROWSER_TEST_F(FullscreenInteractiveBrowserTest,
                       MAYBE_NotifyFullscreenAcquired) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b{allowfullscreen})");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(main_frame, "document.body.webkitRequestFullscreen();",
                       content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    observer.Wait();
  }

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(ExecJs(child_frame, "document.body.webkitRequestFullscreen();",
                       content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    observer.Wait();
  }

  // Exit fullscreen on the child frame.
  // This will not work with --site-per-process until crbug.com/617369
  // is fixed.
  if (!content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    {
      FullscreenWebContentsObserver observer(web_contents, main_frame);
      EXPECT_TRUE(ExecJs(child_frame, "document.webkitExitFullscreen();",
                         content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);

  // Make the top page fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(main_frame, "document.body.webkitRequestFullscreen();",
                       content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    observer.Wait();
  }

  // Make the child frame fullscreen.
  {
    FullscreenWebContentsObserver observer(web_contents, child_frame);
    EXPECT_TRUE(ExecJs(child_frame, "document.body.webkitRequestFullscreen();",
                       content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    observer.Wait();
  }

  // Exit fullscreen on the child frame.
  {
    FullscreenWebContentsObserver observer(web_contents, main_frame);
    EXPECT_TRUE(ExecJs(child_frame, "document.webkitExitFullscreen();",
                       content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(FullscreenInteractiveBrowserTest,
                       FullscreenDurationUmaLogged) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  base::HistogramTester histogram_tester;

  FullscreenWebContentsObserver observer(web_contents, main_frame);
  EXPECT_TRUE(ExecJs(main_frame, "document.body.requestFullscreen();"));
  observer.Wait();
  EXPECT_TRUE(ExecJs(main_frame, "document.exitFullscreen();"));
  observer.WaitForExit();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount(
      kFullscreenDurationMetricKeyRequestFullscreen, 1);
}

// TODO(crbug.com/40810181): Flaky on Chrome OS.
// TODO(crbug.com/40133132): Flaky on Linux.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_FullscreenDurationUmaLoggedCrossProcess \
  DISABLED_FullscreenDurationUmaLoggedCrossProcess
#else
#define MAYBE_FullscreenDurationUmaLoggedCrossProcess \
  FullscreenDurationUmaLoggedCrossProcess
#endif
IN_PROC_BROWSER_TEST_F(FullscreenInteractiveBrowserTest,
                       MAYBE_FullscreenDurationUmaLoggedCrossProcess) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b{allowfullscreen})");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = ChildFrameAt(main_frame, 0);
  base::HistogramTester histogram_tester;

  FullscreenWebContentsObserver observer(web_contents, child_frame);
  EXPECT_TRUE(ExecJs(child_frame, "document.body.requestFullscreen();"));
  observer.Wait();
  EXPECT_TRUE(ExecJs(main_frame, "document.exitFullscreen();"));
  observer.WaitForExit();

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount(
      kFullscreenDurationMetricKeyRequestFullscreen, 1);
}
