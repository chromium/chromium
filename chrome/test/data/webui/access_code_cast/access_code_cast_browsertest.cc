// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

class AccessCodeCastTest : public WebUIMochaBrowserTest {
 protected:
  AccessCodeCastTest() {
    set_test_loader_host(chrome::kChromeUIAccessCodeCastHost);
  }

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        media_router::prefs::kAccessCodeCastEnabled, true);
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessCodeCastUI};
};

IN_PROC_BROWSER_TEST_F(AccessCodeCastTest, App) {
  RunTest("access_code_cast/access_code_cast_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastTest, BrowserProxy) {
  RunTest("access_code_cast/browser_proxy_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastTest, ErrorMessage) {
  RunTest("access_code_cast/error_message_test.js", "mocha.run()");
}

// TODO(crbug.com/40864933): PasscodeInput has started acting flaky ().
// Disabling for now pending investigation.
IN_PROC_BROWSER_TEST_F(AccessCodeCastTest, DISABLED_PasscodeInput) {
  RunTest("access_code_cast/passcode_input_test.js", "mocha.run()");
}
