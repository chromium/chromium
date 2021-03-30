// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using StandardOnboardingTabsParams =
    StartupTabProviderImpl::StandardOnboardingTabsParams;

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
    params.is_signed_in = true;
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

TEST(StartupTabProviderTest, GetInitialPrefsTabsForState) {
  std::vector<GURL> input = {GURL(u"https://new_tab_page"),
                             GURL(u"https://www.google.com"),
                             GURL(u"https://welcome_page")};

  StartupTabs output =
      StartupTabProviderImpl::GetInitialPrefsTabsForState(true, input);

  ASSERT_EQ(3U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_FALSE(output[0].is_pinned);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_FALSE(output[1].is_pinned);
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[2].url);
  EXPECT_FALSE(output[2].is_pinned);
}

TEST(StartupTabProviderTest, GetInitialPrefsTabsForState_FirstRunOnly) {
  std::vector<GURL> input = {GURL(u"https://www.google.com")};

  StartupTabs output =
      StartupTabProviderImpl::GetInitialPrefsTabsForState(false, input);

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
  pref.urls = {GURL(u"https://www.google.com")};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState_WrongType) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  pref_default.urls = {GURL(u"https://www.google.com")};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref_default, false);

  EXPECT_TRUE(output.empty());

  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  pref_last.urls = {GURL(u"https://www.google.com")};

  output = StartupTabProviderImpl::GetPreferencesTabsForState(pref_last, false);

  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPreferencesTabsForState_NotFirstBrowser) {
  SessionStartupPref pref(SessionStartupPref::Type::URLS);
  pref.urls = {GURL(u"https://www.google.com")};

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
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  Profile* incognito = profile.GetPrimaryOTRProfile();
  StartupTabs output = StartupTabProviderImpl().GetOnboardingTabs(incognito);
  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetNewTabPageTabsForState_ExtensionsCheckup) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);

  StartupTabs output = StartupTabProviderImpl::GetExtensionCheckupTabsForState(
      /*serve_extensions_page=*/true);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("chrome://extensions/?checkup=shown", output[0].url);
}
