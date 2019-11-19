// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class InProcessBrowserTestP
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<const char*> {
};

IN_PROC_BROWSER_TEST_P(InProcessBrowserTestP, TestP) {
  EXPECT_EQ(0, strcmp("foo", GetParam()));
}

INSTANTIATE_TEST_SUITE_P(IPBTP,
                         InProcessBrowserTestP,
                         ::testing::Values("foo"));

// WebContents observer that can detect provisional load failures.
class LoadFailObserver : public content::WebContentsObserver {
 public:
  explicit LoadFailObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        failed_load_(false),
        error_code_(net::OK) { }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetNetErrorCode() == net::OK)
      return;

    failed_load_ = true;
    error_code_ = navigation_handle->GetNetErrorCode();
    validated_url_ = navigation_handle->GetURL();
  }

  bool failed_load() const { return failed_load_; }
  net::Error error_code() const { return error_code_; }
  const GURL& validated_url() const { return validated_url_; }

 private:
  bool failed_load_;
  net::Error error_code_;
  GURL validated_url_;

  DISALLOW_COPY_AND_ASSIGN(LoadFailObserver);
};

// Tests that InProcessBrowserTest cannot resolve external host, in this case
// "google.com" and "cnn.com". Using external resources is disabled by default
// in InProcessBrowserTest because it causes flakiness.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, ExternalConnectionFail) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const char* const kURLs[] = {
    "http://www.google.com/",
    "http://www.cnn.com/"
  };
  for (size_t i = 0; i < base::size(kURLs); ++i) {
    GURL url(kURLs[i]);
    LoadFailObserver observer(contents);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.failed_load());
    EXPECT_EQ(net::ERR_NOT_IMPLEMENTED, observer.error_code());
    EXPECT_EQ(url, observer.validated_url());
  }
}

// Verify that AfterStartupTaskUtils considers startup to be complete
// prior to test execution so tasks posted by tests are never deferred.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, AfterStartupTaskUtils) {
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
}

// On Mac this crashes inside cc::SingleThreadProxy::SetNeedsCommit. See
// https://ci.chromium.org/b/8923336499994443392
#if !defined(OS_MACOSX)
class SingleProcessBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

#if defined(OS_LINUX) || defined(OS_WIN)
// TODO(https://crbug.com/931233): Reenable on Linux.
// TODO(https://crbug.com/987448): Reenable on Windows.
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif

IN_PROC_BROWSER_TEST_F(SingleProcessBrowserTest, MAYBE_Test) {
  // Should not crash.
}

#endif

}  // namespace
