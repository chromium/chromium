// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class WelcomeBrowserTest : public WebUIMochaBrowserTest {
 protected:
  WelcomeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(welcome::kForceEnabled);
    set_test_loader_host(chrome::kChromeUIWelcomeHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, AppChooser) {
  RunTest("welcome/app_chooser_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, WelcomeApp) {
  RunTest("welcome/welcome_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, SigninView) {
  RunTest("welcome/signin_view_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, NavigationMixin) {
  RunTest("welcome/navigation_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, ModuleMetrics) {
  RunTest("welcome/module_metrics_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, SetAsDefault) {
  RunTest("welcome/nux_set_as_default_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(WelcomeBrowserTest, NtpBackground) {
  RunTest("welcome/nux_ntp_background_test.js", "mocha.run()");
}
