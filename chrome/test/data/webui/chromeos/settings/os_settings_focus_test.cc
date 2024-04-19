// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class OsSettingsFocusTest : public WebUIMochaFocusTest {
 protected:
  OsSettingsFocusTest() {
    set_test_loader_host(chrome::kChromeUIOSSettingsHost);
  }

  void RunSettingsTest(const std::string& current_path) {
    // All OS Settings test files are located in the directory
    // chromeos/settings/.
    const std::string path_with_parent_directory =
        base::StrCat({std::string("chromeos/settings/"), current_path});
    RunTest(path_with_parent_directory, "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(OsSettingsFocusTest, OsPrivacyPageSecureDns) {
  RunSettingsTest("os_privacy_page/secure_dns_interactive_test.js");
}
