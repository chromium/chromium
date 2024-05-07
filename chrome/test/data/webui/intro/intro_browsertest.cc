// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/test/browser_test.h"

class IntroBrowserTest : public WebUIMochaBrowserTest {
 protected:
  IntroBrowserTest() { set_test_loader_host(chrome::kChromeUIIntroHost); }
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(IntroBrowserTest, SignInPromo) {
  RunTest("intro/sign_in_promo_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(IntroBrowserTest, DiceApp) {
  RunTest("intro/dice_app_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(IntroBrowserTest, LacrosApp) {
  RunTest("intro/lacros_app_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
