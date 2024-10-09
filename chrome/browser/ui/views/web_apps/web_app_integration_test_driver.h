// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_TEST_DRIVER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_TEST_DRIVER_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"
#endif

class Browser;
class PageActionIconView;

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class TestWebUI;
}

namespace web_app::integration_tests {

// Enumerations used by the integration tests framework actions. These are C++
// versions of the enumerations in the file chrome/test/webapps/data/enums.md.

enum class Site : int {
  kStandalone,
  kStandaloneNestedA,
  kStandaloneNestedB,
  kStandaloneNotStartUrl,
  kMinimalUi,
  kTabbed,
  kTabbedWithHomeTab,
  kTabbedNestedA,
  kTabbedNestedB,
  kTabbedNestedC,
  kNotPromotable,
  kWco,
  kFileHandler,
  kNoServiceWorker,
  kNotInstalled,
  kScreenshots,
  kHasSubApps,
  kSubApp1,
  kSubApp2,
  kChromeUrl,
};

enum class InstallableSite {
  kStandalone,
  kStandaloneNestedA,
  kStandaloneNestedB,
  kStandaloneNotStartUrl,
  kMinimalUi,
  kTabbed,
  kTabbedWithHomeTab,
  kWco,
  kFileHandler,
  kNoServiceWorker,
  kNotInstalled,
  kScreenshots,
  kChromeUrl,
};

enum class Title { kStandaloneOriginal, kStandaloneUpdated };

enum class Color { kRed, kGreen };

enum class ProfileClient { kClient2, kClient1 };

enum class ProfileName { kDefault, kProfile2 };

enum class UserDisplayPreference { kStandalone, kBrowser };

enum class IsShown { kShown, kNotShown };

enum class IsOn { kOn, kOff };

enum class Display { kBrowser, kStandalone, kMinimalUi, kTabbed, kWco };

enum class WindowOptions { kWindowed, kBrowser };

enum class ShortcutOptions { kWithShortcut, kNoShortcut };

enum class InstallMode { kWebApp, kWebShortcut };

enum class AllowDenyOptions { kAllow, kDeny };

enum class AskAgainOptions { kAskAgain, kRemember };

enum class FileExtension { kFoo, kBar };

enum class Number { kOne, kTwo };

enum class FilesOptions {
  kOneFooFile,
  kMultipleFooFiles,
  kOneBarFile,
  kMultipleBarFiles,
  kAllFooAndBarFiles
};

enum class UpdateDialogResponse {
  kAcceptUpdate,
  kCancelDialogAndUninstall,
  kCancelUninstallAndAcceptUpdate,
  kSkipDialog
};

enum class SubAppInstallDialogOptions {
  kUserAllow,
  kUserDeny,
  kPolicyOverride
};

enum class AppShimCorruption { kNoExecutable, kIncompatibleVersion };

// These structs are used to store the current state of the world before & after
// each state-change action.

struct TabState {
  explicit TabState(GURL tab_url) : url(std::move(tab_url)) {}
  TabState(const TabState&) = default;
  TabState& operator=(const TabState&) = default;
  bool operator==(const TabState& other) const { return url == other.url; }

  GURL url;
};

struct BrowserState {
  BrowserState(Browser* browser_ptr,
               base::flat_map<content::WebContents*, TabState> tab_state,
               content::WebContents* active_web_contents,
               const webapps::AppId& app_id,
               bool launch_icon_visible);
  ~BrowserState();
  BrowserState(const BrowserState&);
  bool operator==(const BrowserState& other) const;

  raw_ptr<Browser, DanglingUntriaged> browser;
  base::flat_map<content::WebContents*, TabState> tabs;
  raw_ptr<content::WebContents, DanglingUntriaged> active_tab;
  // If this isn't an app browser, `app_id` is empty.
  webapps::AppId app_id;
  bool launch_icon_shown;
};

struct AppState {
  AppState(webapps::AppId app_id,
           std::string app_name,
           GURL app_scope,
           apps::RunOnOsLoginMode run_on_os_login_mode,
           blink::mojom::DisplayMode effective_display_mode,
           std::optional<mojom::UserDisplayMode> user_display_mode,
           std::string manifest_launcher_icon_filename,
           bool is_installed_locally,
           bool is_shortcut_created);
  ~AppState();
  AppState(const AppState&);
  bool operator==(const AppState& other) const;

  webapps::AppId id;
  std::string name;
  GURL scope;
  apps::RunOnOsLoginMode run_on_os_login_mode;
  blink::mojom::DisplayMode effective_display_mode;
  std::optional<mojom::UserDisplayMode> user_display_mode;
  std::string manifest_launcher_icon_filename;
  bool is_installed_locally;
  bool is_shortcut_created;
};

struct ProfileState {
  ProfileState(base::flat_map<Browser*, BrowserState> browser_state,
               base::flat_map<webapps::AppId, AppState> app_state);
  ~ProfileState();
  ProfileState(const ProfileState&);
  bool operator==(const ProfileState& other) const;

  base::flat_map<Browser*, BrowserState> browsers;
  base::flat_map<webapps::AppId, AppState> apps;
};

struct StateSnapshot {
  explicit StateSnapshot(base::flat_map<Profile*, ProfileState> profile_state);
  ~StateSnapshot();
  StateSnapshot(const StateSnapshot&);
  bool operator==(const StateSnapshot& other) const;

  base::flat_map<Profile*, ProfileState> profiles;
};
std::ostream& operator<<(std::ostream& os, const StateSnapshot& snapshot);

class WebAppIntegrationTestDriver : WebAppInstallManagerObserver {
 public:
  class TestDelegate {
   public:
    // Exposing normal functionality of testing::InProcBrowserTest:
    virtual Browser* CreateBrowser(Profile* profile) = 0;
    virtual void CloseBrowserSynchronously(Browser* browser) = 0;
    virtual void AddBlankTabAndShow(Browser* browser) = 0;
    virtual const net::EmbeddedTestServer* EmbeddedTestServer() const = 0;
    virtual Profile* GetDefaultProfile() = 0;

    // Functionality specific to web app integration test type (e.g. sync or
    // non-sync tests).
    virtual bool IsSyncTest() = 0;
    virtual void SyncTurnOff() = 0;
    virtual void SyncTurnOn() = 0;
    virtual void SyncSignOut(Profile*) = 0;
    virtual void SyncSignIn(Profile*) = 0;
    virtual void AwaitWebAppQuiescence() = 0;
    virtual Profile* GetProfileClient(ProfileClient client) = 0;
  };

  explicit WebAppIntegrationTestDriver(TestDelegate* delegate);
  ~WebAppIntegrationTestDriver() override;

  // These functions are expected to be called by any test fixtures that use
  // this helper.
  void SetUp();
  void SetUpOnMainThread();
  void TearDownOnMainThread();

  // Automated Testing Actions
  //
  // Actions are defined in chrome/test/webapps/data/actions.md

  // State change actions:
  void HandleAppIdentityUpdateDialogResponse(UpdateDialogResponse response);
  void AwaitManifestUpdate(Site site_mode);
  void CloseCustomToolbar();
  void ClosePwa();
  void MaybeClosePwa();
  void DisableRunOnOsLoginFromAppSettings(Site site);
  void DisableRunOnOsLoginFromAppHome(Site site);
  void EnableRunOnOsLoginFromAppSettings(Site site);
  void EnableRunOnOsLoginFromAppHome(Site site);
  void EnterFullScreenApp();
  void ExitFullScreenApp();
  void DisableFileHandling(Site site);
  void EnableFileHandling(Site site);
  void DisableWindowControlsOverlay(Site site);
  void EnableWindowControlsOverlay(Site site);
  void CreateShortcut(Site site, WindowOptions window_options);
  // TODO(crbug.com/346323629): Remove InstallableSite and convert callsites to
  // Site since universal install is available now.
  void InstallMenuOption(InstallableSite site);
  void InstallLocally(Site site);
  void InstallOmniboxIcon(InstallableSite site);
  void InstallPolicyApp(Site site,
                        ShortcutOptions shortcut,
                        WindowOptions window,
                        InstallMode mode);
  // TODO(b/240449120): Standardize behavior to install preinstalled apps when
  // CUJs for that are added.
  void InstallPreinstalledApp(Site site);
  void InstallIsolatedApp(Site site);
  void InstallSubApp(Site parent_app,
                     Site sub_app,
                     SubAppInstallDialogOptions option);
  void RemoveSubApp(Site parent_app, Site sub_app);
  // These functions install apps which are tabbed and creates shortcuts.
  void ApplyRunOnOsLoginPolicyAllowed(Site site);
  void ApplyRunOnOsLoginPolicyBlocked(Site site);
  void ApplyRunOnOsLoginPolicyRunWindowed(Site site);
  void DeletePlatformShortcut(Site site);
  void RemoveRunOnOsLoginPolicy(Site site);
  void LaunchFileExpectDialog(Site site,
                              FilesOptions files_options,
                              AllowDenyOptions allow_deny,
                              AskAgainOptions ask_again);
  void LaunchFileExpectNoDialog(Site site, FilesOptions files_options);
  void LaunchFromChromeApps(Site site);
  void LaunchFromLaunchIcon(Site site);
  void LaunchFromMenuOption(Site site);
  void LaunchFromPlatformShortcut(Site site);
#if BUILDFLAG(IS_MAC)
  void LaunchFromAppShimFallback(Site site);
#endif
  void OpenAppSettingsFromChromeApps(Site site);
  void OpenAppSettingsFromAppMenu(Site site);
  void OpenAppSettingsFromCommand(Site site);
  void CreateShortcutsFromList(Site site);
  void NavigateAppHome();
  void NavigateBrowser(Site site);
  void NavigatePwa(Site app, Site to);
  void NavigateNotfoundUrl();
  void NewAppTab(Site site);
  void ManifestUpdateIcon(Site site, UpdateDialogResponse response);
  void ManifestUpdateTitle(Site site,
                           Title title,
                           UpdateDialogResponse response);
  void ManifestUpdateDisplay(Site site, Display display);
  void ManifestUpdateScopeTo(Site app, Site scope);
  void OpenInChrome();
  void SetOpenInTabFromAppHome(Site site);
  void SetOpenInTabFromAppSettings(Site site);
  void SetOpenInWindowFromAppHome(Site site);
  void SetOpenInWindowFromAppSettings(Site site);
  void SwitchIncognitoProfile();
  void SwitchProfileClients(ProfileClient client);
  void SwitchActiveProfile(ProfileName profile_name);
  void SyncTurnOff();
  void SyncTurnOn();
  void SyncSignOut();
  void SyncSignIn();
  void UninstallFromList(Site site);
  void UninstallFromMenu(Site site);
  void UninstallFromAppSettings(Site site);
  void UninstallPolicyApp(Site site);
  void UninstallFromOs(Site site);
#if BUILDFLAG(IS_MAC)
  void CorruptAppShim(Site site, AppShimCorruption corruption);
  void QuitAppShim(Site site);
#endif

  // State Check Actions:
  void CheckAppListEmpty();
  void CheckAppInListIconCorrect(Site site);
  void CheckAppInListNotLocallyInstalled(Site site);
  void CheckAppInListWindowed(Site site);
  void CheckAppInListTabbed(Site site);
  void CheckAppNavigation(Site site);
  void CheckAppNavigationIsStartUrl();
  void CheckAppTabIsSite(Site site, Number number);
  void CheckAppTabCreated();
  void CheckBrowserNavigation(Site site);
  void CheckBrowserNavigationIsAppSettings(Site site);
  void CheckBrowserNotAtAppHome();
  void CheckAppNotInList(Site site);
  void CheckAppIcon(Site site, Color color);
  void CheckAppTitle(Site site, Title title);
  void CheckCreateShortcutNotShown();
  void CheckCreateShortcutShown();
  void CheckWindowModeIsNotVisibleInAppSettings(Site site);
  void CheckFilesLoadedInSite(Site site, FilesOptions files_options);
  void CheckInstallIconShown();
  void CheckInstallIconNotShown();
  void CheckLaunchIconShown();
  void CheckLaunchIconNotShown();
  void CheckTabCreated(Number number);
  void CheckTabNotCreated();
  void CheckCustomToolbar();
  void CheckNoToolbar();
  void CheckPlatformShortcutAndIcon(Site site);
  void CheckPlatformShortcutNotExists(Site site);
  void CheckRunOnOsLoginEnabled(Site site);
  void CheckRunOnOsLoginDisabled(Site site);
  void CheckSiteHandlesFile(Site site, FileExtension file_extension);
  void CheckSiteNotHandlesFile(Site site, FileExtension file_extension);
  void CheckUserCannotSetRunOnOsLoginAppSettings(Site site);
  void CheckUserCannotSetRunOnOsLoginAppHome(Site site);
  void CheckUserDisplayModeInternal(mojom::UserDisplayMode user_display_mode);
  void CheckWindowClosed();
  void CheckWindowCreated();
  void CheckPwaWindowCreated(Site site, Number number);
  void CheckPwaWindowCreatedInProfile(Site site,
                                      Number number,
                                      ProfileName profile_name);
  void CheckWindowNotCreated();
  void CheckWindowControlsOverlay(Site site, IsOn is_on);
  void CheckWindowControlsOverlayToggle(Site site, IsShown is_shown);
  void CheckWindowControlsOverlayToggleIcon(IsShown is_shown);
  void CheckWindowDisplayBrowser();
  void CheckWindowDisplayMinimal();
  void CheckWindowDisplayTabbed();
  void CheckWindowDisplayStandalone();
  void CheckNotHasSubApp(Site parent_app, Site sub_app);
  void CheckHasSubApp(Site parent_app, Site sub_app);
  void CheckNoSubApps(Site parent_app);
  void CheckAppLoadedInTab(Site site);

 protected:
  // WebAppInstallManagerObserver:
  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;

 private:
  // Must be called at the beginning of every state change action function.
  // Returns if the test should continue.
  [[nodiscard]] bool BeforeStateChangeAction(const char* function);
  // Must be called at the end of every state change action function.
  void AfterStateChangeAction();
  // Must be called at the beginning of every state check action function.
  // Returns if the test should continue.
  [[nodiscard]] bool BeforeStateCheckAction(const char* function);
  // Must be called at the end of every state check action function.
  void AfterStateCheckAction();

  void AwaitManifestSystemIdle();

  webapps::AppId GetAppIdBySiteMode(Site site);
  GURL GetUrlForSite(Site site, const std::string& suffix = "");
  std::optional<AppState> GetAppBySiteMode(StateSnapshot* state_snapshot,
                                           Profile* profile,
                                           Site site);

  WebAppProvider* GetProviderForProfile(Profile* profile);

  std::unique_ptr<StateSnapshot> ConstructStateSnapshot();

  Profile* GetOrCreateProfile(ProfileName profile_name);

  content::WebContents* GetCurrentTab(Browser* browser);
  GURL GetInScopeURL(Site site);
  base::FilePath GetShortcutPath(base::FilePath shortcut_dir,
                                 const std::string& app_name,
                                 const webapps::AppId& app_id);
  void InstallPolicyAppInternal(Site site,
                                base::Value default_launch_container,
                                const bool create_shortcut,
                                const bool install_as_shortcut);
  void ApplyRunOnOsLoginPolicy(Site site, const char* policy);

  void UninstallPolicyAppById(Profile* profile, const webapps::AppId& id);
  void ForceUpdateManifestContents(Site site,
                                   const GURL& app_url_with_manifest_param);
  void MaybeNavigateTabbedBrowserInScope(Site site);

  enum class NavigationMode { kNewTab, kCurrentTab };
  void NavigateTabbedBrowserToSite(const GURL& url, NavigationMode mode);

  // Returns an existing app browser if one exists, or launches a new one if
  // not.
  Browser* GetAppBrowserForSite(Site site, bool launch_if_not_open = true);

  bool IsShortcutAndIconCreated(Profile* profile,
                                const std::string& name,
                                const webapps::AppId& id);

  bool DoIconColorsMatch(Profile* profile,
                         const std::string& name,
                         const webapps::AppId& id);

  bool IsFileHandledBySite(Site site, FileExtension file_extension);
  void SetFileHandlingEnabled(Site site, bool enabled);
  void LaunchFile(Site site, FilesOptions files_options);

  void LaunchAppStartupBrowserCreator(const webapps::AppId& app_id);
#if BUILDFLAG(IS_MAC)
  bool LaunchFromAppShim(Site site,
                         const std::vector<GURL>& urls,
                         bool wait_for_complete_launch);
#endif

  void CheckAppSettingsAppState(Profile* profile, const AppState& app_state);

  void CheckPwaWindowCreatedImpl(Profile* profile, Site site, Number number);

  base::FilePath GetResourceFile(base::FilePath::StringPieceType relative_path);

  std::vector<base::FilePath> GetTestFilePaths(FilesOptions file_options);

  void SyncAndInstallPreinstalledAppConfig(const GURL& install_url,
                                           std::string_view app_config_string);

  Browser* browser();
  Profile* profile();
  std::vector<Profile*> GetAllProfiles();

  Browser* app_browser() { return app_browser_; }
  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
  PageActionIconView* pwa_install_view();
  IntentChipButton* intent_chip_view();

  const net::EmbeddedTestServer& GetTestServerForSiteMode(Site site_mode) const;

#if !BUILDFLAG(IS_CHROMEOS)
  webapps::AppHomePageHandler GetTestAppHomePageHandler(
      content::TestWebUI* web_ui);
#endif

  base::ScopedTempDir scoped_temp_dir_;

  base::flat_set<webapps::AppId> previous_manifest_updates_;

  // |waiting_for_update_*| variables are either all populated or all not
  // populated. These signify that the test is currently waiting for the
  // given |waiting_for_update_id_| to receive an update before continuing.
  std::optional<webapps::AppId> waiting_for_update_id_;
  std::unique_ptr<base::RunLoop> waiting_for_update_run_loop_;

  raw_ptr<TestDelegate> delegate_;
  // State snapshots, captured before and after "state change" actions are
  // executed, and inspected by "state check" actions to verify behavior.
  std::unique_ptr<StateSnapshot> before_state_change_action_state_;
  std::unique_ptr<StateSnapshot> after_state_change_action_state_;
  // Keeps track if we are currently executing an action. Updated in the
  // `BeforeState*Action()` and `AfterState*Action()` methods, and used by the
  // manifest update logic to ensure that the action states are appropriately
  // kept up to date.
  // The number represents the level of nested actions we are in (as an action
  // can often call another action).
  int executing_action_level_ = 0;

  raw_ptr<Profile, AcrossTasksDanglingUntriaged> active_profile_ = nullptr;
  webapps::AppId active_app_id_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> app_browser_ = nullptr;

  // Normally BeforeState*Action returns false if a fatal error has been
  // reported in a previous action, to avoid actions operating on potentially
  // invalid state. If we're in tear down though, we always want to execute
  // all actions.
  bool in_tear_down_ = false;

  bool is_performing_manifest_update_ = false;

  std::unique_ptr<views::NamedWidgetShownWaiter> app_id_update_dialog_waiter_;
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      observation_{this};
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;

  std::unique_ptr<base::RunLoop> window_controls_overlay_callback_for_testing_ =
      nullptr;

  base::flat_set<Site> site_remember_deny_open_file_;
  base::AutoReset<std::optional<web_app::AppIdentityUpdate>>
      update_dialog_scope_;

  base::ScopedClosureRunner valid_chrome_url_for_webapps_registration_;

  base::TimeTicks start_time_ = base::TimeTicks::Now();
};

// Simple base browsertest class usable by all non-sync web app integration
// tests.
class WebAppIntegrationTest : public InProcessBrowserTest,
                              public WebAppIntegrationTestDriver::TestDelegate {
 public:
  WebAppIntegrationTest();
  ~WebAppIntegrationTest() override;

  // InProcessBrowserTest:
  void SetUp() override;

  // BrowserTestBase:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  // WebAppIntegrationTestDriver::TestDelegate:
  Browser* CreateBrowser(Profile* profile) override;
  void CloseBrowserSynchronously(Browser* browser) override;
  void AddBlankTabAndShow(Browser* browser) override;
  const net::EmbeddedTestServer* EmbeddedTestServer() const override;
  Profile* GetDefaultProfile() override;

  bool IsSyncTest() override;
  void SyncTurnOff() override;
  void SyncTurnOn() override;
  void SyncSignOut(Profile*) override;
  void SyncSignIn(Profile*) override;
  void AwaitWebAppQuiescence() override;
  Profile* GetProfileClient(ProfileClient client) override;

 protected:
  WebAppIntegrationTestDriver helper_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace web_app::integration_tests

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_TEST_DRIVER_H_
