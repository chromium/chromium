// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Browser;
class PageActionIconView;

namespace base {
class CommandLine;
}  // namespace base

namespace web_app {

struct TabState {
  TabState(GURL tab_url, bool is_tab_installable)
      : url(tab_url), is_installable(is_tab_installable) {}
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
               bool is_an_app_browser,
               bool install_icon_visible,
               bool launch_icon_visible);
  ~BrowserState();
  BrowserState(const BrowserState&);
  bool operator==(const BrowserState& other) const;

  Browser* browser;
  base::flat_map<content::WebContents*, TabState> tabs;
  content::WebContents* active_tab;
  bool is_app_browser;
  bool install_icon_shown;
  bool launch_icon_shown;
};

struct AppState {
  AppState(AppId app_id,
           const std::string app_name,
           const GURL app_scope,
           const blink::mojom::DisplayMode& effective_display_mode,
           const blink::mojom::DisplayMode& user_display_mode,
           bool is_installed_locally);
  ~AppState();
  AppState(const AppState&);
  bool operator==(const AppState& other) const;

  AppId id;
  std::string name;
  GURL scope;
  blink::mojom::DisplayMode effective_display_mode;
  blink::mojom::DisplayMode user_display_mode;
  bool is_installed_locally;
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
std::ostream& operator<<(std::ostream& out, const StateSnapshot& snapshot);

class WebAppIntegrationBrowserTestBase : AppRegistrarObserver {
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

  explicit WebAppIntegrationBrowserTestBase(TestDelegate* delegate);
  ~WebAppIntegrationBrowserTestBase() override;

  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override;

  // State snapshot helpers
  // Supported site modes:
  //  * SiteA
  //  * SiteAFoo
  //  * SiteABar
  //  * SiteB
  //  * SiteC
  absl::optional<AppState> GetAppBySiteMode(StateSnapshot* state_snapshot,
                                            Profile* profile,
                                            const std::string& site_mode);

  static absl::optional<TabState> GetStateForActiveTab(
      BrowserState browser_state);
  static absl::optional<AppState>
  GetStateForAppId(StateSnapshot* state_snapshot, Profile* profile, AppId id);
  static absl::optional<BrowserState> GetStateForBrowser(
      StateSnapshot* state_snapshot,
      Profile* profile,
      Browser* browser);
  static absl::optional<ProfileState> GetStateForProfile(
      StateSnapshot* state_snapshot,
      Profile* profile);

  void SetUp();
  void SetUpOnMainThread();
  void TearDownOnMainThread();

  // Script-generated tests will call these methods right before/after calling
  // each action. Useful for common code that should be executed by most or all
  // actions, such as constructing state snapshots after state change actions.
  // Adding a before/after call around each action call may look a bit messier,
  // but this removes a burden of remembering to execute this test-framework
  // related code for future authors of action imiplementations, allowing them
  // to focus entirely on action-related code.
  void BeforeStateChangeAction();
  void AfterStateChangeAction();
  void BeforeStateCheckAction();
  void AfterStateCheckAction();

  // Automated Testing Actions
  //
  // Actions are defined in the following spreadsheet:
  // https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit#gid=2008870403
  //
  // Internal actions are actions that do not test the entire user-action-flow,
  // but give partial coverage (as close to complete as possible) of said code
  // paths.
  //
  // State change actions are declared (and implemented) above state check
  // actions.
  void InstallPolicyAppInternal(const std::string& site_mode,
                                base::Value default_launch_container,
                                const bool create_shortcut);
  void ClosePwa();
  void InstallCreateShortcutTabbed(const std::string& site_mode);
  void InstallCreateShortcutWindowed(const std::string& site_mode);
  void InstallMenuOption(const std::string& site_mode);
  void InstallLocally(const std::string& site_mode);
  void InstallOmniboxIcon(const std::string& site_mode);
  void InstallPolicyAppTabbedNoShortcut(const std::string& site_mode);
  void InstallPolicyAppTabbedShortcut(const std::string& site_mode);
  void InstallPolicyAppWindowedNoShortcut(const std::string& site_mode);
  void InstallPolicyAppWindowedShortcut(const std::string& site_mode);
  void LaunchFromChromeApps(const std::string& site_mode);
  void NavigateTabbedBrowserToSite(const GURL& url);
  void NavigateBrowser(const std::string& site_mode);
  void ManifestUpdateDisplayMinimal(const std::string& site_mode);
  void SetOpenInTab(const std::string& site_mode);
  void SetOpenInWindow(const std::string& site_mode);
  void SwitchProfileClients(const std::string& client_mode);
  void SyncTurnOff();
  void SyncTurnOn();
  void UninstallFromMenu(const std::string& site_mode);
  void UninstallPolicyApp(const std::string& site_mode);

  // State Check Actions
  void CheckAppListEmpty();
  void CheckAppInListNotLocallyInstalled(const std::string& site_mode);
  void CheckAppInListWindowed(const std::string& site_mode);
  void CheckAppInListTabbed(const std::string& site_mode);
  void CheckAppNotInList(const std::string& site_mode);
  void CheckInstallable();
  void CheckInstallIconShown();
  void CheckInstallIconNotShown();
  void CheckLaunchIconShown();
  void CheckLaunchIconNotShown();
  void CheckManifestDisplayModeInternal(DisplayMode display_mode);
  void CheckTabCreated();
  void CheckUserDisplayModeInternal(DisplayMode display_mode);
  void CheckWindowClosed();
  void CheckWindowCreated();
  void CheckWindowDisplayMinimal();
  void CheckWindowDisplayStandalone();

  // Supported params:
  //  * site_a
  //  * site_a/foo
  //  * site_a/bar
  //  * site_b
  //  * site_c
  GURL GetInstallableAppURL(const std::string& site_mode);
  WebAppProvider* GetProviderForProfile(Profile* profile);

 private:
  StateSnapshot ConstructStateSnapshot();

  // Supported params:
  //  * site_a
  //  * site_a/foo
  //  * site_a/bar
  //  * site_b
  //  * site_c
  GURL GetAppURLForManifest(const std::string& site_mode,
                            DisplayMode display_mode);
  content::WebContents* GetCurrentTab(Browser* browser);
  GURL GetInScopeURL(const std::string& site_mode);
  GURL GetNonInstallableAppURL();
  GURL GetOutOfScopeURL(const std::string& site_mode);
  WebAppProvider* GetProvider() {
    return WebAppProvider::GetForTest(profile());
  }
  GURL GetURLForSiteMode(const std::string& site_mode);
  void InstallCreateShortcut(bool open_in_window);

  // This action only works if no navigations to the given app_url occur
  // between app installation and calls to this action.
  bool AreNoAppWindowsOpen(Profile* profile, const AppId& app_id);
  void ForceUpdateManifestContents(const std::string& site_mode,
                                   GURL app_url_with_manifest_param);
  void MaybeWaitForManifestUpdates();

  void MaybeNavigateTabbedBrowserInScope(const std::string& site_mode);
  void SetOpenInTabInternal(const std::string& site_mode);
  void SetOpenInWindowInternal(const std::string& site_mode);

  Browser* browser();
  const net::EmbeddedTestServer* embedded_test_server();
  Profile* profile() {
    if (!active_profile_) {
      active_profile_ = delegate_->GetAllProfiles()[0];
    }
    return active_profile_;
  }
  Browser* app_browser() { return app_browser_; }
  PageActionIconView* pwa_install_view();
  PageActionIconView* intent_picker_view();

  ScopedOsHooksSuppress os_hooks_suppress_;

  // Variables used to facilitate waiting for manifest updates, as there isn't
  // a formal 'action' that a user can take to wait for this, as it happens
  // behind the scenes.
  base::flat_set<AppId> app_ids_with_pending_manifest_updates_;
  // |waiting_for_update_*| variables are either all populated or all not
  // populated. These signify that the test is currently waiting for the
  // given |waiting_for_update_id_| to receive an update before continuing.
  absl::optional<AppId> waiting_for_update_id_;
  std::unique_ptr<base::RunLoop> waiting_for_update_run_loop_;

  TestDelegate* delegate_;
  // State snapshots, captured before and after "state change" actions are
  // executed, and inspected by "state check" actions to verify behavior.
  std::unique_ptr<StateSnapshot> before_state_change_action_state_;
  std::unique_ptr<StateSnapshot> after_state_change_action_state_;
  base::flat_map<std::string, bool> site_installability_map_;
  Browser* app_browser_ = nullptr;
  Browser* active_browser_ = nullptr;
  Profile* active_profile_ = nullptr;
  std::vector<AppId> app_ids_;
  AppId active_app_id_;

  base::ScopedObservation<web_app::WebAppRegistrar,
                          web_app::AppRegistrarObserver>
      observation_{this};
};

// Simple base browsertest class usable by all non-sync web app integration
// tests.
class WebAppIntegrationBrowserTest
    : public InProcessBrowserTest,
      public WebAppIntegrationBrowserTestBase::TestDelegate {
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
  WebAppIntegrationBrowserTestBase helper_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_
