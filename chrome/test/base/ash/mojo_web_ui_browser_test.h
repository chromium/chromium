// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_MOJO_WEB_UI_BROWSER_TEST_H_
#define CHROME_TEST_BASE_ASH_MOJO_WEB_UI_BROWSER_TEST_H_

#include "chrome/test/base/ash/web_ui_browser_test.h"

// The runner of Mojo WebUI javascript based tests. The main difference between
// this and WebUIBrowserTest is that tests subclassing from this class use a
// mojo pipe to send the test result, so there is no reliance on chrome.send().
class MojoWebUIBrowserTest : public BaseWebUIBrowserTest {
 public:
  MojoWebUIBrowserTest();
  ~MojoWebUIBrowserTest() override;

  void set_use_mojo_modules() { use_mojo_modules_ = true; }

  // WebUIBrowserTest:
  void BrowsePreload(const GURL& browse_to) override;
  void SetUpOnMainThread() override;
  void SetupHandlers() override;

 private:
  class WebUITestContentBrowserClient;
  std::unique_ptr<WebUITestContentBrowserClient> test_content_browser_client_;

  bool use_mojo_modules_ = false;
};

#endif  // CHROME_TEST_BASE_ASH_MOJO_WEB_UI_BROWSER_TEST_H_
