// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_

#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

struct TabState {
  TabState(GURL tab_url, bool is_tab_installable)
      : url(tab_url), is_installable(is_tab_installable) {}
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
  AppState(web_app::AppId app_id,
           const std::string app_name,
           const GURL app_scope,
           const blink::mojom::DisplayMode& effective_display_mode,
           const blink::mojom::DisplayMode& user_display_mode,
           bool is_installed_locally);
  ~AppState();
  AppState(const AppState&);
  bool operator==(const AppState& other) const;

  web_app::AppId id;
  std::string name;
  GURL scope;
  blink::mojom::DisplayMode effective_display_mode;
  blink::mojom::DisplayMode user_display_mode;
  bool is_installed_locally;
};

struct ProfileState {
  ProfileState(base::flat_map<Browser*, BrowserState> browser_state,
               base::flat_map<web_app::AppId, AppState> app_state);
  ~ProfileState();
  ProfileState(const ProfileState&);
  bool operator==(const ProfileState& other) const;

  base::flat_map<Browser*, BrowserState> browsers;
  base::flat_map<web_app::AppId, AppState> apps;
};

struct StateSnapshot {
  explicit StateSnapshot(base::flat_map<Profile*, ProfileState> profile_state);
  ~StateSnapshot();
  StateSnapshot(const StateSnapshot&);
  bool operator==(const StateSnapshot& other) const;

  base::flat_map<Profile*, ProfileState> profiles;
};

class WebAppIntegrationBrowserTestBase {
 public:
  struct TestDelegate {
    virtual Browser* CreateBrowser(Profile* profile) = 0;
    virtual void AddBlankTabAndShow(Browser* browser) = 0;
    virtual net::EmbeddedTestServer* EmbeddedTestServer() = 0;
    virtual std::vector<Profile*> GetAllProfiles() = 0;
    virtual bool UserSigninInternal() = 0;
    virtual void TurnSyncOff() = 0;
    virtual void TurnSyncOn() = 0;
  };

  explicit WebAppIntegrationBrowserTestBase(TestDelegate* delegate);
  ~WebAppIntegrationBrowserTestBase();

  static base::Optional<ProfileState> GetStateForProfile(
      StateSnapshot* state_snapshot,
      Profile* profile);
  static base::Optional<BrowserState> GetStateForBrowser(
      StateSnapshot* state_snapshot,
      Profile* profile,
      Browser* browser);
  static base::Optional<TabState> GetStateForActiveTab(
      BrowserState browser_state);
  static base::Optional<AppState> GetStateForAppId(
      StateSnapshot* state_snapshot,
      Profile* profile,
      web_app::AppId id);

  static bool IsInspectionAction(const std::string& action);
  static std::string StripAllWhitespace(std::string line);
  static std::string GetCommandLineTestOverride();

  void SetUp(base::FilePath test_data_dir);
  void SetUpOnMainThread();

  // Test Framework
  static base::FilePath GetTestFilePath(base::FilePath test_data_dir,
                                        const std::string& file_name);
  static std::vector<std::string> ReadTestInputFile(
      base::FilePath test_data_dir,
      const std::string& file_name);
  static std::vector<std::string> GetPlatformIgnoredTests(
      base::FilePath test_data_dir,
      const std::string& file_name);
  static std::vector<std::string> BuildAllPlatformTestCaseSet(
      base::FilePath test_data_dir,
      const std::string& test_case_file_name);
  void ParseParams(std::string action_strings);
  void ExecuteAction(const std::string& action_string);

  // Automated Testing Actions
  void AddPolicyAppInternal(base::Value default_launch_container);
  void ClosePWA();
  void InstallCreateShortcutTabbed();
  void InstallLocally();
  web_app::AppId InstallOmniboxOrMenu();
  void LaunchInternal();
  void ListAppsInternal();
  void NavigateTabbedBrowserToSite(const GURL& url);
  void RemovePolicyApp();
  void SetOpenInTabInternal();
  void SetOpenInWindowInternal();
  void SwitchProfileClients();
  void TurnSyncOff();
  void TurnSyncOn();
  void UninstallFromMenu();
  void UninstallInternal();
  void UserSigninInternal();

  // Assert Actions
  void AssertAppNotLocallyInstalledInternal();
  void AssertAppNotInList();
  void AssertManifestDisplayModeInternal(DisplayMode display_mode);
  void AssertUserDisplayModeInternal(DisplayMode display_mode);
  void AssertInstallable();
  void AssertInstallIconShown();
  void AssertInstallIconNotShown();
  void AssertLaunchIconShown();
  void AssertLaunchIconNotShown();
  void AssertTabCreated();
  void AssertWindowCreated();

  // Helpers
  std::vector<std::string>& testing_actions() { return testing_actions_; }
  std::vector<AppId> GetAppIdsForProfile(Profile* profile);
  GURL GetInstallableAppURL();
  WebAppProvider* GetProviderForProfile(Profile* profile);

 private:
  StateSnapshot ConstructStateSnapshot();
  const net::EmbeddedTestServer* embedded_test_server();
  GURL GetNonInstallableAppURL();
  GURL GetInScopeURL();
  GURL GetOutOfScopeURL();

  content::WebContents* GetCurrentTab(Browser* browser);
  WebAppProvider* GetProvider() { return WebAppProvider::Get(profile()); }
  Browser* browser();
  Profile* profile() {
    if (!active_profile_) {
      active_profile_ = delegate_->GetAllProfiles()[0];
    }
    return active_profile_;
  }
  Browser* app_browser() { return app_browser_; }
  PageActionIconView* pwa_install_view();

  TestDelegate* delegate_;
  std::unique_ptr<StateSnapshot> before_action_state_;
  std::unique_ptr<StateSnapshot> after_action_state_;
  base::flat_map<std::string, bool> site_installability_map_;
  Browser* app_browser_ = nullptr;
  Browser* active_browser_ = nullptr;
  Profile* active_profile_ = nullptr;
  std::vector<AppId> app_ids_;
  std::vector<std::string> testing_actions_;
  AppId active_app_id_;
  ScopedOsHooksSuppress os_hooks_suppress_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_
