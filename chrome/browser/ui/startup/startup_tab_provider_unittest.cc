// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

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
  Profile* incognito = profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  StartupTabs output = StartupTabProviderImpl().GetOnboardingTabs(incognito);
  EXPECT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetCommandLineTabs) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  // Set up and inject a real instance for the profile.
  TemplateURLServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      &profile,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

  // Empty arguments case.
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    EXPECT_TRUE(output.empty());
  }

  // Simple use. Pass google.com URL.
  {
    base::CommandLine command_line({"", "https://google.com"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("https://google.com"), output[0].url);
  }

  // Two URL case.
  {
    base::CommandLine command_line(
        {"", "https://google.com", "https://gmail.com"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_EQ(2u, output.size());
    EXPECT_EQ(GURL("https://google.com"), output[0].url);
    EXPECT_EQ(GURL("https://gmail.com"), output[1].url);
  }

  // Vista way search query.
  {
    base::CommandLine command_line({"", "? Foo"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(
        GURL("https://www.google.com/search?q=Foo&sourceid=chrome&ie=UTF-8"),
        output[0].url);
  }

  // "file:" path fix up.
  {
    base::CommandLine command_line({"", "file:foo.txt"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("file:///foo.txt"), output[0].url);
  }

  // Unsafe scheme should be filtered out.
  {
    base::CommandLine command_line({"", "chrome://flags"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_TRUE(output.empty());
  }

  // Exceptional settings page.
  {
    base::CommandLine command_line(
        {"", "chrome://settings/resetProfileSettings"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("chrome://settings/resetProfileSettings"), output[0].url);
  }
  {
    base::CommandLine command_line(
        {"", "chrome://settings/resetProfileSettings#cct"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    // Allowed only on Windows.
#if defined(OS_WIN)
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("chrome://settings/resetProfileSettings#cct"),
              output[0].url);
#else
    EXPECT_TRUE(output.empty());
#endif
  }

  // chrome://settings/ page handling.
  {
    base::CommandLine command_line({"", "chrome://settings/syncSetup"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // On Chrome OS (ash-chrome), settings page is allowed to be specified.
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("chrome://settings/syncSetup"), output[0].url);
#else
    // On other platforms, it is blocked.
    EXPECT_TRUE(output.empty());
#endif
  }

  // about:blank URL.
  {
    base::CommandLine command_line({"", "about:blank"});
    StartupTabs output = StartupTabProviderImpl().GetCommandLineTabs(
        command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("about:blank"), output[0].url);
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(StartupTabProviderTest, GetCrosapiTabs) {
  base::test::TaskEnvironment task_environment;

  chromeos::LacrosService lacros_service;

  // Non kOpenWindowWithUrls case.
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->initial_browser_action =
        crosapi::mojom::InitialBrowserAction::kUseStartupPreference;
    // The given URLs should be ignored.
    params->startup_urls = std::vector<GURL>{GURL("https://google.com")};
    lacros_service.SetInitParamsForTests(std::move(params));
    StartupTabs output = StartupTabProviderImpl().GetCrosapiTabs();
    EXPECT_TRUE(output.empty());
  }

  // Simple use. Pass google.com URL.
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->initial_browser_action =
        crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls;
    // The given URLs should be ignored.
    params->startup_urls = std::vector<GURL>{GURL("https://google.com")};
    lacros_service.SetInitParamsForTests(std::move(params));
    StartupTabs output = StartupTabProviderImpl().GetCrosapiTabs();
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("https://google.com"), output[0].url);
  }

  // Two URL case.
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->initial_browser_action =
        crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls;
    params->startup_urls = std::vector<GURL>{GURL("https://google.com"),
                                             GURL("https://gmail.com")};
    lacros_service.SetInitParamsForTests(std::move(params));
    StartupTabs output = StartupTabProviderImpl().GetCrosapiTabs();
    ASSERT_EQ(2u, output.size());
    EXPECT_EQ(GURL("https://google.com"), output[0].url);
    EXPECT_EQ(GURL("https://gmail.com"), output[1].url);
  }

  // Unsafe scheme should be filtered out.
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->initial_browser_action =
        crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls;
    params->startup_urls = std::vector<GURL>{GURL("chrome://flags")};
    lacros_service.SetInitParamsForTests(std::move(params));
    StartupTabs output = StartupTabProviderImpl().GetCrosapiTabs();
    EXPECT_TRUE(output.empty());
  }

  // Exceptional settings page.
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->initial_browser_action =
        crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls;
    params->startup_urls =
        std::vector<GURL>{GURL("chrome://settings/resetProfileSettings")};
    lacros_service.SetInitParamsForTests(std::move(params));
    StartupTabs output = StartupTabProviderImpl().GetCrosapiTabs();
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("chrome://settings/resetProfileSettings"), output[0].url);
  }

  // about:blank URL.
  {
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->initial_browser_action =
        crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls;
    params->startup_urls = std::vector<GURL>{GURL("about:blank")};
    lacros_service.SetInitParamsForTests(std::move(params));
    StartupTabs output = StartupTabProviderImpl().GetCrosapiTabs();
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("about:blank"), output[0].url);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
