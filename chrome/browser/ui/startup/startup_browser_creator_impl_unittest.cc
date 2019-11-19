// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"

#include "base/command_line.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/common/url_constants.h"
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
constexpr uint32_t kWelcomeBackTab = 1 << 6;
constexpr uint32_t kPostCrashTab = 1 << 7;

class FakeStartupTabProvider : public StartupTabProvider {
 public:
  // For each option passed, the corresponding adder below will add a sentinel
  // tab and return true. For options not passed, the adder will return false.
  explicit FakeStartupTabProvider(uint32_t options) : options_(options) {}

  StartupTabs GetOnboardingTabs(Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kOnboardingTabs)
      tabs.emplace_back(GURL("https://onboarding"), false);
    return tabs;
  }

  StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const override {
    StartupTabs tabs;
    if (options_ & kDistributionFirstRunTabs)
      tabs.emplace_back(GURL("https://distribution"), false);
    return tabs;
  }

  StartupTabs GetResetTriggerTabs(Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kResetTriggerTabs)
      tabs.emplace_back(GURL("https://reset-trigger"), false);
    return tabs;
  }

  StartupTabs GetPinnedTabs(const base::CommandLine& command_line_,
                            Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kPinnedTabs)
      tabs.emplace_back(GURL("https://pinned"), true);
    return tabs;
  }

  StartupTabs GetPreferencesTabs(const base::CommandLine& command_line_,
                                 Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kPreferencesTabs)
      tabs.emplace_back(GURL("https://prefs"), false);
    return tabs;
  }

  StartupTabs GetNewTabPageTabs(const base::CommandLine& command_line_,
                                Profile* profile) const override {
    StartupTabs tabs;
    if (options_ & kNewTabPageTabs)
      tabs.emplace_back(GURL("https://new-tab"), false);
    return tabs;
  }

  StartupTabs GetWelcomeBackTabs(Profile* profile,
                                 StartupBrowserCreator* browser_creator,
                                 bool process_startup) const override {
    StartupTabs tabs;
    if (process_startup && (options_ & kWelcomeBackTab))
      tabs.emplace_back(GURL("https://welcome-back"), false);
    return tabs;
  }

  StartupTabs GetPostCrashTabs(
      bool has_incompatible_applications) const override {
    StartupTabs tabs;
    if (has_incompatible_applications && (options_ & kPostCrashTab))
      tabs.emplace_back(GURL("https://incompatible-applications"), false);
    return tabs;
  }

 private:
  const uint32_t options_;
};

}  // namespace

// "Standard" case: Tabs specified in onboarding, reset trigger, pinned tabs, or
// preferences shouldn't interfere with each other. Nothing specified on the
// command line. Reset trigger always appears first.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs) {
  FakeStartupTabProvider provider(kOnboardingTabs | kResetTriggerTabs |
                                  kPinnedTabs | kPreferencesTabs |
                                  kNewTabPageTabs);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  StartupTabs output = impl.DetermineStartupTabs(
      provider, StartupTabs(), true, false, false, false, true, true);
  ASSERT_EQ(4U, output.size());
  EXPECT_EQ("reset-trigger", output[0].url.host());
  EXPECT_EQ("onboarding", output[1].url.host());
  EXPECT_EQ("prefs", output[2].url.host());
  EXPECT_EQ("pinned", output[3].url.host());

  // No extra onboarding content for managed starts.
  output = impl.DetermineStartupTabs(provider, StartupTabs(), true, false,
                                     false, false, false, true);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ("reset-trigger", output[0].url.host());
  EXPECT_EQ("prefs", output[1].url.host());
  EXPECT_EQ("pinned", output[2].url.host());

  // No onboarding if not enabled even if promo is allowed.
  output = impl.DetermineStartupTabs(provider, StartupTabs(), true, false,
                                     false, false, true, false);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ("reset-trigger", output[0].url.host());
  EXPECT_EQ("prefs", output[1].url.host());
  EXPECT_EQ("pinned", output[2].url.host());
}

// Only the New Tab Page should appear in Incognito mode, skipping all the usual
// tabs.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs_Incognito) {
  FakeStartupTabProvider provider(kOnboardingTabs | kDistributionFirstRunTabs |
                                  kResetTriggerTabs | kPinnedTabs |
                                  kPreferencesTabs | kNewTabPageTabs);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  StartupTabs output = impl.DetermineStartupTabs(
      provider, StartupTabs(), true, true, false, false, true, true);
  ASSERT_EQ(1U, output.size());
  // Check for the actual NTP URL, rather than the sentinel returned by the
  // fake, because the Provider is ignored entirely when short-circuited by
  // incognito logic.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);
}

// Also only show the New Tab Page after a crash, except if there is a
// problem application.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs_Crash) {
  FakeStartupTabProvider provider(
      kOnboardingTabs | kDistributionFirstRunTabs | kResetTriggerTabs |
      kPinnedTabs | kPreferencesTabs | kNewTabPageTabs | kPostCrashTab);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  // Regular Crash Recovery case:
  StartupTabs output = impl.DetermineStartupTabs(
      provider, StartupTabs(), true, false, true, false, true, true);
  ASSERT_EQ(1U, output.size());
  // Check for the actual NTP URL, rather than the sentinel returned by the
  // fake, because the Provider is ignored entirely when short-circuited by
  // the post-crash logic.
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), output[0].url);

  // Crash Recovery case with problem applications:
  output = impl.DetermineStartupTabs(provider, StartupTabs(), true, false, true,
                                     true, true, true);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ(GURL("https://incompatible-applications"), output[0].url);
}

// If Master Preferences specifies content, this should block all other
// policies. The only exception is command line URLs, tested below.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs_MasterPrefs) {
  FakeStartupTabProvider provider(kOnboardingTabs | kDistributionFirstRunTabs |
                                  kResetTriggerTabs | kPinnedTabs |
                                  kPreferencesTabs | kNewTabPageTabs);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  StartupTabs output = impl.DetermineStartupTabs(
      provider, StartupTabs(), true, false, false, false, true, true);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("distribution", output[0].url.host());
}

// URLs specified on the command line should always appear, and should block
// all other tabs except the Reset Trigger tab.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs_CommandLine) {
  FakeStartupTabProvider provider(kOnboardingTabs | kDistributionFirstRunTabs |
                                  kResetTriggerTabs | kPinnedTabs |
                                  kPreferencesTabs | kNewTabPageTabs);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  StartupTabs cmd_line_tabs = {StartupTab(GURL("https://cmd-line"), false)};

  StartupTabs output = impl.DetermineStartupTabs(
      provider, cmd_line_tabs, true, false, false, false, true, true);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ("reset-trigger", output[0].url.host());
  EXPECT_EQ("cmd-line", output[1].url.host());
  EXPECT_EQ("pinned", output[2].url.host());

  // Also test that both incognito and crash recovery don't interfere with
  // command line tabs.

  // Incognito
  output = impl.DetermineStartupTabs(provider, cmd_line_tabs, true, true, false,
                                     false, true, true);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("cmd-line", output[0].url.host());

  // Crash Recovery
  output = impl.DetermineStartupTabs(provider, cmd_line_tabs, true, false, true,
                                     false, true, true);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("cmd-line", output[0].url.host());

  // Crash Recovery with incompatible applications.
  output = impl.DetermineStartupTabs(provider, cmd_line_tabs, true, false, true,
                                     true, true, true);
  ASSERT_EQ(1U, output.size());
  EXPECT_EQ("cmd-line", output[0].url.host());
}

// New Tab Page should appear alongside pinned tabs and the reset trigger, but
// should be superseded by onboarding tabs and by tabs specified in preferences.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs_NewTabPage) {
  FakeStartupTabProvider provider_allows_ntp(kPinnedTabs | kResetTriggerTabs |
                                             kNewTabPageTabs);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  StartupTabs output =
      impl.DetermineStartupTabs(provider_allows_ntp, StartupTabs(), true, false,
                                false, false, true, true);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ("reset-trigger", output[0].url.host());
  EXPECT_EQ("new-tab", output[1].url.host());
  EXPECT_EQ("pinned", output[2].url.host());
}

// The welcome back page should appear before any other session restore tabs.
TEST(StartupBrowserCreatorImplTest, DetermineStartupTabs_WelcomeBackPage) {
  FakeStartupTabProvider provider_allows_ntp(kPinnedTabs | kPreferencesTabs |
                                             kWelcomeBackTab);
  Creator impl(base::FilePath(),
               base::CommandLine(base::CommandLine::NO_PROGRAM),
               chrome::startup::IS_FIRST_RUN);

  StartupTabs output =
      impl.DetermineStartupTabs(provider_allows_ntp, StartupTabs(), true, false,
                                false, false, true, true);
  ASSERT_EQ(3U, output.size());
  EXPECT_EQ("welcome-back", output[0].url.host());
  EXPECT_EQ("prefs", output[1].url.host());
  EXPECT_EQ("pinned", output[2].url.host());

  // No welcome back for non-startup opens.
  output = impl.DetermineStartupTabs(provider_allows_ntp, StartupTabs(), false,
                                     false, false, false, true, true);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ("prefs", output[0].url.host());
  EXPECT_EQ("pinned", output[1].url.host());

  // No welcome back for managed starts even if first run.
  output = impl.DetermineStartupTabs(provider_allows_ntp, StartupTabs(), true,
                                     false, false, false, false, true);
  ASSERT_EQ(2U, output.size());
  EXPECT_EQ("prefs", output[0].url.host());
  EXPECT_EQ("pinned", output[1].url.host());
}

TEST(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_Startup) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

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
}

TEST(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_CmdLineTabs) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

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

  // Exception: this can be overridden by passing a switch.
  output = Creator::DetermineBrowserOpenBehavior(
      pref_urls, Creator::HAS_NEW_WINDOW_SWITCH | Creator::HAS_CMD_LINE_TABS);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);
}

TEST(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_PostCrash) {
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);

  // Launching after crash should block session restore.
  Creator::BrowserOpenBehavior output = Creator::DetermineBrowserOpenBehavior(
      pref_last, Creator::PROCESS_STARTUP | Creator::IS_POST_CRASH_LAUNCH);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);
}

TEST(StartupBrowserCreatorImplTest, DetermineBrowserOpenBehavior_NotStartup) {
  SessionStartupPref pref_default(SessionStartupPref::Type::DEFAULT);
  SessionStartupPref pref_last(SessionStartupPref::Type::LAST);
  SessionStartupPref pref_urls(SessionStartupPref::Type::URLS);

  // Launch after startup without command-line tabs should always create a new
  // window.
  Creator::BrowserOpenBehavior output =
      Creator::DetermineBrowserOpenBehavior(pref_default, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_last, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);

  output = Creator::DetermineBrowserOpenBehavior(pref_urls, 0);
  EXPECT_EQ(Creator::BrowserOpenBehavior::NEW, output);
}
