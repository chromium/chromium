// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

class SubresourceFilterInternalsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SubresourceFilterInternalsBrowserTest() {
    set_test_loader_host(chrome::kChromeUISubresourceFilterInternalsHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterInternalsBrowserTest, App) {
  RunTest("subresource_filter/subresource_filter_internals_test.js",
          "mocha.run()");
}
