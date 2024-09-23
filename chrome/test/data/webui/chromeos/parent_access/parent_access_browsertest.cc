// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class ParentAccessMochaTest : public WebUIMochaBrowserTest {
 protected:
  ParentAccessMochaTest() {
    set_test_loader_host(chrome::kChromeUIParentAccessHost);
  }

  void RunParentAccessTest(const std::string& test_path) {
    RunTest(base::StrCat({"chromeos/parent_access/", test_path}),
            "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, App) {
  RunParentAccessTest("parent_access_app_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, After) {
  RunParentAccessTest("parent_access_after_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, Controller) {
  RunParentAccessTest("parent_access_controller_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, Disabled) {
  RunParentAccessTest("parent_access_disabled_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, UiHandler) {
  RunParentAccessTest("parent_access_ui_handler_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, Ui) {
  RunParentAccessTest("parent_access_ui_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, ExtensionApprovals) {
  RunParentAccessTest("extension_approvals_test.js");
}

IN_PROC_BROWSER_TEST_F(ParentAccessMochaTest, WebviewManager) {
  RunParentAccessTest("webview_manager_test.js");
}
