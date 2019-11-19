// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_types.h"

class Browser;
class GURL;
class PrefRegistrySimple;

namespace base {
class CommandLine;
}

// class containing helpers for BrowserMain to spin up a new instance and
// initialize the profile.
class StartupBrowserCreator {
 public:
  typedef std::vector<Profile*> Profiles;

  StartupBrowserCreator();
  ~StartupBrowserCreator();

  // Adds a url to be opened during first run. This overrides the standard
  // tabs shown at first run.
  void AddFirstRunTab(const GURL& url);

  // Configures the instance to include the specified "welcome back" page in a
  // tab before other tabs (e.g., those from session restore). This is used for
  // specific launches via retention experiments for which no URLs are provided
  // on the command line. No "welcome back" page is shown to supervised users.
  void set_welcome_back_page(bool welcome_back_page) {
    welcome_back_page_ = welcome_back_page;
  }
  bool welcome_back_page() const { return welcome_back_page_; }

  // This function is equivalent to ProcessCommandLine but should only be
  // called during actual process startup.
  bool Start(const base::CommandLine& cmd_line,
             const base::FilePath& cur_dir,
             Profile* last_used_profile,
             const Profiles& last_opened_profiles);

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line we need to process.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |startup_profile_dir| is the directory that contains the profile that the
  // command line arguments will be executed under.
  static void ProcessCommandLineAlreadyRunning(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      const base::FilePath& startup_profile_dir);

  // Opens the set of startup pages from the current session startup prefs.
  static void OpenStartupPages(Browser* browser, bool process_startup);

  // Returns true if we're launching a profile synchronously. In that case, the
  // opened window should not cause a session restore.
  static bool InSynchronousProfileLaunch();

  // Launches a browser window associated with |profile|. |command_line| should
  // be the command line passed to this process. |cur_dir| can be empty, which
  // implies that the directory of the executable should be used.
  // |process_startup| indicates whether this is the first browser.
  // |is_first_run| indicates that this is a new profile.
  bool LaunchBrowser(const base::CommandLine& command_line,
                     Profile* profile,
                     const base::FilePath& cur_dir,
                     chrome::startup::IsProcessStartup is_process_startup,
                     chrome::startup::IsFirstRun is_first_run);

  // When called the first time, reads the value of the preference kWasRestarted
  // and resets it to false. Subsequent calls return the value which was read
  // the first time.
  static bool WasRestarted();

  static SessionStartupPref GetSessionStartupPref(
      const base::CommandLine& command_line,
      Profile* profile);

  // For faking that no profiles have been launched yet.
  static void ClearLaunchedProfilesForTesting();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend class CloudPrintProxyPolicyTest;
  friend class CloudPrintProxyPolicyStartupTest;
  friend class StartupBrowserCreatorImpl;
  // TODO(crbug.com/642442): Remove this when first_run_tabs gets refactored.
  friend class StartupTabProviderImpl;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterNormalStart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ReadingWasRestartedAfterRestart);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, UpdateWithTwoProfiles);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest, LastUsedProfileActivated);

  bool ProcessCmdLineImpl(const base::CommandLine& command_line,
                          const base::FilePath& cur_dir,
                          bool process_startup,
                          Profile* last_used_profile,
                          const Profiles& last_opened_profiles);

  // Launch browser for |last_opened_profiles| if it's not empty. Otherwise,
  // launch browser for |last_used_profile|. Return false if any browser is
  // failed to be launched. Otherwise, return true.
  bool LaunchBrowserForLastProfiles(const base::CommandLine& command_line,
                                    const base::FilePath& cur_dir,
                                    bool process_startup,
                                    Profile* last_used_profile,
                                    const Profiles& last_opened_profiles);

  // Launch the |last_used_profile| with the full command line, and the other
  // |last_opened_profiles| without the URLs to launch. Return false if any
  // browser is failed to be launched. Otherwise, return true.

  bool ProcessLastOpenedProfiles(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      chrome::startup::IsProcessStartup is_process_startup,
      chrome::startup::IsFirstRun is_first_run,
      Profile* last_used_profile,
      const Profiles& last_opened_profiles);

  // Returns the list of URLs to open from the command line.
  static std::vector<GURL> GetURLsFromCommandLine(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile);

  // This function performs command-line handling and is invoked only after
  // start up (for example when we get a start request for another process).
  // |command_line| holds the command line being processed.
  // |cur_dir| is the current working directory that the original process was
  // invoked from.
  // |profile| is the profile the apps will be launched in.
  static bool ProcessLoadApps(const base::CommandLine& command_line,
                              const base::FilePath& cur_dir,
                              Profile* profile);

  // Callback after a profile has been created.
  static void ProcessCommandLineOnProfileCreated(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile,
      Profile::CreateStatus status);

  // Returns true once a profile was activated. Used by the
  // StartupBrowserCreatorTest.LastUsedProfileActivated test.
  static bool ActivatedProfile();

  // Additional tabs to open during first run.
  std::vector<GURL> first_run_tabs_;

  // The page to be shown in a tab when welcoming a user back to Chrome.
  bool welcome_back_page_ = false;

  // True if we have already read and reset the preference kWasRestarted. (A
  // member variable instead of a static variable inside WasRestarted because
  // of testing.)
  static bool was_restarted_read_;

  static bool in_synchronous_profile_launch_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreator);
};

// Returns true if |profile| has exited uncleanly and has not been launched
// after the unclean exit.
bool HasPendingUncleanExit(Profile* profile);

// Returns the path that contains the profile that should be loaded on process
// startup.
base::FilePath GetStartupProfilePath(const base::FilePath& user_data_dir,
                                     const base::CommandLine& command_line);

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
// Returns the profile that should be loaded on process startup. This is either
// the profile returned by GetStartupProfilePath, or the guest profile if the
// above profile is locked. The guest profile denotes that we should open the
// user manager. Returns null if the above profile cannot be opened. In case of
// opening the user manager, returns null if either the guest profile or the
// system profile cannot be opened.
Profile* GetStartupProfile(const base::FilePath& user_data_dir,
                           const base::CommandLine& command_line);

// Returns the profile that should be loaded on process startup when
// GetStartupProfile() returns null. As with GetStartupProfile(), returning the
// guest profile means the caller should open the user manager. This may return
// null if neither any profile nor the user manager can be opened.
Profile* GetFallbackStartupProfile();
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_H_
