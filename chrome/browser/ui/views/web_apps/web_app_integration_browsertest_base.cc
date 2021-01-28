// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"

#include "base/base_paths.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/re2/src/re2/re2.h"

namespace web_app {

namespace {

const std::string kTestCaseFilename =
    "web_app_integration_browsertest_cases.csv";
const std::string kExpectationsFilename = "TestExpectations";
const std::string kPlatformName =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "ChromeOS";
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    "Linux";
#elif defined(OS_MAC)
    "Mac";
#elif defined(OS_WIN)
    "Win";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Command-line switch that overrides test case input. Takes a comma
// separated list of testing actions. This aids in development of tests
// by allowing one to run a single test at a time, and avoid running every
// test case in the suite.
const char kWebAppIntegrationTestCase[] = "web-app-integration-test-case";

}  // anonymous namespace

BrowserState::BrowserState(
    Browser* browser_ptr,
    base::flat_map<content::WebContents*, TabState> tab_state,
    content::WebContents* active_web_contents,
    bool is_an_app_browser,
    bool install_icon_visible,
    bool launch_icon_visible)
    : browser(browser_ptr),
      tabs(std::move(tab_state)),
      active_tab(active_web_contents),
      is_app_browser(is_an_app_browser),
      install_icon_shown(install_icon_visible),
      launch_icon_shown(launch_icon_visible) {}
BrowserState::~BrowserState() = default;
BrowserState::BrowserState(const BrowserState&) = default;
bool BrowserState::operator==(const BrowserState& other) const {
  return browser == other.browser && tabs == other.tabs &&
         active_tab == other.active_tab &&
         is_app_browser == other.is_app_browser &&
         install_icon_shown == other.install_icon_shown &&
         launch_icon_shown == other.launch_icon_shown;
}

AppState::AppState(web_app::AppId app_id,
                   const std::string app_name,
                   const GURL app_scope,
                   const blink::mojom::DisplayMode& app_display_mode)
    : id(app_id),
      name(app_name),
      scope(app_scope),
      display_mode(app_display_mode) {}
AppState::~AppState() = default;
AppState::AppState(const AppState&) = default;
bool AppState::operator==(const AppState& other) const {
  return id == other.id && name == other.name && scope == other.scope &&
         display_mode == other.display_mode;
}

StateSnapshot::StateSnapshot(
    base::flat_map<Browser*, BrowserState> browser_state,
    base::flat_map<web_app::AppId, AppState> app_state)
    : browsers(std::move(browser_state)), apps(std::move(app_state)) {}
StateSnapshot::~StateSnapshot() = default;
StateSnapshot::StateSnapshot(const StateSnapshot&) = default;
bool StateSnapshot::operator==(const StateSnapshot& other) const {
  return browsers == other.browsers && apps == other.apps;
}

WebAppIntegrationBrowserTestBase::WebAppIntegrationBrowserTestBase(
    InProcessBrowserTest* in_process_browser_test)
    : in_process_browser_test_(in_process_browser_test),
      https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

WebAppIntegrationBrowserTestBase::~WebAppIntegrationBrowserTestBase() = default;

// static
base::Optional<BrowserState>
WebAppIntegrationBrowserTestBase::GetStateForBrowser(
    base::flat_map<Browser*, BrowserState> browser_state_map,
    Browser* browser) {
  auto it = browser_state_map.find(browser);
  return it == browser_state_map.end()
             ? base::nullopt
             : base::make_optional<BrowserState>(it->second);
}

// static
base::Optional<TabState> WebAppIntegrationBrowserTestBase::GetStateForActiveTab(
    BrowserState browser_state) {
  if (!browser_state.active_tab) {
    return base::nullopt;
  }

  auto it = browser_state.tabs.find(browser_state.active_tab);
  DCHECK(it != browser_state.tabs.end());
  return base::make_optional<TabState>(it->second);
}

// static
base::Optional<AppState> WebAppIntegrationBrowserTestBase::GetStateForAppId(
    base::flat_map<web_app::AppId, AppState> apps,
    web_app::AppId id) {
  auto it = apps.find(id);
  return it == apps.end() ? base::nullopt
                          : base::make_optional<AppState>(it->second);
}

// static
bool WebAppIntegrationBrowserTestBase::IsInspectionAction(
    const std::string& action) {
  return base::StartsWith(action, "assert_");
}

// static
std::string WebAppIntegrationBrowserTestBase::StripAllWhitespace(
    std::string line) {
  std::string output;
  output.reserve(line.size());
  for (const char& c : line) {
    if (!isspace(c)) {
      output += c;
    }
  }
  return output;
}

// static
std::string WebAppIntegrationBrowserTestBase::GetCommandLineTestOverride() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kWebAppIntegrationTestCase)) {
    return command_line->GetSwitchValueASCII(kWebAppIntegrationTestCase);
  }
  return "";
}

void WebAppIntegrationBrowserTestBase::SetUp(base::FilePath test_data_dir) {
  https_server_.AddDefaultHandlers(test_data_dir);
  ASSERT_TRUE(https_server_.Start());

  webapps::TestAppBannerManagerDesktop::SetUp();
}

void WebAppIntegrationBrowserTestBase::SetUpOnMainThread() {
  os_hooks_suppress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();

  pwa_install_view_ =
      BrowserView::GetBrowserViewForBrowser(in_process_browser_test_->browser())
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kPwaInstall);
  ASSERT_TRUE(pwa_install_view_);
  EXPECT_FALSE(pwa_install_view_->GetVisible());
}

void WebAppIntegrationBrowserTestBase::ParseParams(std::string action_strings) {
  // Useful for debugging since all tests are run in a single parameterized
  // test.
  LOG(ERROR) << "Test case: " << action_strings;
  testing_actions_ = base::SplitString(
      action_strings, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

base::FilePath WebAppIntegrationBrowserTestBase::GetTestFilePath(
    base::FilePath test_data_dir,
    const std::string& file_name) {
  return test_data_dir.AppendASCII(file_name);
}

std::vector<std::string> WebAppIntegrationBrowserTestBase::ReadTestInputFile(
    base::FilePath test_data_dir,
    const std::string& file_name) {
  std::vector<std::string> test_cases;
  std::string command_line_test_case = GetCommandLineTestOverride();
  if (!command_line_test_case.empty()) {
    test_cases.push_back(StripAllWhitespace(command_line_test_case));
    return test_cases;
  }

  base::FilePath file = GetTestFilePath(test_data_dir, file_name);
  std::string contents;
  if (!base::ReadFileToString(file, &contents)) {
    return test_cases;
  }

  std::vector<std::string> file_lines = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : file_lines) {
    if (line[0] == '#') {
      continue;
    }

    if (line.find('|') == std::string::npos) {
      test_cases.push_back(StripAllWhitespace(line));
      continue;
    }

    std::vector<std::string> platforms_and_test = base::SplitString(
        line, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (platforms_and_test[0].find(kPlatformName) != std::string::npos) {
      test_cases.push_back(StripAllWhitespace(platforms_and_test[1]));
    }
  }

  return test_cases;
}

std::vector<std::string>
WebAppIntegrationBrowserTestBase::GetPlatformIgnoredTests(
    base::FilePath test_data_dir,
    const std::string& file_name) {
  base::FilePath file = GetTestFilePath(test_data_dir, file_name);
  std::string contents;
  std::vector<std::string> platform_expectations;
  if (!base::ReadFileToString(file, &contents)) {
    return platform_expectations;
  }

  std::vector<std::string> file_lines = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : file_lines) {
    if (line[0] == '#') {
      continue;
    }

    std::string platform;
    std::string expectation;
    std::string test_case;
    RE2::FullMatch(
        line, "crbug.com/\\d* \\[ (\\w*) \\] \\[ (\\w*) \\] ([\\w*,\\s*]*)",
        &platform, &expectation, &test_case);
    if (platform == kPlatformName) {
      if (expectation == "Skip") {
        platform_expectations.push_back(StripAllWhitespace(test_case));
      } else {
        NOTREACHED() << "Unsupported expectation " << expectation;
      }
    }
  }
  return platform_expectations;
}

std::vector<std::string>
WebAppIntegrationBrowserTestBase::BuildAllPlatformTestCaseSet(
    base::FilePath test_data_dir) {
  std::vector<std::string> test_cases_all =
      ReadTestInputFile(test_data_dir, kTestCaseFilename);
  std::sort(test_cases_all.begin(), test_cases_all.end());

  std::vector<std::string> ignored_cases =
      GetPlatformIgnoredTests(test_data_dir, kExpectationsFilename);
  std::sort(ignored_cases.begin(), ignored_cases.end());

  std::vector<std::string> final_tests(test_cases_all.size());
  auto iter = std::set_difference(test_cases_all.begin(), test_cases_all.end(),
                                  ignored_cases.begin(), ignored_cases.end(),
                                  final_tests.begin());
  final_tests.resize(iter - final_tests.begin());
  return final_tests;
}

// Non-assert actions implemented before assert actions. Implemented in
// alphabetical order.
void WebAppIntegrationBrowserTestBase::ExecuteAction(
    const std::string& action_string) {
  if (base::EndsWith(action_string, "site_b")) {
    FAIL() << "site_b actions not yet supported: " << action_string;
  }

  if (!IsInspectionAction(action_string)) {
    before_action_state_ = std::move(after_action_state_);
  }

  if (base::StartsWith(action_string, "add_policy_app_internal_tabbed")) {
    AddPolicyAppInternal(base::Value(kDefaultLaunchContainerTabValue));
  } else if (base::StartsWith(action_string,
                              "add_policy_app_internal_windowed")) {
    AddPolicyAppInternal(base::Value(kDefaultLaunchContainerWindowValue));
  } else if (action_string == "close_pwa") {
    ClosePWA();
  } else if (action_string == "install_create_shortcut_tabbed") {
    InstallCreateShortcutTabbed();
  } else if (base::StartsWith(action_string, "install_internal_windowed")) {
    InstallOmniboxOrMenu();
  } else if (action_string == "install_omnibox_or_menu") {
    InstallOmniboxOrMenu();
  } else if (base::StartsWith(action_string, "launch_internal")) {
    LaunchInternal();
  } else if (action_string == "list_apps_internal") {
    ListAppsInternal();
  } else if (base::StartsWith(action_string, "navigate_browser_in_scope")) {
    NavigateToSite(browser(), GetInScopeURL());
  } else if (base::StartsWith(action_string, "navigate_installable")) {
    NavigateToSite(browser(), GetInstallableAppURL());
  } else if (action_string == "navigate_not_installable") {
    NavigateToSite(browser(), GetNonInstallableAppURL());
  } else if (action_string == "remove_policy_app") {
    RemovePolicyApp();
  } else if (base::StartsWith(action_string, "set_open_in_tab_internal")) {
    SetOpenInTabInternal();
  } else if (base::StartsWith(action_string, "set_open_in_window_internal")) {
    SetOpenInWindowInternal();
  } else if (action_string == "uninstall_from_menu") {
    UninstallFromMenu();
  } else if (base::StartsWith(action_string, "uninstall_internal")) {
    UninstallInternal();
  } else if (action_string == "assert_app_in_list_not_windowed") {
    AssertAppInListNotWindowed();
  } else if (base::StartsWith(action_string, "assert_app_not_in_list")) {
    AssertAppNotInList();
  } else if (action_string == "assert_display_mode_standalone_internal") {
    AssertDisplayModeInternal(DisplayMode::kStandalone);
  } else if (action_string == "assert_display_mode_browser_internal") {
    AssertDisplayModeInternal(DisplayMode::kBrowser);
  } else if (action_string == "assert_installable") {
    AssertInstallable();
  } else if (action_string == "assert_install_icon_shown") {
    AssertInstallIconShown();
  } else if (action_string == "assert_install_icon_not_shown") {
    AssertInstallIconNotShown();
  } else if (action_string == "assert_launch_icon_shown") {
    AssertLaunchIconShown();
  } else if (action_string == "assert_launch_icon_not_shown") {
    AssertLaunchIconNotShown();
  } else if (action_string == "assert_no_crash") {
  } else if (action_string == "assert_tab_created") {
    AssertTabCreated();
  } else if (action_string == "assert_window_created") {
    AssertWindowCreated();
  } else {
    FAIL() << "Unimplemented action: " << action_string;
  }

  if (IsInspectionAction(action_string)) {
    DCHECK(!after_action_state_ ||
           *after_action_state_ == ConstructStateSnapshot());
  } else {
    after_action_state_ =
        std::make_unique<StateSnapshot>(ConstructStateSnapshot());
  }
}

// Automated Testing Actions
void WebAppIntegrationBrowserTestBase::AddPolicyAppInternal(
    base::Value default_launch_container) {
  GURL url = GetInstallableAppURL();
  auto* web_app_registrar =
      WebAppProvider::Get(profile())->registrar().AsWebAppRegistrar();
  base::RunLoop run_loop;
  WebAppInstallObserver observer(profile());
  observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        bool is_installed = web_app_registrar->IsInstalled(app_id);
        GURL installed_url = web_app_registrar->GetAppStartUrl(app_id);
        if (is_installed && installed_url.is_valid() &&
            installed_url.spec() == url.spec()) {
          active_app_id_ = app_id;
          run_loop.Quit();
        }
      }));
  {
    base::Value item(base::Value::Type::DICTIONARY);
    item.SetKey(kUrlKey, base::Value(url.spec()));
    item.SetKey(kDefaultLaunchContainerKey,
                std::move(default_launch_container));
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kWebAppInstallForceList);
    update->Append(item.Clone());
  }
  run_loop.Run();
}

void WebAppIntegrationBrowserTestBase::ClosePWA() {
  DCHECK(app_browser_);
  app_browser_->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser_);
}

void WebAppIntegrationBrowserTestBase::InstallCreateShortcutTabbed() {
  chrome::SetAutoAcceptWebAppDialogForTesting(/*auto_accept=*/true,
                                              /*auto_open_in_window=*/false);
  WebAppInstallObserver observer(profile());
  CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  active_app_id_ = observer.AwaitNextInstall();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
}

web_app::AppId WebAppIntegrationBrowserTestBase::InstallOmniboxOrMenu() {
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);

  web_app::AppId app_id;
  base::RunLoop run_loop;
  web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&app_id, &run_loop](const web_app::AppId& installed_app_id,
                           web_app::InstallResultCode code) {
        app_id = installed_app_id;
        run_loop.Quit();
      }));

  pwa_install_view()->ExecuteForTesting();

  run_loop.Run();

  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  active_app_id_ = app_id;
  auto* browser_list = BrowserList::GetInstance();
  app_browser_ = browser_list->GetLastActive();
  DCHECK(AppBrowserController::IsWebApp(app_browser_));

  return app_id;
}

void WebAppIntegrationBrowserTestBase::LaunchInternal() {
  auto* web_app_provider = GetProvider();
  AppRegistrar& app_registrar = web_app_provider->registrar();
  DisplayMode display_mode =
      app_registrar.GetAppEffectiveDisplayMode(active_app_id_);
  if (display_mode == blink::mojom::DisplayMode::kStandalone) {
    app_browser_ = LaunchWebAppBrowserAndWait(
        ProfileManager::GetActiveUserProfile(), active_app_id_);
  } else {
    ui_test_utils::UrlLoadObserver url_observer(
        WebAppProviderBase::GetProviderBase(profile())
            ->registrar()
            .GetAppLaunchUrl(active_app_id_),
        content::NotificationService::AllSources());
    LaunchBrowserForWebAppInTab(profile(), active_app_id_);
    url_observer.Wait();
  }
}

void WebAppIntegrationBrowserTestBase::ListAppsInternal() {
  auto* web_app_registrar =
      WebAppProvider::Get(profile())->registrar().AsWebAppRegistrar();
  app_ids_ = web_app_registrar->GetAppIds();
}

NavigateToSiteResult WebAppIntegrationBrowserTestBase::NavigateToSite(
    Browser* browser,
    const GURL& url) {
  content::WebContents* web_contents = GetCurrentTab(browser);
  auto* app_banner_manager =
      webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);

  ui_test_utils::NavigateToURL(browser, url);
  bool installable = app_banner_manager->WaitForInstallableCheck();

  last_navigation_result_ =
      NavigateToSiteResult{web_contents, app_banner_manager, installable};
  site_installability_map_[url.spec()] = installable;
  return last_navigation_result_;
}

void WebAppIntegrationBrowserTestBase::RemovePolicyApp() {
  GURL url = GetInstallableAppURL();
  base::RunLoop run_loop;
  WebAppInstallObserver observer(profile());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        if (active_app_id_ == app_id) {
          run_loop.Quit();
        }
      }));
  {
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kWebAppInstallForceList);
    update->EraseListValueIf([&](const base::Value& item) {
      const base::Value* url_value = item.FindKey(kUrlKey);
      return url_value && url_value->GetString() == url.spec();
    });
  }
  run_loop.Run();
}

void WebAppIntegrationBrowserTestBase::SetOpenInTabInternal() {
  auto& app_registry_controller =
      WebAppProvider::Get(profile())->registry_controller();
  app_registry_controller.SetAppUserDisplayMode(
      active_app_id_, blink::mojom::DisplayMode::kBrowser, true);
}

void WebAppIntegrationBrowserTestBase::SetOpenInWindowInternal() {
  auto& app_registry_controller =
      WebAppProvider::Get(profile())->registry_controller();
  app_registry_controller.SetAppUserDisplayMode(
      active_app_id_, blink::mojom::DisplayMode::kStandalone, true);
}

// TODO(https://crbug.com/1159651): Support this action on CrOS.
void WebAppIntegrationBrowserTestBase::UninstallFromMenu() {
  DCHECK(app_browser_);
  base::RunLoop run_loop;
  WebAppInstallObserver observer(profile());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        if (app_id == active_app_id_) {
          run_loop.Quit();
        }
      }));

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  auto app_menu_model =
      std::make_unique<WebAppMenuModel>(/*provider=*/nullptr, app_browser_);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  const bool found = app_menu_model->GetModelAndIndexForCommandId(
      WebAppMenuModel::kUninstallAppCommandId, &model, &index);
  EXPECT_TRUE(found);
  EXPECT_TRUE(model->IsEnabledAt(index));

  app_menu_model->ExecuteCommand(WebAppMenuModel::kUninstallAppCommandId,
                                 /*event_flags=*/0);
  // The |app_menu_model| must be destroyed here, as the |run_loop| waits
  // until the app is fully uninstalled, which includes closing and deleting
  // the app_browser_.
  app_menu_model.reset();
  app_browser_ = nullptr;
  run_loop.Run();
}

void WebAppIntegrationBrowserTestBase::UninstallInternal() {
  WebAppProviderBase* const provider =
      WebAppProviderBase::GetProviderBase(profile());
  base::RunLoop run_loop;

  DCHECK(provider->install_finalizer().CanUserUninstallExternalApp(
      active_app_id_));
  provider->install_finalizer().UninstallExternalAppByUser(
      active_app_id_, base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_TRUE(uninstalled);
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Assert Actions
void WebAppIntegrationBrowserTestBase::AssertAppInListNotWindowed() {
  EXPECT_TRUE(base::Contains(app_ids_, active_app_id_));
  DCHECK(after_action_state_);
  base::Optional<AppState> app_state =
      GetStateForAppId(after_action_state_->apps, active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(DisplayMode::kBrowser, app_state->display_mode);
}

void WebAppIntegrationBrowserTestBase::AssertAppNotInList() {
  EXPECT_FALSE(base::Contains(app_ids_, active_app_id_));
}

void WebAppIntegrationBrowserTestBase::AssertDisplayModeInternal(
    DisplayMode display_mode) {
  DCHECK(after_action_state_);
  base::Optional<AppState> app_state =
      GetStateForAppId(after_action_state_->apps, active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(display_mode, app_state->display_mode);
}

void WebAppIntegrationBrowserTestBase::AssertInstallable() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_->browsers, browser());
  ASSERT_TRUE(browser_state.has_value());
  base::Optional<TabState> active_tab =
      GetStateForActiveTab(browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
  EXPECT_TRUE(active_tab->is_installable);
}

void WebAppIntegrationBrowserTestBase::AssertInstallIconShown() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_->browsers, browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->install_icon_shown);
  EXPECT_TRUE(pwa_install_view()->GetVisible());
}

void WebAppIntegrationBrowserTestBase::AssertInstallIconNotShown() {
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_->browsers, browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->install_icon_shown);
  EXPECT_FALSE(pwa_install_view()->GetVisible());
}

void WebAppIntegrationBrowserTestBase::AssertLaunchIconShown() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_->browsers, browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->launch_icon_shown);
}

void WebAppIntegrationBrowserTestBase::AssertLaunchIconNotShown() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_->browsers, browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->launch_icon_shown);
}

void WebAppIntegrationBrowserTestBase::AssertTabCreated() {
  DCHECK(before_action_state_);
  DCHECK(after_action_state_);
  base::Optional<BrowserState> most_recent_browser_state =
      GetStateForBrowser(after_action_state_->browsers, browser());
  base::Optional<BrowserState> previous_browser_state =
      GetStateForBrowser(before_action_state_->browsers, browser());
  ASSERT_TRUE(most_recent_browser_state.has_value());
  ASSERT_TRUE(previous_browser_state.has_value());
  EXPECT_GT(most_recent_browser_state->tabs.size(),
            previous_browser_state->tabs.size());

  base::Optional<TabState> active_tab =
      GetStateForActiveTab(most_recent_browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
  EXPECT_EQ(GetInstallableAppURL(), active_tab->url);
}

void WebAppIntegrationBrowserTestBase::AssertWindowCreated() {
  DCHECK(before_action_state_);
  DCHECK(after_action_state_);
  EXPECT_GT(after_action_state_->browsers.size(),
            before_action_state_->browsers.size());
}

GURL WebAppIntegrationBrowserTestBase::GetInstallableAppURL() {
  return https_server_.GetURL("/banners/manifest_test_page.html");
}

GURL WebAppIntegrationBrowserTestBase::GetNonInstallableAppURL() {
  return https_server_.GetURL("/banners/no_manifest_test_page.html");
}

GURL WebAppIntegrationBrowserTestBase::GetInScopeURL() {
  return https_server_.GetURL("/banners/manifest_test_page.html");
}

GURL WebAppIntegrationBrowserTestBase::GetOutOfScopeURL() {
  return https_server_.GetURL("/out_of_scope/index.html");
}

content::WebContents* WebAppIntegrationBrowserTestBase::GetCurrentTab(
    Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

StateSnapshot WebAppIntegrationBrowserTestBase::ConstructStateSnapshot() {
  base::flat_map<Browser*, BrowserState> browser_state;
  auto* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    TabStripModel* tabs = browser->tab_strip_model();
    base::flat_map<content::WebContents*, TabState> tab_state_map;
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* tab = tabs->GetWebContentsAt(i);
      DCHECK(tab);
      GURL url = tab->GetURL();
      auto* app_banner_manager =
          webapps::TestAppBannerManagerDesktop::FromWebContents(tab);
      bool installable = app_banner_manager->WaitForInstallableCheck();

      tab_state_map.emplace(tab, TabState(url, installable));
    }
    bool is_app_browser = AppBrowserController::IsWebApp(browser);
    bool install_icon_visible = false;
    bool launch_icon_visible = false;
    content::WebContents* active_tab = tabs->GetActiveWebContents();
    if (!is_app_browser) {
      install_icon_visible =
          GetAppMenuCommandState(IDC_INSTALL_PWA, browser) == kEnabled;
      launch_icon_visible =
          GetAppMenuCommandState(IDC_OPEN_IN_PWA_WINDOW, browser) == kEnabled;
    }
    browser_state.emplace(
        browser, BrowserState(browser, tab_state_map, active_tab,
                              AppBrowserController::IsWebApp(browser),
                              install_icon_visible, launch_icon_visible));
  }

  auto* registrar = WebAppProvider::Get(browser()->profile())
                        ->registrar()
                        .AsWebAppRegistrar();
  auto app_ids = registrar->GetAppIds();
  base::flat_map<AppId, AppState> app_state;
  for (const auto& app_id : app_ids) {
    app_state.emplace(app_id,
                      AppState(app_id, registrar->GetAppShortName(app_id),
                               registrar->GetAppScope(app_id),
                               registrar->GetAppEffectiveDisplayMode(app_id)));
  }
  return StateSnapshot(std::move(browser_state), std::move(app_state));
}

}  // namespace web_app
