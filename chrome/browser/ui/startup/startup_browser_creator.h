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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_types.h"

class Browser;
class GURL;
class OldLaunchModeRecorder;
class PrefRegistrySimple;

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
enum class StartupProfileMode {
  // Regular startup with a browser window.
  kBrowserWindow,
  // Profile picker window should be shown on startup.
  kProfilePicker,
  // Chrome cannot start because no profiles are available.
  kError
};

// Bundles the startup profile path together with a StartupProfileMode.
// Depending on the `mode` value, `path` is either:
// - regular profile path for kBrowserWindow; if the guest mode is requested,
//   contains default profile path with kBrowserWindow mode
// - guest profile path for kProfilePicker,
// - empty path for kError
// TODO(https://crbug.com/1150326): return a guest profile path for the Guest
// mode and an empty path for kProfilePicker mode
struct StartupProfilePathInfo {
  base::FilePath path;
  StartupProfileMode mode;
};

// Bundles the startup profile together with a StartupProfileMode.
// Depending on the `mode` value, `profile` is either:
// - regular profile for kBrowserWindow; if the Guest mode is requested,
//   contains default profile with kBrowserWindow mode
// - guest profile for kProfilePicker,
// - nullptr for kError
// TODO(https://crbug.com/1150326): return a guest profile for the Guest mode
// and return nullptr for kProfilePicker.
struct StartupProfileInfo {
  raw_ptr<Profile> profile;
  StartupProfileMode mode;
};

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

#if BUILDFLAG(IS_WIN)
  // Configures the instance to include the specified "welcome back" page in a
  // tab before other tabs (e.g., those from session restore). This is used for
  // specific launches via retention experiments for which no URLs are provided
  // on the command line. No "welcome back" page is shown to supervised users.
  void set_welcome_back_page(bool welcome_back_page) {
    welcome_back_page_ = welcome_back_page;
  }
  bool welcome_back_page() const { return welcome_back_page_; }
#endif  // BUILDFLAG(IS_WIN)

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
  // |process_startup| indicates whether this is the first browser.
  // |is_first_run| indicates that this is a new profile.
  // If |launch_mode_recorder| is non null, and a browser is launched, a launch
  // mode histogram will be recorded.
  void LaunchBrowser(
      const base::CommandLine& command_line,
      Profile* profile,
      const base::FilePath& cur_dir,
      chrome::startup::IsProcessStartup process_startup,
      chrome::startup::IsFirstRun is_first_run,
      std::unique_ptr<OldLaunchModeRecorder> launch_mode_recorder);

  // Launches browser for `last_opened_profiles` if it's not empty. Otherwise,
  // launches browser for `profile_info`.
  void LaunchBrowserForLastProfiles(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      chrome::startup::IsProcessStartup process_startup,
      chrome::startup::IsFirstRun is_first_run,
      StartupProfileInfo profile_info,
      const Profiles& last_opened_profiles);

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
  // TODO(crbug.com/642442): Remove this when first_run_tabs gets refactored.
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

  // Callback after a profile has been initialized.
  static void ProcessCommandLineOnProfileInitialized(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      StartupProfileMode mode,
      Profile* profile);

  // Returns true once a profile was activated. Used by the
  // StartupBrowserCreatorTest.LastUsedProfileActivated test.
  static bool ActivatedProfile();

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

#if BUILDFLAG(IS_WIN)
  // The page to be shown in a tab when welcoming a user back to Chrome.
  bool welcome_back_page_ = false;
#endif  // BUILDFLAG(IS_WIN)

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
// startup.
// When the profile picker is shown on startup, this returns the Guest profile
// path. On Mac, the startup profile path is also used to open URLs at startup,
// bypassing the profile picker, because the profile picker does not support it.
// TODO(https://crbug.com/1155158): Remove this parameter once the picker
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
