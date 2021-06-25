// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
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
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/url_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "third_party/blink/public/common/features.h"
#endif

using testing::Return;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/chrome_browser_application_mac.h"
#endif

using testing::_;
using extensions::Extension;

namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

const char kAppId[] = "dofnemchnjfeendjmdhaldenaiabpiad";
const char kAppName[] = "Test App";
const char kStartUrl[] = "https://test.com";

// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile()));

  // Find the new browser.
  Browser* other_browser = nullptr;
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
  creator.Launch(profile, std::vector<GURL>(), false, nullptr);
  return chrome::FindBrowserWithProfile(profile);
}

Browser* CloseBrowserAndOpenNew(Browser* browser, Profile* profile) {
  browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser);
  return OpenNewBrowser(profile);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

typedef base::Optional<policy::PolicyLevel> PolicyVariant;

// This class waits until all browser windows are closed, and then runs
// a quit closure.
class AllBrowsersClosedWaiter : public BrowserListObserver {
 public:
  explicit AllBrowsersClosedWaiter(base::OnceClosure quit_closure);
  AllBrowsersClosedWaiter(const AllBrowsersClosedWaiter&) = delete;
  AllBrowsersClosedWaiter& operator=(const AllBrowsersClosedWaiter&) = delete;
  ~AllBrowsersClosedWaiter() override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  base::OnceClosure quit_closure_;
};

AllBrowsersClosedWaiter::AllBrowsersClosedWaiter(base::OnceClosure quit_closure)
    : quit_closure_(std::move(quit_closure)) {
  BrowserList::AddObserver(this);
}

AllBrowsersClosedWaiter::~AllBrowsersClosedWaiter() {
  BrowserList::RemoveObserver(this);
}

void AllBrowsersClosedWaiter::OnBrowserRemoved(Browser* browser) {
  if (chrome::GetTotalBrowserCount() == 0)
    std::move(quit_closure_).Run();
}

// This class waits for a specified number of sessions to be restored.
class SessionsRestoredWaiter {
 public:
  explicit SessionsRestoredWaiter(base::OnceClosure quit_closure,
                                  int num_session_restores_expected);
  SessionsRestoredWaiter(const SessionsRestoredWaiter&) = delete;
  SessionsRestoredWaiter& operator=(const SessionsRestoredWaiter&) = delete;
  ~SessionsRestoredWaiter();

 private:
  // Callback for session restore notifications.
  void OnSessionRestoreDone(int num_tabs_restored);

  // For automatically unsubscribing from callback-based notifications.
  base::CallbackListSubscription callback_subscription_;
  base::OnceClosure quit_closure_;
  int num_session_restores_expected_;
  int num_sessions_restored_ = 0;
};

SessionsRestoredWaiter::SessionsRestoredWaiter(
    base::OnceClosure quit_closure,
    int num_session_restores_expected)
    : quit_closure_(std::move(quit_closure)),
      num_session_restores_expected_(num_session_restores_expected) {
  callback_subscription_ = SessionRestore::RegisterOnSessionRestoredCallback(
      base::BindRepeating(&SessionsRestoredWaiter::OnSessionRestoreDone,
                          base::Unretained(this)));
}

SessionsRestoredWaiter::~SessionsRestoredWaiter() = default;

void SessionsRestoredWaiter::OnSessionRestoreDone(int num_tabs_restored) {
  if (++num_sessions_restored_ == num_session_restores_expected_)
    std::move(quit_closure_).Run();
}

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    return nullptr;
  }

  // A helper function that checks the session restore UI (infobar) is shown
  // when Chrome starts up after crash.
  void EnsureRestoreUIWasShown(content::WebContents* web_contents) {
#if defined(OS_MAC)
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    EXPECT_EQ(1U, infobar_service->infobar_count());
#endif  // defined(OS_MAC)
  }
};

class OpenURLsPopupObserver : public BrowserListObserver {
 public:
  OpenURLsPopupObserver() = default;

  void OnBrowserAdded(Browser* browser) override { added_browser_ = browser; }

  void OnBrowserRemoved(Browser* browser) override {}

  Browser* added_browser_ = nullptr;
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

  Browser* popup = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  ASSERT_TRUE(popup->is_type_popup());
  ASSERT_EQ(popup, observer.added_browser_);

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a null or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls);
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// We don't do non-process-startup browser launches on ChromeOS.
// Session restore for process-startup browser launches is tested
// in session_restore_uitest.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

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

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

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
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

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
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

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
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a tab.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_REGULAR);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

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

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ValidNotificationLaunchId) {
  // Simulate a launch from the notification_helper process which appends the
  // kNotificationLaunchId switch to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

  // The launch delegates to the notification system and doesn't open any new
  // browser window.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, InvalidNotificationLaunchId) {
  // Simulate a launch with invalid launch id, which will fail.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kNotificationLaunchId, L"");
  StartupBrowserCreator browser_creator;
  ASSERT_FALSE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), /*process_startup=*/false,
      browser()->profile(), {}));

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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
  Browser* new_browser = nullptr;
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
  new_browser = FindOneOtherBrowserForProfile(other_profile, nullptr);
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

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile1, true));
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1,
                               embedded_test_server()->GetURL("/empty.html"));
  CloseBrowserSynchronously(browser1);

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile2, true));
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

  base::RunLoop run_loop;
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile1,
                        last_opened_profiles);
  run_loop.Run();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  new_browser = FindOneOtherBrowserForProfile(profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
  new_browser = FindOneOtherBrowserForProfile(profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html", tab_strip->GetWebContentsAt(0)->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ProfilesWithoutPagesNotLaunched) {
  ASSERT_TRUE(embedded_test_server()->Start());

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
  Browser* browser_last = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile_last, true));
  chrome::NewTab(browser_last);
  ui_test_utils::NavigateToURL(browser_last,
                               embedded_test_server()->GetURL("/empty.html"));

  // Close the browser without deleting |profile_last|.
  ScopedProfileKeepAlive profile_last_keep_alive(
      profile_last, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser_last);

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

  base::RunLoop run_loop;
  // Only profile_last should get its session restored.
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home1,
                        last_opened_profiles);
  run_loop.Run();

  Browser* new_browser = nullptr;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the NTP (and the welcome page on
  // relevant platforms).
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home1));
  new_browser = FindOneOtherBrowserForProfile(profile_home1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(new_browser->profile()),
            tab_strip->GetWebContentsAt(0)->GetURL());

  // profile_urls opened the urls.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetURL());

  // profile_last opened the last open pages.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last));
  new_browser = FindOneOtherBrowserForProfile(profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u, chrome::GetBrowserCount(profile_home2));
}

// This tests that opening multiple profiles with session restore enabled,
// shutting down, and then launching with kNoStartupWindow doesn't restore
// the previously opened profiles.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, RestoreWithNoStartupWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 2 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile1 = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile1);
  Profile* profile2 = profile_manager->GetProfile(dest_path2);
  ASSERT_TRUE(profile2);

  DisableWelcomePages({profile1, profile2});

  // Set the profiles to open last visited pages.
  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile1, pref_last);
  SessionStartupPref::SetStartupPref(profile2, pref_last);

  Profile* default_profile = browser()->profile();

  // TODO(crbug.com/88586): Adapt this test for DestroyProfileOnBrowserClose if
  // needed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive default_profile_keep_alive(
      default_profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open a page with profile1 and profile2.
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, profile1, true});
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1,
                               embedded_test_server()->GetURL("/empty.html"));

  Browser* browser2 = Browser::Create({Browser::TYPE_NORMAL, profile2, true});
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2,
                               embedded_test_server()->GetURL("/empty.html"));
  // Exit the browser, saving the multi-profile session state.
  chrome::ExecuteCommand(browser(), IDC_EXIT);
  {
    base::RunLoop run_loop;
    AllBrowsersClosedWaiter waiter(run_loop.QuitClosure());
    run_loop.Run();
  }

#if defined(OS_MAC)
  // While we closed all the browsers above, this doesn't quit the Mac app,
  // leaving the app in a half-closed state. Cancel the termination to put the
  // Mac app back into a known state.
  chrome_browser_application_mac::CancelTerminate();
#endif

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitch(switches::kNoStartupWindow);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles = {profile1, profile2};
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // TODO(davidbienvenu): Waiting for some sort of browser is started
  // notification would be better. But, we're not opening any browser
  // windows, so we'd need to invent a new notification.
  content::RunAllTasksUntilIdle();

  // No browser windows should be opened.
  EXPECT_EQ(chrome::GetBrowserCount(profile1), 0u);
  EXPECT_EQ(chrome::GetBrowserCount(profile2), 0u);

  base::CommandLine empty(base::CommandLine::NO_PROGRAM);
  base::RunLoop run_loop;
  SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(empty, {},
                                                          dest_path1);
  run_loop.Run();

  // profile1 and profile2 browser windows should be opened.
  EXPECT_EQ(chrome::GetBrowserCount(profile1), 1u);
  EXPECT_EQ(chrome::GetBrowserCount(profile2), 1u);
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

#if !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
#endif  // !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)

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
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home));
  new_browser = FindOneOtherBrowserForProfile(profile_home, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));

  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last));
  new_browser = FindOneOtherBrowserForProfile(profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

#if !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Each profile should have one session restore bubble shown, so we should
  // observe count 3 in bucket 0 (which represents bubble shown).
  histogram_tester.ExpectBucketCount("SessionCrashed.Bubble", 0, 3);
#endif  // !defined(OS_MAC) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile1->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetIsSigninRequired(true);

  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        profile1, last_opened_profiles);

  ASSERT_EQ(0u, chrome::GetBrowserCount(profile1));
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
}

// An observer that returns back to test code after a new browser is added to
// the BrowserList.
class BrowserAddedObserver : public BrowserListObserver {
 public:
  BrowserAddedObserver() { BrowserList::AddObserver(this); }

  ~BrowserAddedObserver() override { BrowserList::RemoveObserver(this); }

  Browser* Wait() {
    run_loop_.Run();
    return browser_;
  }

 protected:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    browser_ = browser;
    run_loop_.Quit();
  }

 private:
  Browser* browser_ = nullptr;
  base::RunLoop run_loop_;
};

class StartupBrowserWithWebAppTest : public StartupBrowserCreatorTest {
 protected:
  StartupBrowserWithWebAppTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorTest::SetUpCommandLine(command_line);
    if (GetTestPreCount() == 1) {
      // Load an app with launch.container = 'window'.
      command_line->AppendSwitchASCII(switches::kAppId, kAppId);
      command_line->AppendSwitchASCII(switches::kProfileDirectory, "Default");
    }
  }
  web_app::WebAppProvider& provider() {
    return *web_app::WebAppProvider::Get(profile());
  }
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWithWebAppTest,
                       PRE_PRE_LastUsedProfilesWithWebApp) {
  // Simulate a browser restart by creating the profiles in the PRE_PRE part.
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
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, profile1, true});
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1,
                               embedded_test_server()->GetURL("/title1.html"));

  Browser* browser2 = Browser::Create({Browser::TYPE_NORMAL, profile2, true});
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2,
                               embedded_test_server()->GetURL("/title2.html"));

  // Set startup preferences for the 2 profiles to restore last session.
  SessionStartupPref pref1(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile2, pref2);

  profile1->GetPrefs()->CommitPendingWrite();
  profile2->GetPrefs()->CommitPendingWrite();

  // Install a web app that we will launch from the command line in
  // the PRE test.
  web_app::WebAppProviderBase* const provider =
      web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
  web_app::InstallFinalizer& web_app_finalizer = provider->install_finalizer();

  web_app::InstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON;

  // Install web app set to open as a tab.
  {
    web_app_finalizer.RemoveLegacyInstallFinalizerForTesting();

    base::RunLoop run_loop;
    WebApplicationInfo info;
    info.start_url = GURL(kStartUrl);
    info.title = base::UTF8ToUTF16(kAppName);
    info.open_as_window = true;
    web_app_finalizer.FinalizeInstall(
        info, options,
        base::BindLambdaForTesting(
            [&](const web_app::AppId& app_id, web_app::InstallResultCode code) {
              EXPECT_EQ(app_id, kAppId);
              EXPECT_EQ(code, web_app::InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();

    EXPECT_EQ(provider->registrar().GetAppUserDisplayMode(kAppId),
              blink::mojom::DisplayMode::kStandalone);
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWithWebAppTest,
                       PRE_LastUsedProfilesWithWebApp) {
  BrowserAddedObserver added_observer;
  content::RunAllTasksUntilIdle();
  // Launching with an app opens the app window via a task, so the test
  // might start before SelectFirstBrowser is called.
  if (!browser()) {
    added_observer.Wait();
    SelectFirstBrowser();
  }
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // An app window should have been launched.
  EXPECT_TRUE(browser()->is_type_app());
  CloseBrowserAsynchronously(browser());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWithWebAppTest,
                       LastUsedProfilesWithWebApp) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

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

  while (SessionRestore::IsRestoring(profile1) ||
         SessionRestore::IsRestoring(profile2)) {
    base::RunLoop().RunUntilIdle();
  }

  // The last open sessions should be restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1));
  new_browser = FindOneOtherBrowserForProfile(profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title1.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2));
  new_browser = FindOneOtherBrowserForProfile(profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title2.html", tab_strip->GetWebContentsAt(0)->GetURL().path());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
class StartupBrowserWebAppUrlHandlingTest : public InProcessBrowserTest {
 protected:
  StartupBrowserWebAppUrlHandlingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableUrlHandlers);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    OverrideAssociationManager();
  }

  web_app::WebAppProviderBase* provider() {
    return web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
  }

  // Install a web app with url_handlers then register it with the
  // UrlHandlerManager. This is sufficient for testing URL matching and launch
  // at startup.
  web_app::AppId InstallWebAppWithUrlHandlers(
      const std::vector<apps::UrlHandlerInfo>& url_handlers) {
    std::unique_ptr<WebApplicationInfo> info =
        std::make_unique<WebApplicationInfo>();
    info->start_url = GURL(kStartUrl);
    info->title = base::UTF8ToUTF16(kAppName);
    info->open_as_window = true;
    info->url_handlers = url_handlers;
    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(info));

    auto& url_handler_manager =
        provider()->os_integration_manager().url_handler_manager_for_testing();

    base::RunLoop run_loop;
    url_handler_manager.RegisterUrlHandlers(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
    return app_id;
  }

  void SetUpCommandlineAndStart(const std::string& url) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendArg(url);

    std::vector<Profile*> last_opened_profiles;
    StartupBrowserCreator browser_creator;
    browser_creator.Start(command_line,
                          g_browser_process->profile_manager()->user_data_dir(),
                          browser()->profile(), last_opened_profiles);
  }

  void OverrideAssociationManager() {
    auto association_manager =
        std::make_unique<web_app::FakeWebAppOriginAssociationManager>();
    association_manager->set_pass_through(true);

    auto& url_handler_manager =
        provider()->os_integration_manager().url_handler_manager_for_testing();
    url_handler_manager.SetAssociationManagerForTesting(
        std::move(association_manager));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       WebAppLaunch_InScopeUrl) {
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL(kStartUrl));

  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // kStartUrl is in app scope.
  SetUpCommandlineAndStart(kStartUrl);

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest,
                       WebAppLaunch_DifferentOriginUrl) {
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL("https://example.com"));
  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // URL is not in app scope but matches url_handlers of installed app.
  SetUpCommandlineAndStart("https://example.com/abc/def");

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Out-of-scope URL launch results in app launch with default launch URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppUrlHandlingTest, UrlNotCaptured) {
  apps::UrlHandlerInfo url_handler;
  url_handler.origin = url::Origin::Create(GURL("https://example.com"));
  web_app::AppId app_id = InstallWebAppWithUrlHandlers({url_handler});

  // This URL is not in scope of installed app and does not match url_handlers.
  SetUpCommandlineAndStart("https://en.example.com/abc/def");

  content::RunAllTasksUntilIdle();

  // Check that new window is not app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(web_app::AppBrowserController::IsForWebApp(browser(), app_id));
  Browser* other_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(other_browser);
  ASSERT_FALSE(
      web_app::AppBrowserController::IsForWebApp(other_browser, app_id));
}
#endif

// These tests are only applicable to Windows currently, as Protocol Handler OS
// registration has not landed for other platforms yet (crbug/1019239).
#if defined(OS_WIN)

class StartupBrowserWebAppProtocolHandlingTest : public InProcessBrowserTest {
 protected:
  StartupBrowserWebAppProtocolHandlingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableProtocolHandlers);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  web_app::WebAppProviderBase* provider() {
    return web_app::WebAppProviderBase::GetProviderBase(browser()->profile());
  }

  // Install a web app with protocol_handlers then register it with the
  // ProtocolHandlerRegistry. This is sufficient for testing URL translation and
  // launch at startup.
  web_app::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<blink::Manifest::ProtocolHandler>& protocol_handlers) {
    std::unique_ptr<WebApplicationInfo> info =
        std::make_unique<WebApplicationInfo>();
    info->start_url = GURL(kStartUrl);
    info->title = base::UTF8ToUTF16(kAppName);
    info->open_as_window = true;
    info->protocol_handlers = protocol_handlers;
    web_app::AppId app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(info));

    auto& protocol_handler_manager =
        provider()
            ->os_integration_manager()
            .protocol_handler_manager_for_testing();

    base::RunLoop run_loop;
    protocol_handler_manager.RegisterOsProtocolHandlers(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
    return app_id;
  }

  void SetUpCommandlineAndStart(const std::string& url) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendArg(url);

    std::vector<Profile*> last_opened_profiles;
    StartupBrowserCreator browser_creator;
    browser_creator.Start(command_line,
                          g_browser_process->profile_manager()->user_data_dir(),
                          browser()->profile(), last_opened_profiles);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppProtocolHandlingTest,
                       WebAppLaunch_WebAppIsLaunchedWithProtocolUrl) {
  // Register web app as a protocol handler that should handle the launch.
  blink::Manifest::ProtocolHandler protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = u"web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString");

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check for new app window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Check the app is launched with the correctly translated URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ("https://test.com/testing=web%2Btest%3A%2F%2FparameterString",
            web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsNotLaunchedWithUnhandledProtocolUrl) {
  // Register web app as a protocol handler that should *not* handle the launch.
  blink::Manifest::ProtocolHandler protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = u"web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  // Launch the browser via a command line with an unhandled protocol URL param.
  SetUpCommandlineAndStart("web+unhandled://parameterString");

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check an app window is not launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_FALSE(web_app::AppBrowserController::IsWebApp(app_browser));

  // Check the browser launches to a blank new tab page.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(chrome::kChromeUINewTabURL, web_contents->GetVisibleURL());
}

#endif  // defined(OS_WIN)

class StartupBrowserCreatorExtensionsCheckupExperimentTest
    : public extensions::ExtensionBrowserTest {
 public:
  // ExtensionsBrowserTest opens about::blank via the command line, and
  // command-line tabs supersede all others, except pinned tabs.
  StartupBrowserCreatorExtensionsCheckupExperimentTest() {
    set_open_about_blank_on_browser_launch(false);
  }
  StartupBrowserCreatorExtensionsCheckupExperimentTest(
      const StartupBrowserCreatorExtensionsCheckupExperimentTest&) = delete;
  StartupBrowserCreatorExtensionsCheckupExperimentTest& operator=(
      const StartupBrowserCreatorExtensionsCheckupExperimentTest&) = delete;

  void SetUp() override {
    // Enable the extensions checkup experiment.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        extensions_features::kExtensionsCheckup,
        {{extensions_features::kExtensionsCheckupEntryPointParameter,
          extensions_features::kStartupEntryPoint}});
    extensions::ExtensionBrowserTest::SetUp();
  }

  void AddExtension() {
    // Adds a non policy-installed extension to the extension registry.
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("good.crx"));
    ASSERT_TRUE(extension);

    constexpr char kGoodExtensionId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile());
    EXPECT_TRUE(registry->enabled_extensions().GetByID(kGoodExtensionId));

    extensions::ExtensionPrefs* prefs =
        extensions::ExtensionPrefs::Get(profile());
    EXPECT_TRUE(prefs->GetInstalledExtensionInfo(kGoodExtensionId));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that when the extensions checkup experiment is enabled for the startup
// entry point and a user has extensions installed, the user is directed to the
// chrome://extensions page upon startup.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorExtensionsCheckupExperimentTest,
                       PRE_ExtensionsCheckup) {
  AddExtension();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorExtensionsCheckupExperimentTest,
                       ExtensionsCheckup) {
  // The new browser should have exactly two tabs (chrome://extensions page and
  // the NTP).
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* extensions_tab = tab_strip->GetWebContentsAt(0);

  // Check that the tab showing the extensions page is the active tab.
  EXPECT_EQ(extensions_tab, tab_strip->GetActiveWebContents());

  // Check that both the extensions page and the ntp page are shown.
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ("chrome://extensions/?checkup=shown",
            extensions_tab->GetLastCommittedURL());
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tab_strip->GetWebContentsAt(1)->GetLastCommittedURL());

  // Once the user sees the extensions page upon startup, they should not see it
  // again.
  Browser* other_browser = CreateBrowser(browser()->profile());

  // Make sure we are observing a new browser instance.
  EXPECT_NE(other_browser, browser());

  TabStripModel* other_tab_strip = other_browser->tab_strip_model();
  ASSERT_EQ(1, other_tab_strip->count());
  EXPECT_NE("chrome://extensions/?checkup=shown",
            other_tab_strip->GetWebContentsAt(0)->GetLastCommittedURL());
}

// Test that when the extensions checkup experiment has been shown and the
// browser is started again, the user is not directed to the
// chrome://extensions page upon startup.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorExtensionsCheckupExperimentTest,
                       PRE_ExtensionsCheckupAlreadyShown) {
  AddExtension();
  extensions::ExtensionPrefs::Get(profile())
      ->SetUserHasSeenExtensionsCheckupOnStartup(true);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorExtensionsCheckupExperimentTest,
                       ExtensionsCheckupAlreadyShown) {
  // The new browser should have exactly one tab (the NTP).
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            tab_strip->GetActiveWebContents()->GetLastCommittedURL());
}

// These tests are not applicable to Chrome OS as neither initial preferences
// nor the onboarding promos exist there.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

class StartupBrowserCreatorFirstRunTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorFirstRunTest() {
    scoped_feature_list_.InitWithFeatures({welcome::kForceEnabled}, {});
  }
  StartupBrowserCreatorFirstRunTest(const StartupBrowserCreatorFirstRunTest&) =
      delete;
  StartupBrowserCreatorFirstRunTest& operator=(
      const StartupBrowserCreatorFirstRunTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyMap policy_map_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(policy::key::kMetricsReportingEnabled,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
#endif  // (defined(OS_LINUX) || defined(OS_CHROMEOS)) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  ON_CALL(provider_, IsInitializationComplete(_)).WillByDefault(Return(true));
  ON_CALL(provider_, IsFirstPolicyLoadComplete(_)).WillByDefault(Return(true));
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
  ASSERT_TRUE(
      launch.Launch(browser()->profile(), std::vector<GURL>(), false, nullptr));

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_MAC)
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
  policy_map_.Set(policy::key::kRestoreOnStartup,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(SessionStartupPref::kPrefValueURLs), nullptr);
  base::ListValue startup_urls;
  startup_urls.AppendString(
      embedded_test_server()->GetURL("/title1.html").spec());
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, startup_urls.Clone(), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(
      launch.Launch(browser()->profile(), std::vector<GURL>(), true, nullptr));

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OS_MAC)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsWithRestoreSession \
    DISABLED_FirstRunTabsWithRestoreSession
#else
#define MAYBE_FirstRunTabsWithRestoreSession FirstRunTabsWithRestoreSession
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsWithRestoreSession) {
  // Simulate the following initial preferences:
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
  ASSERT_TRUE(
      launch.Launch(browser()->profile(), std::vector<GURL>(), true, nullptr));

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
  profile_manager->RegisterTestingProfile(std::move(profile1), true);

  Browser* browser = OpenNewBrowser(profile1_ptr);
  ASSERT_TRUE(browser);

  TabStripModel* tab_strip = browser->tab_strip_model();

  // Ensure that the standard Welcome page appears on second run on Win 10, and
  // on first run on all other platforms.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(chrome::kChromeUIWelcomeURL,
            tab_strip->GetWebContentsAt(0)->GetURL().possibly_invalid_spec());

  // TODO(crbug.com/88586): Adapt this test for DestroyProfileOnBrowserClose.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1_ptr, ProfileKeepAliveOrigin::kBrowserWindow);

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
                  policy::POLICY_SOURCE_CLOUD, base::Value(4), nullptr);
  base::Value url_list(base::Value::Type::LIST);
  url_list.Append(embedded_test_server()->GetURL("/title1.html").spec());
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
  profile_manager->RegisterTestingProfile(std::move(profile1), true);

  Browser* browser = OpenNewBrowser(profile1_ptr);
  ASSERT_TRUE(browser);

  TabStripModel* tab_strip = browser->tab_strip_model();

  // TODO(crbug.com/88586): Adapt this test for DestroyProfileOnBrowserClose.
  ScopedProfileKeepAlive profile1_keep_alive(
      profile1_ptr, ProfileKeepAliveOrigin::kBrowserWindow);

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

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class StartupBrowserCreatorWelcomeBackTest : public InProcessBrowserTest {
 protected:
  StartupBrowserCreatorWelcomeBackTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    profile_ = browser()->profile();

    // Keep the browser process and Profile running when all browsers are
    // closed.
    scoped_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
    scoped_profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kBrowserWindow);
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
                 base::Value(4), nullptr);
      base::Value url_list(base::Value::Type::LIST);
      url_list.Append("http://managed.site.com/");
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

  void TearDownOnMainThread() override {
    scoped_profile_keep_alive_.reset();
    scoped_keep_alive_.reset();
  }

 private:
  Profile* profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> scoped_profile_keep_alive_;
  StartupBrowserCreator browser_creator_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
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
    ASSERT_TRUE(base::WriteFile(
        temp_dir_.GetPath().Append(chrome::kLocalStateFilename), json));
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
    EXPECT_TRUE(launch.Launch(profile, std::vector<GURL>(), true, nullptr));

    // This should have created a new browser window.
    Browser* new_browser = FindOneOtherBrowser(browser());
    EXPECT_TRUE(new_browser);

    return InfoBarService::FromWebContents(
        new_browser->tab_strip_model()->GetWebContentsAt(0));
  }

  const CommandLineFlagSecurityWarningsPolicy policy_;

 private:
  void SetUpInProcessBrowserTestFixture() override {
    ON_CALL(policy_provider_, IsInitializationComplete(_))
        .WillByDefault(Return(true));
    ON_CALL(policy_provider_, IsFirstPolicyLoadComplete(_))
        .WillByDefault(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    if (policy_ != CommandLineFlagSecurityWarningsPolicy::kNoPolicy) {
      bool is_enabled =
          policy_ == CommandLineFlagSecurityWarningsPolicy::kEnabled;
      policy::PolicyMap policies;
      policies.Set(policy::key::kCommandLineFlagSecurityWarningsEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_PLATFORM, base::Value(is_enabled),
                   nullptr);
      policy_provider_.UpdateChromePolicy(policies);
    }
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies that infobars are not displayed in Kiosk mode.
class StartupBrowserCreatorInfobarsKioskTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorInfobarsKioskTest() = default;

 protected:
  InfoBarService* LaunchKioskBrowserAndGetCreatedInfobarService(
      const std::string& extra_switch) {
    Profile* profile = browser()->profile();
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kKioskMode);
    command_line.AppendSwitch(extra_switch);
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IS_NOT_FIRST_RUN);
    EXPECT_TRUE(launch.Launch(profile, std::vector<GURL>(), true, nullptr));

    // This should have created a new browser window.
    Browser* new_browser = FindOneOtherBrowser(browser());
    EXPECT_TRUE(new_browser);
    if (!new_browser)
      return nullptr;

    return InfoBarService::FromWebContents(
        new_browser->tab_strip_model()->GetActiveWebContents());
  }
};

// Verify that the Automation Enabled infobar is still shown in Kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForEnableAutomation) {
  InfoBarService* infobar_service =
      LaunchKioskBrowserAndGetCreatedInfobarService(
          switches::kEnableAutomation);
  ASSERT_TRUE(infobar_service);

  bool found_automation_infobar = false;
  for (size_t i = 0; i < infobar_service->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE) {
      found_automation_infobar = true;
    }
  }

  EXPECT_TRUE(found_automation_infobar);
}

// Verify that the Bad Flags infobar is not shown in kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForBadFlag) {
  // BadFlagsPrompt::ShowBadFlagsPrompt uses CommandLine::ForCurrentProcess
  // instead of the command-line passed to StartupBrowserCreator. In browser
  // tests, this references the browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  // Passing the kDisableWebSecurity argument here presently does not do
  // anything because of the aforementioned limitation.
  // https://crbug.com/1060293
  InfoBarService* infobar_service =
      LaunchKioskBrowserAndGetCreatedInfobarService(
          switches::kDisableWebSecurity);
  ASSERT_TRUE(infobar_service);

  for (size_t i = 0; i < infobar_service->infobar_count(); i++) {
    infobars::InfoBar* infobar = infobar_service->infobar_at(i);
    EXPECT_NE(infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
              infobar->delegate()->GetIdentifier());
  }
}

// Checks the correct behavior of the profile picker on startup. This feature is
// not available on ChromeOS.
class StartupBrowserCreatorPickerTestBase : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorPickerTestBase() {
    scoped_feature_list_.InitAndEnableFeature(features::kNewProfilePicker);
    // This test configures command line params carefully. Make sure
    // InProcessBrowserTest does _not_ add about:blank as a startup URL to the
    // command line.
    set_open_about_blank_on_browser_launch(false);
  }
  StartupBrowserCreatorPickerTestBase(
      const StartupBrowserCreatorPickerTestBase&) = delete;
  StartupBrowserCreatorPickerTestBase& operator=(
      const StartupBrowserCreatorPickerTestBase&) = delete;
  ~StartupBrowserCreatorPickerTestBase() override = default;

  void CreateMultipleProfiles() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // Create two additional profiles because the main test profile is created
    // later in the startup process and so we need to have at least 2 fake
    // profiles.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::vector<base::FilePath> profile_paths = {
        profile_manager->user_data_dir().Append(
            FILE_PATH_LITERAL("New Profile 1")),
        profile_manager->user_data_dir().Append(
            FILE_PATH_LITERAL("New Profile 2"))};
    for (const auto& profile_path : profile_paths) {
      ASSERT_TRUE(profile_manager->GetProfile(profile_path));
      // Mark newly created profiles as active.
      ProfileAttributesEntry* entry =
          profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path);
      ASSERT_NE(entry, nullptr);
      entry->SetActiveTimeToNow();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

struct ProfilePickerSetup {
  bool expected_to_show;
  base::Optional<std::string> switch_name;
  base::Optional<std::string> switch_value_ascii;
  base::Optional<GURL> url_arg;
  bool session_restore = false;
};

// Checks the correct behavior of the profile picker on startup. This feature is
// not available on ChromeOS.
class StartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<ProfilePickerSetup> {
 public:
  StartupBrowserCreatorPickerTest() = default;
  StartupBrowserCreatorPickerTest(const StartupBrowserCreatorPickerTest&) =
      delete;
  StartupBrowserCreatorPickerTest& operator=(
      const StartupBrowserCreatorPickerTest&) = delete;
  ~StartupBrowserCreatorPickerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    if (GetParam().url_arg) {
      command_line->AppendArg(GetParam().url_arg->spec());
    } else if (GetParam().switch_value_ascii) {
      DCHECK(GetParam().switch_name);
      command_line->AppendSwitchASCII(*GetParam().switch_name,
                                      *GetParam().switch_value_ascii);
    } else if (GetParam().switch_name) {
      command_line->AppendSwitch(*GetParam().switch_name);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, PRE_TestSetup) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  if (!GetParam().session_restore)
    CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, TestSetup) {
  if (GetParam().expected_to_show) {
    // Opens the picker and thus does not open any new browser window for the
    // main profile.
    EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  } else {
    // The picker is skipped which means a browser window is opened on startup.
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorPickerTest,
    ::testing::Values(
// Flaky: https://crbug.com/1126886
#if !defined(USE_OZONE) && !defined(OS_WIN)
        // Picker should be shown in normal multi-profile startup situation.
        ProfilePickerSetup{/*expected_to_show=*/true},
#endif
        // Skip the picker for various command-line params and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kIncognito},
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kApp},
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kAppId},
        // Skip the picker when a specific profile is requested (used e.g. by
        // profile specific desktop shortcuts on Win).
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kProfileDirectory,
                           /*switch_value_ascii=*/"Custom Profile"},
        // Skip the picker when a URL is provided on command-line (used by the
        // OS when Chrome is the default web browser) and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/base::nullopt,
                           /*switch_value_ascii=*/base::nullopt,
                           /*url_arg=*/GURL("https://www.foo.com/")},
        // Regression test for http://crbug.com/1166192
        // Picker should be shown even in case of session restore.
        ProfilePickerSetup{/*expected_to_show=*/true,
                           /*switch_name=*/base::nullopt,
                           /*switch_value_ascii=*/base::nullopt,
                           /*url_arg=*/base::nullopt,
                           /*session_restore=*/true}));

class GuestStartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  GuestStartupBrowserCreatorPickerTest() {
    TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, GetParam());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kGuest);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(GuestStartupBrowserCreatorPickerTest,
                       PRE_SkipsPickerWithGuest) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_P(GuestStartupBrowserCreatorPickerTest,
                       SkipsPickerWithGuest) {
  // The picker is skipped which means a browser window is opened on startup.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  if (Profile::IsEphemeralGuestProfileEnabled()) {
    EXPECT_TRUE(browser()->profile()->IsEphemeralGuestProfile());
  } else {
    EXPECT_TRUE(browser()->profile()->IsGuestSession());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         GuestStartupBrowserCreatorPickerTest,
                         /*ephemeral_guest_profile_enabled=*/testing::Bool());

class StartupBrowserCreatorPickerNoParamsTest
    : public StartupBrowserCreatorPickerTestBase {};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerNoParamsTest,
                       PRE_ShowPickerWhenAlreadyLaunched) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorPickerNoParamsTest,
                       ShowPickerWhenAlreadyLaunched) {
  // Preprequisite: The picker is shown on the first start-up
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());

  // Simulate a second start when the browser is already running.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  base::FilePath current_dir = base::FilePath();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_dir,
      GetStartupProfilePath(user_data_dir, current_dir, command_line,
                            /*ignore_profile_picker=*/false));
  base::RunLoop().RunUntilIdle();

  // The picker is shown again if no profile was previously opened.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
