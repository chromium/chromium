// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;

class MockLoginUIService : public LoginUIService {
 public:
  explicit MockLoginUIService(content::BrowserContext* context)
      : LoginUIService(static_cast<Profile*>(context)) {}
  ~MockLoginUIService() override {}
  MOCK_METHOD3(DisplayLoginResult,
               void(Browser* browser,
                    const base::string16& error_message,
                    const base::string16& email));
  MOCK_METHOD0(SetProfileBlockingErrorMessage, void(void));
};

std::unique_ptr<KeyedService> CreateLoginUIService(
    content::BrowserContext* context) {
  return std::make_unique<MockLoginUIService>(context);
}

class UserManagerUIBrowserTest : public InProcessBrowserTest,
                                 public testing::WithParamInterface<bool> {
 public:
  UserManagerUIBrowserTest() {}
};

IN_PROC_BROWSER_TEST_F(UserManagerUIBrowserTest, PageLoads) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIMdUserManagerUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::string16 title = web_contents->GetTitle();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), title);

  // If the page has loaded correctly, then there should be an account picker.
  int num_account_pickers = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "domAutomationController.send("
      "document.getElementsByClassName('account-picker').length)",
      &num_account_pickers));
  EXPECT_EQ(1, num_account_pickers);

  int num_pods = -1;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "domAutomationController.send("
      "parseInt(document.getElementById('pod-row').getAttribute('ncolumns')))",
      &num_pods));

  // There should be one user pod for each profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(num_pods, static_cast<int>(profile_manager->GetNumberOfProfiles()));
}

// https://crbug.com/945795
IN_PROC_BROWSER_TEST_F(UserManagerUIBrowserTest,
                       DISABLED_PageRedirectsToAboutChrome) {
  std::string user_manager_url = chrome::kChromeUIMdUserManagerUrl;
  user_manager_url += profiles::kUserManagerSelectProfileAboutChrome;

  ui_test_utils::NavigateToURL(browser(), GURL(user_manager_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // If this is a Windows style path, escape all the slashes.
  std::string profile_path;
  base::ReplaceChars(browser()->profile()->GetPath().MaybeAsASCII(),
      "\\", "\\\\", &profile_path);

  std::string launch_js =
      base::StringPrintf("Oobe.launchUser('%s')", profile_path.c_str());

  bool result = content::ExecuteScript(web_contents, launch_js);
  EXPECT_TRUE(result);
  base::RunLoop().RunUntilIdle();

  content::WebContents* about_chrome_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL current_URL = about_chrome_contents->GetVisibleURL();
  EXPECT_EQ(GURL(chrome::kChromeUIHelpURL), current_URL);
}

class UserManagerUIAuthenticatedUserBrowserTest
    : public UserManagerUIBrowserTest {
 public:
  void Init() {
    ui_test_utils::NavigateToURL(browser(),
                                 GURL(chrome::kChromeUIMdUserManagerUrl));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    profile_ = browser()->profile();
    EXPECT_TRUE(
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath(), &entry_));
  }

  void LaunchAuthenticatedUser(const std::string& email) {
    std::string profile_path;
    base::ReplaceChars(profile_->GetPath().MaybeAsASCII(), "\\", "\\\\",
                       &profile_path);
    std::string launch_js = base::StringPrintf(
        "chrome.send('authenticatedLaunchUser', ['%s', '%s', ''])",
        profile_path.c_str(), email.c_str());
    EXPECT_TRUE(content::ExecuteScript(web_contents_, launch_js));
  }

  content::WebContents* web_contents_;
  Profile* profile_;
  ProfileAttributesEntry* entry_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(UserManagerUIAuthenticatedUserBrowserTest, Reauth) {
  Init();
  signin_util::SetForceSigninForTesting(true);
  entry_->SetLocalAuthCredentials("1mock_credentials");

  LaunchAuthenticatedUser("email@mock.com");

  histogram_tester_.ExpectBucketCount(
      kAuthenticatedLaunchUserEventMetricsName,
      AuthenticatedLaunchUserEvent::LOCAL_REAUTH_DIALOG, 1);
  histogram_tester_.ExpectBucketCount(
      kAuthenticatedLaunchUserEventMetricsName,
      AuthenticatedLaunchUserEvent::GAIA_REAUTH_DIALOG, 1);
  histogram_tester_.ExpectTotalCount(kAuthenticatedLaunchUserEventMetricsName,
                                     2);
}

IN_PROC_BROWSER_TEST_F(UserManagerUIAuthenticatedUserBrowserTest,
                       SupervisedUserBlocked) {
  Init();
  entry_->SetIsSigninRequired(true);
  entry_->SetSupervisedUserId("supervised_user_id");
  MockLoginUIService* service = static_cast<MockLoginUIService*>(
      LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_, base::BindRepeating(&CreateLoginUIService)));
  EXPECT_CALL(*service, DisplayLoginResult(_, _, _));

  LaunchAuthenticatedUser("");
  histogram_tester_.ExpectUniqueSample(
      kAuthenticatedLaunchUserEventMetricsName,
      AuthenticatedLaunchUserEvent::SUPERVISED_PROFILE_BLOCKED_WARNING, 1);
}

IN_PROC_BROWSER_TEST_F(UserManagerUIAuthenticatedUserBrowserTest,
                       NormalUserBlocked) {
  Init();
  signin_util::SetForceSigninForTesting(true);
  entry_->SetIsSigninRequired(true);
  entry_->SetActiveTimeToNow();
  MockLoginUIService* service = static_cast<MockLoginUIService*>(
      LoginUIServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_, base::BindRepeating(&CreateLoginUIService)));
  EXPECT_CALL(*service, SetProfileBlockingErrorMessage());

  LaunchAuthenticatedUser("");

  histogram_tester_.ExpectUniqueSample(
      kAuthenticatedLaunchUserEventMetricsName,
      AuthenticatedLaunchUserEvent::USED_PROFILE_BLOCKED_WARNING, 1);
  signin_util::ResetForceSigninForTesting();
}

IN_PROC_BROWSER_TEST_F(UserManagerUIAuthenticatedUserBrowserTest,
                       ForcedPrimarySignin) {
  Init();
  signin_util::SetForceSigninForTesting(true);

  LaunchAuthenticatedUser("");

  histogram_tester_.ExpectUniqueSample(
      kAuthenticatedLaunchUserEventMetricsName,
      AuthenticatedLaunchUserEvent::FORCED_PRIMARY_SIGNIN_DIALOG, 1);
}

// TODO(mlerman): Test that unlocking a locked profile causes the extensions
// service to become unblocked.
