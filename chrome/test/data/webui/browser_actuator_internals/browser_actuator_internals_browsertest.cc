// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/browser_actuator/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"

class BrowserActuatorInternalsAppBrowserTest : public WebUIMochaBrowserTest {
 protected:
  BrowserActuatorInternalsAppBrowserTest() {
    set_test_loader_host(
        browser_actuator::kChromeUIBrowserActuatorInternalsHost);
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{browser_actuator::kBrowserActuator,
                              browser_actuator::kBrowserActuatorInternals},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserActuatorInternalsAppBrowserTest, App) {
  RunTest("browser_actuator_internals/browser_actuator_internals_app_test.js",
          "mocha.run()");
}
