// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_

#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/startup/startup_types.h"
#endif

class Profile;
class StartupBrowserCreator;
struct SessionStartupPref;

namespace base {
class CommandLine;
}  // namespace base

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

// Provides the sets of tabs to be shown at startup for given sets of policy.
// For instance, this class answers the question, "which tabs, if any, need to
// be shown for first run/onboarding?" Provided as a virtual interface to allow
// faking in unit tests.
class StartupTabProvider {
 public:
  // Gathers URLs from a initial preferences file indicating first run logic
  // specific to this distribution. Transforms any such URLs per policy and
  // returns them. Also clears the value of first_run_urls_ in the provided
  // BrowserCreator.
  virtual StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const = 0;

  // Checks for the presence of a trigger indicating the need to offer a Profile
  // Reset on this profile. Returns any tabs which should be shown accordingly.
  virtual StartupTabs GetResetTriggerTabs(Profile* profile) const = 0;

  // Returns the user's pinned tabs, if they should be shown.
  virtual StartupTabs GetPinnedTabs(const base::CommandLine& command_line,
                                    Profile* profile) const = 0;

  // Returns tabs, if any, specified in the user's preferences as the default
  // content for a new window.
  virtual StartupTabs GetPreferencesTabs(const base::CommandLine& command_line,
                                         Profile* profile) const = 0;

  // Returns the New Tab Page, if the user's preferences indicate a
  // configuration where it must be passed explicitly.
  virtual StartupTabs GetNewTabPageTabs(const base::CommandLine& command_line,
                                        Profile* profile) const = 0;

  // Returns the Incompatible Applications settings subpage if any incompatible
  // applications exist.
  virtual StartupTabs GetPostCrashTabs(
      bool has_incompatible_applications) const = 0;

  // Returns the URLs given via the command line arguments to be opened at
  // launching.
  virtual StartupTabs GetCommandLineTabs(const base::CommandLine& command_line,
                                         const base::FilePath& cur_dir,
                                         Profile* profile) const = 0;

  // Indicates whether the command line arguments includes tabs to be opened on
  // startup.
  //
  // A `CommandLineTabsPresent::kUnknown` value means that we are not able to
  // fully parse some arguments, the definitive result can be obtained by
  // calling `GetCommandLineTabs()` and passing a `Profile`.
  virtual CommandLineTabsPresent HasCommandLineTabs(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir) const = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Returns tabs related to the What's New UI (if applicable).
  virtual StartupTabs GetNewFeaturesTabs(bool whats_new_enabled) const = 0;

  // Returns tabs required for the Privacy Sandbox confirmation dialog. If a
  // suitable tab is present in |other_startup_tabs| no tab is returned,
  // otherwise a suitable tab based on |profile| is returned.
  virtual StartupTabs GetPrivacySandboxTabs(
      Profile* profile,
      const StartupTabs& other_startup_tabs) const = 0;
#endif  // !BUILDFLAG(IS_ANDROID)
};

class StartupTabProviderImpl : public StartupTabProvider {
 public:
  StartupTabProviderImpl() = default;
  StartupTabProviderImpl(const StartupTabProviderImpl&) = delete;
  StartupTabProviderImpl& operator=(const StartupTabProviderImpl&) = delete;

  // The static helper methods below implement the policies relevant to the
  // respective Get*Tabs methods, but do not gather or interact with any
  // system state relating to making those policy decisions. Exposed for
  // testing.

  // Processes first run URLs specified in initial preferences file, replacing
  // any "magic word" URL hosts with appropriate URLs.
  static StartupTabs GetInitialPrefsTabsForState(
      bool is_first_run,
      const std::vector<GURL>& first_run_tabs);

  // Determines which tabs should be shown as a result of the presence/absence
  // of a Reset Trigger on this profile.
  static StartupTabs GetResetTriggerTabsForState(bool profile_has_trigger);

  // Determines whether the startup preference requires the contents of
  // |pinned_tabs| to be shown. This is needed to avoid duplicates, as the
  // session restore logic will also resurface pinned tabs on its own.
  static StartupTabs GetPinnedTabsForState(
      const SessionStartupPref& pref,
      const StartupTabs& pinned_tabs,
      bool profile_has_other_tabbed_browser);

  // Determines whether preferences and window state indicate that
  // user-specified tabs should be shown as the default new window content, and
  // returns the specified tabs if so.
  static StartupTabs GetPreferencesTabsForState(
      const SessionStartupPref& pref,
      bool profile_has_other_tabbed_browser);

  // Determines whether startup preferences require the New Tab Page to be
  // explicitly specified. Session Restore does not expect the NTP to be passed.
  static StartupTabs GetNewTabPageTabsForState(const SessionStartupPref& pref);

  // Determines if the Incompatible Applications settings subpage should be
  // shown.
  static StartupTabs GetPostCrashTabsForState(
      bool has_incompatible_applications);

#if !BUILDFLAG(IS_ANDROID)
  // Determines if the what's new page should be shown.
  static StartupTabs GetNewFeaturesTabsForState(bool whats_new_enabled);

  // Determines whether an additional tab to display the Privacy Sandbox
  // confirmation dialog over is required, and if so, the URL of that tab.
  // |ntp_url| must be the final NTP location, and not the generic new tab url.
  static StartupTabs GetPrivacySandboxTabsForState(
      extensions::ExtensionRegistry* extension_registry,
      const GURL& ntp_url,
      const StartupTabs& other_startup_tabs);
#endif

  // In branded Windows builds, adds the URL for the Incompatible Applications
  // subpage of the Chrome settings.
  static void AddIncompatibleApplicationsUrl(StartupTabs* tabs);

  // Gets the URL for the page which offers to reset the user's profile
  // settings.
  static GURL GetTriggeredResetSettingsUrl();

  // StartupTabProvider:
  StartupTabs GetDistributionFirstRunTabs(
      StartupBrowserCreator* browser_creator) const override;
  StartupTabs GetResetTriggerTabs(Profile* profile) const override;
  StartupTabs GetPinnedTabs(const base::CommandLine& command_line,
                            Profile* profile) const override;
  StartupTabs GetPreferencesTabs(const base::CommandLine& command_line,
                                 Profile* profile) const override;
  StartupTabs GetNewTabPageTabs(const base::CommandLine& command_line,
                                Profile* profile) const override;
  StartupTabs GetPostCrashTabs(
      bool has_incompatible_applications) const override;
  StartupTabs GetCommandLineTabs(const base::CommandLine& command_line,
                                 const base::FilePath& cur_dir,
                                 Profile* profile) const override;
  CommandLineTabsPresent HasCommandLineTabs(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir) const override;

#if !BUILDFLAG(IS_ANDROID)
  StartupTabs GetNewFeaturesTabs(bool whats_new_enabled) const override;
  StartupTabs GetPrivacySandboxTabs(
      Profile* profile,
      const StartupTabs& other_startup_tabs) const override;
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  struct ParsedCommandLineTabArg {
    // Indicates whether a tab URL could be parsed from the argument.
    CommandLineTabsPresent tab_parsed = CommandLineTabsPresent::kUnknown;

    // URL for the tab to be created from this argument, will be populated when
    // `tab_parsed` is `CommandLineTabsPresent::kYes`.
    GURL tab_url;
  };

  // Parses a command line argument to extract a `ParsedCommandLineTabArg`
  //
  // Note that it is possible that we detect that a tab can be created, but
  // can't parse the URL. In that case we return an empty one. `maybe_profile`
  // should be provided for better accuracy in the parsing.
  static ParsedCommandLineTabArg ParseTabFromCommandLineArg(
      base::FilePath::StringPieceType arg,
      const base::FilePath& cur_dir,
      Profile* maybe_profile);
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_PROVIDER_H_
