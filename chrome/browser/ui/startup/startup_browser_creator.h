// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/startup/startup_types.h"

class Browser;
class GURL;
class PrefRegistrySimple;
class Profile;

namespace base {
class CommandLine;
}

namespace web_app {
namespace integration_tests {
class WebAppIntegrationTestDriver;
}
FORWARD_DECLARE_TEST(WebAppEngagementBrowserTest, CommandLineTab);
FORWARD_DECLARE_TEST(WebAppEngagementBrowserTest, CommandLineWindowByUrl);
FORWARD_DECLARE_TEST(WebAppEngagementBrowserTest, CommandLineWindowByAppId);
}  // namespace web_app

// Indicates how Chrome should start up the first profile.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StartupProfileMode {
  // Regular startup with a browser window.
  kBrowserWindow = 0,
  // Profile picker window should be shown on startup.
  kProfilePicker = 1,
  // Chrome cannot start because no profiles are available.
  kError = 2,

  kMaxValue = kError,
};

// Indicates the reason why the StartupProfileMode was chosen.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StartupProfileModeReason {
  kError = 0,

  // Cases when the profile picker is shown:
  kMultipleProfiles = 1,
  kPickerForcedByPolicy = 2,

  // Cases when the profile picker is not shown:
  kGuestModeRequested = 10,
  kGuestSessionLacros = 11,
  kProfileDirSwitch = 12,
  kProfileEmailSwitch = 13,
  kIgnoreProfilePicker = 14,
  kCommandLineTabs = 15,
  kPickerNotSupported = 16,
  kWasRestarted = 17,
  kIncognitoModeRequested = 18,
  kAppRequested = 19,
  kUninstallApp = 20,
  kGcpwSignin = 21,
  kLaunchWithoutWindow = 22,
  // The check for Win notifications seems to be done twice. Record these
  // separately, just in case.
  kNotificationLaunchIdWin1 = 23,
  kNotificationLaunchIdWin2 = 24,
  kPickerDisabledByPolicy = 25,
  kProfilesDisabledLacros = 26,
  kSingleProfile = 27,
  kInactiveProfiles = 28,
  kUserOptedOut = 29,

  kMaxValue = kUserOptedOut,
};

// Bundles the startup profile path together with a `StartupProfileMode`.
// Depending on `StartupProfileModeFromReason(reason)`, `path` is either:
// - regular profile path for `kBrowserWindow`; if the guest mode is requested,
//   may contain either the default profile path or the guest profile path
// - empty profile path for `kProfilePicker` and `kError`
// TODO(crbug.com/40157821): return a guest profile path for the Guest
// mode.
struct StartupProfilePathInfo {
  base::FilePath path;
  StartupProfileModeReason reason = StartupProfileModeReason::kError;
};

// Bundles the startup profile together with a StartupProfileMode.
// Depending on the `mode` value, `profile` is either:
// - regular profile for `kBrowserWindow`; if the Guest mode is requested,
//   may contain either the default profile path or the guest profile path
// - nullptr for `kProfilePicker` and `kError`
// TODO(crbug.com/40157821): return a guest profile for the Guest mode.
struct StartupProfileInfo {
  raw_ptr<Profile, LeakedDanglingUntriaged> profile;
  StartupProfileMode mode;
};

// Whether the profile picker should be shown based on `reason`.
StartupProfileMode StartupProfileModeFromReason(
    StartupProfileModeReason reason);

// class containing helpers for BrowserMain to spin up a new instance and
// initialize the profile.
class StartupBrowserCreator {
 public:
  typedef std::vector<Profile*> Profiles;

  StartupBrowserCreator();
  StartupBrowserCreator(const StartupBrowserCreator&) = delete;
  StartupBrowserCreator& operator=(const StartupBrowserCreator&) = delete;
  ~StartupBrowserCreator();

  // Adds urls to be opened during first run. This overrides the standard
  // tabs shown at first run.
  // Invalid URLs (per `GURL::is_valid()`) are skipped.
  void AddFirstRunTabs(const std::vector<GURL>& urls);

  // This function is equivalent to ProcessCommandLine but should only be
  // called during actual process startup.
  bool Start(const base::CommandLine& cmd_line,
             const base::FilePath& cur_dir,
             StartupProfileInfo profile_info,
             const Profiles& last_opened_profiles);

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line we need to process.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |profile_path_info| contains the directory that contains the profile that
  // the command line arguments will be executed under.
  static void ProcessCommandLineAlreadyRunning(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      const StartupProfilePathInfo& profile_path_info);

  // Opens the set of startup pages from the current session startup prefs.
  static void OpenStartupPages(
      Browser* browser,
      chrome::startup::IsProcessStartup process_startup);

  // Returns true if we're launching a profile synchronously. In that case, the
  // opened window should not cause a session restore.
  static bool InSynchronousProfileLaunch();

  // Launches a browser window associated with |profile|. |command_line| should
  // be the command line passed to this process. |cur_dir| can be empty, which
  // implies that the directory of the executable should be used.
  // `process_startup` indicates whether this is the first browser.
  // `is_first_run` indicates that this is a new profile.
  // `restore_tabbed_browser` should only be flipped false by Ash full restore
  // code path, suppressing restoring a normal browser when there were only PWAs
  // open in previous session. See crbug.com/1463906.
  void LaunchBrowser(
      const base::CommandLine& command_line,
      Profile* profile,
      const base::FilePath& cur_dir,
      chrome::startup::IsProcessStartup process_startup,
      chrome::startup::IsFirstRun is_first_run,
      bool restore_tabbed_browser);

  // Launches browser for `last_opened_profiles` if it's not empty. Otherwise,
  // launches browser for `profile_info`. `restore_tabbed_browser` should
  // only be flipped false by Ash full restore code path, suppressing restoring
  // a normal browser when there were only PWAs open in previous session. See
  // crbug.com/1463906.
  void LaunchBrowserForLastProfiles(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      chrome::startup::IsProcessStartup process_startup,
      chrome::startup::IsFirstRun is_first_run,
      StartupProfileInfo profile_info,
      const Profiles& last_opened_profiles,
      bool restore_tabbed_browser);

  // Returns true during browser process startup if the previous browser was
  // restarted. This only returns true before the first StartupBrowserCreator
  // destructs. WasRestarted() will update prefs::kWasRestarted to false, but
  // caches the value of kWasRestarted until StartupBrowserCreator's
  // dtor is called. After the dtor is called, this function returns the value
  // of the preference which is expected to be false as per above.
  static bool WasRestarted();

  static SessionStartupPref GetSessionStartupPref(
      const base::CommandLine& command_line,
      const Profile* profile);

  // For faking that no profiles have been launched yet.
  static void ClearLaunchedProfilesForTesting();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if Chrome is intended to load a profile and launch without any
  // window.
  static bool ShouldLoadProfileWithoutWindow(
      const base::CommandLine& command_line);

 private:
  friend class StartupBrowserCreatorImpl;
  friend class StartupBrowserCreatorInfobarsTest;
  friend class StartupBrowserCreatorInfobarsWithoutStartupWindowTest;
  // TODO(crbug.com/40482804): Remove this when first_run_tabs gets refactored.
  friend class StartupTabProviderImpl;
  friend class web_app::integration_tests::WebAppIntegrationTestDriver;
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterNormalStart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterRestart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, UpdateWithTwoProfiles);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, LastUsedProfileActivated);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ValidNotificationLaunchId);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           InvalidNotificationLaunchId);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserWithListAppsFeature,
                           ListAppsForAllProfiles);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserWithListAppsFeature,
                           ListAppsForGivenProfile);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTest,
                           OpenAppShortcutNoPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTest,
                           OpenAppShortcutTabPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTest,
                           OpenAppShortcutWindowPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTest,
                           OpenPolicyForcedAppShortcut);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                           OpenAppShortcutNoPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                           OpenAppShortcutTabPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                           OpenAppShortcutWindowPref);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                           OpenPolicyForcedAppShortcut);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, OpenAppUrlShortcut);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           OpenAppUrlIncognitoShortcut);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserWithRealWebAppTest,
                           LastUsedProfilesWithRealWebApp);
  FRIEND_TEST_ALL_PREFIXES(web_app::WebAppEngagementBrowserTest,
                           CommandLineTab);
  FRIEND_TEST_ALL_PREFIXES(web_app::WebAppEngagementBrowserTest,
                           CommandLineWindowByUrl);
  FRIEND_TEST_ALL_PREFIXES(web_app::WebAppEngagementBrowserTest,
                           CommandLineWindowByAppId);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           LastUsedProfilesWithWebApp);

  bool ProcessCmdLineImpl(const base::CommandLine& command_line,
                          const base::FilePath& cur_dir,
                          chrome::startup::IsProcessStartup process_startup,
                          StartupProfileInfo profile_info,
                          const Profiles& last_opened_profiles);

  // Launches the |last_used_profile| with the full command line, and the other
  // |last_opened_profiles| without the URLs to launch.
  void ProcessLastOpenedProfiles(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      chrome::startup::IsProcessStartup process_startup,
      chrome::startup::IsFirstRun is_first_run,
      Profile* last_used_profile,
      const Profiles& last_opened_profiles);

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line being processed.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |profile| is the profile the apps will be launched in.
  static bool ProcessLoadApps(const base::CommandLine& command_line,
                              const base::FilePath& cur_dir,
                              Profile* profile);

  // Callback after a profile has been initialized. `profile` should be nullptr
  // if `mode` is `StartupProfileMode::kProfilePicker`.
  static void ProcessCommandLineWithProfile(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      StartupProfileMode mode,
      Profile* profile);

  // Returns true once a profile was activated. Used by the
  // StartupBrowserCreatorTest.LastUsedProfileActivated test.
  static bool ActivatedProfile();

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  // True if we have already read and reset the preference kWasRestarted. (A
  // member variable instead of a static variable inside WasRestarted because
  // of testing.)
  static bool was_restarted_read_;

  static bool in_synchronous_profile_launch_;

  static bool is_launching_browser_for_last_profiles_;
};

// Returns true if |profile| has exited uncleanly and has not been launched
// after the unclean exit.
bool HasPendingUncleanExit(Profile* profile);

// Adds launched |profile| to ProfileLaunchObserver.
void AddLaunchedProfile(Profile* profile);

// Returns the path that contains the profile that should be loaded on process
// startup. This can do blocking operations to check if the profile exists in
// the case of using --profile-directory and
// --ignore-profile-directory-if-not-exists together.
// When the profile picker is shown on startup, this returns the Guest profile
// path. On Mac, the startup profile path is also used to open URLs at startup,
// bypassing the profile picker, because the profile picker does not support it.
// TODO(crbug.com/40159795): Remove this parameter once the picker
// supports opening URLs.
StartupProfilePathInfo GetStartupProfilePath(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    bool ignore_profile_picker);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
// Returns the profile that should be loaded on process startup. This is either
// the profile returned by GetStartupProfilePath, or the guest profile along
// with StartupProfileMode::kProfilePicker mode if the profile picker should be
// opened. Returns nullptr with kError if neither the regular profile nor the
// profile picker can be opened.
StartupProfileInfo GetStartupProfile(const base::FilePath& cur_dir,
                                     const base::CommandLine& command_line);

// Returns the profile that should be loaded on process startup when
// GetStartupProfile() returns kError. This may return kError if neither any
// profile nor the profile picker can be opened.
StartupProfileInfo GetFallbackStartupProfile();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
