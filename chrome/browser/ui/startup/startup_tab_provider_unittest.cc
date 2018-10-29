// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/ui/webui/welcome/nux/constants.h"
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

using StandardOnboardingTabsParams =
    StartupTabProviderImpl::StandardOnboardingTabsParams;
using Win10OnboardingTabsParams =
    StartupTabProviderImpl::Win10OnboardingTabsParams;

TEST(StartupTabProviderTest, GetStandardOnboardingTabsForState) {
  {
    // Show welcome page to new unauthenticated profile on first run.
    StandardOnboardingTabsParams params;
    params.is_first_run = true;
    params.is_signin_allowed = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
  {
    // After first run, display welcome page using variant view.
    StandardOnboardingTabsParams params;
    params.is_signin_allowed = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
}

TEST(StartupTabProviderTest, GetStandardOnboardingTabsForState_Negative) {
  {
    // Do not show the welcome page to the same profile twice.
    StandardOnboardingTabsParams params;
    params.is_first_run = true;
    params.has_seen_welcome_page = true;
    params.is_signin_allowed = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);
    EXPECT_TRUE(output.empty());
  }
  {
    // Do not show the welcome page to authenticated users.
    StandardOnboardingTabsParams params;
    params.is_first_run = true;
    params.is_signin_allowed = true;
    params.is_signin_in_progress = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);
    EXPECT_TRUE(output.empty());
  }
  {
    // Do not show the welcome page if sign-in is disabled.
    StandardOnboardingTabsParams params;
    params.is_first_run = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);
    EXPECT_TRUE(output.empty());
  }
  {
    // Do not show the welcome page to supervised users.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;
    standard_params.is_supervised_user = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(
            standard_params);
    EXPECT_TRUE(output.empty());
  }
  {
    // Do not show the welcome page if force-sign-in policy is enabled.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;
    standard_params.is_force_signin_enabled = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(
            standard_params);
    EXPECT_TRUE(output.empty());
  }
}

#if defined(OS_WIN)
TEST(StartupTabProviderTest, GetWin10OnboardingTabsForState) {
  {
    // Show Win 10 Welcome page if it has not been seen, but the standard page
    // has.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.has_seen_welcome_page = true;
    standard_params.is_signin_allowed = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
              output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
  {
    // Show standard Welcome page if the Win 10 Welcome page has been seen, but
    // the standard page has not.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.has_seen_win10_promo = true;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
  {
    // If neither page has been seen, the Win 10 Welcome page takes precedence
    // this launch.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
              output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
}

TEST(StartupTabProviderTest, GetWin10OnboardingTabsForState_LaterRunVariant) {
  StandardOnboardingTabsParams standard_params;
  standard_params.is_signin_allowed = true;
  {
    // Show a variant of the Win 10 Welcome page after first run, if it has not
    // been seen.
    Win10OnboardingTabsParams win10_params;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(true),
              output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
  {
    // Show a variant of the standard Welcome page after first run, if the Win
    // 10 Welcome page has already been seen but the standard has not.
    Win10OnboardingTabsParams win10_params;
    win10_params.has_seen_win10_promo = true;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
}

TEST(StartupTabProviderTest, GetWin10OnboardingTabsForState_Negative) {
  {
    // Do not show either page if it has already been shown.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.has_seen_welcome_page = true;
    standard_params.is_signin_allowed = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.has_seen_win10_promo = true;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    EXPECT_TRUE(output.empty());
  }
  {
    // Do not show either page to supervised users.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;
    standard_params.is_supervised_user = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    EXPECT_TRUE(output.empty());
  }
  {
    // If Chrome is already the default browser, don't show the Win 10 Welcome
    // page, and don't preempt the standard Welcome page.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.set_default_browser_allowed = true;
    win10_params.is_default_browser = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
    EXPECT_FALSE(output[0].is_pinned);
  }
  {
    // If the user is signed in, block showing the standard Welcome page.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;
    standard_params.is_signed_in = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.has_seen_win10_promo = true;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    EXPECT_TRUE(output.empty());
  }
  {
    // If sign-in is in progress, block showing the standard Welcome page.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;
    standard_params.is_signin_in_progress = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.has_seen_win10_promo = true;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    EXPECT_TRUE(output.empty());
  }
  {
    // If sign-in is disabled, block showing the standard Welcome page.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;

    Win10OnboardingTabsParams win10_params;
    win10_params.has_seen_win10_promo = true;
    win10_params.set_default_browser_allowed = true;

    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, win10_params);

    EXPECT_TRUE(output.empty());
  }
}

TEST(StartupTabProviderTest,
     GetWin10OnboardingTabsForState_SetDefaultBrowserNotAllowed) {
  {
    // Skip the Win 10 promo if setting the default browser is not allowed.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.is_signin_allowed = true;
    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, Win10OnboardingTabsParams());

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
  }
  {
    // After first run, no onboarding content is displayed when setting the
    // default browser is not allowed.
    StandardOnboardingTabsParams standard_params;
    standard_params.is_first_run = true;
    standard_params.has_seen_welcome_page = true;
    standard_params.is_signin_allowed = true;
    StartupTabs output = StartupTabProviderImpl::GetWin10OnboardingTabsForState(
        standard_params, Win10OnboardingTabsParams());

    EXPECT_TRUE(output.empty());
  }
}
#endif  // defined(OS_WIN)

TEST(StartupTabProviderTest, GetMasterPrefsTabsForState) {
  std::vector<GURL> input = {GURL(base::ASCIIToUTF16("https://new_tab_page")),
                             GURL(base::ASCIIToUTF16("https://www.google.com")),
                             GURL(base::ASCIIToUTF16("https://welcome_page"))};

  StartupTabs output =
      StartupTabProviderImpl::GetMasterPrefsTabsForState(true, input);

  ASSERT_EQ(3U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_FALSE(output[1].is_pinned);
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[2].url);
  EXPECT_FALSE(output[2].is_pinned);
}

TEST(StartupTabProviderTest, GetMasterPrefsTabsForState_FirstRunOnly) {
  std::vector<GURL> input = {
      GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetMasterPrefsTabsForState(false, input);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetResetTriggerTabsForState) {
  StartupTabs output =
      StartupTabProviderImpl::GetResetTriggerTabsForState(true);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(StartupTabProviderImpl::GetTriggeredResetSettingsUrl(),
            output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
}

TEST(StartupTabProviderTest, GetResetTriggerTabsForState_Negative) {
  StartupTabs output =
      StartupTabProviderImpl::GetResetTriggerTabsForState(false);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPinnedTabsForState) {
  StartupTabs pinned = {StartupTab(GURL("https://www.google.com"), true)};
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  StartupTabs output = StartupTabProviderImpl::GetPinnedTabsForState(
      pref_default, pinned, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());

  output =
      StartupTabProviderImpl::GetPinnedTabsForState(pref_urls, pinned, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, GetPinnedTabsForState_Negative) {
  StartupTabs pinned = {StartupTab(GURL("https://www.google.com"), true)};
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);

  // Session restore preference should block reading pinned tabs.
  StartupTabs output =
      StartupTabProviderImpl::GetPinnedTabsForState(pref_last, pinned, false);

  ASSERT_TRUE(output.empty());

  // Pinned tabs are not added when this profile already has a nonempty tabbed
  // browser open.
  output =
      StartupTabProviderImpl::GetPinnedTabsForState(pref_default, pinned, true);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState) {
  SessionStartupPref pref(SessionStartupPref::Type::URLS);
  pref.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState_WrongType) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  pref_default.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref_default, false);

  EXPECT_TRUE(output.empty());

  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  pref_last.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  output = StartupTabProviderImpl::GetPreferencesTabsForState(pref_last, false);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState_NotFirstBrowser) {
  SessionStartupPref pref(SessionStartupPref::Type::URLS);
  pref.urls = {GURL(base::ASCIIToUTF16("https://www.google.com"))};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref, true);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetNewTabPageTabsForState) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  StartupTabs output =
      StartupTabProviderImpl::GetNewTabPageTabsForState(pref_default);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);

  output = StartupTabProviderImpl::GetNewTabPageTabsForState(pref_urls);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
}

TEST(StartupTabProviderTest, GetNewTabPageTabsForState_Negative) {
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);

  StartupTabs output =
      StartupTabProviderImpl::GetNewTabPageTabsForState(pref_last);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, IncognitoProfile) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  Profile* incognito = profile.GetOffTheRecordProfile();
  StartupTabs output = StartupTabProviderImpl().GetOnboardingTabs(incognito);
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWin10WelcomePageUrl(false),
              output[0].url.GetOrigin());
  } else {
    EXPECT_TRUE(output.empty());
  }
#else
  EXPECT_TRUE(output.empty());
#endif
}
