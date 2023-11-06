// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Base test class that performs additional setup that is only needed when the
// `ash::standalone_browser::features::kLacrosOnly` flag is used.
// TODO(crbug.com/1457360): Decide whether this needs to be added to
// WebUIMochaBrowserTest directly. Keeping it separate for now until more tests
// that need it are migrated to WebUIMochaBrowserTest.
class LacrosOnlyMochaBrowserTest : public WebUIMochaBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    if (ash_starter_->HasLacrosArgument()) {
      ASSERT_TRUE(ash_starter_->PrepareEnvironmentForLacros());
    }
    WebUIMochaBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    if (ash_starter_->HasLacrosArgument()) {
      ash_starter_->StartLacros(this);
    }

    if (crosapi::browser_util::IsLacrosEnabled() && browser() == nullptr) {
      // Create a new Ash browser window so test code using browser() can work
      // even when Lacros is the only browser.
      // TODO(crbug.com/1450158): Remove uses of browser() from such tests.
      chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
      SelectFirstBrowser();
    }
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

 private:
  std::unique_ptr<test::AshBrowserTestStarter> ash_starter_;
};

class InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase
    : public LacrosOnlyMochaBrowserTest {
 protected:
  InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
    scoped_feature_list_.InitWithFeatures(
        {
            ash::standalone_browser::features::kLacrosOnly,
            ash::standalone_browser::features::kLacrosProfileMigrationForceOff,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class InlineLoginBrowserTest : public WebUIMochaBrowserTest {
 protected:
  InlineLoginBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "inline_login/inline_login_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf("runMochaTest('InlineLoginTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, Initialize) {
  RunTestCase("Initialize");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, WebUICallbacks) {
  RunTestCase("WebUICallbacks");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, AuthenticatorCallbacks) {
  RunTestCase("AuthenticatorCallbacks");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, BackButton) {
  RunTestCase("BackButton");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTest, OkButton) {
  RunTestCase("OkButton");
}

class InlineLoginBrowserTestWithArcAccountRestrictionsEnabled
    : public InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase {
 protected:
  void RunTestCase(const std::string& testCase) {
    InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase::RunTest(
        base::StringPrintf(
            "inline_login/inline_login_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf("runMochaTest('InlineLoginTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTestWithArcAccountRestrictionsEnabled,
                       Initialize) {
  RunTestCase("Initialize");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTestWithArcAccountRestrictionsEnabled,
                       WebUICallbacks) {
  RunTestCase("WebUICallbacks");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTestWithArcAccountRestrictionsEnabled,
                       AuthenticatorCallbacks) {
  RunTestCase("AuthenticatorCallbacks");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTestWithArcAccountRestrictionsEnabled,
                       BackButton) {
  RunTestCase("BackButton");
}

IN_PROC_BROWSER_TEST_F(InlineLoginBrowserTestWithArcAccountRestrictionsEnabled,
                       OkButton) {
  RunTestCase("OkButton");
}

class InlineLoginWelcomePageBrowserTest : public WebUIMochaBrowserTest {
 protected:
  InlineLoginWelcomePageBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "inline_login/inline_login_welcome_page_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf("runMochaTest('InlineLoginWelcomePageTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, Reauthentication) {
  RunTestCase("Reauthentication");
}

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, OkButton) {
  RunTestCase("OkButton");
}

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, Checkbox) {
  RunTestCase("Checkbox");
}

IN_PROC_BROWSER_TEST_F(InlineLoginWelcomePageBrowserTest, GoBack) {
  RunTestCase("GoBack");
}

class InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled
    : public InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase {
 protected:
  void RunTestCase(const std::string& testCase) {
    InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase::RunTest(
        base::StringPrintf(
            "inline_login/inline_login_welcome_page_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf("runMochaTest('InlineLoginWelcomePageTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(
    InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled,
    Reauthentication) {
  RunTestCase("Reauthentication");
}

IN_PROC_BROWSER_TEST_F(
    InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled,
    OkButton) {
  RunTestCase("OkButton");
}

IN_PROC_BROWSER_TEST_F(
    InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled,
    GoBack) {
  RunTestCase("GoBack");
}

IN_PROC_BROWSER_TEST_F(
    InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled,
    IsAvailableInArc) {
  RunTestCase("IsAvailableInArc");
}

IN_PROC_BROWSER_TEST_F(
    InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled,
    ToggleHidden) {
  RunTestCase("ToggleHidden");
}

IN_PROC_BROWSER_TEST_F(
    InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled,
    LinkClick) {
  RunTestCase("LinkClick");
}

class InlineLoginArcAccountPickerBrowserTest
    : public InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase {
 protected:
  void RunTestCase(const std::string& testCase) {
    InlineLoginBrowserTestWithArcAccountRestrictionsEnabledBase::RunTest(
        base::StringPrintf(
            "inline_login/arc_account_picker_page_test.js&reason=%d",
            static_cast<int>(
                signin_metrics::Reason::kForcedSigninPrimaryAccount)),
        base::StringPrintf(
            "runMochaTest('InlineLoginArcPickerPageTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginArcAccountPickerBrowserTest,
                       ArcPickerActive) {
  RunTestCase("ArcPickerActive");
}

IN_PROC_BROWSER_TEST_F(InlineLoginArcAccountPickerBrowserTest,
                       ArcPickerHiddenForReauth) {
  RunTestCase("ArcPickerHiddenForReauth");
}

IN_PROC_BROWSER_TEST_F(InlineLoginArcAccountPickerBrowserTest,
                       ArcPickerHiddenNoAccounts) {
  RunTestCase("ArcPickerHiddenNoAccounts");
}

IN_PROC_BROWSER_TEST_F(InlineLoginArcAccountPickerBrowserTest, AddAccount) {
  RunTestCase("AddAccount");
}

IN_PROC_BROWSER_TEST_F(InlineLoginArcAccountPickerBrowserTest,
                       MakeAvailableInArc) {
  RunTestCase("MakeAvailableInArc");
}

class InlineLoginSigninBlockedByPolicyPageBrowserTest
    : public WebUIMochaBrowserTest {
 protected:
  InlineLoginSigninBlockedByPolicyPageBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeSigninHost);
  }

  void RunTestCase(const std::string& testCase) {
    WebUIMochaBrowserTest::RunTest(
        base::StringPrintf(
            "inline_login/"
            "inline_login_signin_blocked_by_policy_page_test.js&reason=%d",
            static_cast<int>(signin_metrics::Reason::kAddSecondaryAccount)),
        base::StringPrintf(
            "runMochaTest('InlineLoginSigninBlockedByPolicyPageTest', '%s');",
            testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(InlineLoginSigninBlockedByPolicyPageBrowserTest,
                       BlockedSigninPage) {
  RunTestCase("BlockedSigninPage");
}

IN_PROC_BROWSER_TEST_F(InlineLoginSigninBlockedByPolicyPageBrowserTest,
                       OkButton) {
  RunTestCase("OkButton");
}

IN_PROC_BROWSER_TEST_F(InlineLoginSigninBlockedByPolicyPageBrowserTest,
                       FireWebUIListenerCallback) {
  RunTestCase("FireWebUIListenerCallback");
}
#endif
