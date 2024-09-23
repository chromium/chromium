// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class DownloadsTest : public WebUIMochaBrowserTest {
 protected:
  DownloadsTest() { set_test_loader_host(chrome::kChromeUIDownloadsHost); }
};

IN_PROC_BROWSER_TEST_F(DownloadsTest, DangerousDownloadInterstitial) {
  RunTest("downloads/bypass_warning_confirmation_interstitial_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DownloadsTest, Item) {
  RunTest("downloads/item_test.js", "runMochaSuite('ItemTest')");
}

IN_PROC_BROWSER_TEST_F(DownloadsTest, Manager) {
  RunTest("downloads/manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DownloadsTest, Toolbar) {
  RunTest("downloads/toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DownloadsTest, SearchService) {
  RunTest("downloads/search_service_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(DownloadsTest, NonExistentUrl) {
  // Invoking the test from a non existent URL chrome://downloads/a/b/.
  set_test_loader_host(std::string(chrome::kChromeUIDownloadsHost) + "/a/b");
  RunTestWithoutTestLoader("downloads/non_existent_url_test.js", "mocha.run()");
}
