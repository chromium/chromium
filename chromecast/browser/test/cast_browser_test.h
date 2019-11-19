// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_TEST_CAST_BROWSER_TEST_H_
#define CHROMECAST_BROWSER_TEST_CAST_BROWSER_TEST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_web_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace content {
class WebContents;
}

namespace chromecast {

class CastWebService;
class CastWebViewFactory;

namespace shell {

// This test allows for running an entire browser-process lifecycle per unit
// test, using Chromecast's cast_shell. This starts up the shell, runs a test
// case, then shuts down the entire shell.
// Note that this process takes 7-10 seconds per test case on Chromecast, so
// fewer test cases with more assertions are preferable.
class CastBrowserTest : public content::BrowserTestBase,
                        public CastWebView::Delegate {
 protected:
  CastBrowserTest();
  ~CastBrowserTest() override;

  // content::BrowserTestBase implementation:
  void SetUp() final;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

  content::WebContents* CreateWebView();
  content::WebContents* NavigateToURL(const GURL& url);

 private:
  // CastWebView::Delegate implementation:
  void OnWindowDestroyed() override;
  void OnVisibilityChange(VisibilityType visibility_type) override;
  bool CanHandleGesture(GestureType gesture_type) override;
  bool ConsumeGesture(GestureType gesture_type) override;
  std::string GetId() override;

  std::unique_ptr<CastWebViewFactory> web_view_factory_;
  std::unique_ptr<CastWebService> web_service_;
  CastWebView::Scoped cast_web_view_;

  base::WeakPtrFactory<CastBrowserTest> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CastBrowserTest);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_TEST_CAST_BROWSER_TEST_H_
