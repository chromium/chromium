// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(OS_CHROMEOS)
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

using testing::Return;
#endif  // !defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using testing::_;
using extensions::Extension;

namespace {

#if !defined(OS_CHROMEOS)
// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile()));

  // Find the new browser.
  Browser* other_browser = NULL;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser)
      other_browser = b;
  }
  return other_browser;
}

bool IsWindows10OrNewer() {
#if defined(OS_WIN)
  return base::win::GetVersion() >= base::win::Version::WIN10;
#else
  return false;
#endif
}


void DisableWelcomePages(const std::vector<Profile*>& profiles) {
  for (Profile* profile : profiles)
    profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);
}

Browser* OpenNewBrowser(Profile* profile) {
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl creator(base::FilePath(), dummy,
                                    chrome::startup::IS_FIRST_RUN);
  creator.Launch(profile, std::vector<GURL>(), false);
  return chrome::FindBrowserWithProfile(profile);
}

Browser* CloseBrowserAndOpenNew(Browser* browser, Profile* profile) {
  browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser);
  return OpenNewBrowser(profile);
}

#endif  // !defined(OS_CHROMEOS)

typedef base::Optional<policy::PolicyLevel> PolicyVariant;

}  // namespace

class StartupBrowserCreatorTest : public extensions::ExtensionBrowserTest {
 protected:
  StartupBrowserCreatorTest() {}

  bool SetUpUserDataDirectory() override {
    return extensions::ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kHomePage, url::kAboutBlankURL);
#if defined(OS_CHROMEOS)
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
#endif
  }

  // Helper functions return void so that we can ASSERT*().
  // Use ASSERT_NO_FATAL_FAILURE around calls to these functions to stop the
  // test if an assert fails.
  void LoadApp(const std::string& app_name,
               const Extension** out_app_extension) {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_name.c_str())));

    *out_app_extension = extension_registry()->GetExtensionById(
        last_loaded_extension_id(), extensions::ExtensionRegistry::ENABLED);
    ASSERT_TRUE(*out_app_extension);

    // Code that opens a new browser assumes we start with exactly one.
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  }

  void SetAppLaunchPref(const std::string& app_id,
                        extensions::LaunchType launch_type) {
    extensions::SetLaunchType(browser()->profile(), app_id, launch_type);
  }

  Browser* FindOneOtherBrowserForProfile(Profile* profile,
                                         Browser* not_this_browser) {
    for (auto* browser : *BrowserList::GetInstance()) {
      if (browser != not_this_browser && browser->profile() == profile)
        return browser;
    }
    return NULL;
  }

  // A helper function that checks the session restore UI (infobar) is shown
  // when Chrome starts up after crash.
  void EnsureRestoreUIWasShown(content::WebContents* web_contents) {
#if defined(OS_MACOSX)
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    EXPECT_EQ(1U, infobar_service->infobar_count());
#endif  // defined(OS_MACOSX)
  }
};

class OpenURLsPopupObserver : public BrowserListObserver {
 public:
  OpenURLsPopupObserver() : added_browser_(NULL) { }

  void OnBrowserAdded(Browser* browser) override { added_browser_ = browser; }

  void OnBrowserRemoved(Browser* browser) override {}

  Browser* added_browser_;
};

// Test that when there is a popup as the active browser any requests to
// StartupBrowserCreatorImpl::OpenURLsInBrowser don't crash because there's no
// explicit profile given.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenURLsPopup) {
  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost"));

  // Note that in our testing we do not ever query the BrowserList for the "last
  // active" browser. That's because the browsers are set as "active" by
  // platform UI toolkit messages, and those messages are not sent during unit
  // testing sessions.

  OpenURLsPopupObserver observer;
  BrowserList::AddObserver(&observer);

  Browser* popup = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  ASSERT_TRUE(popup->is_type_popup());
  ASSERT_EQ(popup, observer.added_browser_);

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a NULL or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls);
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// We don't do non-process-startup browser launches on ChromeOS.
// Session restore for process-startup browser launches is tested
// in session_restore_uitest.
#if !defined(OS_CHROMEOS)
// Verify that startup URLs are honored when the process already exists but has
// no tabbed browser windows (eg. as if the process is running only due to a
// background application.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindowWithNoTabbedBrowsers) {
  // Use a couple same-site HTTP URLs.
  ASSERT_TRUE(embedded_test_server()->Start());
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  urls.push_back(embedded_test_server()->GetURL("/title2.html"));

  Profile* profile = browser()->profile();

  DisableWelcomePages({profile});

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile, pref);

  // Keep the browser process running while browsers are closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  Browser* new_browser = OpenNewBrowser(profile);
  ASSERT_TRUE(new_browser);

  std::vector<GURL> expected_urls(urls);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(static_cast<int>(expected_urls.size()), tab_strip->count());
  for (size_t i = 0; i < expected_urls.size(); i++)
    EXPECT_EQ(expected_urls[i], tab_strip->GetWebContentsAt(i)->GetURL());

  // The two test_server tabs, despite having the same site, should be in
  // different SiteInstances.
  EXPECT_NE(
      tab_strip->GetWebContentsAt(tab_strip->count() - 2)->GetSiteInstance(),
      tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetSiteInstance());
}

// Verify that startup URLs aren't used when the process already exists
// and has other tabbed browser windows.  This is the common case of starting a
// new browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindow) {
  // Use a couple arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  DisableWelcomePages({browser()->profile()});

  Browser* new_browser = OpenNewBrowser(browser()->profile());
  ASSERT_TRUE(new_browser);

  // The new browser should have exactly one tab (not the startup URLs).
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tab_strip->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppUrlShortcut) {
  // Add --app=<url> to the command line. Tests launching legacy apps which may
  // have been created by "Add to Desktop" in old versions of Chrome.
  // TODO(mgiuca): Delete this feature (https://crbug.com/751029). We are
  // keeping it for now to avoid disrupting existing workflows.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  command_line.AppendSwitchASCII(switches::kApp, url.spec());

  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // The new window should be an app window.
  EXPECT_TRUE(new_browser->is_type_app());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  // At this stage, the web contents' URL should be the one passed in to --app
  // (but it will not yet be committed into the navigation controller).
  EXPECT_EQ("title2.html", web_contents->GetVisibleURL().ExtractFileName());

  // Wait until the navigation is complete. Then the URL will be committed to
  // the navigation controller.
  content::TestNavigationObserver observer(web_contents, 1);
  observer.Wait();
  EXPECT_EQ("title2.html",
            web_contents->GetLastCommittedURL().ExtractFileName());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // No pref was set, so the app should have opened in a tab in the existing
  // window.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab_strip->GetWebContentsAt(1));

  // It should be a standard tabbed window, not an app window.
  EXPECT_FALSE(browser()->is_type_app());
  EXPECT_TRUE(browser()->is_type_normal());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutWindowPref) {
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // Pref was set to open in a window, so the app should have opened in a
  // window.  The launch should have created a new browser. Find the new
  // browser.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Expect an app window.
  EXPECT_TRUE(new_browser->is_type_app());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutTabPref) {
  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a tab.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_REGULAR);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // When an app shortcut is open and the pref indicates a tab should open, the
  // tab is open in the existing browser window.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(tab_strip->GetActiveWebContents(), tab_strip->GetWebContentsAt(1));

  // The browser's app_name should not include the app's ID: it is in a normal
  // tabbed browser.
  EXPECT_EQ(browser()->app_name_.find(extension_app->id()), std::string::npos)
      << browser()->app_name_;
}

#endif  // !defined(OS_CHROMEOS)

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ValidNotificationLaunchId) {
  // Simulate a launch from the notification_helper process which appends the
  // kNotificationLaunchId switch to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");
  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // The launch delegates to the notification system and doesn't open any new
  // browser window.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, InvalidNotificationLaunchId) {
  // Simulate a launch with invalid launch id, which will fail.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kNotificationLaunchId, L"");
  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_FALSE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // No new browser window is open.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       NotificationLaunchIdDisablesLastOpenProfiles) {
  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  Profile* other_profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    other_profile = profile_manager->GetProfile(dest_path);
  }
  ASSERT_TRUE(other_profile);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Simulate a launch.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");

  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(other_profile);

  StartupBrowserCreator browser_creator;
  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // |browser()| is still around at this point, even though we've closed its
  // window. Thus the browser count for default_profile is 1.
  ASSERT_EQ(1u, chrome::GetBrowserCount(default_profile));

  // When the kNotificationLaunchId switch is present, any last opened profile
  // is ignored. Thus there is no browser for other_profile.
  ASSERT_EQ(0u, chrome::GetBrowserCount(other_profile));
}

#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterRestart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterNormalStart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, false);
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsForTwoProfiles) {
  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  Profile* other_profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    other_profile = profile_manager->GetProfile(dest_path);
  }
  ASSERT_TRUE(other_profile);

  // Use a couple arbitrary URLs.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(default_profile, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(other_profile, pref2);

  DisableWelcomePages({default_profile, other_profile});

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(other_profile);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // urls1 were opened in a browser for default_profile, and urls2 were opened
  // in a browser for other_profile.
  Browser* new_browser = NULL;
  // |browser()| is still around at this point, even though we've closed its
  // window. Thus the browser count for default_profile is 2.
  ASSERT_EQ(2u, chrome::GetBrowserCount(default_profile));
  new_browser = FindOneOtherBrowserForProfile(default_profile, browser());
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the desired URL for the profile.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(0)->GetURL());

  ASSERT_EQ(1u, chrome::GetBrowserCount(other_profile));
  new_browser = FindOneOtherBrowserForProfile(other_profile, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls2[0], tab_strip->GetWebContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, PRE_UpdateWithTwoProfiles) {
  // Simulate a browser restart by creating the profiles in the PRE_ part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }
  DisableWelcomePages({profile1, profile2});

  // Open some urls with the browsers, and close them.
  Browser* browser1 =
      new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile1, true));
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1,
                               embedded_test_server()->GetURL("/empty.html"));
  CloseBrowserSynchronously(browser1);

  Browser* browser2 =
      new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile2, true));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2,
                               embedded_test_server()->GetURL("/form.html"));
  CloseBrowserSynchronously(browser2);

  // Set different startup preferences for the 2 profiles.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(profile2, pref2);

  profile1->GetPrefs()->CommitPendingWrite();
  profile2->GetPrefs()->CommitPendingWrite();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, UpdateWithTwoProfiles) {
  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
    ASSERT_TRUE(profile1);

    profile2 = profile_manager->GetProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
    ASSERT_TRUE(profile2);
  }

  // Simulate a launch after a browser update.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile1,
                        last_opened_profiles);

  while (SessionRestore::IsRestoring(profile1) ||
         SessionRestore::IsRestoring(profile2))
    base::RunLoop().RunUntilIdle();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = NULL;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  new_browser = FindOneOtherBrowserForProfile(profile1, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
  new_browser = FindOneOtherBrowserForProfile(profile2, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html", tab_strip->GetWebContentsAt(0)->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ProfilesWithoutPagesNotLaunched) {
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 4 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));
  base::FilePath dest_path4 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 4"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile_home1 = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile_home1);
  Profile* profile_home2 = profile_manager->GetProfile(dest_path2);
  ASSERT_TRUE(profile_home2);
  Profile* profile_last = profile_manager->GetProfile(dest_path3);
  ASSERT_TRUE(profile_last);
  Profile* profile_urls = profile_manager->GetProfile(dest_path4);
  ASSERT_TRUE(profile_urls);

  DisableWelcomePages(
      {profile_home1, profile_home2, profile_last, profile_urls});

  // Set the profiles to open urls, open last visited pages or display the home
  // page.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(profile_home1, pref_home);
  SessionStartupPref::SetStartupPref(profile_home2, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(profile_urls, pref_urls);

  // Open a page with profile_last.
  Browser* browser_last = new Browser(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile_last, true));
  chrome::NewTab(browser_last);
  ui_test_utils::NavigateToURL(browser_last,
                               embedded_test_server()->GetURL("/empty.html"));
  CloseBrowserAsynchronously(browser_last);

  // Close the main browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home1);
  last_opened_profiles.push_back(profile_home2);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home1,
                        last_opened_profiles);

  while (SessionRestore::IsRestoring(default_profile) ||
         SessionRestore::IsRestoring(profile_home1) ||
         SessionRestore::IsRestoring(profile_home2) ||
         SessionRestore::IsRestoring(profile_last) ||
         SessionRestore::IsRestoring(profile_urls))
    base::RunLoop().RunUntilIdle();

  Browser* new_browser = NULL;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the NTP (and the welcome page on
  // relevant platforms).
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home1));
  new_browser = FindOneOtherBrowserForProfile(profile_home1, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));

  // profile_urls opened the urls.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetURL());

  // profile_last opened the last open pages.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last));
  new_browser = FindOneOtherBrowserForProfile(profile_last, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u, chrome::GetBrowserCount(profile_home2));
}

// Flaky. See https://crbug.com/819976.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       DISABLED_ProfilesLaunchedAfterCrash) {
  // After an unclean exit, all profiles will be launched. However, they won't
  // open any pages automatically.

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 3 profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));

  Profile* profile_home = nullptr;
  Profile* profile_last = nullptr;
  Profile* profile_urls = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile_home = profile_manager->GetProfile(dest_path1);
    ASSERT_TRUE(profile_home);
    profile_last = profile_manager->GetProfile(dest_path2);
    ASSERT_TRUE(profile_last);
    profile_urls = profile_manager->GetProfile(dest_path3);
    ASSERT_TRUE(profile_urls);
  }

  // Set the profiles to open the home page, last visited pages or URLs.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(profile_home, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(profile_urls, pref_urls);

  // Simulate a launch after an unclear exit.
  CloseBrowserAsynchronously(browser());
  static_cast<ProfileImpl*>(profile_home)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;
  static_cast<ProfileImpl*>(profile_last)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;
  static_cast<ProfileImpl*>(profile_urls)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;

#if !defined(OS_MACOSX) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use HistogramTester to make sure a bubble is shown when it's not on
  // platform Mac OS X and it's not official Chrome build.
  //
  // On Mac OS X, an infobar is shown to restore the previous session, which
  // is tested by function EnsureRestoreUIWasShown.
  //
  // Under a Google Chrome build, it is not tested because a task is posted to
  // the file thread before the bubble is shown. It is difficult to make sure
  // that the histogram check runs after all threads have finished their tasks.
  base::HistogramTester histogram_tester;
#endif  // !defined(OS_MACOSX) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitchASCII(switches::kTestType, "browser");
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home,
                        last_opened_profiles);

  // No profiles are getting restored, since they all display the crash info
  // bar.
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_home));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_last));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_urls));

  // The profile which normally opens the home page displays the new tab page.
  // The welcome page is also shown for relevant platforms.
  Browser* new_browser = NULL;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home));
  new_browser = FindOneOtherBrowserForProfile(profile_home, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));

  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last));
  new_browser = FindOneOtherBrowserForProfile(profile_last, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

#if !defined(OS_MACOSX) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Each profile should have one session restore bubble shown, so we should
  // observe count 3 in bucket 0 (which represents bubble shown).
  histogram_tester.ExpectBucketCount("SessionCrashed.Bubble", 0, 3);
#endif  // !defined(OS_MACOSX) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       LaunchMultipleLockedProfiles) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  Profile* profile1 = nullptr;
  Profile* profile2 = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = profile_manager->GetProfile(
        user_data_dir.Append(FILE_PATH_LITERAL("New Profile 1")));
    profile2 = profile_manager->GetProfile(
        user_data_dir.Append(FILE_PATH_LITERAL("New Profile 2")));
  }
  ASSERT_TRUE(profile1);
  ASSERT_TRUE(profile2);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile2, pref);
  ProfileAttributesEntry* entry = nullptr;
  ASSERT_TRUE(profile_manager->GetProfileAttributesStorage()
                  .GetProfileAttributesWithPath(profile1->GetPath(), &entry));
  entry->SetIsSigninRequired(true);

  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        profile1, last_opened_profiles);

  ASSERT_EQ(0u, chrome::GetBrowserCount(profile1));
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
}

#endif  // !defined(OS_CHROMEOS)

// These tests are not applicable to Chrome OS as neither master_preferences nor
// the onboarding promos exist there.
#if !defined(OS_CHROMEOS)

class StartupBrowserCreatorFirstRunTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorFirstRunTest() {
    scoped_feature_list_.InitWithFeatures({welcome::kForceEnabled}, {});
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

  policy::MockConfigurationPolicyProvider provider_;
  policy::PolicyMap policy_map_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorFirstRunTest);
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if defined(OS_LINUX) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(policy::key::kMetricsReportingEnabled,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  std::make_unique<base::Value>(false), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
#endif  // defined(OS_LINUX) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest, AddFirstRunTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title2.html"));

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();

  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
  EXPECT_EQ("title2.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
    DISABLED_RestoreOnStartupURLsPolicySpecified
#else
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
    RestoreOnStartupURLsPolicySpecified
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_RestoreOnStartupURLsPolicySpecified) {
  if (IsWindows10OrNewer())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;

  DisableWelcomePages({browser()->profile()});

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(
      policy::key::kRestoreOnStartup, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::WrapUnique(new base::Value(SessionStartupPref::kPrefValueURLs)),
      nullptr);
  base::ListValue startup_urls;
  startup_urls.AppendString(
      embedded_test_server()->GetURL("/title1.html").spec());
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, startup_urls.CreateDeepCopy(),
                  nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the URL specified through policy is shown and no sync promo has
  // been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsWithRestoreSession \
    DISABLED_FirstRunTabsWithRestoreSession
#else
#define MAYBE_FirstRunTabsWithRestoreSession FirstRunTabsWithRestoreSession
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsWithRestoreSession) {
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "/title1.html"
  //  ],
  //  "session" : {
  //    "restore_on_startup" : 1
  //   },
  //   "sync_promo" : {
  //     "user_skipped" : true
  //   }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kRestoreOnStartup, 1);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tab is shown and no other pages are present.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest, WelcomePages) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  std::unique_ptr<Profile> profile1;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")), nullptr,
        Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }
  Profile* profile1_ptr = profile1.get();
  ASSERT_TRUE(profile1_ptr);
  profile_manager->RegisterTestingProfile(std::move(profile1), true, false);

  Browser* browser = OpenNewBrowser(profile1_ptr);
  ASSERT_TRUE(browser);

  TabStripModel* tab_strip = browser->tab_strip_model();

  // Ensure that the standard Welcome page appears on second run on Win 10, and
  // on first run on all other platforms.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::kChromeUIWelcomeURL,
            tab_strip->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());

  browser = CloseBrowserAndOpenNew(browser, profile1_ptr);
  ASSERT_TRUE(browser);
  tab_strip = browser->tab_strip_model();

  // Ensure that the new tab page appears on subsequent runs.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tab_strip->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       WelcomePagesWithPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(policy::key::kRestoreOnStartup,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(4),
                  nullptr);
  auto url_list = std::make_unique<base::Value>(base::Value::Type::LIST);
  url_list->Append(
      base::Value(embedded_test_server()->GetURL("/title1.html").spec()));
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                  policy::POLICY_SOURCE_CLOUD, std::move(url_list), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  std::unique_ptr<Profile> profile1;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile1 = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")), nullptr,
        Profile::CreateMode::CREATE_MODE_SYNCHRONOUS);
  }
  Profile* profile1_ptr = profile1.get();
  ASSERT_TRUE(profile1_ptr);
  profile_manager->RegisterTestingProfile(std::move(profile1), true, false);

  Browser* browser = OpenNewBrowser(profile1_ptr);
  ASSERT_TRUE(browser);

  TabStripModel* tab_strip = browser->tab_strip_model();

  // Windows 10 has its own Welcome page but even that should not show up when
  // the policy is set.
  if (IsWindows10OrNewer()) {
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ("title1.html",
              tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());

    browser = CloseBrowserAndOpenNew(browser, profile1_ptr);
    ASSERT_TRUE(browser);
    tab_strip = browser->tab_strip_model();
  }

  // Ensure that the policy page page appears on second run on Win 10, and
  // on first run on all other platforms.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());

  browser = CloseBrowserAndOpenNew(browser, profile1_ptr);
  ASSERT_TRUE(browser);
  tab_strip = browser->tab_strip_model();

  // Ensure that the policy page page appears on subsequent runs.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

#endif  // !defined(OS_CHROMEOS)

class StartupBrowserCreatorWelcomeBackTest : public InProcessBrowserTest {
 protected:
  StartupBrowserCreatorWelcomeBackTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    profile_ = browser()->profile();

    // Keep the browser process running when all browsers are closed.
    scoped_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);

    // Close the browser opened by InProcessBrowserTest.
    CloseBrowserSynchronously(browser());
    ASSERT_EQ(0U, BrowserList::GetInstance()->size());
  }

  void StartBrowser(PolicyVariant variant) {
    browser_creator_.set_welcome_back_page(true);

    if (variant) {
      policy::PolicyMap values;
      values.Set(policy::key::kRestoreOnStartup, variant.value(),
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(4), nullptr);
      auto url_list = std::make_unique<base::Value>(base::Value::Type::LIST);
      url_list->Append(base::Value("http://managed.site.com/"));
      values.Set(policy::key::kRestoreOnStartupURLs, variant.value(),
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::move(url_list), nullptr);
      provider_.UpdateChromePolicy(values);
    }

    ASSERT_TRUE(browser_creator_.Start(
        base::CommandLine(base::CommandLine::NO_PROGRAM), base::FilePath(),
        profile_,
        g_browser_process->profile_manager()->GetLastOpenedProfiles()));
    ASSERT_EQ(1U, BrowserList::GetInstance()->size());
  }

  void ExpectUrlInBrowserAtPosition(const GURL& url, int tab_index) {
    Browser* browser = BrowserList::GetInstance()->get(0);
    TabStripModel* tab_strip = browser->tab_strip_model();
    EXPECT_EQ(url, tab_strip->GetWebContentsAt(tab_index)->GetURL());
  }

  void TearDownOnMainThread() override { scoped_keep_alive_.reset(); }

 private:
  Profile* profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive_;
  StartupBrowserCreator browser_creator_;
  policy::MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWelcomeBackTest,
                       WelcomeBackStandardNoPolicy) {
  ASSERT_NO_FATAL_FAILURE(StartBrowser(PolicyVariant()));
  ExpectUrlInBrowserAtPosition(StartupTabProviderImpl::GetWelcomePageUrl(false),
                               0);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWelcomeBackTest,
                       WelcomeBackStandardMandatoryPolicy) {
  ASSERT_NO_FATAL_FAILURE(
      StartBrowser(PolicyVariant(policy::POLICY_LEVEL_MANDATORY)));
  ExpectUrlInBrowserAtPosition(GURL("http://managed.site.com/"), 0);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWelcomeBackTest,
                       WelcomeBackStandardRecommendedPolicy) {
  ASSERT_NO_FATAL_FAILURE(
      StartBrowser(PolicyVariant(policy::POLICY_LEVEL_RECOMMENDED)));
  ExpectUrlInBrowserAtPosition(GURL("http://managed.site.com/"), 0);
}

// Validates that prefs::kWasRestarted is automatically reset after next browser
// start.
class StartupBrowserCreatorWasRestartedFlag : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorWasRestartedFlag() = default;
  ~StartupBrowserCreatorWasRestartedFlag() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kUserDataDir, temp_dir_.GetPath());
    std::string json;
    base::DictionaryValue local_state;
    local_state.SetBoolean(prefs::kWasRestarted, true);
    base::JSONWriter::Write(local_state, &json);
    ASSERT_EQ(json.length(),
              static_cast<size_t>(base::WriteFile(
                  temp_dir_.GetPath().Append(chrome::kLocalStateFilename),
                  json.c_str(), json.length())));
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWasRestartedFlag, Test) {
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
}

// The kCommandLineFlagSecurityWarningsEnabled policy doesn't exist on ChromeOS.
#if !defined(OS_CHROMEOS)
enum class CommandLineFlagSecurityWarningsPolicy {
  kNoPolicy,
  kEnabled,
  kDisabled,
};

// Verifies that infobars are displayed (or not) depending on enterprise policy.
class StartupBrowserCreatorInfobarsTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          CommandLineFlagSecurityWarningsPolicy> {
 public:
  StartupBrowserCreatorInfobarsTest() : policy_(GetParam()) {}

 protected:
  InfoBarService* LaunchBrowserAndGetCreatedInfobarService(
      const base::CommandLine& command_line) {
    Profile* profile = browser()->profile();
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IS_NOT_FIRST_RUN);
    EXPECT_TRUE(launch.Launch(profile, std::vector<GURL>(), true));

    // This should have created a new browser window.
    Browser* new_browser = FindOneOtherBrowser(browser());
    EXPECT_TRUE(new_browser);

    return InfoBarService::FromWebContents(
        new_browser->tab_strip_model()->GetWebContentsAt(0));
  }

  const CommandLineFlagSecurityWarningsPolicy policy_;

 private:
  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    if (policy_ != CommandLineFlagSecurityWarningsPolicy::kNoPolicy) {
      bool is_enabled =
          policy_ == CommandLineFlagSecurityWarningsPolicy::kEnabled;
      policy::PolicyMap policies;
      policies.Set(policy::key::kCommandLineFlagSecurityWarningsEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_PLATFORM,
                   std::make_unique<base::Value>(is_enabled), nullptr);
      policy_provider_.UpdateChromePolicy(policies);
    }
  }

  policy::MockConfigurationPolicyProvider policy_provider_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       CheckInfobarForEnableAutomation) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableAutomation);
  InfoBarService* infobar_service =
      LaunchBrowserAndGetCreatedInfobarService(command_line);
  ASSERT_TRUE(infobar_service);

  bool found_automation_infobar = false;
  for (size_t i = 0; i < infobar_service->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE) {
      found_automation_infobar = true;
    }
  }

  EXPECT_EQ(found_automation_infobar,
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       CheckInfobarForBadFlag) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // Test one of the flags from |bad_flags_prompt.cc|. Any of the flags should
  // have the same behavior.
  command_line.AppendSwitch(switches::kDisableWebSecurity);
  // BadFlagsPrompt::ShowBadFlagsPrompt uses CommandLine::ForCurrentProcess
  // instead of the command-line passed to StartupBrowserCreator. In browser
  // tests, this references the browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);
  InfoBarService* infobar_service =
      LaunchBrowserAndGetCreatedInfobarService(command_line);
  ASSERT_TRUE(infobar_service);

  bool found_bad_flags_infobar = false;
  for (size_t i = 0; i < infobar_service->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE) {
      found_bad_flags_infobar = true;
    }
  }

  EXPECT_EQ(found_bad_flags_infobar,
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

INSTANTIATE_TEST_SUITE_P(
    PolicyControl,
    StartupBrowserCreatorInfobarsTest,
    ::testing::Values(CommandLineFlagSecurityWarningsPolicy::kNoPolicy,
                      CommandLineFlagSecurityWarningsPolicy::kEnabled,
                      CommandLineFlagSecurityWarningsPolicy::kDisabled));
#endif  // !defined(OS_CHROMEOS)
