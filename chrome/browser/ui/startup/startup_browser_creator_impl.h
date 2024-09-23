// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "url/gurl.h"

class Browser;
class Profile;
class StartupBrowserCreator;
class StartupTabProvider;
struct SessionStartupPref;

namespace base {
class CommandLine;
class FilePath;
}

namespace internals {
GURL GetTriggeredResetSettingsURL();
}  // namespace internals

// Assists launching the application and appending the initial tabs for a
// browser window.
class StartupBrowserCreatorImpl {
 public:
  // There are two ctors. The first one implies a NULL browser_creator object
  // and thus no access to distribution-specific first-run behaviors. The
  // second one is always called when the browser starts even if it is not
  // the first run.  |is_first_run| indicates that this is a new profile.
  StartupBrowserCreatorImpl(const base::FilePath& cur_dir,
                            const base::CommandLine& command_line,
                            chrome::startup::IsFirstRun is_first_run);
  StartupBrowserCreatorImpl(const base::FilePath& cur_dir,
                            const base::CommandLine& command_line,
                            StartupBrowserCreator* browser_creator,
                            chrome::startup::IsFirstRun is_first_run);
  StartupBrowserCreatorImpl(const StartupBrowserCreatorImpl&) = delete;
  StartupBrowserCreatorImpl& operator=(const StartupBrowserCreatorImpl&) =
      delete;
  ~StartupBrowserCreatorImpl() = default;

  // If command line specifies kiosk mode, or full screen mode, switch
  // to full screen.
  static void MaybeToggleFullscreen(Browser* browser);

  // Creates the necessary windows for startup. |process_startup| indicates
  // whether Chrome is just starting up or already running and the user wants to
  // launch another instance. `restore_tabbed_browser` should only be
  // flipped false by Ash full restore code path, suppressing restoring a normal
  // browser when there were only PWAs open in previous session. See
  // crbug.com/1463906.
  void Launch(Profile* profile,
              chrome::startup::IsProcessStartup process_startup,
              bool restore_tabbed_browser);

  // Convenience for OpenTabsInBrowser that converts |urls| into a set of
  // Tabs.
  Browser* OpenURLsInBrowser(Browser* browser,
                             chrome::startup::IsProcessStartup process_startup,
                             const std::vector<GURL>& urls);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, RestorePinnedTabs);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, AppIdSwitch);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest, DetermineStartupTabs);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_Incognito);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_Crash);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_InitialPrefs);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_CommandLine);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_Crosapi);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_NewTabPage);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_WelcomeBackPage);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineBrowserOpenBehavior_Startup);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineBrowserOpenBehavior_CmdLineTabs);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineBrowserOpenBehavior_PostCrash);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineBrowserOpenBehavior_NotStartup);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_NewFeaturesPage);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest,
                           DetermineStartupTabs_PrivacySandbox);

  enum class LaunchResult {
    kNormally,
    kWithGivenUrls,  // URLs are given from platform, e.g. via command line.
  };

  struct DetermineStartupTabsResult {
    DetermineStartupTabsResult(StartupTabs tabs, LaunchResult launch_result);
    DetermineStartupTabsResult(DetermineStartupTabsResult&&);
    DetermineStartupTabsResult& operator=(DetermineStartupTabsResult&&);
    ~DetermineStartupTabsResult();

    StartupTabs tabs;  // List of startup tabs.
    LaunchResult launch_result;
  };

  enum class WelcomeRunType {
    NONE,                // Do not inject the welcome page for this run.
    FIRST_TAB,           // Inject the welcome page as the first tab.
    FIRST_RUN_LAST_TAB,  // Inject the welcome page as the last first-run tab.
  };

  // Window behaviors possible when opening Chrome.
  enum class BrowserOpenBehavior {
    NEW,                  // Open in a new browser.
    SYNCHRONOUS_RESTORE,  // Attempt a synchronous session restore.
    USE_EXISTING,         // Attempt to add to an existing tabbed browser.
  };

  // Boolean flags used to indicate state for DetermineBrowserOpenBehavior.
  enum BehaviorFlags {
    PROCESS_STARTUP = (1 << 0),
    IS_POST_CRASH_LAUNCH = (1 << 1),
    HAS_NEW_WINDOW_SWITCH = (1 << 2),
    HAS_CMD_LINE_TABS = (1 << 3),
  };

  using BrowserOpenBehaviorOptions = uint32_t;

  // Creates a tab for each of the Tabs in |tabs|. If browser is non-null
  // and a tabbed browser, the tabs are added to it. Otherwise a new tabbed
  // browser is created and the tabs are added to it. The browser the tabs
  // are added to is returned, which is either |browser|, the newly created
  // browser, or nullptr if browser could not be created.
  Browser* OpenTabsInBrowser(Browser* browser,
                             chrome::startup::IsProcessStartup process_startup,
                             const StartupTabs& tabs);

  // Determines the URLs to be shown at startup by way of various policies
  // (welcome, pinned tabs, etc.), determines whether a session restore
  // is necessary, and opens the URLs in a new or restored browser accordingly.
  // `restore_tabbed_browser` should only be flipped false by Ash full
  // restore code path, suppressing restoring a normal browser when there were
  // only PWAs open in previous session. See crbug.com/1463906.
  void DetermineURLsAndLaunch(chrome::startup::IsProcessStartup process_startup,
                              bool restore_tabbed_browser);

  // Returns a tuple of
  // - the tabs to be shown on startup, based on the policy functions in
  //   the given StartupTabProvider, the given tabs passed by the command line,
  //   and the interactions between those policies.
  // - Whether there's launch tabs.
  DetermineStartupTabsResult DetermineStartupTabs(
      const StartupTabProvider& provider,
      chrome::startup::IsProcessStartup process_startup,
      bool is_ephemeral_profile,
      bool is_post_crash_launch,
      bool has_incompatible_applications,
      bool promotional_tabs_enabled,
      bool whats_new_enabled,
      bool privacy_sandbox_confirmation_required);

  // Begins an asynchronous session restore if current state allows it (e.g.,
  // this is not process startup) and SessionService indicates that one is
  // necessary. Returns true if restore was initiated, or false if launch
  // should continue (either synchronously, or asynchronously without
  // restoring).
  bool MaybeAsyncRestore(const StartupTabs& tabs,
                         chrome::startup::IsProcessStartup process_startup,
                         bool is_post_crash_launch);

  // Returns a browser displaying the contents of |tabs|. Based on |behavior|,
  // this may attempt a session restore or create a new browser. May also allow
  // DOM Storage to begin cleanup once it's clear it is not needed anymore.
  Browser* RestoreOrCreateBrowser(
      const StartupTabs& tabs,
      BrowserOpenBehavior behavior,
      SessionRestore::BehaviorBitmask restore_options,
      chrome::startup::IsProcessStartup process_startup,
      bool is_post_crash_launch);

  // Determines how the launch flow should obtain a Browser.
  static BrowserOpenBehavior DetermineBrowserOpenBehavior(
      const SessionStartupPref& pref,
      BrowserOpenBehaviorOptions options);

  // Returns the relevant bitmask options which must be passed when restoring a
  // session. `restore_tabbed_browser` should only be flipped false by Ash
  // full restore code path, suppressing restoring a normal browser when there
  // were only PWAs open in previous session. See crbug.com/1463906.
  static SessionRestore::BehaviorBitmask DetermineSynchronousRestoreOptions(
      bool has_create_browser_default,
      bool has_create_browser_switch,
      bool was_mac_login_or_resume,
      bool restore_tabbed_browser);

  // Returns whether `switches::kKioskMode` is set on the command line of
  // the current process. This is a static method to avoid accidentally reading
  // it from `command_line_`.
  static bool IsKioskModeEnabled();

  const base::FilePath cur_dir_;
  const raw_ref<const base::CommandLine> command_line_;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<StartupBrowserCreator> browser_creator_;
  chrome::startup::IsFirstRun is_first_run_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
