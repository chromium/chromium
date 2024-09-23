// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_test.h"

using SigninTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(SigninTest, SyncConfirmationDefaultModal) {
  set_test_loader_host(chrome::kChromeUISyncConfirmationHost);
  RunTest("signin/sync_confirmation_test.js", "mocha.run()");
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SigninTest, SyncConfirmationInterceptModal) {
  set_test_loader_host(chrome::kChromeUISyncConfirmationHost);
  RunTest(base::StringPrintf(
              "signin/sync_confirmation_test.js&style=%d",
              static_cast<int>(SyncConfirmationStyle::kSigninInterceptModal)),
          "mocha.run()");
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

IN_PROC_BROWSER_TEST_F(SigninTest, SyncConfirmationWindow) {
  set_test_loader_host(chrome::kChromeUISyncConfirmationHost);
  RunTest(base::StringPrintf("signin/sync_confirmation_test.js&style=%d",
                             static_cast<int>(SyncConfirmationStyle::kWindow)),
          "mocha.run()");
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(SigninTest, SigninReauth) {
  set_test_loader_host(chrome::kChromeUISigninReauthHost);
  RunTest(base::StringPrintf(
              "signin/signin_reauth_test.js&access_point=%d",
              static_cast<int>(
                  signin_metrics::ReauthAccessPoint::kPasswordSaveBubble)),
          "mocha.run()");
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SigninTest, DiceWebSigninIntercept) {
  set_test_loader_host(chrome::kChromeUIDiceWebSigninInterceptHost);
  RunTest("signin/dice_web_signin_intercept_test.js", "mocha.run()");
}
IN_PROC_BROWSER_TEST_F(SigninTest, DiceWebSigninInterceptChromeSignin) {
  set_test_loader_host(chrome::kChromeUIDiceWebSigninInterceptHost);
  RunTest("signin/dice_web_signin_intercept_chrome_signin_test.js",
          "mocha.run()");
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

IN_PROC_BROWSER_TEST_F(SigninTest, ProfileCustomizationTest) {
  set_test_loader_host(chrome::kChromeUIProfileCustomizationHost);
  RunTest("signin/profile_customization_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SigninTest, SigninLegacyManagedUserProfileNotice) {
  set_test_loader_host(chrome::kChromeUIManagedUserProfileNoticeHost);
  RunTest("signin/legacy_managed_user_profile_notice_test.js", "mocha.run()");
}
