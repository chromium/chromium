// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/values.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using StandardOnboardingTabsParams =
    StartupTabProviderImpl::StandardOnboardingTabsParams;

#if BUILDFLAG(IS_WIN)
#define CMD_ARG(x) L##x
#else
#define CMD_ARG(x) x
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(StartupTabProviderTest, GetStandardOnboardingTabsForState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kForYouFre);

  {
    // Show welcome page to new unauthenticated profile on first run.
    StandardOnboardingTabsParams params;
    params.is_first_run = true;
    params.is_signin_allowed = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[0].url);
    EXPECT_EQ(output[0].type, StartupTab::Type::kNormal);
  }
  {
    // After first run, display welcome page using variant view.
    StandardOnboardingTabsParams params;
    params.is_signin_allowed = true;
    StartupTabs output =
        StartupTabProviderImpl::GetStandardOnboardingTabsForState(params);

    ASSERT_EQ(1U, output.size());
    EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(true), output[0].url);
    EXPECT_EQ(output[0].type, StartupTab::Type::kNormal);
  }
}
#endif

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
    standard_params.is_child_account = true;
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
                             GURL(u"https://www.google.com")};

  StartupTabs output =
      StartupTabProviderImpl::GetInitialPrefsTabsForState(true, input);

  ASSERT_EQ(2U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_EQ(output[0].type, StartupTab::Type::kNormal);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_EQ(output[1].type, StartupTab::Type::kNormal);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(StartupTabProviderTest, GetInitialPrefsTabsForState_WelcomeWithoutFre) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kForYouFre);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kForYouFre));
  std::vector<GURL> input = {GURL(u"https://new_tab_page"),
                             GURL(u"https://www.google.com"),
                             GURL(u"https://welcome_page")};

  StartupTabs output =
      StartupTabProviderImpl::GetInitialPrefsTabsForState(true, input);

  ASSERT_EQ(3U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_EQ(output[0].type, StartupTab::Type::kNormal);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_EQ(output[1].type, StartupTab::Type::kNormal);
  EXPECT_EQ(StartupTabProviderImpl::GetWelcomePageUrl(false), output[2].url);
  EXPECT_EQ(output[2].type, StartupTab::Type::kNormal);
}

TEST(StartupTabProviderTest, GetInitialPrefsTabsForState_WelcomeWithFre) {
  base::test::ScopedFeatureList scoped_feature_list{kForYouFre};
  ASSERT_TRUE(base::FeatureList::IsEnabled(kForYouFre));

  std::vector<GURL> input = {GURL(u"https://new_tab_page"),
                             GURL(u"https://www.google.com"),
                             GURL(u"https://welcome_page")};

  StartupTabs output =
      StartupTabProviderImpl::GetInitialPrefsTabsForState(true, input);

  ASSERT_EQ(2U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
  EXPECT_EQ(output[0].type, StartupTab::Type::kNormal);
  EXPECT_EQ(input[1], output[1].url);
  EXPECT_EQ(output[1].type, StartupTab::Type::kNormal);
}
#endif

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
  EXPECT_EQ(output[0].type, StartupTab::Type::kNormal);
}

TEST(StartupTabProviderTest, GetResetTriggerTabsForState_Negative) {
  StartupTabs output =
      StartupTabProviderImpl::GetResetTriggerTabsForState(false);

  ASSERT_TRUE(output.empty());
}

TEST(StartupTabProviderTest, GetPinnedTabsForState) {
  StartupTabs pinned = {
      StartupTab(GURL("https://www.google.com"), StartupTab::Type::kPinned)};
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
  StartupTabs pinned = {
      StartupTab(GURL("https://www.google.com"), StartupTab::Type::kPinned)};
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
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);
  SessionStartupPref pref_last_and_urls(
      SessionStartupPref::Type::LAST_AND_URLS);
  pref_urls.urls = {GURL(u"https://www.google.com")};
  pref_last_and_urls.urls = {GURL(u"https://www.google.com")};

  StartupTabs output =
      StartupTabProviderImpl::GetPreferencesTabsForState(pref_urls, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
  EXPECT_EQ(StartupTab::Type::kNormal, output[0].type);

  output = StartupTabProviderImpl::GetPreferencesTabsForState(
      pref_last_and_urls, false);

  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("www.google.com", output[0].url.host());
  EXPECT_EQ(StartupTab::Type::kFromLastAndUrlsStartupPref, output[0].type);
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(StartupTabProviderTest, ForYouFreEnabled) {
  base::test::ScopedFeatureList scoped_feature_list{kForYouFre};
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  StartupTabs output = StartupTabProviderImpl().GetOnboardingTabs(&profile);
  ASSERT_EQ(0U, output.size());
}
#endif

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
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    EXPECT_TRUE(output.empty());

    EXPECT_EQ(CommandLineTabsPresent::kNo,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }

  // Simple use. Pass google.com URL.
  {
    base::CommandLine command_line(
        {CMD_ARG(""), CMD_ARG("https://google.com")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("https://google.com"), output[0].url);

    EXPECT_EQ(CommandLineTabsPresent::kYes,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }

  // Two URL case.
  {
    base::CommandLine command_line({CMD_ARG(""), CMD_ARG("https://google.com"),
                                    CMD_ARG("https://gmail.com")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_EQ(2u, output.size());
    EXPECT_EQ(GURL("https://google.com"), output[0].url);
    EXPECT_EQ(GURL("https://gmail.com"), output[1].url);

    EXPECT_EQ(CommandLineTabsPresent::kYes,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }

  // Vista way search query.
  {
    base::CommandLine command_line({CMD_ARG(""), CMD_ARG("? Foo")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(
        GURL("https://www.google.com/search?q=Foo&sourceid=chrome&ie=UTF-8"),
        output[0].url);

    EXPECT_EQ(CommandLineTabsPresent::kUnknown,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Unsafe scheme should be filtered out.
  // Note that chrome:// URLs are allowed on Lacros so that trustworthy calls
  // from Ash will work (URLs from untrustworthy applications are filtered
  // before getting to StartupTabProvider).
  {
    base::CommandLine command_line({CMD_ARG(""), CMD_ARG("chrome://flags")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_TRUE(output.empty());

    EXPECT_EQ(CommandLineTabsPresent::kNo,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Exceptional settings page.
  {
    base::CommandLine command_line(
        {CMD_ARG(""), CMD_ARG("chrome://settings/resetProfileSettings")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("chrome://settings/resetProfileSettings"), output[0].url);

    EXPECT_EQ(CommandLineTabsPresent::kYes,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }

  // chrome://settings/ page handling.
  {
    base::CommandLine command_line(
        {CMD_ARG(""), CMD_ARG("chrome://settings/syncSetup")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);

    auto has_tabs = instance.HasCommandLineTabs(command_line, base::FilePath());
#if BUILDFLAG(IS_CHROMEOS)
    // On Chrome OS (ash-chrome), settings page is allowed to be specified.
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("chrome://settings/syncSetup"), output[0].url);

    EXPECT_EQ(CommandLineTabsPresent::kYes, has_tabs);
#else
    // On other platforms, it is blocked.
    EXPECT_TRUE(output.empty());

    EXPECT_EQ(CommandLineTabsPresent::kNo, has_tabs);
#endif
  }

  // about:blank URL.
  {
    base::CommandLine command_line({CMD_ARG(""), CMD_ARG("about:blank")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("about:blank"), output[0].url);

    EXPECT_EQ(CommandLineTabsPresent::kYes,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }
}

// This test fails on Windows. TODO(crbug.com/1439648): Investigate and
// fix this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_GetCommandLineTabsFileUrl DISABLED_GetCommandLineTabsFileUrl
#else
#define MAYBE_GetCommandLineTabsFileUrl GetCommandLineTabsFileUrl
#endif
TEST(StartupTabProviderTest, MAYBE_GetCommandLineTabsFileUrl) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  // Set up and inject a real instance for the profile.
  TemplateURLServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      &profile,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

  // "file:" path fix up.
  {
    base::CommandLine command_line({CMD_ARG(""), CMD_ARG("file:foo.txt")});
    StartupTabProviderImpl instance;
    StartupTabs output =
        instance.GetCommandLineTabs(command_line, base::FilePath(), &profile);
    ASSERT_EQ(1u, output.size());
    EXPECT_EQ(GURL("file:///foo.txt"), output[0].url);

    EXPECT_EQ(CommandLineTabsPresent::kYes,
              instance.HasCommandLineTabs(command_line, base::FilePath()));
  }
}

#if !BUILDFLAG(IS_ANDROID)

class StartupTabProviderPrivacySandboxTest : public testing::Test {
 protected:
  extensions::ExtensionRegistry* registry() {
    return extensions::ExtensionRegistry::Get(&profile_);
  }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(StartupTabProviderPrivacySandboxTest, GetPrivacySandboxTabsForState) {
  // If no suitable tabs are available, and the profile does not have a custom
  // NTP, a generic new tab URL should be returned.
  auto output = StartupTabProviderImpl::GetPrivacySandboxTabsForState(
      registry(), GURL(chrome::kChromeUINewTabPageURL),
      {{StartupTab(GURL("https://www.unrelated.com"))}});
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
}

TEST_F(StartupTabProviderPrivacySandboxTest,
       GetPrivacySandboxTabsForState_SuitableTabAlready) {
  // If there is already a suitable tab available for the Privacy Sandbox
  // dialog, no additional tab should be added.
  auto output = StartupTabProviderImpl::GetPrivacySandboxTabsForState(
      registry(), GURL(chrome::kChromeUINewTabPageURL),
      {{StartupTab(GURL("chrome://newtab"))}});
  ASSERT_EQ(0U, output.size());
}

TEST_F(StartupTabProviderPrivacySandboxTest,
       GetPrivacySandboxTabsForState_ExtensionControlledNtp) {
  // Create an extension which overrides the NTP url. Even if a new tab is
  // available as part of the startup tabs, an additional about:blank should be
  // returned.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("1")
          .SetManifestKey("chrome_url_overrides",
                          base::Value::Dict().Set("newtab", "custom_tab.html"))
          .Build();
  registry()->AddEnabled(extension);
  auto output = StartupTabProviderImpl::GetPrivacySandboxTabsForState(
      registry(), GURL(chrome::kChromeUINewTabPageURL),
      {StartupTab(GURL(chrome::kChromeUINewTabURL))});
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(url::kAboutBlankURL), output[0].url);
}

TEST_F(StartupTabProviderPrivacySandboxTest,
       GetPrivacySandboxTabsForState_DseControlledNtp) {
  // If the user's DSE is changing the newtab such that it is no longer Chrome
  // controlled, and the user has no other suitable startup tab, about:blank
  // should be used.
  auto output = StartupTabProviderImpl::GetPrivacySandboxTabsForState(
      registry(), GURL("https://wwww.example.com/newtab"),
      {StartupTab(GURL(chrome::kChromeUINewTabURL))});
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL(url::kAboutBlankURL), output[0].url);
}

#endif  // !BUILDFLAG(IS_ANDROID)
