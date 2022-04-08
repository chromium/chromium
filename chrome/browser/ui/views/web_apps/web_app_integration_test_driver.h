// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_TEST_DRIVER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_TEST_DRIVER_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class Browser;
class PageActionIconView;

namespace base {
class CommandLine;
}  // namespace base

namespace web_app {

struct TabState {
  TabState(GURL tab_url, bool is_tab_installable)
      : url(std::move(tab_url)), is_installable(is_tab_installable) {}
  TabState(const TabState&) = default;
  TabState& operator=(const TabState&) = default;
  bool operator==(const TabState& other) const {
    return url == other.url && is_installable == other.is_installable;
  }

  GURL url;
  bool is_installable;
};

struct BrowserState {
  BrowserState(Browser* browser_ptr,
               base::flat_map<content::WebContents*, TabState> tab_state,
               content::WebContents* active_web_contents,
               const AppId& app_id,
               bool install_icon_visible,
               bool launch_icon_visible);
  ~BrowserState();
  BrowserState(const BrowserState&);
  bool operator==(const BrowserState& other) const;

  Browser* browser;
  base::flat_map<content::WebContents*, TabState> tabs;
  content::WebContents* active_tab;
  // If this isn't an app browser, `app_id` is empty.
  AppId app_id;
  bool install_icon_shown;
  bool launch_icon_shown;
};

struct AppState {
  AppState(AppId app_id,
           std::string app_name,
           GURL app_scope,
           apps::WindowMode window_mode,
           apps::RunOnOsLoginMode run_on_os_login_mode,
           blink::mojom::DisplayMode effective_display_mode,
           blink::mojom::DisplayMode user_display_mode,
           std::string manifest_launcher_icon_filename,
           bool is_installed_locally,
           bool is_shortcut_created);
  ~AppState();
  AppState(const AppState&);
  bool operator==(const AppState& other) const;

  AppId id;
  std::string name;
  GURL scope;
  apps::WindowMode window_mode;
  apps::RunOnOsLoginMode run_on_os_login_mode;
  blink::mojom::DisplayMode effective_display_mode;
  blink::mojom::DisplayMode user_display_mode;
  std::string manifest_launcher_icon_filename;
  bool is_installed_locally;
  bool is_shortcut_created;
};

struct ProfileState {
  ProfileState(base::flat_map<Browser*, BrowserState> browser_state,
               base::flat_map<AppId, AppState> app_state);
  ~ProfileState();
  ProfileState(const ProfileState&);
  bool operator==(const ProfileState& other) const;

  base::flat_map<Browser*, BrowserState> browsers;
  base::flat_map<AppId, AppState> apps;
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
  struct TestDelegate {
    // Exposing normal functionality of testing::InProcBrowserTest:
    virtual Browser* CreateBrowser(Profile* profile) = 0;
    virtual void AddBlankTabAndShow(Browser* browser) = 0;
    virtual net::EmbeddedTestServer* EmbeddedTestServer() = 0;
    virtual std::vector<Profile*> GetAllProfiles() = 0;

    // Functionality specific to web app integration test type (e.g. sync or
    // non-sync tests).
    virtual bool IsSyncTest() = 0;
    virtual void SyncTurnOff() = 0;
    virtual void SyncTurnOn() = 0;
    virtual void AwaitWebAppQuiescence() = 0;
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
  // Actions are defined in the following spreadsheet:
  // https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit#gid=2008870403

  // State change actions:
  void AcceptAppIdUpdateDialog();
  void CloseCustomToolbar();
  void ClosePwa();
  void DisableRunOnOsLogin(const std::string& site_mode);
  void EnableRunOnOsLogin(const std::string& site_mode);
  void DisableWindowControlsOverlay(const std::string& site_mode);
  void EnableWindowControlsOverlay(const std::string& site_mode);
  void InstallCreateShortcutTabbed(const std::string& site_mode);
  void InstallCreateShortcutWindowed(const std::string& site_mode);
  void InstallMenuOption(const std::string& site_mode);
  void InstallLocally(const std::string& site_mode);
  void InstallOmniboxIcon(const std::string& site_mode);
  void InstallPolicyAppTabbedNoShortcut(const std::string& site_mode);
  void InstallPolicyAppTabbedShortcut(const std::string& site_mode);
  void InstallPolicyAppWindowedNoShortcut(const std::string& site_mode);
  void InstallPolicyAppWindowedShortcut(const std::string& site_mode);
  // These functions install apps which are tabbed and creates shortcuts.
  void ApplyRunOnOsLoginPolicyAllowed(const std::string& site_mode);
  void ApplyRunOnOsLoginPolicyBlocked(const std::string& site_mode);
  void ApplyRunOnOsLoginPolicyRunWindowed(const std::string& site_mode);
  void RemoveRunOnOsLoginPolicy(const std::string& site_mode);
  void LaunchFromChromeApps(const std::string& site_mode);
  void LaunchFromLaunchIcon(const std::string& site_mode);
  void LaunchFromMenuOption(const std::string& site_mode);
  void LaunchFromPlatformShortcut(const std::string& site_mode);
  void OpenAppSettingsFromChromeApps(const std::string& site_mode);
  void OpenAppSettingsFromAppMenu(const std::string& site_mode);
  void NavigateBrowser(const std::string& site_mode);
  void NavigatePwaSiteAFooTo(const std::string& site_mode);
  void NavigatePwaSiteATo(const std::string& site_mode);
  void NavigateNotfoundUrl();
  void NavigateTabbedBrowserToSite(const GURL& url);
  void ManifestUpdateIcon(const std::string& site_mode);
  void ManifestUpdateTitle(const std::string& site_mode);
  void ManifestUpdateDisplayBrowser(const std::string& site_mode);
  void ManifestUpdateDisplayMinimal(const std::string& site_mode);
  void ManifestUpdateDisplay(const std::string& site_mode,
                             const std::string& display);
  void ManifestUpdateScopeSiteAFooTo(const std::string& scope_mode);
  void OpenInChrome();
  void SetOpenInTab(const std::string& site_mode);
  void SetOpenInWindow(const std::string& site_mode);
  void SwitchProfileClients(const std::string& client_mode);
  void SyncTurnOff();
  void SyncTurnOn();
  void UninstallFromList(const std::string& site_mode);
  void UninstallFromMenu(const std::string& site_mode);
  void UninstallFromAppSettings(const std::string& site_mode);
  void UninstallPolicyApp(const std::string& site_mode);
  void UninstallFromOs(const std::string& site_mode);

  // State Check Actions:
  void CheckAppListEmpty();
  void CheckAppInListNotLocallyInstalled(const std::string& site_mode);
  void CheckAppInListWindowed(const std::string& site_mode);
  void CheckAppInListTabbed(const std::string& site_mode);
  void CheckAppNavigationIsStartUrl();
  void CheckBrowserNavigationIsAppSettings(const std::string& site_mode);
  void CheckAppNotInList(const std::string& site_mode);
  void CheckAppIconSiteA(const std::string& color);
  void CheckAppTitleSiteA(const std::string& title);
  void CheckAppWindowMode(const std::string& site_mode,
                          apps::WindowMode window_mode);
  void CheckInstallable();
  void CheckInstallIconShown();
  void CheckInstallIconNotShown();
  void CheckLaunchIconShown();
  void CheckLaunchIconNotShown();
  void CheckTabCreated();
  void CheckTabNotCreated();
  void CheckCustomToolbar();
  void CheckNoToolbar();
  void CheckPlatformShortcutAndIcon(const std::string& site_mode);
  void CheckPlatformShortcutNotExists(const std::string& site_mode);
  void CheckRunOnOsLoginEnabled(const std::string& site_mode);
  void CheckRunOnOsLoginDisabled(const std::string& site_mode);
  void CheckUserCannotSetRunOnOsLogin(const std::string& site_mode);
  void CheckUserDisplayModeInternal(DisplayMode display_mode);
  void CheckWindowClosed();
  void CheckWindowCreated();
  void CheckWindowControlsOverlay(const std::string& site_mode,
                                  const std::string& is_on);
  void CheckWindowControlsOverlayToggle(const std::string& site_mode,
                                        const std::string& is_shown);
  void CheckWindowDisplayBrowser();
  void CheckWindowDisplayMinimal();
  void CheckWindowDisplayStandalone();

 protected:
  // WebAppInstallManagerObserver:
  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override;

 private:
  // Must be called at the beginning of every state change action function.
  void BeforeStateChangeAction(const char* function);
  // Must be called at the end of every state change action function.
  void AfterStateChangeAction();
  // Must be called at the beginning of every state check action function.
  void BeforeStateCheckAction(const char* function);
  // Must be called at the end of every state check action function.
  void AfterStateCheckAction();

  AppId GetAppIdBySiteMode(const std::string& site_mode);
  GURL GetAppStartURL(const std::string& site_mode);
  absl::optional<AppState> GetAppBySiteMode(StateSnapshot* state_snapshot,
                                            Profile* profile,
                                            const std::string& site_mode);

  WebAppProvider* GetProviderForProfile(Profile* profile);

  std::unique_ptr<StateSnapshot> ConstructStateSnapshot();

  std::string GetBrowserWindowTitle(Browser* browser);
  content::WebContents* GetCurrentTab(Browser* browser);
  GURL GetInScopeURL(const std::string& site_mode);
  GURL GetScopeForSiteMode(const std::string& site_mode);
  GURL GetURLForSiteMode(const std::string& site_mode);
  void InstallCreateShortcut(bool open_in_window);

  void InstallPolicyAppInternal(const std::string& site_mode,
                                base::Value default_launch_container,
                                bool create_shortcut);
  void ApplyRunOnOsLoginPolicy(const std::string& site_mode,
                               const char* policy);

  void UninstallPolicyAppById(const AppId& id);
  // This action only works if no navigations to the given app_url occur
  // between app installation and calls to this action.
  bool AreNoAppWindowsOpen(Profile* profile, const AppId& app_id);
  void ForceUpdateManifestContents(const std::string& site_mode,
                                   const GURL& app_url_with_manifest_param);
  void MaybeWaitForManifestUpdates();

  void MaybeNavigateTabbedBrowserInScope(const std::string& site_mode);

  // Returns an existing app browser if one exists, or launches a new one if
  // not.
  Browser* GetAppBrowserForSite(const std::string& site_mode,
                                bool launch_if_not_open = true);

  bool IsShortcutAndIconCreated(Profile* profile,
                                const std::string& name,
                                const AppId& id);

  void SetRunOnOsLoginMode(const std::string& site_mode,
                           apps::RunOnOsLoginMode login_mode);

  void LaunchAppStartupBrowserCreator(const AppId& app_id);

  void CheckAppSettingsAppState(Profile* profile, const AppState& app_state);

  Browser* browser();
  const net::EmbeddedTestServer* embedded_test_server();
  Profile* profile() {
    if (!active_profile_) {
      active_profile_ = delegate_->GetAllProfiles()[0];
    }
    return active_profile_;
  }
  Browser* app_browser() { return app_browser_; }
  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }
  PageActionIconView* pwa_install_view();
  PageActionIconView* intent_picker_view();

  base::flat_set<AppId> previous_manifest_updates_;

  // Variables used to facilitate waiting for manifest updates, as there isn't
  // a formal 'action' that a user can take to wait for this, as it happens
  // behind the scenes.
  base::flat_set<AppId> app_ids_with_pending_manifest_updates_;
  // |waiting_for_update_*| variables are either all populated or all not
  // populated. These signify that the test is currently waiting for the
  // given |waiting_for_update_id_| to receive an update before continuing.
  absl::optional<AppId> waiting_for_update_id_;
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

  raw_ptr<Browser> active_browser_ = nullptr;
  raw_ptr<Profile> active_profile_ = nullptr;
  AppId active_app_id_;
  raw_ptr<Browser> app_browser_ = nullptr;

  std::unique_ptr<views::NamedWidgetShownWaiter> app_id_update_dialog_waiter_;
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      observation_{this};
  std::unique_ptr<ScopedShortcutOverrideForTesting> shortcut_override_;
};

// Simple base browsertest class usable by all non-sync web app integration
// tests.
class WebAppIntegrationBrowserTest
    : public InProcessBrowserTest,
      public WebAppIntegrationTestDriver::TestDelegate {
 public:
  WebAppIntegrationBrowserTest();
  ~WebAppIntegrationBrowserTest() override;

  // InProcessBrowserTest:
  void SetUp() override;

  // BrowserTestBase:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  // WebAppIntegrationBrowserTestBase::TestDelegate:
  Browser* CreateBrowser(Profile* profile) override;
  void AddBlankTabAndShow(Browser* browser) override;
  net::EmbeddedTestServer* EmbeddedTestServer() override;

  std::vector<Profile*> GetAllProfiles() override;

  bool IsSyncTest() override;
  void SyncTurnOff() override;
  void SyncTurnOn() override;
  void AwaitWebAppQuiescence() override;

 protected:
  WebAppIntegrationTestDriver helper_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_TEST_DRIVER_H_
