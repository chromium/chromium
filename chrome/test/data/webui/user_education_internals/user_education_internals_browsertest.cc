// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

class UserEducationInternalsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  UserEducationInternalsBrowserTest() {
    set_test_loader_host(chrome::kChromeUIUserEducationInternalsHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    WebUIMochaBrowserTest::OnWebContentsAvailable(web_contents);
    RegisterAutoAcceptDialog(web_contents);
  }

 private:
  void RegisterAutoAcceptDialog(content::WebContents* web_contents) {
    auto* dialog_manager =
        javascript_dialogs::TabModalDialogManager::FromWebContents(
            web_contents);
    dialog_manager->SetDialogShownCallbackForTesting(
        base::BindOnce(&UserEducationInternalsBrowserTest::OnDialogShown,
                       base::Unretained(this), web_contents));
  }

  void OnDialogShown(content::WebContents* web_contents) {
    auto* dialog_manager =
        javascript_dialogs::TabModalDialogManager::FromWebContents(
            web_contents);
    dialog_manager->HandleJavaScriptDialog(web_contents, true, nullptr);
    // Register callback again for future dialogs.
    RegisterAutoAcceptDialog(web_contents);
  }
};

IN_PROC_BROWSER_TEST_F(UserEducationInternalsBrowserTest, All) {
  RunTest("user_education_internals/user_education_internals_test.js",
          "mocha.run()");
}
