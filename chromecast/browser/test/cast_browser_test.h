// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_TEST_CAST_BROWSER_TEST_H_
#define CHROMECAST_BROWSER_TEST_CAST_BROWSER_TEST_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_web_view.h"
#include "content/public/test/browser_test_base.h"

namespace content {
class WebContents;
}

namespace chromecast {

class CastWebService;

namespace shell {

// This test allows for running an entire browser-process lifecycle per unit
// test, using Chromecast's cast_shell. This starts up the shell, runs a test
// case, then shuts down the entire shell.
// Note that this process takes 7-10 seconds per test case on Chromecast, so
// fewer test cases with more assertions are preferable.
class CastBrowserTest : public content::BrowserTestBase {
 public:
  CastBrowserTest(const CastBrowserTest&) = delete;
  CastBrowserTest& operator=(const CastBrowserTest&) = delete;

 protected:
  CastBrowserTest();
  ~CastBrowserTest() override;

  CastWebView* cast_web_view() const { return cast_web_view_.get(); }

  // content::BrowserTestBase implementation:
  void SetUp() final;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

  content::WebContents* CreateWebView();
  content::WebContents* NavigateToURL(const GURL& url);

 private:
  std::unique_ptr<CastWebService> web_service_;
  CastWebView::Scoped cast_web_view_;

  base::WeakPtrFactory<CastBrowserTest> weak_factory_{this};
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_CAST_BROWSER_TEST_H_
