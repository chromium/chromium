// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_test_base.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class CrExtensionsFocusTest : public WebUIMochaFocusTest {
 protected:
  CrExtensionsFocusTest() {
    set_test_loader_host(chrome::kChromeUIExtensionsHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrExtensionsFocusTest, UninstallFocus) {
  RunTest("extensions/manager_unit_test.js",
          "runMochaTest('ExtensionManagerUnitTest', 'UninstallFocus')");
}

IN_PROC_BROWSER_TEST_F(CrExtensionsFocusTest, UpdateShortcut) {
  RunTest("extensions/keyboard_shortcuts_test.js",
          "runMochaTest('ExtensionShortcutTest', 'UpdateShortcut')");
}

class CrExtensionsOptionsPageTest : public ExtensionSettingsTestBase {
 protected:
  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    web_contents->Focus();
  }
};

// Disabled due to flakiness, see https://crbug.com/945654
IN_PROC_BROWSER_TEST_F(CrExtensionsOptionsPageTest, DISABLED_All) {
  set_test_loader_host(chrome::kChromeUIExtensionsHost);
  InstallExtensionWithInPageOptions();
  RunTest("extensions/extension_options_dialog_test.js", "mocha.run()");
}
