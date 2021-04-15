// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "url/gurl.h"

class Browser;
class LaunchModeRecorder;
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
GURL GetWelcomePageURL();
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

  // Creates the necessary windows for startup. Returns true on success,
  // false on failure. process_startup is true if Chrome is just
  // starting up. If process_startup is false, it indicates Chrome was
  // already running and the user wants to launch another instance.
  bool Launch(Profile* profile,
              const std::vector<GURL>& urls_to_open,
              bool process_startup,
              std::unique_ptr<LaunchModeRecorder> launch_mode_recorder);

  // Convenience for OpenTabsInBrowser that converts |urls| into a set of
  // Tabs.
  Browser* OpenURLsInBrowser(Browser* browser,
                             bool process_startup,
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
                           DetermineStartupTabs_ExtensionCheckupPage);
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorImplTest, ShouldLaunch);

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
                             bool process_startup,
                             const StartupTabs& tabs);

  // Determines the URLs to be shown at startup by way of various policies
  // (welcome, pinned tabs, etc.), determines whether a session restore
  // is necessary, and opens the URLs in a new or restored browser accordingly.
  void DetermineURLsAndLaunch(bool process_startup,
                              const std::vector<GURL>& cmd_line_urls);

  // Returns the tabs to be shown on startup, based on the policy functions in
  // the given StartupTabProvider, the given tabs passed by the command line,
  // and the interactions between those policies.
  StartupTabs DetermineStartupTabs(const StartupTabProvider& provider,
                                   const StartupTabs& cmd_line_tabs,
                                   bool process_startup,
                                   bool is_ephemeral_profile,
                                   bool is_post_crash_launch,
                                   bool has_incompatible_applications,
                                   bool promotional_tabs_enabled,
                                   bool welcome_enabled,
                                   bool serve_extensions_page);

  // Begins an asynchronous session restore if current state allows it (e.g.,
  // this is not process startup) and SessionService indicates that one is
  // necessary. Returns true if restore was initiated, or false if launch
  // should continue (either synchronously, or asynchronously without
  // restoring).
  bool MaybeAsyncRestore(const StartupTabs& tabs,
                         bool process_startup,
                         bool is_post_crash_launch);

  // Returns a browser displaying the contents of |tabs|. Based on |behavior|,
  // this may attempt a session restore or create a new browser. May also allow
  // DOM Storage to begin cleanup once it's clear it is not needed anymore.
  Browser* RestoreOrCreateBrowser(
      const StartupTabs& tabs,
      BrowserOpenBehavior behavior,
      SessionRestore::BehaviorBitmask restore_options,
      bool process_startup,
      bool is_post_crash_launch);

  // Adds any startup infobars to the selected tab of the given browser.
  void AddInfoBarsIfNecessary(
      Browser* browser,
      chrome::startup::IsProcessStartup is_process_startup);

  // Determines how the launch flow should obtain a Browser.
  static BrowserOpenBehavior DetermineBrowserOpenBehavior(
      const SessionStartupPref& pref,
      BrowserOpenBehaviorOptions options);

  // Returns the relevant bitmask options which must be passed when restoring a
  // session.
  static SessionRestore::BehaviorBitmask DetermineSynchronousRestoreOptions(
      bool has_create_browser_default,
      bool has_create_browser_switch,
      bool was_mac_login_or_resume);

  // Returns whether or not a browser window should be created/restored.
  static bool ShouldLaunch(const base::CommandLine& command_line);

  const base::FilePath cur_dir_;
  const base::CommandLine& command_line_;
  Profile* profile_ = nullptr;
  StartupBrowserCreator* browser_creator_;
  bool is_first_run_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_BROWSER_CREATOR_IMPL_H_
