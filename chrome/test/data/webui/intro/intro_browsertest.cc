// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"

class IntroBrowserTest : public WebUIMochaBrowserTest {
 protected:
  IntroBrowserTest() { set_test_loader_host(chrome::kChromeUIIntroHost); }
};

IN_PROC_BROWSER_TEST_F(IntroBrowserTest, SignInPromo) {
  RunTest("intro/sign_in_promo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(IntroBrowserTest, IntroApp) {
  RunTest("intro/intro_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(IntroBrowserTest, SignInCelebration) {
  RunTest("intro/sign_in_celebration_test.js", "mocha.run()");
}

class IntroBrowserTestWithRefreshEnabled : public IntroBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kFirstRunDesktopRefresh};
};

IN_PROC_BROWSER_TEST_F(IntroBrowserTestWithRefreshEnabled, SignInPromoRefresh) {
  RunTest("intro/sign_in_promo_refresh_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(IntroBrowserTestWithRefreshEnabled,
                       DefaultBrowserRefresh) {
  RunTest("intro/default_browser_refresh_test.js", "mocha.run()");
}
