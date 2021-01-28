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
           const blink::mojom::DisplayMode& app_display_mode);
  ~AppState();
  AppState(const AppState&);
  bool operator==(const AppState& other) const;

  web_app::AppId id;
  std::string name;
  GURL scope;
  blink::mojom::DisplayMode display_mode;
};

struct StateSnapshot {
  StateSnapshot(base::flat_map<Browser*, BrowserState> browser_state,
                base::flat_map<web_app::AppId, AppState> app_state);
  ~StateSnapshot();
  StateSnapshot(const StateSnapshot&);
  bool operator==(const StateSnapshot& other) const;

  base::flat_map<Browser*, BrowserState> browsers;
  base::flat_map<web_app::AppId, AppState> apps;
};

struct NavigateToSiteResult {
  content::WebContents* web_contents;
  webapps::TestAppBannerManagerDesktop* app_banner_manager;
  bool installable;
};

class WebAppIntegrationBrowserTestBase {
 public:
  explicit WebAppIntegrationBrowserTestBase(
      InProcessBrowserTest* in_process_browser_test);
  ~WebAppIntegrationBrowserTestBase();

  static base::Optional<BrowserState> GetStateForBrowser(
      base::flat_map<Browser*, BrowserState> browser_state_map,
      Browser* browser);
  static base::Optional<TabState> GetStateForActiveTab(
      BrowserState browser_state);
  static base::Optional<AppState> GetStateForAppId(
      base::flat_map<web_app::AppId, AppState> apps,
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
      base::FilePath test_data_dir);
  void ParseParams(std::string action_strings);
  void ExecuteAction(const std::string& action_string);

  // Automated Testing Actions
  void AddPolicyAppInternal(base::Value default_launch_container);
  void ClosePWA();
  void InstallCreateShortcutTabbed();
  web_app::AppId InstallOmniboxOrMenu();
  void LaunchInternal();
  void ListAppsInternal();
  NavigateToSiteResult NavigateToSite(Browser* browser, const GURL& url);
  void RemovePolicyApp();
  void SetOpenInTabInternal();
  void SetOpenInWindowInternal();
  void UninstallFromMenu();
  void UninstallInternal();

  // Assert Actions
  void AssertAppInListNotWindowed();
  void AssertAppNotInList();
  void AssertDisplayModeInternal(DisplayMode display_mode);
  void AssertInstallable();
  void AssertInstallIconShown();
  void AssertInstallIconNotShown();
  void AssertLaunchIconShown();
  void AssertLaunchIconNotShown();
  void AssertTabCreated();
  void AssertWindowCreated();

  std::vector<std::string>& testing_actions() { return testing_actions_; }
  GURL GetInstallableAppURL();

 private:
  StateSnapshot ConstructStateSnapshot();
  GURL GetNonInstallableAppURL();
  GURL GetInScopeURL();
  GURL GetOutOfScopeURL();

  content::WebContents* GetCurrentTab(Browser* browser);
  Browser* browser() { return in_process_browser_test_->browser(); }
  Profile* profile() { return browser()->profile(); }
  Browser* app_browser() { return app_browser_; }
  WebAppProvider* GetProvider() { return WebAppProvider::Get(profile()); }
  PageActionIconView* pwa_install_view() { return pwa_install_view_; }

  InProcessBrowserTest* in_process_browser_test_;
  std::unique_ptr<StateSnapshot> before_action_state_;
  std::unique_ptr<StateSnapshot> after_action_state_;
  base::flat_map<std::string, bool> site_installability_map_;
  Browser* app_browser_ = nullptr;
  std::vector<AppId> app_ids_;
  std::vector<std::string> testing_actions_;
  NavigateToSiteResult last_navigation_result_;
  AppId active_app_id_;
  net::EmbeddedTestServer https_server_;
  base::FilePath test_data_dir_;
  PageActionIconView* pwa_install_view_ = nullptr;
  ScopedOsHooksSuppress os_hooks_suppress_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INTEGRATION_BROWSERTEST_BASE_H_
