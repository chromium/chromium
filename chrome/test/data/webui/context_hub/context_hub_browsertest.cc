// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

class ContextHubBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ContextHubBrowserTest() {
    set_test_loader_host(chrome::kChromeUIContextHubHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    // chrome://context-hub is an internal-only WebUI.
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_F(ContextHubBrowserTest, Topics) {
  RunTestWithoutTestLoader("context_hub/topics_test.js", "mocha.run()");
}
