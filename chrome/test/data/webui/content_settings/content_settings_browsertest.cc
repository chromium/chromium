// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

namespace {

class ContentSettingsMochaTest : public WebUIMochaBrowserTest {
 public:
  ContentSettingsMochaTest() {
    set_test_loader_host(chrome::kChromeUIContentSettingsHost);
  }

  void SetUpOnMainThread() override {
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ContentSettingsMochaTest, CustomElements) {
  RunTest("content_settings/app_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContentSettingsMochaTest, ContentSettingsCustomElement) {
  RunTest("content_settings/content_settings_test.js", "mocha.run();");
}

}  // namespace
