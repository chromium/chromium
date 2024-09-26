// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/startup/web_app_startup_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_management_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/ui/views/web_apps/protocol_handler_launch_dialog_view.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#endif

using testing::Return;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(IS_WIN)

using extensions::Extension;
using testing::_;
using web_app::WebAppProvider;

namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

const char kAppId[] = "dofnemchnjfeendjmdhaldenaiabpiad";
const char16_t kAppName[] = u"Test App";
const char kStartUrl[] = "https://test.com";

// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile()));

  // Find the new browser.
  Browser* other_browser = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b != browser)
      other_browser = b;
  }
  return other_browser;
}

void DisableWhatsNewPage() {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
}

Browser* OpenNewBrowser(Profile* profile) {
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl creator(base::FilePath(), dummy,
                                    chrome::startup::IsFirstRun::kYes);
  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  creator.Launch(profile, chrome::startup::IsProcessStartup::kNo,
                 /*restore_tabbed_browser=*/true);
  Browser* new_browser = new_browser_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);
  return new_browser;
}

bool HasInfoBar(infobars::ContentInfoBarManager* infobar_manager,
                const infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return base::Contains(infobar_manager->infobars(), identifier,
                        &infobars::InfoBar::GetIdentifier);
}

struct StartupBrowserCreatorFlagTypeValue {
  std::string flag;
  infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier;
  // True if the infobar is supposed to be shown in every tab, false if it is
  // only supposed to be shown once.
  bool is_global_infobar;
};

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

typedef std::optional<policy::PolicyLevel> PolicyVariant;

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
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (browser != not_this_browser && browser->profile() == profile)
        return browser;
    }
    return nullptr;
  }

  // A helper function that checks the session restore UI (infobar) is shown
  // when Chrome starts up after crash.
  void EnsureRestoreUIWasShown(content::WebContents* web_contents) {
#if BUILDFLAG(IS_MAC)
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    EXPECT_EQ(1U, infobar_manager->infobars().size());
#endif  // BUILDFLAG(IS_MAC)
  }
};

class OpenURLsPopupObserver : public BrowserListObserver {
 public:
  OpenURLsPopupObserver() = default;

  void OnBrowserAdded(Browser* browser) override { added_browser_ = browser; }

  void OnBrowserRemoved(Browser* browser) override {}

  raw_ptr<Browser> added_browser_ = nullptr;
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
  chrome::startup::IsFirstRun first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a null or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, chrome::startup::IsProcessStartup::kNo, urls);
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

  DisableWhatsNewPage();

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
    EXPECT_EQ(expected_urls[i],
              tab_strip->GetWebContentsAt(i)->GetVisibleURL());

  // The two test_server tabs, despite having the same site, should be in
  // different SiteInstances.
  EXPECT_NE(
      tab_strip->GetWebContentsAt(tab_strip->count() - 2)->GetSiteInstance(),
      tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetSiteInstance());
}

// Verify that startup URLs aren't used when the process already exists
// and has other tabbed browser windows.  This is the common case of starting a
// new browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsOnNewWindow) {
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

  DisableWhatsNewPage();

  Browser* new_browser = OpenNewBrowser(browser()->profile());
  ASSERT_TRUE(new_browser);

  // The new browser should have exactly one tab (not the startup URLs).
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(
      chrome::kChromeUINewTabURL,
      tab_strip->GetWebContentsAt(0)->GetVisibleURL().possibly_invalid_spec());
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
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

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

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppUrlIncognitoShortcut) {
  // Add --app=<url> and --incognito to the command line. Tests launching
  // legacy apps which may have been created by "Add to Desktop" in old versions
  // of Chrome. Some existing workflows (especially testing scenarios) also
  // use the --incognito command line.
  // TODO(mgiuca): Delete this feature (https://crbug.com/751029). We are
  // keeping it for now to avoid disrupting existing workflows.
  // IMPORTANT NOTE: This is being committed because it is an easy fix, but
  // this use case is not officially supported. If a future refactor or
  // feature launch causes this to break again, we have no formal
  // responsibility to make this continue working. If you rely on the
  // combination of these two flags, you WILL be broken in the future.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  command_line.AppendSwitchASCII(switches::kApp, url.spec());
  command_line.AppendSwitch(switches::kIncognito);

  Browser* incognito = CreateIncognitoBrowser();

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {incognito->profile(), StartupProfileMode::kBrowserWindow}, {}));

  Browser* new_browser = FindOneOtherBrowser(incognito);
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

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       LaunchWebAppWhileKeepAliveRegistryIsShutdown) {
  // Command line to simulate app launch.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "app_id_1");

  // Simulate keep alive registry shutdown and try to launch the app and verify
  // that we don't crash.
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(true);
  web_app::startup::MaybeHandleWebAppLaunch(
      command_line, base::FilePath(FILE_PATH_LITERAL("\\path")),
      browser()->profile(), chrome::startup::IsFirstRun::kNo);
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_LaunchWebAppWhileBrowserShutdown \
  DISABLED_LaunchWebAppWhileBrowserShutdown
#else
#define MAYBE_LaunchWebAppWhileBrowserShutdown LaunchWebAppWhileBrowserShutdown
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       MAYBE_LaunchWebAppWhileBrowserShutdown) {
  // Test callback for verifying browser shutdown is called.
  base::test::TestFuture<void> browser_shutdown_complete;
  web_app::startup::SetBrowserShutdownCompleteCallbackForTesting(
      browser_shutdown_complete.GetCallback());

  // Command line to simulate app launch.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "app_id_1");

  web_app::startup::MaybeHandleWebAppLaunch(
      command_line, base::FilePath(FILE_PATH_LITERAL("\\path")),
      browser()->profile(), chrome::startup::IsFirstRun::kNo);
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER));

  // Start browser shutdown to trigger AppTerminatingCallback()
  chrome::AttemptExit();

  // Make sure OnBrowserShutdown() is called via AppTerminationCallback
  EXPECT_TRUE(browser_shutdown_complete.Wait());
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER));
}

namespace {

enum class ChromeAppDeprecationFeatureValue {
  kDefault,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  kEnabledWithNoLaunch,
  kDisabled,
#endif
};

std::string ChromeAppDeprecationFeatureValueToString(
    const ::testing::TestParamInfo<ChromeAppDeprecationFeatureValue>&
        param_info) {
  std::string result;
  switch (param_info.param) {
    case ChromeAppDeprecationFeatureValue::kDefault:
      result = "ChromeAppDeprecationFeatureDefault";
      break;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case ChromeAppDeprecationFeatureValue::kEnabledWithNoLaunch:
      result = "ChromeAppDeprecationFeatureEnabledWithNoLaunch";
      break;
    case ChromeAppDeprecationFeatureValue::kDisabled:
      result = "ChromeAppDeprecationFeatureDisabled";
      break;
#endif
  }
  return result;
}

}  // namespace

class StartupBrowserCreatorChromeAppShortcutTest
    : public StartupBrowserCreatorTest,
      public ::testing::WithParamInterface<ChromeAppDeprecationFeatureValue> {
 protected:
  StartupBrowserCreatorChromeAppShortcutTest() {
    switch (GetParam()) {
      case ChromeAppDeprecationFeatureValue::kDefault:
        break;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      case ChromeAppDeprecationFeatureValue::kEnabledWithNoLaunch:
        scoped_feature_list_.InitAndEnableFeature(
            features::kChromeAppsDeprecation);
        break;
      case ChromeAppDeprecationFeatureValue::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(
            features::kChromeAppsDeprecation);
        break;
#endif
    }
  }

  void SetUpOnMainThread() override {
    StartupBrowserCreatorTest::SetUpOnMainThread();
  }

  void ExpectBlockLaunch(const std::string& app_id, bool force_install_dialog) {
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    auto waiter = views::NamedWidgetShownWaiter(
        views::test::AnyWidgetTestPasskey{},
        force_install_dialog ? "ForceInstalledDeprecatedAppsDialogView"
                             : "DeprecatedAppsDialogView");
#endif
    // Should have opened the requested homepage about:blank in 1st window.
    TabStripModel* tab_strip = browser()->tab_strip_model();
    EXPECT_EQ(1, tab_strip->count());
    EXPECT_FALSE(browser()->is_type_app());
    EXPECT_TRUE(browser()->is_type_normal());
    EXPECT_EQ(GURL(url::kAboutBlankURL),
              tab_strip->GetWebContentsAt(0)->GetLastCommittedURL());
    // Should have opened the chrome://apps unsupported app flow in 2nd window.
    Browser* other_browser = FindOneOtherBrowser(browser());
    ASSERT_TRUE(other_browser);
    TabStripModel* other_tab_strip = other_browser->tab_strip_model();
    EXPECT_EQ(1, other_tab_strip->count());
    EXPECT_FALSE(other_browser->is_type_app());
    EXPECT_TRUE(other_browser->is_type_normal());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

    GURL expected_url =
        force_install_dialog
            ? GURL(chrome::kChromeUIAppsWithForceInstalledDeprecationDialogURL +
                   app_id)
            : GURL(chrome::kChromeUIAppsWithDeprecationDialogURL + app_id);
    EXPECT_EQ(expected_url,
              other_tab_strip->GetWebContentsAt(0)->GetVisibleURL());

    // Verify that the Deprecated Apps Dialog View also shows up.
    EXPECT_TRUE(waiter.WaitIfNeededAndGet() != nullptr);
#endif
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  enum class ExpectedLaunchBehavior{kLaunchAnywaysInTab, kLaunchAnywaysInWindow,
                                    kNoLaunch};
  void ExpectBlockLaunchWithLaunchBehavior(const std::string& app_id,
                                           bool force_install_dialog) {
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    auto waiter = views::NamedWidgetShownWaiter(
        views::test::AnyWidgetTestPasskey{},
        force_install_dialog ? "ForceInstalledDeprecatedAppsDialogView"
                             : "DeprecatedAppsDialogView");
    // Should have opened the requested homepage about:blank in 1st window.
    TabStripModel* tab_strip = browser()->tab_strip_model();
    EXPECT_EQ(1, tab_strip->count());
    EXPECT_FALSE(browser()->is_type_app());
    EXPECT_TRUE(browser()->is_type_normal());
    EXPECT_EQ(GURL(url::kAboutBlankURL),
              tab_strip->GetWebContentsAt(0)->GetLastCommittedURL());
    // Should have opened the chrome://apps unsupported app flow in 2nd window.
    Browser* other_browser = FindOneOtherBrowser(browser());
    DCHECK(other_browser);
    TabStripModel* other_tab_strip = other_browser->tab_strip_model();
    EXPECT_EQ(1, other_tab_strip->count());
    EXPECT_FALSE(other_browser->is_type_app());
    EXPECT_TRUE(other_browser->is_type_normal());

    GURL expected_url =
        force_install_dialog
            ? GURL(chrome::kChromeUIAppsWithForceInstalledDeprecationDialogURL +
                   app_id)
            : GURL(chrome::kChromeUIAppsWithDeprecationDialogURL + app_id);
    EXPECT_EQ(expected_url,
              other_tab_strip->GetWebContentsAt(0)->GetVisibleURL());

    std::set<Browser*> initial_browsers;
    for (Browser* initial_browser : *BrowserList::GetInstance()) {
      initial_browsers.insert(initial_browser);
    }

    content::TestNavigationObserver same_tab_observer(
        other_tab_strip->GetActiveWebContents(), 1,
        content::MessageLoopRunner::QuitMode::DEFERRED,
        /*ignore_uncommitted_navigations=*/false);

    // Verify that the Deprecated Apps Dialog View also shows up.
    auto* dialog = waiter.WaitIfNeededAndGet();
    EXPECT_TRUE(dialog != nullptr);
    if (force_install_dialog) {
      // The 'accept' option in the force-install dialog is "launch anyways".
      dialog->widget_delegate()->AsDialogDelegate()->Accept();
    } else {
      // The 'cancel' option in the deprecation dialog is "launch anyways".
      dialog->widget_delegate()->AsDialogDelegate()->Cancel();
    }

    // To ensure that no launch happens, run the run loop until idle.
    base::RunLoop().RunUntilIdle();
    Browser* app_browser = ui_test_utils::GetBrowserNotInSet(initial_browsers);
    EXPECT_EQ(app_browser, nullptr);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  bool IsExpectedToAllowLaunch() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    return false;
#else
    return true;
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  if (IsExpectedToAllowLaunch()) {
    // No pref was set, so the app should have opened in a tab in the existing
    // window.
    tab_waiter.Wait();
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ(tab_strip->GetActiveWebContents(),
              tab_strip->GetWebContentsAt(1));

    // It should be a standard tabbed window, not an app window.
    EXPECT_FALSE(browser()->is_type_app());
    EXPECT_TRUE(browser()->is_type_normal());

    // It should have loaded the requested app.
    const std::u16string expected_title(
        u"app_with_tab_container/empty.html title");
    content::TitleWatcher title_watcher(tab_strip->GetActiveWebContents(),
                                        expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    ExpectBlockLaunch(extension_app->id(), /*force_install_dialog=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenAppShortcutWindowPref) {
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ui_test_utils::BrowserChangeObserver browser_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  if (IsExpectedToAllowLaunch()) {
    // Pref was set to open in a window, so the app should have opened in a
    // window.  The launch should have created a new browser. Find the new
    // browser.
    Browser* new_browser = browser_waiter.Wait();
    ASSERT_TRUE(new_browser);

    // Expect an app window.
    EXPECT_TRUE(new_browser->is_type_app());

    // The browser's app_name should include the app's ID.
    EXPECT_NE(new_browser->app_name().find(extension_app->id()),
              std::string::npos)
        << new_browser->app_name();
  } else {
    ExpectBlockLaunch(extension_app->id(), /*force_install_dialog=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenAppShortcutTabPref) {
  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a tab.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_REGULAR);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  if (IsExpectedToAllowLaunch()) {
    // When an app shortcut is open and the pref indicates a tab should open,
    // the tab is open in the existing browser window.
    tab_waiter.Wait();
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ(tab_strip->GetActiveWebContents(),
              tab_strip->GetWebContentsAt(1));

    // The browser's app_name should not include the app's ID: it is in a normal
    // tabbed browser.
    EXPECT_EQ(browser()->app_name().find(extension_app->id()),
              std::string::npos)
        << browser()->app_name();

    // It should have loaded the requested app.
    const std::u16string expected_title(
        u"app_with_tab_container/empty.html title");
    content::TitleWatcher title_watcher(tab_strip->GetActiveWebContents(),
                                        expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    ExpectBlockLaunch(extension_app->id(), /*force_install_dialog=*/false);
  }
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTest,
                       OpenPolicyForcedAppShortcut) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Install a test policy provider which will mark the app as force-installed.
  extensions::TestManagementPolicyProvider policy_provider(
      extensions::TestManagementPolicyProvider::MUST_REMAIN_INSTALLED);
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(browser()->profile());
  extension_system->management_policy()->RegisterProvider(&policy_provider);

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  if (IsExpectedToAllowLaunch()) {
    tab_waiter.Wait();

    // Policy force-installed app should be allowed regardless of Chrome App
    // Deprecation status.
    //
    // No app launch pref was set, so the app should have opened in a tab in the
    // existing window.
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ(tab_strip->GetActiveWebContents(),
              tab_strip->GetWebContentsAt(1));

    // It should be a standard tabbed window, not an app window.
    EXPECT_FALSE(browser()->is_type_app());
    EXPECT_TRUE(browser()->is_type_normal());

    // It should have loaded the requested app.
    const std::u16string expected_title(
        u"app_with_tab_container/empty.html title");
    content::TitleWatcher title_watcher(tab_strip->GetActiveWebContents(),
                                        expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    ExpectBlockLaunch(extension_app->id(), /*force_install_dialog=*/true);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorChromeAppShortcutTest,
    ::testing::Values(
        ChromeAppDeprecationFeatureValue::kDefault
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        ,
        ChromeAppDeprecationFeatureValue::kEnabledWithNoLaunch,
        ChromeAppDeprecationFeatureValue::kDisabled
#endif
        ),
    ChromeAppDeprecationFeatureValueToString);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

using StartupBrowserCreatorChromeAppShortcutTestWithLaunch =
    StartupBrowserCreatorChromeAppShortcutTest;

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                       OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  ExpectBlockLaunchWithLaunchBehavior(extension_app->id(),
                                      /*force_install_dialog=*/false);

  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                       OpenAppShortcutWindowPref) {
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ui_test_utils::BrowserChangeObserver browser_waiter(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  ExpectBlockLaunchWithLaunchBehavior(extension_app->id(),
                                      /*force_install_dialog=*/false);

  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                       OpenAppShortcutTabPref) {
  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a tab.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_REGULAR);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  ExpectBlockLaunchWithLaunchBehavior(extension_app->id(),
                                      /*force_install_dialog=*/false);

  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
                       OpenPolicyForcedAppShortcut) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = nullptr;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Install a test policy provider which will mark the app as force-installed.
  extensions::TestManagementPolicyProvider policy_provider(
      extensions::TestManagementPolicyProvider::MUST_REMAIN_INSTALLED);
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(browser()->profile());
  extension_system->management_policy()->RegisterProvider(&policy_provider);

  // When we start, the browser should already have an open tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ui_test_utils::TabAddedWaiter tab_waiter(browser());

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  ExpectBlockLaunchWithLaunchBehavior(extension_app->id(),
                                      /*force_install_dialog=*/true);

  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
}

// These tests are specifically for testing what happens when the "Launch
// Anyways" button is pressed.
INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorChromeAppShortcutTestWithLaunch,
    ::testing::Values(
        ChromeAppDeprecationFeatureValue::kEnabledWithNoLaunch),
    ChromeAppDeprecationFeatureValueToString);

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ValidNotificationLaunchId) {
  // Simulate a launch from the notification_helper process which appends the
  // kNotificationLaunchId switch to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|aumi|0|https://example.com/|notification_id");

  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

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
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

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

  Profile& other_profile =
      profiles::testing::CreateProfileSync(profile_manager, dest_path);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Simulate a launch.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(
      switches::kNotificationLaunchId,
      L"1|1|0|Default|0|https://example.com/|notification_id");

  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&other_profile);

  StartupBrowserCreator browser_creator;
  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        {default_profile, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  // |browser()| is still around at this point, even though we've closed its
  // window. Thus the browser count for default_profile is 1.
  ASSERT_EQ(1u, chrome::GetBrowserCount(default_profile));

  // When the kNotificationLaunchId switch is present, any last opened profile
  // is ignored. Thus there is no browser for other_profile.
  ASSERT_EQ(0u, chrome::GetBrowserCount(&other_profile));
}

#endif  // BUILDFLAG(IS_WIN)

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

#if !BUILDFLAG(IS_CHROMEOS)
// If startup pref is set as LAST_AND_URLS, startup urls should be opened in a
// new browser window separated from the last-session-restored browser. This
// test does not apply to ChromeOS. Ash-chrome and lacros-chrome handle startup
// URLs in a different way.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupPrefSetAsLastAndURLs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create a new profile.
  base::FilePath dest_path =
      profile_manager->user_data_dir().Append(FILE_PATH_LITERAL("New Profile"));
  Profile& profile =
      profiles::testing::CreateProfileSync(profile_manager, dest_path);

  DisableWhatsNewPage();

  const GURL t1_url = embedded_test_server()->GetURL("/title1.html");
  const GURL t2_url = embedded_test_server()->GetURL("/title2.html");
  const GURL t3_url = embedded_test_server()->GetURL("/title3.html");

  // Set the profiles to open both urls and last visited pages.
  SessionStartupPref startup_pref(SessionStartupPref::LAST_AND_URLS);
  std::vector<GURL> urls_to_open;
  urls_to_open.push_back(t1_url);
  urls_to_open.push_back(t2_url);
  startup_pref.urls = urls_to_open;
  SessionStartupPref::SetStartupPref(&profile, startup_pref);

  // Open |t3_url| in a tab.
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile, true));
  TabStripModel* tab_strip_model = new_browser->tab_strip_model();
  ui_test_utils::NavigateToURLWithDisposition(
      new_browser, t3_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(t3_url,
            tab_strip_model->GetWebContentsAt(0)->GetLastCommittedURL());

  // Close the browser without deleting |profile|.
  ScopedProfileKeepAlive profile_keep_alive(
      &profile, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(new_browser);

  // Close the main browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(browser()->profile());
  last_opened_profiles.push_back(&profile);

  base::RunLoop run_loop;
  browser_creator.Start(
      dummy, profile_manager->user_data_dir(),
      {browser()->profile(), StartupProfileMode::kBrowserWindow},
      last_opened_profiles);
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  run_loop.Run();

  const auto wait_for_load_stop_for_browser = [](Browser* browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      EXPECT_TRUE(content::WaitForLoadStop(contents));
    }
  };

  // |profile| restored the last open pages and opened the urls in an active new
  // window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(&profile));
  Browser* pref_urls_opened_browser =
      chrome::FindLastActiveWithProfile(&profile);
  ASSERT_TRUE(pref_urls_opened_browser);
  Browser* last_session_opened_browser =
      FindOneOtherBrowserForProfile(&profile, pref_urls_opened_browser);
  ASSERT_TRUE(last_session_opened_browser);
  // Check the last-session-restored browser.
  EXPECT_NO_FATAL_FAILURE(
      wait_for_load_stop_for_browser(last_session_opened_browser));
  tab_strip_model = last_session_opened_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(t3_url, tab_strip_model->GetWebContentsAt(0)->GetVisibleURL());
  // Check the pref-urls-opened browser.
  EXPECT_NO_FATAL_FAILURE(
      wait_for_load_stop_for_browser(pref_urls_opened_browser));
  tab_strip_model = pref_urls_opened_browser->tab_strip_model();
  EXPECT_EQ(2, tab_strip_model->GetTabCount());
  EXPECT_EQ(t1_url, tab_strip_model->GetWebContentsAt(0)->GetVisibleURL());
  EXPECT_EQ(t2_url, tab_strip_model->GetWebContentsAt(1)->GetVisibleURL());
  EXPECT_EQ(0, tab_strip_model->active_index());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsForTwoProfiles) {
  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));
  Profile& other_profile =
      profiles::testing::CreateProfileSync(profile_manager, dest_path);

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
  SessionStartupPref::SetStartupPref(&other_profile, pref2);

  DisableWhatsNewPage();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(&other_profile);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {default_profile, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

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
  EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  ASSERT_EQ(1u, chrome::GetBrowserCount(&other_profile));
  new_browser = FindOneOtherBrowserForProfile(&other_profile, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls2[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, PRE_UpdateWithTwoProfiles) {
  // Simulate a browser restart by creating the profiles in the PRE_ part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  DisableWhatsNewPage();

  // Don't delete Profiles too early.
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile1, true));
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));
  CloseBrowserSynchronously(browser1);

  Browser* browser2 = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile2, true));
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/form.html")));
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
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(&profile2, pref2);

  profile1.GetPrefs()->CommitPendingWrite();
  profile2.GetPrefs()->CommitPendingWrite();
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, UpdateWithTwoProfiles) {
  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));

  // Simulate a launch after a browser update.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile1);
  last_opened_profiles.push_back(&profile2);

  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile1, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);
  run_loop.Run();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1.restored_last_session());
  EXPECT_TRUE(profile2.restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile1));
  new_browser = FindOneOtherBrowserForProfile(&profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile2));
  new_browser = FindOneOtherBrowserForProfile(&profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
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

  Profile& profile_home1 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path1);
  Profile& profile_home2 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path2);
  Profile& profile_last =
      profiles::testing::CreateProfileSync(profile_manager, dest_path3);
  Profile& profile_urls =
      profiles::testing::CreateProfileSync(profile_manager, dest_path4);

  DisableWhatsNewPage();

  // Set the profiles to open urls, open last visited pages or display the home
  // page.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(&profile_home1, pref_home);
  SessionStartupPref::SetStartupPref(&profile_home2, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(&profile_urls, pref_urls);

  // Open a page with profile_last.
  Browser* browser_last = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, &profile_last, true));
  chrome::NewTab(browser_last);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_last, embedded_test_server()->GetURL("/empty.html")));

  // Close the browser without deleting |profile_last|.
  ScopedProfileKeepAlive profile_last_keep_alive(
      &profile_last, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser_last);

  // Close the main browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile_home1);
  last_opened_profiles.push_back(&profile_home2);
  last_opened_profiles.push_back(&profile_last);
  last_opened_profiles.push_back(&profile_urls);

  base::RunLoop run_loop;
  // Only profile_last should get its session restored.
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 1);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile_home1, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);
  run_loop.Run();

  Browser* new_browser = nullptr;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the NTP.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_home1));
  new_browser = FindOneOtherBrowserForProfile(&profile_home1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(new_browser->profile()),
            tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  // profile_urls opened the urls.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_urls));
  new_browser = FindOneOtherBrowserForProfile(&profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetVisibleURL());

  // profile_last opened the last open pages.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_last));
  new_browser = FindOneOtherBrowserForProfile(&profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u, chrome::GetBrowserCount(&profile_home2));
}

// This tests that opening multiple profiles with session restore enabled,
// shutting down, and then launching with kNoStartupWindow doesn't restore
// the previously opened profiles.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40176564): enable this test on Lacros.
#define MAYBE_RestoreWithNoStartupWindow DISABLED_RestoreWithNoStartupWindow
#else
#define MAYBE_RestoreWithNoStartupWindow RestoreWithNoStartupWindow
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       MAYBE_RestoreWithNoStartupWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 2 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  Profile& profile1 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path1);
  Profile& profile2 =
      profiles::testing::CreateProfileSync(profile_manager, dest_path2);
  DisableWhatsNewPage();

  // Set the profiles to open last visited pages.
  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile1, pref_last);
  SessionStartupPref::SetStartupPref(&profile2, pref_last);

  Profile* default_profile = browser()->profile();

  // TODO(crbug.com/40594327): Adapt this test for DestroyProfileOnBrowserClose
  // if needed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive default_profile_keep_alive(
      default_profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile1_keep_alive(
      &profile1, ProfileKeepAliveOrigin::kBrowserWindow);
  ScopedProfileKeepAlive profile2_keep_alive(
      &profile2, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open a page with profile1 and profile2.
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, &profile1, true});
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/empty.html")));

  Browser* browser2 = Browser::Create({Browser::TYPE_NORMAL, &profile2, true});
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/empty.html")));
  // Exit the browser, saving the multi-profile session state.
  chrome::ExecuteCommand(browser(), IDC_EXIT);
  {
    base::RunLoop run_loop;
    AllBrowsersClosedWaiter waiter(run_loop.QuitClosure());
    run_loop.Run();
  }

#if BUILDFLAG(IS_MAC)
  // While we closed all the browsers above, this doesn't quit the Mac app,
  // leaving the app in a half-closed state. Cancel the termination to put the
  // Mac app back into a known state.
  chrome_browser_application_mac::CancelTerminate();
#endif

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitch(switches::kNoStartupWindow);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles = {&profile1, &profile2};
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {default_profile, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  // TODO(davidbienvenu): Waiting for some sort of browser is started
  // notification would be better. But, we're not opening any browser
  // windows, so we'd need to invent a new notification.
  content::RunAllTasksUntilIdle();

  // No browser windows should be opened.
  EXPECT_EQ(chrome::GetBrowserCount(&profile1), 0u);
  EXPECT_EQ(chrome::GetBrowserCount(&profile2), 0u);

  base::CommandLine empty(base::CommandLine::NO_PROGRAM);
  base::RunLoop run_loop;
  testing::SessionsRestoredWaiter restore_waiter(run_loop.QuitClosure(), 2);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      empty, {}, {dest_path1, StartupProfileModeReason::kWasRestarted});
  run_loop.Run();

  // profile1 and profile2 browser windows should be opened.
  EXPECT_EQ(chrome::GetBrowserCount(&profile1), 1u);
  EXPECT_EQ(chrome::GetBrowserCount(&profile2), 1u);
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
  Profile& profile_home =
      profiles::testing::CreateProfileSync(profile_manager, dest_path1);
  Profile& profile_last =
      profiles::testing::CreateProfileSync(profile_manager, dest_path2);
  Profile& profile_urls =
      profiles::testing::CreateProfileSync(profile_manager, dest_path3);

  // Set the profiles to open the home page, last visited pages or URLs.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(&profile_home, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(&profile_urls, pref_urls);

  // Simulate a launch after an unclear exit.
  CloseBrowserAsynchronously(browser());
  ExitTypeService::GetInstanceForProfile(&profile_home)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(&profile_last)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  ExitTypeService::GetInstanceForProfile(&profile_urls)
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitchASCII(switches::kTestType, "browser");
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile_home);
  last_opened_profiles.push_back(&profile_last);
  last_opened_profiles.push_back(&profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        {&profile_home, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  // No profiles are getting restored, since they all display the crash info
  // bar.
  EXPECT_FALSE(SessionRestore::IsRestoring(&profile_home));
  EXPECT_FALSE(SessionRestore::IsRestoring(&profile_last));
  EXPECT_FALSE(SessionRestore::IsRestoring(&profile_urls));

  // The profile which normally opens the home page displays the new tab page.
  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_home));
  new_browser = FindOneOtherBrowserForProfile(&profile_home, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();

  // The new browser should have only the NTP.
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));

  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_last));
  new_browser = FindOneOtherBrowserForProfile(&profile_last, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile_urls));
  new_browser = FindOneOtherBrowserForProfile(&profile_urls, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_TRUE(search::IsInstantNTP(tab_strip->GetWebContentsAt(0)));
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       LaunchMultipleLockedProfiles) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  ASSERT_TRUE(embedded_test_server()->Start());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager,
      user_data_dir.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager,
      user_data_dir.Append(FILE_PATH_LITERAL("New Profile 2")));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(&profile1);
  last_opened_profiles.push_back(&profile2);
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(&profile2, pref);

  ProfileAttributesEntry* entry1 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile1.GetPath());
  ASSERT_NE(entry1, nullptr);
  entry1->LockForceSigninProfile(true);

  ProfileAttributesEntry* entry2 =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile2.GetPath());
  ASSERT_NE(entry2, nullptr);
  entry2->LockForceSigninProfile(false);

  browser_creator.Start(command_line, profile_manager->user_data_dir(),
                        {&profile1, StartupProfileMode::kBrowserWindow},
                        last_opened_profiles);

  ASSERT_EQ(0u, chrome::GetBrowserCount(&profile1));
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile2));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
webapps::AppId InstallPWAWithName(Profile* profile,
                                  const GURL& start_url,
                                  const std::string& app_name) {
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info->title = base::UTF8ToUTF16(app_name);
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

class StartupBrowserWithListAppsFeature : public StartupBrowserCreatorTest {
 public:
  StartupBrowserWithListAppsFeature() {
    scoped_feature_list_.InitAndEnableFeature(features::kListWebAppsSwitch);
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWithListAppsFeature,
                       ListAppsForAllProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  Profile* profile1 = browser()->profile();

  // Create a new profile.
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager,
      user_data_dir.Append(FILE_PATH_LITERAL("New Profile 1")));

  // Install web apps for the two profiles.
  auto example_url1 = GURL("http://www.example_one.com");
  std::string app_name1 = "A Test Web App1";
  webapps::AppId app_id1 =
      InstallPWAWithName(profile1, example_url1, app_name1);
  auto example_url2 = GURL("http://www.example_two.com");
  std::string app_name2 = "A Test Web App2";
  webapps::AppId app_id2 =
      InstallPWAWithName(profile1, example_url2, app_name2);
  auto example_url3 = GURL("http://www.example_three.com");
  std::string app_name3 = "A Test Web App3";
  webapps::AppId app_id3 =
      InstallPWAWithName(&profile2, example_url3, app_name3);
  auto example_url4 = GURL("http://www.example_four.com");
  std::string app_name4 = "A Test Web App4";
  webapps::AppId app_id4 =
      InstallPWAWithName(&profile2, example_url4, app_name4);

  // Launch web apps for the two profiles.
  Browser* app_browser1 =
      web_app::LaunchWebAppBrowserAndWait(profile1, app_id1);
  Browser* app_browser2 =
      web_app::LaunchWebAppBrowserAndWait(&profile2, app_id3);
  ASSERT_NE(app_browser1, nullptr);
  ASSERT_NE(app_browser2, nullptr);

  // List web apps for all profiles.
  std::vector<Profile*> expected_profiles = {&profile2, profile1};
  std::vector<webapps::AppId*> expected_installed_apps_id = {
      &app_id4, &app_id3, &app_id2, &app_id1};
  std::vector<std::string*> expected_installed_apps_name = {
      &app_name4, &app_name3, &app_name2, &app_name1};
  std::vector<webapps::AppId*> expected_open_apps_id = {&app_id1, &app_id3};
  std::vector<std::string*> expected_open_apps_name = {&app_name1, &app_name3};
  base::Value::Dict apps_for_all_profiles;
  base::Value::List installed_apps_for_all_profile;
  base::Value::List open_apps_for_all_profile;
  for (int i = 0; i < 2; i++) {
    // Get installed web apps.
    base::Value::Dict installed_item_info;
    installed_item_info.Set("profile_id",
                            expected_profiles[i]->GetBaseName().AsUTF8Unsafe());
    base::Value::List installed_apps_per_profile;
    for (int j = 0; j < 2; j++) {
      base::Value::Dict web_app_info;
      web_app_info.Set("id", *expected_installed_apps_id[i * 2 + j]);
      web_app_info.Set("name", *expected_installed_apps_name[i * 2 + j]);
      installed_apps_per_profile.Append(std::move(web_app_info));
    }
    installed_item_info.Set("web_apps", std::move(installed_apps_per_profile));
    installed_apps_for_all_profile.Append(std::move(installed_item_info));
    // Get open web apps.
    base::Value::Dict open_item_info;
    open_item_info.Set("profile_id",
                       expected_profiles[1 - i]->GetBaseName().AsUTF8Unsafe());
    base::Value::List open_apps_per_profile;
    base::Value::Dict web_app_info;
    web_app_info.Set("id", *expected_open_apps_id[i]);
    web_app_info.Set("name", *expected_open_apps_name[i]);
    open_apps_per_profile.Append(std::move(web_app_info));
    open_item_info.Set("web_apps", std::move(open_apps_per_profile));
    open_apps_for_all_profile.Append(std::move(open_item_info));
  }
  apps_for_all_profiles.Set("installed_web_apps",
                            std::move(installed_apps_for_all_profile));
  apps_for_all_profiles.Set("open_web_apps",
                            std::move(open_apps_for_all_profile));

  std::string expected_info;
  JSONStringValueSerializer serializer(&expected_info);
  serializer.set_pretty_print(true);
  EXPECT_TRUE(serializer.Serialize(apps_for_all_profiles));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::FilePath output_path =
      user_data_dir.Append(FILE_PATH_LITERAL("AppsForAllProfiles.json"));
  command_line.AppendSwitchPath(switches::kListApps, output_path);
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  CloseBrowserSynchronously(app_browser1);
  CloseBrowserSynchronously(app_browser2);
  CloseBrowserSynchronously(browser());

  content::RunAllTasksUntilIdle();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(output_path, &file_contents));
    ASSERT_EQ(expected_info, file_contents);
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWithListAppsFeature,
                       ListAppsForGivenProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir = profile_manager->user_data_dir();
  Profile* profile1 = browser()->profile();

  // Create a new profile.
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager,
      user_data_dir.Append(FILE_PATH_LITERAL("New Profile 1")));

  // Install web apps for the two profiles.
  auto example_url1 = GURL("http://www.example_one.com");
  std::string app_name1 = "A Test Web App1";
  webapps::AppId app_id1 =
      InstallPWAWithName(profile1, example_url1, app_name1);
  auto example_url2 = GURL("http://www.example_two.com");
  std::string app_name2 = "A Test Web App2";
  webapps::AppId app_id2 =
      InstallPWAWithName(profile1, example_url2, app_name2);
  auto example_url3 = GURL("http://www.example_three.com");
  std::string app_name3 = "A Test Web App3";
  webapps::AppId app_id3 =
      InstallPWAWithName(&profile2, example_url3, app_name3);
  auto example_url4 = GURL("http://www.example_four.com");
  std::string app_name4 = "A Test Web App4";
  webapps::AppId app_id4 =
      InstallPWAWithName(&profile2, example_url4, app_name4);

  // Launch web apps for the two profiles.
  Browser* app_browser1 =
      web_app::LaunchWebAppBrowserAndWait(profile1, app_id1);
  Browser* app_browser2 =
      web_app::LaunchWebAppBrowserAndWait(&profile2, app_id3);
  ASSERT_NE(app_browser1, nullptr);
  ASSERT_NE(app_browser2, nullptr);

  // List web apps for the given profile.

  // Get installed web apps.
  base::Value::List installed_apps_for_given_profile;
  base::Value::Dict installed_item_info;
  installed_item_info.Set("profile_id", profile2.GetBaseName().AsUTF8Unsafe());
  base::Value::List installed_apps_per_profile;
  base::Value::Dict web_app_info1;
  web_app_info1.Set("name", app_name4);
  web_app_info1.Set("id", app_id4);
  installed_apps_per_profile.Append(std::move(web_app_info1));
  base::Value::Dict web_app_info2;
  web_app_info2.Set("name", app_name3);
  web_app_info2.Set("id", app_id3);
  installed_apps_per_profile.Append(std::move(web_app_info2));
  installed_item_info.Set("web_apps", std::move(installed_apps_per_profile));
  installed_apps_for_given_profile.Append(std::move(installed_item_info));

  // Get open web apps.
  base::Value::List open_apps_for_given_profile;
  base::Value::Dict open_item_info;
  open_item_info.Set("profile_id", profile2.GetBaseName().AsUTF8Unsafe());
  base::Value::List open_apps_per_profile;
  base::Value::Dict web_app_info3;
  web_app_info3.Set("name", app_name3);
  web_app_info3.Set("id", app_id3);
  open_apps_per_profile.Append(std::move(web_app_info3));
  open_item_info.Set("web_apps", std::move(open_apps_per_profile));
  open_apps_for_given_profile.Append(std::move(open_item_info));

  base::Value::Dict apps_for_given_profiles;
  apps_for_given_profiles.Set("installed_web_apps",
                              std::move(installed_apps_for_given_profile));
  apps_for_given_profiles.Set("open_web_apps",
                              std::move(open_apps_for_given_profile));
  std::string expected_info;
  JSONStringValueSerializer serializer(&expected_info);
  serializer.set_pretty_print(true);
  EXPECT_TRUE(serializer.Serialize(apps_for_given_profiles));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::FilePath output_path =
      user_data_dir.Append(FILE_PATH_LITERAL("AppsForGivenProfile.json"));
  command_line.AppendSwitchPath(switches::kListApps, output_path);
  command_line.AppendSwitchASCII(switches::kProfileBaseName, "New Profile 1");
  ASSERT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
      command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
      {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));

  CloseBrowserSynchronously(app_browser1);
  CloseBrowserSynchronously(app_browser2);
  CloseBrowserSynchronously(browser());

  content::RunAllTasksUntilIdle();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string file_contents;
    ASSERT_TRUE(base::ReadFileToString(output_path, &file_contents));
    ASSERT_EQ(expected_info, file_contents);
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_CHROMEOS)
webapps::AppId InstallPWA(Profile* profile, const GURL& start_url) {
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

class StartupBrowserCreatorRestartTest : public StartupBrowserCreatorTest,
                                         public BrowserListObserver {
 protected:
  StartupBrowserCreatorRestartTest() { BrowserList::AddObserver(this); }
  ~StartupBrowserCreatorRestartTest() override {
    // We might have already been removed but it's safe to call again.
    BrowserList::RemoveObserver(this);
  }

  void SetUpInProcessBrowserTestFixture() override {
    std::string_view test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    if (base::StartsWith(test_name, "PRE_")) {
      // The PRE_ test will call chrome::AttemptRestart().
      mock_relaunch_callback_ = std::make_unique<::testing::StrictMock<
          base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>>();
      EXPECT_CALL(*mock_relaunch_callback_, Run);
      relaunch_chrome_override_ =
          std::make_unique<upgrade_util::ScopedRelaunchChromeBrowserOverride>(
              mock_relaunch_callback_->Get());
    }
  }

  void OnBrowserAdded(Browser* browser) override {
    std::string_view test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();

    // The non PRE_ test will start up as if it was restarted.
    // Check that, then remove the observer.
    if (!base::StartsWith(test_name, "PRE_")) {
      EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
      EXPECT_FALSE(browser_added_check_passed_);
      browser_added_check_passed_ = true;
      BrowserList::RemoveObserver(this);
    }
  }

  bool browser_added_check_passed_ = false;

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  std::unique_ptr<
      base::MockCallback<upgrade_util::RelaunchChromeBrowserCallback>>
      mock_relaunch_callback_;
  std::unique_ptr<upgrade_util::ScopedRelaunchChromeBrowserOverride>
      relaunch_chrome_override_;
};

// Open an App and restart in preparation for the real test.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorRestartTest,
                       PRE_ProfileRestartedAppRestore) {
  // Ensure services are started.
  Profile* test_profile = browser()->profile();

  AppSessionServiceFactory::GetForProfileForSessionRestore(test_profile);
  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(test_profile, pref_last);

  // Install web app
  auto example_url = GURL("http://www.example.com");
  webapps::AppId app_id = InstallPWA(test_profile, example_url);
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(test_profile, app_id);

  ASSERT_NE(app_browser, nullptr);
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);
  ASSERT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  chrome::AttemptRestart();

  PrefService* pref_service = g_browser_process->local_state();
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kWasRestarted));
}

// This test tests a specific scenario where the browser is marked as restarted
// and a SessionBrowserCreatorImpl::MaybeAsyncRestore is triggered.
// ShouldRestoreApps will return true because the profile is marked as
// restarted which will trigger apps to restore. If apps are open at this point
// and an app restore occurs, apps will be duplicated. This test ensures that
// does not occur. This test doesn't build on non app_session_service
// platforms, hence the buildflag disablement.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorRestartTest,
                       ProfileRestartedAppRestore) {
  Profile* test_profile = browser()->profile();

  // StartupBrowserCreator() has already run in SetUp(), so it would already be
  // reset by this point.
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_TRUE(browser_added_check_passed_);
  // Now close the original (and last alive) tabbed browser window
  // note: there is still an app open
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1U, BrowserList::GetInstance()->size());

  // Now hit the codepath that would get hit if someone opened chrome
  // from a desktop shortcut or similar.
  SessionRestoreTestHelper restore_waiter;
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl creator(base::FilePath(), dummy,
                                    chrome::startup::IsFirstRun::kNo);
  creator.Launch(test_profile, chrome::startup::IsProcessStartup::kNo,
                 /*restore_tabbed_browser=*/true);
  restore_waiter.Wait();

  // We expect a browser to open, but we should NOT get a duplicate app.
  // Note at this point, the profile IsRestarted() is still true.
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  bool app_found = false;
  bool browser_found = false;
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      ASSERT_FALSE(app_found);
      app_found = true;
    } else if (browser->type() == Browser::Type::TYPE_NORMAL) {
      ASSERT_FALSE(browser_found);
      browser_found = true;
    }
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
  raw_ptr<Browser> browser_ = nullptr;
  base::RunLoop run_loop_;
};

class StartupBrowserWithWebAppTest : public StartupBrowserCreatorTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorTest::SetUpCommandLine(command_line);
    if (GetTestPreCount() == 1) {
      // Load an app with launch.container = 'window'.

#if BUILDFLAG(IS_MAC)
      // While the non-mac version of this test would pass on macOS, it isn't
      // testing a code path that would actually be used on macOS, and thus not
      // very useful as a test. Instead test the way an app shim would launch
      // Chrome in the background to launch an app.
      command_line->AppendSwitch(switches::kNoStartupWindow);
#else
      command_line->AppendSwitchASCII(switches::kAppId, kAppId);
      command_line->AppendSwitchASCII(switches::kProfileDirectory, "Default");
#endif
    }
  }
  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWithWebAppTest,
                       PRE_PRE_LastUsedProfilesWithWebApp) {
  // Simulate a browser restart by creating the profiles in the PRE_PRE part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  DisableWhatsNewPage();

  // Open some urls with the browsers, and close them.
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, &profile1, true});
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/title1.html")));

  Browser* browser2 = Browser::Create({Browser::TYPE_NORMAL, &profile2, true});
  chrome::NewTab(browser2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("/title2.html")));

  // Set startup preferences for the 2 profiles to restore last session.
  SessionStartupPref pref1(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile2, pref2);

  profile1.GetPrefs()->CommitPendingWrite();
  profile2.GetPrefs()->CommitPendingWrite();

  // Install a web app that we will launch from the command line in
  // the PRE test.
  WebAppProvider* const provider =
      WebAppProvider::GetForTest(browser()->profile());

  // Install web app set to open as a standalone window.
  {
    std::unique_ptr<web_app::WebAppInstallInfo> info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(kStartUrl));
    info->title = kAppName;
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    provider->scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), web_app::WebAppInstallParams());

    EXPECT_EQ(result.Get<webapps::AppId>(), kAppId);
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    EXPECT_EQ(provider->registrar_unsafe().GetAppUserDisplayMode(kAppId),
              web_app::mojom::UserDisplayMode::kStandalone);

#if BUILDFLAG(IS_MAC)
    AppShimRegistry::Get()->OnAppInstalledForProfile(
        kAppId, browser()->profile()->GetPath());
#endif
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWithWebAppTest,
                       PRE_LastUsedProfilesWithWebApp) {
  {
    BrowserAddedObserver added_observer;

#if BUILDFLAG(IS_MAC)
    // Simulate an app shim connecting and launching an app.
    apps::AppShimManager::Get()->LoadAndLaunchAppForTesting(kAppId);
#endif

    content::RunAllTasksUntilIdle();
    // Launching with an app opens the app window via a task, so the test
    // might start before SelectFirstBrowser is called.
    if (!browser()) {
      added_observer.Wait();
      SelectFirstBrowser();
    }
  }
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  // An app window should have been launched.
  EXPECT_TRUE(browser()->is_type_app());
  CloseBrowserSynchronously(browser());
}

// TODO(crbug.com/327256043): Flaky on win
#if BUILDFLAG(IS_WIN)
#define MAYBE_LastUsedProfilesWithWebApp DISABLED_LastUsedProfilesWithWebApp
#else
#define MAYBE_LastUsedProfilesWithWebApp LastUsedProfilesWithWebApp
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserWithWebAppTest,
                       MAYBE_LastUsedProfilesWithWebApp) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& profile2 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));

  while (SessionRestore::IsRestoring(&profile1) ||
         SessionRestore::IsRestoring(&profile2)) {
    base::RunLoop().RunUntilIdle();
  }

  // The last open sessions should be restored.
  EXPECT_TRUE(profile1.restored_last_session());
  EXPECT_TRUE(profile2.restored_last_session());

  Browser* new_browser = nullptr;
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile1));
  new_browser = FindOneOtherBrowserForProfile(&profile1, nullptr);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title1.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile2));
  new_browser = FindOneOtherBrowserForProfile(&profile2, nullptr);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title2.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
class StartupBrowserCreatorTestWithGuestParam
    : public StartupBrowserCreatorTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsGuest() const { return GetParam(); }

  GURL GetTestURL() const { return GURL("https://www.youtube.com"); }

  // Creates a browser for a new profile (which may be Guest, based on
  // `IsGuest()`).
  Browser* CreateBrowser() {
    if (IsGuest()) {
      profiles::SwitchToGuestProfile();
    } else {
      base::FilePath profile_path = g_browser_process->profile_manager()
                                        ->GenerateNextProfileDirectoryPath();
      profiles::SwitchToProfile(profile_path, /*always_create=*/true);
    }
    Browser* test_browser = ui_test_utils::WaitForBrowserToOpen();
    profiles::SetLastUsedProfile(test_browser->profile()->GetBaseName());
    return test_browser;
  }

  void OpenTabAlreadyRunning() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendArg(GetTestURL().spec());
    ChromeBrowserMainParts::ProcessSingletonNotificationCallback(
        command_line, /*current_directory=*/{});
  }
};

// Tests that receiving a launch notification while Chrome is already running
// opens the URL in the current browser window.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorTestWithGuestParam,
                       ProcessCommandLineAlreadyRunning) {
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                             KeepAliveRestartOption::DISABLED);
  CloseBrowserSynchronously(browser());

  // Create a browser for a new profile.
  Browser* test_browser = CreateBrowser();
  ASSERT_TRUE(test_browser);
  ASSERT_EQ(test_browser->profile()->IsGuestSession(), IsGuest());
  TabStripModel* tab_strip = test_browser->tab_strip_model();
  int initial_tab_count = tab_strip->count();

  // Open a URL while a browser is already open.
  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  OpenTabAlreadyRunning();
  content::WebContents* contents = tab_waiter.Wait();

  EXPECT_EQ(initial_tab_count + 1, tab_strip->count());
  EXPECT_EQ(contents, tab_strip->GetWebContentsAt(tab_strip->count() - 1));
  EXPECT_EQ(GetTestURL(), contents->GetVisibleURL());
}

// Tests that receiving a launch notification while Chrome is already running,
// but there was no browser window, reopens the last profile if it was regular,
// and opens the profile picker if it was guest.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorTestWithGuestParam,
                       ProcessCommandLineAlreadyRunningAfterBrowserClose) {
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
                             KeepAliveRestartOption::DISABLED);
  CloseBrowserSynchronously(browser());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create a browser for a new profile.
  Browser* test_browser = CreateBrowser();
  Profile* last_profile = test_browser->profile();
  ASSERT_TRUE(test_browser);
  ASSERT_EQ(last_profile->IsGuestSession(), IsGuest());

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  if (!IsGuest()) {
    // Keep the profile alive to avoid unloading and immediately reloading it,
    // which causes some flakiness within the HistoryService.
    // This is not done for the guest profile because:
    // - the test scenario does not involve reloading the guest profile,
    // - it is not allowed to take a keep alive on a OTR profile.
    profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        last_profile, ProfileKeepAliveOrigin::kBackgroundMode);
  }

  CloseBrowserSynchronously(test_browser);
  // Closing the browser did not change the last used profile.
  EXPECT_EQ(profile_manager->GetLastUsedProfileDir(), last_profile->GetPath());
  ASSERT_FALSE(ProfilePicker::IsOpen());

  // Open a URL after the last active browser was closed.
  OpenTabAlreadyRunning();

  if (IsGuest()) {
    // The profile picker opens. There is no browser, the URL is not loaded.
    profiles::testing::WaitForPickerWidgetCreated();
    EXPECT_EQ(0u, BrowserList::GetInstance()->size());
  } else {
    // The last used profile is reopened and the URL is loaded.
    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    Profile* profile = browser->profile();
    EXPECT_FALSE(profile->IsGuestSession());
    TabStripModel* tab_strip = browser->tab_strip_model();
    EXPECT_EQ(
        tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetVisibleURL(),
        GetTestURL());
    EXPECT_FALSE(ProfilePicker::IsOpen());
    EXPECT_EQ(1u, BrowserList::GetInstance()->size());
    EXPECT_EQ(last_profile, profile);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         StartupBrowserCreatorTestWithGuestParam,
                         testing::Bool());

class StartupBrowserWithRealWebAppTest : public StartupBrowserCreatorTest {
 protected:
  StartupBrowserWithRealWebAppTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {}

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserWithRealWebAppTest,
                       PRE_PRE_LastUsedProfilesWithRealWebApp) {
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  // Simulate a browser restart by creating the profiles in the PRE_PRE part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  DisableWhatsNewPage();

  // Open some urls with the browsers, and close them.
  SessionServiceFactory::GetForProfileForSessionRestore(&profile1);
  Browser* browser1 = Browser::Create({Browser::TYPE_NORMAL, &profile1, true});
  chrome::NewTab(browser1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser1, embedded_test_server()->GetURL("/title1.html")));
  browser1->window()->Show();
  browser1->window()->Maximize();

  // Set startup preferences to restore last session.
  SessionStartupPref pref1(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(&profile1, pref1);
  profile1.GetPrefs()->CommitPendingWrite();

  SessionStartupPref::SetStartupPref(browser()->profile(), pref1);
  browser()->profile()->GetPrefs()->CommitPendingWrite();

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1u, chrome::GetBrowserCount(&profile1));
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWithRealWebAppTest,
                       PRE_LastUsedProfilesWithRealWebApp) {
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath dest_path = profile_manager->user_data_dir();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));

  auto example_url = GURL("http://www.example.com");
  webapps::AppId new_app_id = InstallPWA(&profile1, example_url);
  Browser* app = web_app::LaunchWebAppBrowserAndWait(&profile1, new_app_id);
  ASSERT_TRUE(app);

  // destroy session services so we don't record this closure.
  // This simulates a user choosing ... -> Exit Chromium.
  for (auto* profile : profile_manager->GetLoadedProfiles()) {
    // Don't construct SessionServices for every type just to
    // shut them down. If they were never created, just skip.
    if (SessionServiceFactory::GetForProfileIfExisting(profile))
      SessionServiceFactory::ShutdownForProfile(profile);

    if (AppSessionServiceFactory::GetForProfileIfExisting(profile))
      AppSessionServiceFactory::ShutdownForProfile(profile);
  }

  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2u, chrome::GetBrowserCount(&profile1));

  // On ozone-linux, for some reason, these profile 1 windows come back in
  // the next test. To reliably ensure they don't, but don't destroy the
  // session restore state, close them while the session services are shutdown.

  Browser* close_this = FindOneOtherBrowserForProfile(&profile1, app);
  CloseBrowserSynchronously(close_this);
  CloseBrowserSynchronously(app);
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_LastUsedProfilesWithRealWebApp \
  DISABLED_LastUsedProfilesWithRealWebApp
#else
#define MAYBE_LastUsedProfilesWithRealWebApp LastUsedProfilesWithRealWebApp
#endif
// TODO(stahon@microsoft.com) App restores are disabled on mac.
// see http://crbug.com/1194201
IN_PROC_BROWSER_TEST_F(StartupBrowserWithRealWebAppTest,
                       MAYBE_LastUsedProfilesWithRealWebApp) {
  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ASSERT_TRUE(StartupBrowserCreator::WasRestarted());
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  Profile& default_profile = profiles::testing::CreateProfileSync(
      profile_manager, dest_path.Append(FILE_PATH_LITERAL("Default")));

  // At this point, nothing is open except the basic browser.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Trigger the restore via StartupBrowserCreator.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IsFirstRun::kNo);
  // Fake |process_startup| true.
  launch.Launch(&profile1, chrome::startup::IsProcessStartup::kYes,
                /*restore_tabbed_browser=*/true);

  // We should get two windows from profile1.
  ASSERT_EQ(3u, BrowserList::GetInstance()->size());
  ASSERT_EQ(1u, chrome::GetBrowserCount(&default_profile));
  ASSERT_EQ(2u, chrome::GetBrowserCount(&profile1));

  while (SessionRestore::IsRestoring(&profile1)) {
    base::RunLoop().RunUntilIdle();
  }

  // Since there's one app being restored, ensure the provider is ready.
  WebAppProvider* provider = WebAppProvider::GetForTest(&profile1);
  ASSERT_TRUE(provider->on_registry_ready().is_signaled());

  // The last open sessions should be restored.
  EXPECT_TRUE(profile1.restored_last_session());

  Browser* new_browser = nullptr;

  // 2x profile1, 1x default profile here.
  ASSERT_EQ(3u, BrowserList::GetInstance()->size());
  ASSERT_EQ(2u, chrome::GetBrowserCount(&profile1));
  ASSERT_EQ(1u, chrome::GetBrowserCount(&default_profile));
  new_browser = FindOneOtherBrowserForProfile(&profile1, nullptr);
  if (new_browser->type() != Browser::Type::TYPE_NORMAL) {
    new_browser = FindOneOtherBrowserForProfile(&profile1, new_browser);
  }
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(new_browser->type(), Browser::Type::TYPE_NORMAL);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ("/title1.html",
            tab_strip->GetWebContentsAt(0)->GetLastCommittedURL().path());

  // Now get the app, it should just be the other browser from this profile.
  new_browser = FindOneOtherBrowserForProfile(&profile1, new_browser);
  ASSERT_EQ(new_browser->type(), Browser::Type::TYPE_APP);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class StartupBrowserWebAppProtocolHandlingTest : public InProcessBrowserTest {
 protected:
  StartupBrowserWebAppProtocolHandlingTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  WebAppProvider* provider() {
    return WebAppProvider::GetForTest(browser()->profile());
  }

  // Install a web app with `protocol_handlers` (and optionally `file_handlers`)
  // then register it with the ProtocolHandlerRegistry. This is sufficient for
  // testing URL translation and launch at startup.
  webapps::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers,
      const std::vector<apps::FileHandler>& file_handlers = {}) {
    std::unique_ptr<web_app::WebAppInstallInfo> info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(kStartUrl));
    info->title = kAppName;
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->protocol_handlers = protocol_handlers;
    info->file_handlers = file_handlers;
    webapps::AppId app_id =
        web_app::test::InstallWebApp(browser()->profile(), std::move(info));
    return app_id;
  }

  void SetUpCommandlineAndStart(const std::string& url,
                                const webapps::AppId& app_id) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendArg(url);
    command_line.AppendSwitchASCII(switches::kAppId, app_id);

    std::vector<Profile*> last_opened_profiles;
    StartupBrowserCreator browser_creator;
    browser_creator.Start(
        command_line, g_browser_process->profile_manager()->user_data_dir(),
        {browser()->profile(), StartupProfileMode::kBrowserWindow},
        last_opened_profiles);
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_WIN)
  // This is needed to stop StartupBrowserWebAppProtocolHandlingTests creating a
  // shortcut in the Windows start menu. The override needs to last until the
  // test is destroyed, because Windows shortcut tasks which create the shortcut
  // can run after the test body returns.
  base::ScopedPathOverride override_start_dir{base::DIR_START_MENU};
#endif  // BUILDFLAG(IS_WIN)
};

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsNotLaunchedWithProtocolUrlAndDialogCancel) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ProtocolHandlerLaunchDialogView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and close it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);

  // Check that no extra window is launched.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithProtocolUrlAndDialogAccept) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ProtocolHandlerLaunchDialogView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});
  bool allowed_protocols_notified = false;
  web_app::WebAppTestRegistryObserverAdapter observer(browser()->profile());
  observer.SetWebAppProtocolSettingsChangedDelegate(
      base::BindLambdaForTesting([&]() { allowed_protocols_notified = true; }));

  web_app::ProtocolHandlerLaunchDialogView::
      SetDefaultRememberSelectionForTesting(true);

  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and accepts it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  web_app::ProtocolHandlerLaunchDialogView::
      SetDefaultRememberSelectionForTesting(false);
  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check that we added this protocol to web app's allowed_launch_protocols
  // on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar_unsafe();
  EXPECT_TRUE(registrar.IsAllowedLaunchProtocol(app_id, "web+test"));
  EXPECT_TRUE(allowed_protocols_notified);

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
    WebAppLaunch_WebAppIsNotTranslatedWithUnhandledProtocolUrl) {
  // Register web app as a protocol handler that should *not* handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  // Launch the browser via a command line with an unhandled protocol URL param.
  SetUpCommandlineAndStart("web+unhandled://parameterString", app_id);

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check an app window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser;
  app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Check the app is launched to the home page and not the translated URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(kStartUrl), web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithAllowedProtocolUrlPref) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ProtocolHandlerLaunchDialogView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  web_app::ProtocolHandlerLaunchDialogView::
      SetDefaultRememberSelectionForTesting(true);
  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and accepts it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  web_app::ProtocolHandlerLaunchDialogView::
      SetDefaultRememberSelectionForTesting(false);

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  // Check that we added this protocol to web app's allowed_launch_protocols
  // on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar_unsafe();
  EXPECT_TRUE(registrar.IsAllowedLaunchProtocol(app_id, "web+test"));

  // Check the first app window is created.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser1;
  app_browser1 = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser1);

  // Launch the browser via a command line with an handled protocol URL
  // param, but this time we expect the permission dialog to not show up.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  // Check the second app window is launched directly this time. The dialog
  // is skipped because we have the allowed protocol scheme for the same
  // app launch.
  Browser* app_browser2;
  // There should be 3 browser windows opened at the moment.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b != browser() && b != app_browser1)
      app_browser2 = b;
  }
  ASSERT_TRUE(app_browser2);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser2, app_id));

  // Check the app is launched with the correctly translated URL.
  TabStripModel* tab_strip = app_browser2->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ("https://test.com/testing=web%2Btest%3A%2F%2FparameterString",
            web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppProtocolHandlingTest,
                       WebAppLaunch_WebAppIsLaunchedWithAllowedProtocol) {
  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and accepts it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  // Check that we did not add this protocol to web app's
  // allowed_launch_protocols on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar_unsafe();
  EXPECT_FALSE(registrar.IsAllowedLaunchProtocol(app_id, "web+test"));

  // Check the first app window is created.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser1;
  app_browser1 = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser1);

  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and accepts it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }

  // Wait for app launch task to complete and launches a new browser.
  ui_test_utils::WaitForBrowserToOpen();

  Browser* app_browser2;
  // There should be 3 browser windows opened at the moment.
  ASSERT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b != browser() && b != app_browser1)
      app_browser2 = b;
  }
  ASSERT_TRUE(app_browser2);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser2, app_id));

  // Check the app is launched with the correctly translated URL.
  TabStripModel* tab_strip = app_browser2->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ("https://test.com/testing=web%2Btest%3A%2F%2FparameterString",
            web_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithDiallowedProtocolUrlPref) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ProtocolHandlerLaunchDialogView");

  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  web_app::ProtocolHandlerLaunchDialogView::
      SetDefaultRememberSelectionForTesting(true);
  // Launch the browser via a command line with a handled protocol URL param.
  SetUpCommandlineAndStart("web+test://parameterString", app_id);

  // The waiter will get the dialog when it shows up and accepts it.
  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kCancelButtonClicked);
  base::RunLoop().RunUntilIdle();

  web_app::ProtocolHandlerLaunchDialogView::
      SetDefaultRememberSelectionForTesting(false);

  // Check that we added this protocol to web app's allowed_launch_protocols
  // on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar_unsafe();
  EXPECT_TRUE(registrar.IsDisallowedLaunchProtocol(app_id, "web+test"));

  // Check the no app window is created.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserWebAppProtocolHandlingTest,
    WebAppLaunch_WebAppIsLaunchedWithDisallowedOnceProtocol) {
  // Register web app as a protocol handler that should handle the launch.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and cancels it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }

  // Check that we did not add this protocol to web app's
  // allowed_launch_protocols on accept.
  web_app::WebAppRegistrar& registrar = provider()->registrar_unsafe();
  EXPECT_FALSE(registrar.IsDisallowedLaunchProtocol(app_id, "web+test"));

  // Check the no app window is created.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));

  {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "ProtocolHandlerLaunchDialogView");

    // Launch the browser via a command line with a handled protocol URL param.
    SetUpCommandlineAndStart("web+test://parameterString", app_id);

    // The waiter will get the dialog when it shows up and accepts it.
    waiter.WaitIfNeededAndGet()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }
  // There should be only 1 browser window opened at the moment.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

class StartupBrowserWebAppProtocolAndFileHandlingTest
    : public StartupBrowserWebAppProtocolHandlingTest {
  base::test::ScopedFeatureList feature_list_{
      blink::features::kFileHandlingAPI};
};

// Verifies that a "file://" URL on the command line is treated as a file
// handling launch, not a protocol handling or URL launch.
IN_PROC_BROWSER_TEST_F(StartupBrowserWebAppProtocolAndFileHandlingTest,
                       WebAppLaunch_FileProtocol) {
  // Install an app with protocol handlers and a handler for plain text files.
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url = std::string(kStartUrl) + "/protocol=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  apps::FileHandler file_handler;
  file_handler.action = GURL(std::string(kStartUrl) + "/file_handler");
  file_handler.accept.push_back({});
  file_handler.accept.back().mime_type = "text/plain";
  file_handler.accept.back().file_extensions = {".txt"};
  webapps::AppId app_id =
      InstallWebAppWithProtocolHandlers({protocol_handler}, {file_handler});

  // Skip the file handler dialog by simulating prior user approval of the API.
  provider()->sync_bridge_unsafe().SetAppFileHandlerApprovalState(
      app_id, web_app::ApiApprovalState::kAllowed);

  // Pass a file:// url on the command line.
  SetUpCommandlineAndStart("file:///C:/test.txt", app_id);

  // Wait for app launch task to complete.
  content::RunAllTasksUntilIdle();

  // Check an app window is launched.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(app_browser);
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Check the app is launched to the file handler URL and not the protocol URL.
  TabStripModel* tab_strip = app_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(file_handler.action, web_contents->GetVisibleURL());

  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// These tests are not applicable to Chrome OS as neither initial preferences
// nor the onboarding promos exist there.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

class StartupBrowserCreatorFirstRunTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorFirstRunTest() = default;
  StartupBrowserCreatorFirstRunTest(const StartupBrowserCreatorFirstRunTest&) =
      delete;
  StartupBrowserCreatorFirstRunTest& operator=(
      const StartupBrowserCreatorFirstRunTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyMap policy_map_;
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(
#if BUILDFLAG(IS_CHROMEOS)
      policy::key::kDeviceMetricsReportingEnabled,
#else
      policy::key::kMetricsReportingEnabled,
#endif
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                              /*is_first_policy_load_complete_return=*/true);
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest, AddFirstRunTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTabs(
      {embedded_test_server()->GetURL("/title1.html"),
       embedded_test_server()->GetURL("/title2.html")});

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IsFirstRun::kYes);
  launch.Launch(browser()->profile(), chrome::startup::IsProcessStartup::kNo,
                /*restore_tabbed_browser=*/true);

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();

  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetVisibleURL().ExtractFileName());
  EXPECT_EQ("title2.html",
            tab_strip->GetWebContentsAt(1)->GetVisibleURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_MAC)
// http://crbug.com/314819
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
  DISABLED_RestoreOnStartupURLsPolicySpecified
#else
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
  RestoreOnStartupURLsPolicySpecified
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_RestoreOnStartupURLsPolicySpecified) {
#if BUILDFLAG(IS_WIN)
  return;
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;

  DisableWhatsNewPage();

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(policy::key::kRestoreOnStartup,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(SessionStartupPref::kPrefValueURLs), nullptr);
  base::Value::List startup_urls;
  startup_urls.Append(embedded_test_server()->GetURL("/title1.html").spec());
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(std::move(startup_urls)), nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IsFirstRun::kYes);
  launch.Launch(browser()->profile(), chrome::startup::IsProcessStartup::kYes,
                /*restore_tabbed_browser=*/true);

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the URL specified through policy is shown and no sync promo has
  // been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetVisibleURL().ExtractFileName());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_MAC)
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
  browser_creator.AddFirstRunTabs(
      {embedded_test_server()->GetURL("/title1.html")});
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IsFirstRun::kYes);
  launch.Launch(browser()->profile(), chrome::startup::IsProcessStartup::kYes,
                /*restore_tabbed_browser=*/true);

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tab is shown and no other pages are present.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetVisibleURL().ExtractFileName());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Validates that prefs::kWasRestarted is automatically reset after next browser
// start.
class StartupBrowserCreatorWasRestartedFlag : public InProcessBrowserTest,
                                              public BrowserListObserver {
 public:
  StartupBrowserCreatorWasRestartedFlag() { BrowserList::AddObserver(this); }
  ~StartupBrowserCreatorWasRestartedFlag() override {
    BrowserList::RemoveObserver(this);
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    std::string json;
    base::Value::Dict local_state;
    local_state.SetByDottedPath(prefs::kWasRestarted, true);
    base::JSONWriter::Write(local_state, &json);

    base::FilePath local_state_path =
        user_data_dir.Append(chrome::kLocalStateFilename);
    if (!base::WriteFile(local_state_path, json)) {
      ADD_FAILURE() << "base::WriteFile() failed, " << local_state_path;
      return false;
    }

    return true;
  }

 protected:
  // SetUpCommandLine is setting kWasRestarted, so these tests all start up
  // with WasRestarted() true.
  void OnBrowserAdded(Browser* browser) override {
    EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
    EXPECT_FALSE(
        g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
    on_browser_added_hit_ = true;
  }

  bool on_browser_added_hit_ = false;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWasRestartedFlag, Test) {
  // OnBrowserAdded() should have been hit before the test body began.
  EXPECT_TRUE(on_browser_added_hit_);
  // This is a bit strange but what occurs is that StartupBrowserCreator runs
  // before this test body is hit and ~StartupBrowserCreator() will reset the
  // restarted state, so here when we read WasRestarted() it should already be
  // reset to false.
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted));
}

// The kCommandLineFlagSecurityWarningsEnabled policy doesn't exist on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
enum class CommandLineFlagSecurityWarningsPolicy {
  kNoPolicy,
  kEnabled,
  kDisabled,
};

// Verifies that infobars are displayed (or not) depending on enterprise policy.
class StartupBrowserCreatorInfobarsTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<StartupBrowserCreatorFlagTypeValue,
                     CommandLineFlagSecurityWarningsPolicy>> {
 public:
  StartupBrowserCreatorInfobarsTest()
      : flag_type_(std::get<0>(GetParam())), policy_(std::get<1>(GetParam())) {}

 protected:
  std::pair<Browser*, infobars::ContentInfoBarManager*>
  LaunchBrowserAndGetCreatedInfoBarManager(
      const base::CommandLine& command_line) {
    BrowserAddedObserver added_observer;

    base::test::TestFuture<void> app_launch_done;
    if (command_line.HasSwitch(switches::kAppId)) {
      web_app::startup::SetStartupDoneCallbackForTesting(
          app_launch_done.GetCallback());
    } else {
      std::move(app_launch_done.GetCallback()).Run();
    }
    EXPECT_TRUE(StartupBrowserCreator().ProcessCmdLineImpl(
        command_line, base::FilePath(), chrome::startup::IsProcessStartup::kNo,
        {browser()->profile(), StartupProfileMode::kBrowserWindow}, {}));
    EXPECT_TRUE(app_launch_done.Wait());

    // Wait until the new browser window has been created. Using
    // `FindOneOtherBrowser` is not sufficient here, because the window may be
    // created asynchronously.
    Browser* new_browser = added_observer.Wait();
    EXPECT_TRUE(new_browser);

    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(
            new_browser->tab_strip_model()->GetWebContentsAt(0));
    EXPECT_TRUE(infobar_manager);

    return std::make_pair(new_browser, infobar_manager);
  }

  const StartupBrowserCreatorFlagTypeValue flag_type_;
  const CommandLineFlagSecurityWarningsPolicy policy_;

 private:
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
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
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest, CheckInfobar) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // all read from CommandLine::ForCurrentProcess and ignore the command line
  // passed to StartupBrowserCreator. In browser tests, this references the
  // browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);
  auto [browser, infobar_manager] =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  EXPECT_TRUE(browser->is_type_normal());

  EXPECT_EQ(HasInfoBar(infobar_manager, flag_type_.infobar_identifier),
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       CheckInfobarIsShownForWebApps) {
  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // all read from CommandLine::ForCurrentProcess and ignore the command line
  // passed to StartupBrowserCreator. In browser tests, this references the
  // browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);

  Profile* test_profile = browser()->profile();
  // Install web app
  GURL example_url("http://www.example.com");
  webapps::AppId app_id = InstallPWA(test_profile, example_url);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, app_id);

  auto [browser, infobar_manager] =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  EXPECT_TRUE(browser->is_type_app());

  EXPECT_EQ(HasInfoBar(infobar_manager, flag_type_.infobar_identifier),
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       CheckInfobarIsShownForAppUrlShortcuts) {
  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // all read from CommandLine::ForCurrentProcess and ignore the command line
  // passed to StartupBrowserCreator. In browser tests, this references the
  // browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);

  // Add --app=<url> to the command line. Tests launching legacy apps which may
  // have been created by "Add to Desktop" in old versions of Chrome.
  // TODO(mgiuca): Delete this feature (https://crbug.com/751029). We are
  // keeping it for now to avoid disrupting existing workflows.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));
  command_line.AppendSwitchASCII(switches::kApp, url.spec());

  auto [browser, infobar_manager] =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  EXPECT_TRUE(browser->is_type_app());

  EXPECT_EQ(HasInfoBar(infobar_manager, flag_type_.infobar_identifier),
            policy_ != CommandLineFlagSecurityWarningsPolicy::kDisabled);
}

// The trybots set the kNoSandbox flag when running browser tests with the
// address sanitizer enabled, which contradicts with the assumption of this test
// that there is no bad flag on the process command line.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CheckInfobarOnlyUsesProcessCommandLine \
  DISABLED_CheckInfobarOnlyUsesProcessCommandLine
#else
#define MAYBE_CheckInfobarOnlyUsesProcessCommandLine \
  CheckInfobarOnlyUsesProcessCommandLine
#endif
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsTest,
                       MAYBE_CheckInfobarOnlyUsesProcessCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  // The flag should not result in an infobar when not set on the process
  // command line via CommandLine::ForCurrentProcess.
  command_line.AppendSwitch(flag_type_.flag);
  auto [browser, infobar_manager] =
      LaunchBrowserAndGetCreatedInfoBarManager(command_line);
  EXPECT_TRUE(browser->is_type_normal());

  EXPECT_FALSE(HasInfoBar(infobar_manager, flag_type_.infobar_identifier));
}

INSTANTIATE_TEST_SUITE_P(
    PolicyControl,
    StartupBrowserCreatorInfobarsTest,
    ::testing::Combine(
        ::testing::Values(
            StartupBrowserCreatorFlagTypeValue{
                switches::kEnableAutomation,
                infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE},
            // Test one of the flags from |bad_flags_prompt.cc|. Any of the
            // flags should have the same behavior.
            StartupBrowserCreatorFlagTypeValue{
                switches::kDisableWebSecurity,
                infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE}),
        ::testing::Values(CommandLineFlagSecurityWarningsPolicy::kNoPolicy,
                          CommandLineFlagSecurityWarningsPolicy::kEnabled,
                          CommandLineFlagSecurityWarningsPolicy::kDisabled)),
    [](const testing::TestParamInfo<
        StartupBrowserCreatorInfobarsTest::ParamType>& info) {
      std::string policyState;
      switch (std::get<1>(info.param)) {
        case CommandLineFlagSecurityWarningsPolicy::kNoPolicy:
          policyState = "no policy";
          break;
        case CommandLineFlagSecurityWarningsPolicy::kEnabled:
          policyState = "policy enabled";
          break;
        case CommandLineFlagSecurityWarningsPolicy::kDisabled:
          policyState = "policy disabled";
          break;
      }

      std::string name = std::get<0>(info.param).flag + " " + policyState;
      std::replace_if(
          name.begin(), name.end(),
          [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
      return name;
    });

// Verifies that infobars are displayed in the first browser window, even when
// the browser is started without an initial browser window by passing the
// `switches::kNoStartupWindow` command line switch.
class StartupBrowserCreatorInfobarsWithoutStartupWindowTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<StartupBrowserCreatorFlagTypeValue> {
 public:
  StartupBrowserCreatorInfobarsWithoutStartupWindowTest()
      : flag_type_(GetParam()) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }

  std::pair<Browser*, infobars::ContentInfoBarManager*>
  LaunchBrowserAndGetCreatedInfoBarManager() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();

    ui_test_utils::BrowserChangeObserver new_browser_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IsFirstRun::kNo);
    launch.Launch(profile, chrome::startup::IsProcessStartup::kNo,
                  /*restore_tabbed_browser=*/true);
    Browser* new_browser = new_browser_observer.Wait();
    if (!new_browser) {
      return std::make_pair(nullptr, nullptr);
    }
    ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);

    return std::make_pair(
        new_browser, infobars::ContentInfoBarManager::FromWebContents(
                         new_browser->tab_strip_model()->GetWebContentsAt(0)));
  }

  const StartupBrowserCreatorFlagTypeValue flag_type_;
};

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorInfobarsWithoutStartupWindowTest,
                       CheckInfobar) {
  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // all read from `CommandLine::ForCurrentProcess` and ignore the command line
  // passed to `StartupBrowserCreator`. In browser tests, this references the
  // browser test's instead of the new process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);

  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  auto [browser, infobar_manager] = LaunchBrowserAndGetCreatedInfoBarManager();
  EXPECT_TRUE(browser);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  ASSERT_TRUE(infobar_manager);
  EXPECT_TRUE(HasInfoBar(infobar_manager, flag_type_.infobar_identifier));

  // Now close and reopen the browser again - and re-check if the infobar is
  // there.
  CloseBrowserSynchronously(browser);

  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  auto [browser2, infobar_manager2] =
      LaunchBrowserAndGetCreatedInfoBarManager();
  EXPECT_TRUE(browser2);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  ASSERT_TRUE(infobar_manager2);
  EXPECT_EQ(flag_type_.is_global_infobar,
            HasInfoBar(infobar_manager2, flag_type_.infobar_identifier));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorInfobarsWithoutStartupWindowTest,
    ::testing::Values(
        StartupBrowserCreatorFlagTypeValue{
            switches::kEnableAutomation,
            infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE, true},
        // Test one of the flags from |bad_flags_prompt.cc|. Any of the
        // flags should have the same behavior.
        StartupBrowserCreatorFlagTypeValue{
            switches::kDisableWebSecurity,
            infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE, false}),
    [](const testing::TestParamInfo<
        StartupBrowserCreatorInfobarsWithoutStartupWindowTest::ParamType>&
           info) {
      std::string name = info.param.flag;
      std::replace_if(
          name.begin(), name.end(),
          [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
      return name;
    });

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies that infobars are not displayed in Kiosk mode.
class StartupBrowserCreatorInfobarsKioskTest : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorInfobarsKioskTest() = default;

 protected:
  infobars::ContentInfoBarManager*
  LaunchKioskBrowserAndGetCreatedInfoBarManager(
      const std::string& extra_switch) {
    Profile* profile = browser()->profile();

    // CommandLine::ForCurrentProcess is used to determine whether kiosk mode is
    // enabled instead of the command-line passed to StartupBrowserCreator. In
    // browser tests, this references the browser test's instead of the new
    // process.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kKioskMode);

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(extra_switch);
    StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                     chrome::startup::IsFirstRun::kNo);
    launch.Launch(profile, chrome::startup::IsProcessStartup::kYes,
                  /*restore_tabbed_browser=*/true);

    // This should have created a new browser window.
    Browser* new_browser = FindOneOtherBrowser(browser());
    EXPECT_TRUE(new_browser);
    if (!new_browser)
      return nullptr;

    return infobars::ContentInfoBarManager::FromWebContents(
        new_browser->tab_strip_model()->GetActiveWebContents());
  }
};

// Verify that the Automation Enabled infobar is still shown in Kiosk mode.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorInfobarsKioskTest,
                       CheckInfobarForEnableAutomation) {
  // CommandLine::ForCurrentProcess is used to determine whether automation is
  // enabled instead of the command-line passed to StartupBrowserCreator. In
  // browser tests, this references the browser test's instead of the new
  // process.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);

  // Passing the kEnableAutomation argument here presently does not do
  // anything because of the aforementioned limitation.
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchKioskBrowserAndGetCreatedInfoBarManager(
          switches::kEnableAutomation);
  ASSERT_TRUE(infobar_manager);

  EXPECT_TRUE(HasInfoBar(
      infobar_manager, infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE));
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
  infobars::ContentInfoBarManager* infobar_manager =
      LaunchKioskBrowserAndGetCreatedInfoBarManager(
          switches::kDisableWebSecurity);
  ASSERT_TRUE(infobar_manager);

  EXPECT_FALSE(HasInfoBar(
      infobar_manager, infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE));
}

// Checks the correct behavior of the profile picker on startup.
class StartupBrowserCreatorPickerTestBase : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorPickerTestBase() {
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
    for (int i = 0; i < 2; ++i) {
      const base::FilePath& profile_path = profile_paths[i];
      profiles::testing::CreateProfileSync(profile_manager, profile_path);
      // Mark newly created profiles as active.
      ProfileAttributesEntry* entry =
          profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path);
      ASSERT_NE(entry, nullptr);
      entry->SetActiveTimeToNow();
      entry->SetAuthInfo(
          base::StringPrintf("gaia_id_%i", i),
          base::UTF8ToUTF16(base::StringPrintf("user%i@gmail.com", i)),
          /*is_consented_primary_account=*/false);
    }
  }
};

struct ProfilePickerSetup {
  enum class ShutdownType {
    kNormal,  // Normal shutdown (e.g. by closing the browser window).
    kExit,    // Exit through the application menu.
    kRestart  // Restart (e.g. after an update).
  };

  bool expected_to_show;
  std::optional<std::string> switch_name;
  std::optional<std::string> switch_value_ascii;
  std::optional<GURL> url_arg;
  ShutdownType shutdown_type = ShutdownType::kNormal;
  std::optional<std::string> extra_switch_name = std::nullopt;
};

// Checks the correct behavior of the profile picker on startup. This feature is
// not available on ChromeOS.
class StartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<ProfilePickerSetup> {
 public:
  StartupBrowserCreatorPickerTest()
      : relaunch_chrome_override_(base::BindRepeating(
            [](const base::CommandLine&) { return true; })) {}
  StartupBrowserCreatorPickerTest(const StartupBrowserCreatorPickerTest&) =
      delete;
  StartupBrowserCreatorPickerTest& operator=(
      const StartupBrowserCreatorPickerTest&) = delete;
  ~StartupBrowserCreatorPickerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);

    if (content::IsPreTest())
      return;  // Don't apply the test parameters to the PRE test.

    if (GetParam().url_arg) {
      command_line->AppendArg(GetParam().url_arg->spec());
    }
    if (GetParam().switch_value_ascii) {
      DCHECK(GetParam().switch_name);
      command_line->AppendSwitchASCII(*GetParam().switch_name,
                                      *GetParam().switch_value_ascii);
    } else if (GetParam().switch_name) {
      command_line->AppendSwitch(*GetParam().switch_name);
    }
    if (GetParam().extra_switch_name) {
      command_line->AppendSwitch(*GetParam().extra_switch_name);
    }
  }

 private:
  // Prevent the browser from automatically relaunching in the PRE_ test. The
  // browser will be relaunched by the main test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride relaunch_chrome_override_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, PRE_TestSetup) {
  CreateMultipleProfiles();

  switch (GetParam().shutdown_type) {
    case ProfilePickerSetup::ShutdownType::kNormal:
      // Need to close the browser window manually so that the real test does
      // not treat it as session restore.
      CloseAllBrowsers();
      break;
    case ProfilePickerSetup::ShutdownType::kExit:
      chrome::AttemptExit();
      break;
    case ProfilePickerSetup::ShutdownType::kRestart:
      chrome::AttemptRestart();
      break;
  }

  ASSERT_EQ(
      g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted),
      GetParam().shutdown_type == ProfilePickerSetup::ShutdownType::kRestart);
}

// Checks that either the ProfilePicker or a browser window is open at startup.
// Except with switches::kNoStartupWindow, for which neither the picker nor a
// browser is open.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerTest, TestSetup) {
  ProfilePickerSetup setup_param = GetParam();

  // Check the ProfilePicker.
  if (setup_param.expected_to_show) {
    if (!ProfilePicker::IsOpen()) {
      base::RunLoop run_loop;
      ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
          run_loop.QuitClosure());
      run_loop.Run();
    }
    EXPECT_TRUE(ProfilePicker::IsOpen());
  } else {
    EXPECT_FALSE(ProfilePicker::IsOpen());
  }

  // Check the browser window.
  if (setup_param.expected_to_show ||
      setup_param.switch_name == switches::kNoStartupWindow) {
    EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  } else {
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  }

  // No Guest profile was created.
  for (const Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    EXPECT_FALSE(profile->IsGuestSession());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorPickerTest,
    ::testing::Values(
// Flaky: https://crbug.com/1126886
#if !BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_WIN)
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
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kNoStartupWindow},
        // Skip the picker when a specific profile is requested (used e.g. by
        // profile specific desktop shortcuts on Win).
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kProfileDirectory,
                           /*switch_value_ascii=*/"Default"},
        // Same, but with the kIgnoreProfileDirectoryIfNotExists flag with the
        // profile existing.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/switches::kProfileDirectory,
            /*switch_value_ascii=*/"Default",
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kNormal,
            /*extra_switch_name=*/
            switches::kIgnoreProfileDirectoryIfNotExists},
        // Show the picker if the profile is ignored due to it not existing.
        ProfilePickerSetup{
            /*expected_to_show=*/true,
            /*switch_name=*/switches::kProfileDirectory,
            /*switch_value_ascii=*/"DoesNotExist",
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kNormal,
            /*extra_switch_name=*/switches::kIgnoreProfileDirectoryIfNotExists},
        // Skip the picker when a specific profile is requested by email.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/switches::kProfileEmail,
                           /*switch_value_ascii=*/"user0@gmail.com"},
        // Show the picker if the profile email is not found.
        ProfilePickerSetup{/*expected_to_show=*/true,
                           /*switch_name=*/switches::kProfileEmail,
                           /*switch_value_ascii=*/"unknown@gmail.com"},
        // Skip the picker when a URL is provided on command-line (used by the
        // OS when Chrome is the default web browser) and use the last used
        // profile, instead.
        ProfilePickerSetup{/*expected_to_show=*/false,
                           /*switch_name=*/std::nullopt,
                           /*switch_value_ascii=*/std::nullopt,
                           /*url_arg=*/GURL("https://www.foo.com/")},
        // Regression test for http://crbug.com/1166192
        // Picker should be shown after exit.
        ProfilePickerSetup{
            /*expected_to_show=*/true,
            /*switch_name=*/std::nullopt,
            /*switch_value_ascii=*/std::nullopt,
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kExit},
        // Regression test for http://crbug.com/1245374
        // Picker should not be shown after restart.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/std::nullopt,
            /*switch_value_ascii=*/std::nullopt,
            /*url_arg=*/std::nullopt,
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kRestart},
        // Skip the picker when a url is requested and the profile is ignored.
        ProfilePickerSetup{
            /*expected_to_show=*/false,
            /*switch_name=*/switches::kProfileDirectory,
            /*switch_value_ascii=*/"DoesNotExist",
            /*url_arg=*/GURL("https://www.foo.com/"),
            /*shutdown_type=*/ProfilePickerSetup::ShutdownType::kNormal,
            /*extra_switch_name=*/
            switches::kIgnoreProfileDirectoryIfNotExists}));

class GuestStartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  GuestStartupBrowserCreatorPickerTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kGuest);
  }
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(GuestStartupBrowserCreatorPickerTest,
                       PRE_SkipsPickerWithGuest) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(GuestStartupBrowserCreatorPickerTest,
                       SkipsPickerWithGuest) {
  // The picker is skipped which means a browser window is opened on startup.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(browser()->profile()->IsGuestSession());
}

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
  profiles::testing::WaitForPickerWidgetCreated();
  ASSERT_EQ(0u, chrome::GetTotalBrowserCount());

  // Close the picker.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ProfilePicker::Hide();
  profiles::testing::WaitForPickerClosed();
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // Simulate a second start when the browser is already running.
  base::FilePath current_dir = base::FilePath();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupProfilePathInfo startup_profile_path_info =
      GetStartupProfilePath(current_dir, command_line,
                            /*ignore_profile_picker=*/false);
  EXPECT_EQ(startup_profile_path_info.reason,
            StartupProfileModeReason::kMultipleProfiles);
  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_dir, startup_profile_path_info);

  // The picker is shown again if no profile was previously opened.
  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
}

class SearchQueryStartupBrowserCreatorPickerTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  SearchQueryStartupBrowserCreatorPickerTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendArg("? Foo");
  }
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(SearchQueryStartupBrowserCreatorPickerTest,
                       PRE_SkipsPickerWithCommandLineSearchQuery) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(SearchQueryStartupBrowserCreatorPickerTest,
                       SkipsPickerWithCommandLineSearchQuery) {
  // A browser window is shown on start-up because the command line contains a
  // search query.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Check the return value of `GetStartupProfilePath()` explicitly.
  base::FilePath current_dir = base::FilePath();
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendArg("? Foo");
  StartupProfilePathInfo startup_profile_path_info =
      GetStartupProfilePath(current_dir, command_line,
                            /*ignore_profile_picker=*/false);
  EXPECT_EQ(startup_profile_path_info.reason,
            StartupProfileModeReason::kCommandLineTabs);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS)

class StartupBrowserCreatorPickerInfobarTest
    : public StartupBrowserCreatorPickerTestBase,
      public ::testing::WithParamInterface<StartupBrowserCreatorFlagTypeValue> {
 public:
  StartupBrowserCreatorPickerInfobarTest() : flag_type_(GetParam()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {}

 protected:
  // Simulates a click on a profile card. The profile picker must be already
  // opened.
  void OpenProfileFromPicker(const base::FilePath& profile_path,
                             bool open_settings) {
    base::Value::List args;
    args.Append(base::FilePathToValue(profile_path));
    profile_picker_handler()->HandleLaunchSelectedProfile(open_settings, args);
  }

  // Returns the profile picker webUI handler. The profile picker must be opened
  // before calling this function.
  ProfilePickerHandler* profile_picker_handler() {
    DCHECK(ProfilePicker::IsOpen());

    views::WebView* web_view = ProfilePicker::GetWebViewForTesting();
    if (web_view == nullptr) {
      return nullptr;
    }

    return web_view->GetWebContents()
        ->GetWebUI()
        ->GetController()
        ->GetAs<ProfilePickerUI>()
        ->GetProfilePickerHandlerForTesting();
  }

  const StartupBrowserCreatorFlagTypeValue flag_type_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerInfobarTest,
                       PRE_ShowsEnableAutomationInfobar) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_P(StartupBrowserCreatorPickerInfobarTest,
                       ShowsEnableAutomationInfobar) {
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // We deliberately set the flag on the process command line instead of on the
  // command_line passed to the StartupBrowserCreator, because these flags are
  // always read from the command line of the current process
  base::CommandLine::ForCurrentProcess()->AppendSwitch(flag_type_.flag);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = nullptr;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile = profile_manager->GetLastUsedProfile();
  }

  ui_test_utils::BrowserChangeObserver new_browser_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  OpenProfileFromPicker(profile->GetPath(), false);
  Browser* new_browser = new_browser_observer.Wait();
  ui_test_utils::WaitUntilBrowserBecomeActive(new_browser);
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(
          new_browser->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_TRUE(HasInfoBar(infobar_manager, flag_type_.infobar_identifier));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    StartupBrowserCreatorPickerInfobarTest,
    ::testing::Values(
        StartupBrowserCreatorFlagTypeValue{
            switches::kEnableAutomation,
            infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE},
        // Test one of the flags from |bad_flags_prompt.cc|. Any of the
        // flags should have the same behavior.
        StartupBrowserCreatorFlagTypeValue{
            switches::kDisableWebSecurity,
            infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE}),
    [](const testing::TestParamInfo<
        StartupBrowserCreatorPickerInfobarTest::ParamType>& info) {
      std::string name = info.param.flag;
      std::replace_if(
          name.begin(), name.end(),
          [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
      return name;
    });

// TODO(crbug.com/40265712): Mocking the logger appears to not work correctly on
// Windows. Investigate why it is not working and enable the test on Windows.
#if !BUILDFLAG(IS_WIN)
class StartupBrowserCreatorIwaCommandLineInstallProfilePickerErrorTest
    : public StartupBrowserCreatorPickerTestBase {
 protected:
  void SetUp() override {
    if (!content::IsPreTest()) {
      EXPECT_CALL(mock_log_, Log(testing::_, testing::_, testing::_, testing::_,
                                 testing::_))
          .Times(testing::AnyNumber());
      EXPECT_CALL(
          mock_log_,
          Log(::logging::LOGGING_ERROR, testing::_, testing::_, testing::_,
              testing::HasSubstr("Command line switches to install IWAs are "
                                 "incompatible with the Profile Picker")));
      mock_log_.StartCapturingLogs();
    }

    StartupBrowserCreatorPickerTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (!content::IsPreTest()) {
      command_line->AppendSwitchASCII("install-isolated-web-app-from-url",
                                      "http://localhost");
    }

    StartupBrowserCreatorPickerTestBase::SetUpCommandLine(command_line);
  }

  base::test::MockLog mock_log_;
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(
    StartupBrowserCreatorIwaCommandLineInstallProfilePickerErrorTest,
    PRE_DoesNotInstallIwaIfProfilePickerOpens) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(
    StartupBrowserCreatorIwaCommandLineInstallProfilePickerErrorTest,
    DoesNotInstallIwaIfProfilePickerOpens) {
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  // The `EXPECT_CALL` call in `SetUp()` will check that an error message about
  // the IWA not being installable is logged.
}
#endif  // !BUILDFLAG(IS_WIN)

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class StartupBrowserCreatorLacrosNoWindowTest
    : public StartupBrowserCreatorPickerTestBase {
 public:
  StartupBrowserCreatorLacrosNoWindowTest() = default;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    if (!content::IsPreTest()) {
      crosapi::mojom::BrowserInitParamsPtr init_params =
          crosapi::mojom::BrowserInitParams::New();
      init_params->initial_browser_action =
          crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow;
      chromeos::BrowserInitParams::SetInitParamsForTests(
          std::move(init_params));
    }

    StartupBrowserCreatorPickerTestBase::CreatedBrowserMainParts(
        browser_main_parts);
  }

  base::FilePath GetDefaultProfileDir() const {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    return profile_manager->user_data_dir().Append(
        profile_manager->GetInitialProfileDir());
  }
};

// Create a secondary profile in a separate PRE run because the existence of
// profiles is checked during startup in the actual test.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorLacrosNoWindowTest,
                       PRE_MultiProfile) {
  CreateMultipleProfiles();
  // Need to close the browser window manually so that the real test does not
  // treat it as session restore.
  CloseAllBrowsers();
}

// Checks that no picker and no browser window are open when there are multiple
// profiles.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorLacrosNoWindowTest, MultiProfile) {
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
}

// Checks that no picker and no browser window are open when there is a single
// profile.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorLacrosNoWindowTest, SingleProfile) {
  EXPECT_FALSE(ProfilePicker::IsOpen());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  // Checks that it's possible to open a profile after startup.
  // Regression test for https://crbug.com/1278549
  base::test::TestFuture<Browser*> future;
  profiles::SwitchToProfile(GetDefaultProfileDir(),
                            /*always_create=*/false, future.GetCallback());
  Profile* profile = future.Get()->profile();
  EXPECT_NE(profile, nullptr);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Checks that it's possible to open the profile picker.
  EXPECT_FALSE(ProfilePicker::IsOpen());
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuManageProfiles));
  profiles::testing::WaitForPickerWidgetCreated();
}

class StartupBrowserCreatorLacrosGuestSessionTest
    : public InProcessBrowserTest {
 public:
  StartupBrowserCreatorLacrosGuestSessionTest() = default;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = crosapi::mojom::SessionType::kGuestSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));

    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }
};

// Checks that a browser window with new tab is open in Guest session.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorLacrosGuestSessionTest, Startup) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(::switches::kIncognito);
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line,
                                   chrome::startup::IsFirstRun::kNo);
  launch.Launch(browser()->profile(), chrome::startup::IsProcessStartup::kYes,
                /*restore_tabbed_browser=*/true);

  // A new browser window should be open.
  Browser* new_browser = FindOneOtherBrowser(browser());
  EXPECT_TRUE(new_browser);
  EXPECT_TRUE(new_browser->profile()->IsGuestSession());

  // The new browser should have exactly one tab (new tab url).
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_TRUE(tab_strip);
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(
      chrome::kChromeUINewTabURL,
      tab_strip->GetWebContentsAt(0)->GetVisibleURL().possibly_invalid_spec());
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
