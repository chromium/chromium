// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/common/url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Creator = StartupBrowserCreatorImpl;

namespace {

// Bits for FakeStartupTabProvider options.
constexpr uint32_t kOnboardingTabs = 1 << 0;
constexpr uint32_t kDistributionFirstRunTabs = 1 << 1;
constexpr uint32_t kResetTriggerTabs = 1 << 2;
constexpr uint32_t kPinnedTabs = 1 << 3;
constexpr uint32_t kPreferencesTabs = 1 << 4;
constexpr uint32_t kNewTabPageTabs = 1 << 5;
constexpr uint32_t kPostCrashTabs = 1 << 6;
constexpr uint32_t kCommandLineTabs = 1 << 7;
#if !BUILDFLAG(IS_ANDROID)
constexpr uint32_t kNewFeaturesTabs = 1 << 8;
constexpr uint32_t kPrivacySandboxTabs = 1 << 9;
#endif  // !BUILDFLAG(IS_ANDROID)

class FakeStartupTabProvider : public StartupTabProvider {
 public:
  // For each option passed, the corresponding adder below will add a sentinel
  // tab and return true. For options not passed, the adder will return false.
  explicit FakeStartupTabProvider(uint32_t options) : options_(options) {}

  StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const override {
    StartupTabs tabs;
    if (options_ & kDistributionFirstRunTabs)
      tabs.emplace_back(GURL("https://distribution"));
    return tabs;
  }

  StartupTabs GetResetTriggerTabs(Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kResetTriggerTabs)
      tabs.emplace_back(GURL("https://reset-trigger"));
    return tabs;
  }

  StartupTabs GetPinnedTabs(const base::CommandLine& command_line_,
                            Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kPinnedTabs)
      tabs.emplace_back(GURL("https://pinned"), StartupTab::Type::kPinned);
    return tabs;
  }

  StartupTabs GetPreferencesTabs(const base::CommandLine& command_line_,
                                 Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kPreferencesTabs)
      tabs.emplace_back(GURL("https://prefs"));
    return tabs;
  }

  StartupTabs GetNewTabPageTabs(const base::CommandLine& command_line_,
                                Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kNewTabPageTabs)
      tabs.emplace_back(GURL("https://new-tab"));
    return tabs;
  }

  StartupTabs GetPostCrashTabs(
      bool has_incompatible_applications) const override {
    StartupTabs tabs;
    if (has_incompatible_applications && (options_ & kPostCrashTabs))
      tabs.emplace_back(GURL("https://incompatible-applications"));
    return tabs;
  }

  StartupTabs GetCommandLineTabs(const base::CommandLine& command_line,
                                 const base::FilePath& cur_dir,
                                 Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kCommandLineTabs)
      tabs.emplace_back(GURL("https://cmd-line"));
    return tabs;
  }

  CommandLineTabsPresent HasCommandLineTabs(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir) const override {
    return (options_ & kCommandLineTabs) ? CommandLineTabsPresent::kYes
                                         : CommandLineTabsPresent::kNo;
  }

#if !BUILDFLAG(IS_ANDROID)
  StartupTabs GetNewFeaturesTabs(bool whats_new_enabled) const override {
    StartupTabs tabs;
    if (options_ & kNewFeaturesTabs)
      tabs.emplace_back(GURL("https://whats-new/"));
    return tabs;
  }

  StartupTabs GetPrivacySandboxTabs(
      Profile* profile,
      const StartupTabs& other_startup_tabs) const override {
    StartupTabs tabs;
    if (options_ & kPrivacySandboxTabs)
      tabs.emplace_back(GURL("https://privacy-sandbox"));
    return tabs;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  const uint32_t options_;
};

}  // namespace

// Comparing a `StartupTab` with a string will compare the tab's host with that
// string. This is used to compare lists more easily via
// `testing::ElementsAreArray`.
bool operator==(const StartupTab& actual_tab,
                const std::string& expected_host) {
  return actual_tab.url.host() == expected_host;
}

class StartupBrowserCreatorImplTest : public testing::Test {
 public:
  StartupBrowserCreatorImplTest() = default;
};

// "Standard" case: Tabs specified in onboarding, reset trigger, pinned tabs, or
// preferences shouldn't interfere with each other. Nothing specified on the
// command line. Reset trigger always appears first.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs) {
  using LaunchResult = Creator::LaunchResult;

  FakeStartupTabProvider provider(kOnboardingTabs | kResetTriggerTabs |
                                  kPinnedTabs | kPreferencesTabs |
                                  kNewTabPageTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kYes);

  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/false,
      /*privacy_sandbox_confirmation_required=*/false);
  EXPECT_EQ(LaunchResult::kNormally, output.launch_result);

  std::vector<std::string> expected_tab_hosts;
  expected_tab_hosts.emplace_back("reset-trigger");
  expected_tab_hosts.emplace_back("prefs");
  expected_tab_hosts.emplace_back("pinned");
  EXPECT_THAT(output.tabs, testing::ElementsAreArray(expected_tab_hosts));

  // No extra onboarding content for managed starts.
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/false, /*whats_new_enabled=*/false,
      /*privacy_sandbox_confirmation_required=*/false);
  EXPECT_EQ(LaunchResult::kNormally, output.launch_result);

  ASSERT_EQ(3U, output.tabs.size());
  EXPECT_EQ("reset-trigger", output.tabs[0].url.host());
  EXPECT_EQ("prefs", output.tabs[1].url.host());
  EXPECT_EQ("pinned", output.tabs[2].url.host());

  // No onboarding if not enabled even if promo is allowed.
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/false,
      /*privacy_sandbox_confirmation_required=*/false);
  EXPECT_EQ(LaunchResult::kNormally, output.launch_result);

  ASSERT_EQ(3U, output.tabs.size());
  EXPECT_EQ("reset-trigger", output.tabs[0].url.host());
  EXPECT_EQ("prefs", output.tabs[1].url.host());
  EXPECT_EQ("pinned", output.tabs[2].url.host());
}

// Only the New Tab Page should appear in Incognito mode, skipping all the usual
// tabs.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_Incognito) {
  FakeStartupTabProvider provider(kOnboardingTabs | kDistributionFirstRunTabs |
                                  kResetTriggerTabs | kPinnedTabs |
                                  kPreferencesTabs | kNewTabPageTabs |
                                  kNewFeaturesTabs | kPrivacySandboxTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kYes);

  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/true, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(1U, output.tabs.size());
  // Check for the actual NTP URL, rather than the sentinel returned by the
  // fake, because the Provider is ignored entirely when short-circuited by
  // incognito logic.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output.tabs[0].url);
}

// Also only show the New Tab Page after a crash, except if there is a
// problem application.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_Crash) {
  FakeStartupTabProvider provider(
      kOnboardingTabs | kDistributionFirstRunTabs | kResetTriggerTabs |
      kPinnedTabs | kPreferencesTabs | kNewTabPageTabs | kNewFeaturesTabs |
      kPostCrashTabs | kPrivacySandboxTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kYes);

  // Regular Crash Recovery case:
  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/true,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);

  ASSERT_EQ(1U, output.tabs.size());
  // Check for the actual NTP URL, rather than the sentinel returned by the
  // fake, because the Provider is ignored entirely when short-circuited by
  // the post-crash logic.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output.tabs[0].url);

  // Crash Recovery case with problem applications:
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/true,
      /*has_incompatible_applications=*/true, /*promotional_tabs_enabled=*/true,
      /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);

  ASSERT_EQ(1U, output.tabs.size());
  EXPECT_EQ(GURL("https://incompatible-applications"), output.tabs[0].url);
}

// If initial preferences specify content, this should block all other
// policies. The only exception is command line URLs, tested below.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_InitialPrefs) {
  FakeStartupTabProvider provider(kOnboardingTabs | kDistributionFirstRunTabs |
                                  kResetTriggerTabs | kPinnedTabs |
                                  kPreferencesTabs | kNewTabPageTabs |
                                  kNewFeaturesTabs | kPrivacySandboxTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kYes);

  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(1U, output.tabs.size());
  EXPECT_EQ("distribution", output.tabs[0].url.host());
}

// URLs specified on the command line should always appear, and should block
// all other tabs except the Reset Trigger tab.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_CommandLine) {
  using LaunchResult = Creator::LaunchResult;

  FakeStartupTabProvider provider(
      kOnboardingTabs | kDistributionFirstRunTabs | kResetTriggerTabs |
      kPinnedTabs | kPreferencesTabs | kNewTabPageTabs | kNewFeaturesTabs |
      kCommandLineTabs | kPrivacySandboxTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kYes);

  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kWithGivenUrls, output.launch_result);

  ASSERT_EQ(3U, output.tabs.size());
  EXPECT_EQ("reset-trigger", output.tabs[0].url.host());
  EXPECT_EQ("cmd-line", output.tabs[1].url.host());
  EXPECT_EQ("pinned", output.tabs[2].url.host());

  // Also test that both incognito and crash recovery don't interfere with
  // command line tabs.

  // Incognito
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/true, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kWithGivenUrls, output.launch_result);

  ASSERT_EQ(1U, output.tabs.size());
  EXPECT_EQ("cmd-line", output.tabs[0].url.host());

  // Crash Recovery
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/true,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kWithGivenUrls, output.launch_result);

  ASSERT_EQ(1U, output.tabs.size());
  EXPECT_EQ("cmd-line", output.tabs[0].url.host());

  // Crash Recovery with incompatible applications.
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/true,
      /*has_incompatible_applications=*/true, /*promotional_tabs_enabled=*/true,
      /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kWithGivenUrls, output.launch_result);
  ASSERT_EQ(1U, output.tabs.size());
  EXPECT_EQ("cmd-line", output.tabs[0].url.host());
}

// New Tab Page should appear alongside pinned tabs and the reset trigger, but
// should be superseded by onboarding tabs and by tabs specified in preferences.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_NewTabPage) {
  FakeStartupTabProvider provider_allows_ntp(kPinnedTabs | kResetTriggerTabs |
                                             kNewTabPageTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kYes);

  auto output = impl.DetermineStartupTabs(
      provider_allows_ntp, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false,
      /*is_post_crash_launch=*/false, /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/false,
      /*privacy_sandbox_confirmation_required=*/false);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(3U, output.tabs.size());
  EXPECT_EQ("reset-trigger", output.tabs[0].url.host());
  EXPECT_EQ("new-tab", output.tabs[1].url.host());
  EXPECT_EQ("pinned", output.tabs[2].url.host());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// If the user's preferences satisfy the conditions, show the What's New page
// upon startup.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_NewFeaturesPage) {
  using LaunchResult = Creator::LaunchResult;

  FakeStartupTabProvider provider(kNewTabPageTabs | kNewFeaturesTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kNo);

  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(2U, output.tabs.size());
  EXPECT_EQ("whats-new", output.tabs[0].url.host());
  EXPECT_EQ("new-tab", output.tabs[1].url.host());

  // New features can appear with prefs/pinned.
  FakeStartupTabProvider provider_with_pinned(
      kPinnedTabs | kPreferencesTabs | kNewTabPageTabs | kNewFeaturesTabs);
  output = impl.DetermineStartupTabs(
      provider_with_pinned, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false,
      /*is_post_crash_launch=*/false, /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(3U, output.tabs.size());
  EXPECT_EQ("whats-new", output.tabs[0].url.host());
  EXPECT_EQ("prefs", output.tabs[1].url.host());
  EXPECT_EQ("pinned", output.tabs[2].url.host());

  // Onboarding overrides What's New.
  base::CommandLine first_run_command_line(base::CommandLine::NO_PROGRAM);
  Creator first_run_impl(base::FilePath(), first_run_command_line,
                         chrome::startup::IsFirstRun::kYes);
  FakeStartupTabProvider provider_with_onboarding(
      kOnboardingTabs | kNewTabPageTabs | kNewFeaturesTabs);
  output = first_run_impl.DetermineStartupTabs(
      provider_with_onboarding, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false,
      /*is_post_crash_launch=*/false, /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(LaunchResult::kNormally, output.launch_result);

  std::vector<std::string> expected_tab_hosts;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    expected_tab_hosts.emplace_back("new-tab");
#else
  // Onboarding is not supported and has no effect.
  expected_tab_hosts.emplace_back("whats-new");
  expected_tab_hosts.emplace_back("new-tab");
#endif
  EXPECT_THAT(output.tabs, testing::ElementsAreArray(expected_tab_hosts));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
// If required, a tab for the Privacy Sandbox dialog should be added.
TEST_F(StartupBrowserCreatorImplTest, DetermineStartupTabs_PrivacySandbox) {
  FakeStartupTabProvider provider(kNewTabPageTabs | kPrivacySandboxTabs);
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  Creator impl(base::FilePath(), command_line,
               chrome::startup::IsFirstRun::kNo);

  auto output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(2U, output.tabs.size());
  EXPECT_EQ("new-tab", output.tabs[0].url.host());
  EXPECT_EQ("privacy-sandbox", output.tabs[1].url.host());

  // The tab for the Privacy Sandbox should be added even if promotional tabs
  // are disabled.
  output = impl.DetermineStartupTabs(
      provider, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/false, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(2U, output.tabs.size());
  EXPECT_EQ("new-tab", output.tabs[0].url.host());
  EXPECT_EQ("privacy-sandbox", output.tabs[1].url.host());

  // A Privacy Sandbox tab should be able to appear alongside other
  // prefs/pinned/feature tabs.
  FakeStartupTabProvider provider_pinned_prefs_features(
      kPinnedTabs | kPreferencesTabs | kNewTabPageTabs | kNewFeaturesTabs |
      kPrivacySandboxTabs);
  output = impl.DetermineStartupTabs(
      provider_pinned_prefs_features, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false, /*is_post_crash_launch=*/false,
      /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  ASSERT_EQ(4U, output.tabs.size());
  EXPECT_EQ("whats-new", output.tabs[0].url.host());
  EXPECT_EQ("prefs", output.tabs[1].url.host());
  EXPECT_EQ("privacy-sandbox", output.tabs[2].url.host());
  EXPECT_EQ("pinned", output.tabs[3].url.host());

  // Any onboarding tabs should prevent a Privacy Sandbox tab being added.
  FakeStartupTabProvider provider_with_onboarding(
      kOnboardingTabs | kNewTabPageTabs | kPrivacySandboxTabs);
  base::CommandLine first_run_command_line(base::CommandLine::NO_PROGRAM);
  Creator first_run_impl(base::FilePath(), first_run_command_line,
                         chrome::startup::IsFirstRun::kYes);
  output = first_run_impl.DetermineStartupTabs(
      provider_with_onboarding, chrome::startup::IsProcessStartup::kYes,
      /*is_ephemeral_profile=*/false,
      /*is_post_crash_launch=*/false, /*has_incompatible_applications=*/false,
      /*promotional_tabs_enabled=*/true, /*whats_new_enabled=*/true,
      /*privacy_sandbox_confirmation_required=*/true);
  EXPECT_EQ(Creator::LaunchResult::kNormally, output.launch_result);
  std::vector<std::string> expected_tab_hosts = {"new-tab", "privacy-sandbox"};
  EXPECT_THAT(output.tabs, testing::ElementsAreArray(expected_tab_hosts));
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_Startup) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);
  SessionStartupPref pref_last_and_urls(
      SessionStartupPref::Type::LAST_AND_URLS);

  // The most typical case: startup, not recovering from a crash, no switches.
  // Test each pref with and without command-line tabs.
  Creator::BrowserOpenBehavior output = Creator::DetermineBrowserOpenBehavior(
      pref_default, Creator::PROCESS_STARTUP);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(
      pref_default, Creator::PROCESS_STARTUP | Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_urls,
                                                 Creator::PROCESS_STARTUP);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(
      pref_urls, Creator::PROCESS_STARTUP | Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last,
                                                 Creator::PROCESS_STARTUP);
  EXPECT_EQ(Creator::BrowserOpenBehavior::SYNCHRONOUS_RESTORE, output);

  output = Creator::DetermineBrowserOpenBehavior(
      pref_last, Creator::PROCESS_STARTUP | Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::SYNCHRONOUS_RESTORE, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last_and_urls,
                                                 Creator::PROCESS_STARTUP);
  EXPECT_EQ(Creator::BrowserOpenBehavior::SYNCHRONOUS_RESTORE, output);

  output = Creator::DetermineBrowserOpenBehavior(
      pref_last_and_urls,
      Creator::PROCESS_STARTUP | Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::SYNCHRONOUS_RESTORE, output);
}

TEST_F(StartupBrowserCreatorImplTest,
       DetermineBrowserOpenBehavior_CmdLineTabs) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);
  SessionStartupPref pref_last_and_urls(
      SessionStartupPref::Type::LAST_AND_URLS);

  // Command line tabs after startup should prompt use of existing window,
  // regardless of pref.
  Creator::BrowserOpenBehavior output = Creator::DetermineBrowserOpenBehavior(
      pref_default, Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::USE_EXISTING, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_urls,
                                                 Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::USE_EXISTING, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last,
                                                 Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::USE_EXISTING, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last_and_urls,
                                                 Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::USE_EXISTING, output);

  // Exception: this can be overridden by passing a switch.
  output = Creator::DetermineBrowserOpenBehavior(
      pref_urls, Creator::HAS_NEW_WINDOW_SWITCH | Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);
}

TEST_F(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_PostCrash) {
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);

  // Launching after crash should block session restore.
  Creator::BrowserOpenBehavior output = Creator::DetermineBrowserOpenBehavior(
      pref_last, Creator::PROCESS_STARTUP | Creator::IS_POST_CRASH_LAUNCH);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);
}

TEST_F(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_NotStartup) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);
  SessionStartupPref pref_last_and_urls(
      SessionStartupPref::Type::LAST_AND_URLS);

  // Launch after startup without command-line tabs should always create a new
  // window.
  Creator::BrowserOpenBehavior output =
      Creator::DetermineBrowserOpenBehavior(pref_default, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_urls, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last_and_urls, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);
}
