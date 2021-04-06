// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"

#include "base/base_paths.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/re2/src/re2/re2.h"

namespace web_app {

namespace {

constexpr char kExpectationsFilename[] = "TestExpectations";
constexpr char kPlatformName[] =
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

class TestAppLauncherHandler : public AppLauncherHandler {
 public:
  TestAppLauncherHandler(extensions::ExtensionService* extension_service,
                         WebAppProvider* provider,
                         content::TestWebUI* test_web_ui)
      : AppLauncherHandler(extension_service, provider) {
    DCHECK(test_web_ui->GetWebContents());
    DCHECK(test_web_ui->GetWebContents()->GetBrowserContext());
    set_web_ui(test_web_ui);
  }
};

std::string BuildScopedTrace(const std::string& action_string,
                             const std::vector<std::string>& testing_actions,
                             bool is_sync_test) {
  const std::string test_case = base::JoinString(testing_actions, ", ");
  return base::StringPrintf(
      "\nFailed test case: %s\n"
      "Failed action: %s\n"
      "To disable this test, add the following line to "
      "//chrome/test/data/web_apps/TestExpectations:\n"
      "crbug.com/XXXXX [ %s ] [ Skip ] %s\n"
      "To run this test in isolation, run the following command:\n"
      "out/Default/%s_tests --gtest_filter=\"*%s*\" "
      "--web-app-integration-test-case=%s\n",
      test_case.c_str(), action_string.c_str(), kPlatformName,
      test_case.c_str(), is_sync_test ? "sync_integration" : "browser",
      is_sync_test ? "TwoClientWebAppsIntegrationSyncTest"
                   : "WebAppIntegrationBrowserTest",
      test_case.c_str());
}

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
                   const blink::mojom::DisplayMode& effective_display_mode,
                   const blink::mojom::DisplayMode& user_display_mode,
                   bool installed_locally)
    : id(app_id),
      name(app_name),
      scope(app_scope),
      effective_display_mode(effective_display_mode),
      user_display_mode(user_display_mode),
      is_installed_locally(installed_locally) {}
AppState::~AppState() = default;
AppState::AppState(const AppState&) = default;
bool AppState::operator==(const AppState& other) const {
  return id == other.id && name == other.name && scope == other.scope &&
         effective_display_mode == other.effective_display_mode &&
         user_display_mode == other.user_display_mode &&
         is_installed_locally == other.is_installed_locally;
}

ProfileState::ProfileState(base::flat_map<Browser*, BrowserState> browser_state,
                           base::flat_map<web_app::AppId, AppState> app_state)
    : browsers(std::move(browser_state)), apps(std::move(app_state)) {}
ProfileState::~ProfileState() = default;
ProfileState::ProfileState(const ProfileState&) = default;
bool ProfileState::operator==(const ProfileState& other) const {
  return browsers == other.browsers && apps == other.apps;
}

StateSnapshot::StateSnapshot(
    base::flat_map<Profile*, ProfileState> profile_state)
    : profiles(std::move(profile_state)) {}
StateSnapshot::~StateSnapshot() = default;
StateSnapshot::StateSnapshot(const StateSnapshot&) = default;
bool StateSnapshot::operator==(const StateSnapshot& other) const {
  return profiles == other.profiles;
}

WebAppIntegrationBrowserTestBase::WebAppIntegrationBrowserTestBase(
    TestDelegate* delegate)
    : delegate_(delegate) {}

WebAppIntegrationBrowserTestBase::~WebAppIntegrationBrowserTestBase() = default;

// static
base::Optional<ProfileState>
WebAppIntegrationBrowserTestBase::GetStateForProfile(
    StateSnapshot* state_snapshot,
    Profile* profile) {
  DCHECK(state_snapshot);
  DCHECK(profile);
  auto it = state_snapshot->profiles.find(profile);
  return it == state_snapshot->profiles.end()
             ? base::nullopt
             : base::make_optional<ProfileState>(it->second);
}

// static
base::Optional<BrowserState>
WebAppIntegrationBrowserTestBase::GetStateForBrowser(
    StateSnapshot* state_snapshot,
    Profile* profile,
    Browser* browser) {
  base::Optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return base::nullopt;
  }

  auto it = profile_state->browsers.find(browser);
  return it == profile_state->browsers.end()
             ? base::nullopt
             : base::make_optional<BrowserState>(it->second);
}

base::Optional<AppState> WebAppIntegrationBrowserTestBase::GetAppByScope(
    StateSnapshot* state_snapshot,
    Profile* profile,
    const std::string& action_param) {
  base::Optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return base::nullopt;
  }

  GURL scope = GetURLForScope(action_param);
  auto it =
      std::find_if(profile_state->apps.begin(), profile_state->apps.end(),
                   [scope](std::pair<web_app::AppId, AppState>& app_entry) {
                     return app_entry.second.scope == scope;
                   });

  return it == profile_state->apps.end()
             ? base::nullopt
             : base::make_optional<AppState>(it->second);
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
    StateSnapshot* state_snapshot,
    Profile* profile,
    web_app::AppId id) {
  base::Optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return base::nullopt;
  }

  auto it = profile_state->apps.find(id);
  return it == profile_state->apps.end()
             ? base::nullopt
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
  webapps::TestAppBannerManagerDesktop::SetUp();
}

void WebAppIntegrationBrowserTestBase::SetUpOnMainThread() {
  os_hooks_suppress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
}

void WebAppIntegrationBrowserTestBase::ParseParams(std::string action_strings) {
  // Useful for debugging since all tests are run in a single parameterized
  // test.
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
    base::FilePath test_data_dir,
    const std::string& test_case_file_name) {
  std::vector<std::string> test_cases_all =
      ReadTestInputFile(test_data_dir, test_case_file_name);
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
  SCOPED_TRACE(BuildScopedTrace(action_string, testing_actions(),
                                delegate_->IsSyncTest()));

  std::string action_param;
  RE2::PartialMatch(action_string, "(site_(a_foo|a_bar|a|b|c))", &action_param);
  if (base::EndsWith(action_param, "_foo")) {
    action_param = "site_a/foo";
  } else if (base::EndsWith(action_param, "_bar")) {
    action_param = "site_a/bar";
  }
  // Add 1 to `param_length` if a param is present to strip the preceding
  // underscore.
  const int param_length =
      action_param.length() ? action_param.length() + 1 : 0;
  std::string action_base =
      action_string.substr(0, action_string.length() - param_length);

  if (!IsInspectionAction(action_base)) {
    before_action_state_ = std::move(after_action_state_);
  }

  if (action_base == "add_policy_app_internal_tabbed") {
    AddPolicyAppInternal(action_param,
                         base::Value(kDefaultLaunchContainerTabValue));
  } else if (action_base == "add_policy_app_internal_windowed") {
    AddPolicyAppInternal(action_param,
                         base::Value(kDefaultLaunchContainerWindowValue));
  } else if (action_base == "close_pwa") {
    ClosePWA();
  } else if (action_base == "install_create_shortcut_tabbed") {
    InstallCreateShortcut(/*open_in_window=*/false);
  } else if (action_base == "install_create_shortcut_windowed") {
    InstallCreateShortcut(/*open_in_window=*/true);
  } else if (action_base == "install_internal_windowed") {
    InstallOmniboxOrMenu();
  } else if (action_base == "install_locally_internal") {
    InstallLocally();
  } else if (action_base == "install_omnibox_or_menu") {
    InstallOmniboxOrMenu();
  } else if (action_base == "launch_internal") {
    LaunchInternal(action_param);
  } else if (action_base == "list_apps_internal") {
    ListAppsInternal();
  } else if (action_base == "navigate_browser_in_scope") {
    NavigateTabbedBrowserToSite(GetInScopeURL(action_param));
  } else if (action_base == "navigate_installable") {
    NavigateTabbedBrowserToSite(GetInstallableAppURL(action_param));
  } else if (action_base == "navigate_not_installable") {
    NavigateTabbedBrowserToSite(GetNonInstallableAppURL());
  } else if (action_base == "remove_policy_app") {
    RemovePolicyApp(action_param.length() ? action_param : "site_a");
  } else if (action_base == "set_open_in_tab_internal") {
    SetOpenInTabInternal(action_param);
  } else if (action_base == "set_open_in_window_internal") {
    SetOpenInWindowInternal(action_param);
  } else if (action_base == "switch_profile_clients") {
    SwitchProfileClients();
  } else if (action_base == "sync_turned_off") {
    TurnSyncOff();
  } else if (action_base == "sync_turned_on") {
    TurnSyncOn();
  } else if (action_base == "uninstall_from_menu") {
    UninstallFromMenu();
  } else if (action_base == "uninstall_internal") {
    UninstallInternal(action_param);
  } else if (action_base == "manifest_update_display_minimal") {
    ManifestUpdateDisplay(action_param, blink::mojom::DisplayMode::kMinimalUi);
  } else if (action_base == "user_signin_internal") {
    UserSigninInternal();
  } else if (action_base == "assert_app_not_locally_installed_internal") {
    AssertAppNotLocallyInstalledInternal();
  } else if (action_base == "assert_app_not_in_list") {
    AssertAppNotInList(action_param);
  } else if (action_base == "assert_installable") {
    AssertInstallable();
  } else if (action_base == "assert_install_icon_shown") {
    AssertInstallIconShown();
  } else if (action_base == "assert_install_icon_not_shown") {
    AssertInstallIconNotShown();
  } else if (action_base == "assert_launch_icon_shown") {
    AssertLaunchIconShown();
  } else if (action_base == "assert_launch_icon_not_shown") {
    AssertLaunchIconNotShown();
  } else if (action_base == "assert_manifest_display_mode_browser_internal") {
    AssertManifestDisplayModeInternal(DisplayMode::kBrowser);
  } else if (action_base ==
             "assert_manifest_display_mode_standalone_internal") {
    AssertManifestDisplayModeInternal(DisplayMode::kStandalone);
  } else if (action_base == "assert_no_crash") {
  } else if (action_base == "assert_tab_created") {
    AssertTabCreated();
  } else if (action_base == "assert_user_display_mode_browser_internal") {
    AssertUserDisplayModeInternal(DisplayMode::kBrowser);
  } else if (action_base == "assert_user_display_mode_standalone_internal") {
    AssertUserDisplayModeInternal(DisplayMode::kStandalone);
  } else if (action_base == "assert_no_crash") {
  } else if (action_base == "assert_tab_created") {
    AssertTabCreated();
  } else if (action_base == "assert_window_closed") {
    AssertWindowClosed();
  } else if (action_base == "assert_window_created") {
    AssertWindowCreated();
  } else if (action_string == "assert_window_display_minimal") {
    AssertWindowDisplayMode(blink::mojom::DisplayMode::kMinimalUi);
  } else if (action_string == "assert_window_display_standalone") {
    AssertWindowDisplayMode(blink::mojom::DisplayMode::kStandalone);
  } else {
    FAIL() << "Unimplemented action: " << action_base;
  }

  if (IsInspectionAction(action_base)) {
    DCHECK(!after_action_state_ ||
           *after_action_state_ == ConstructStateSnapshot());
  } else {
    after_action_state_ =
        std::make_unique<StateSnapshot>(ConstructStateSnapshot());
  }
}

// Automated Testing Actions
void WebAppIntegrationBrowserTestBase::AddPolicyAppInternal(
    const std::string& action_param,
    base::Value default_launch_container) {
  GURL url = GetInstallableAppURL(action_param);
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

void WebAppIntegrationBrowserTestBase::InstallCreateShortcut(
    bool open_in_window) {
  chrome::SetAutoAcceptWebAppDialogForTesting(
      /*auto_accept=*/true,
      /*auto_open_in_window=*/open_in_window);
  WebAppInstallObserver observer(profile());
  CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  active_app_id_ = observer.AwaitNextInstall();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
}

void WebAppIntegrationBrowserTestBase::InstallLocally() {
  content::TestWebUI test_web_ui;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  DCHECK(web_contents);
  test_web_ui.set_web_contents(web_contents);
  TestAppLauncherHandler handler(/*extension_service=*/nullptr, GetProvider(),
                                 &test_web_ui);
  base::ListValue web_app_ids;
  web_app_ids.Append(active_app_id_);

  base::RunLoop run_loop;
  std::unique_ptr<WebAppInstallObserver> observer =
      WebAppInstallObserver::CreateInstallWithOsHooksListener(profile(),
                                                              {active_app_id_});
  observer->SetWebAppInstalledWithOsHooksDelegate(base::BindLambdaForTesting(
      [&](const AppId& installed_app_id) { run_loop.Quit(); }));
  handler.HandleInstallAppLocally(&web_app_ids);
  run_loop.Run();
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

  DCHECK(pwa_install_view()->GetVisible());
  pwa_install_view()->ExecuteForTesting();

  run_loop.Run();

  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  active_app_id_ = app_id;
  auto* browser_list = BrowserList::GetInstance();
  app_browser_ = browser_list->GetLastActive();
  DCHECK(AppBrowserController::IsWebApp(app_browser_));

  return app_id;
}

void WebAppIntegrationBrowserTestBase::LaunchInternal(
    const std::string& action_param) {
  base::Optional<AppState> app_state =
      GetAppByScope(before_action_state_.get(), profile(), action_param);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_param;
  auto app_id = app_state->id;
  auto* web_app_provider = GetProvider();
  AppRegistrar& app_registrar = web_app_provider->registrar();
  DisplayMode display_mode = app_registrar.GetAppEffectiveDisplayMode(app_id);
  if (display_mode == blink::mojom::DisplayMode::kBrowser) {
    ui_test_utils::UrlLoadObserver url_observer(
        app_registrar.GetAppLaunchUrl(app_id),
        content::NotificationService::AllSources());
    LaunchBrowserForWebAppInTab(profile(), app_id);
    url_observer.Wait();
  } else {
    app_browser_ = LaunchWebAppBrowserAndWait(profile(), app_id);
  }
}

void WebAppIntegrationBrowserTestBase::ListAppsInternal() {
  auto* web_app_registrar =
      WebAppProvider::Get(profile())->registrar().AsWebAppRegistrar();
  app_ids_ = web_app_registrar->GetAppIds();
}

void WebAppIntegrationBrowserTestBase::NavigateTabbedBrowserToSite(
    const GURL& url) {
  DCHECK(browser());
  content::WebContents* web_contents = GetCurrentTab(browser());
  auto* app_banner_manager =
      webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);

  ui_test_utils::NavigateToURL(browser(), url);
  app_banner_manager->WaitForInstallableCheck();
}

void WebAppIntegrationBrowserTestBase::RemovePolicyApp(
    const std::string& action_param) {
  GURL url = GetInstallableAppURL(action_param);
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
    size_t removed_count =
        update->EraseListValueIf([&](const base::Value& item) {
          const base::Value* url_value = item.FindKey(kUrlKey);
          return url_value && url_value->GetString() == url.spec();
        });
    ASSERT_GT(removed_count, 0U);
  }
  run_loop.Run();
}

void WebAppIntegrationBrowserTestBase::SetOpenInTabInternal(
    const std::string& action_param) {
  base::Optional<AppState> app_state =
      GetAppByScope(before_action_state_.get(), profile(), action_param);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_param;
  auto app_id = app_state->id;
  auto& app_registry_controller =
      WebAppProvider::Get(profile())->registry_controller();
  app_registry_controller.SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kBrowser, true);
}

void WebAppIntegrationBrowserTestBase::SetOpenInWindowInternal(
    const std::string& action_param) {
  base::Optional<AppState> app_state =
      GetAppByScope(before_action_state_.get(), profile(), action_param);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_param;
  auto app_id = app_state->id;
  auto& app_registry_controller =
      WebAppProvider::Get(profile())->registry_controller();
  app_registry_controller.SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kStandalone, true);
}

void WebAppIntegrationBrowserTestBase::SwitchProfileClients() {
  std::vector<Profile*> profiles = delegate_->GetAllProfiles();
  ASSERT_EQ(2U, profiles.size())
      << "Cannot switch profile clients if delegate only supports one profile";
  DCHECK(active_profile_);
  if (active_profile_ == profiles[0]) {
    active_profile_ = profiles[1];
  } else if (active_profile_ == profiles[1]) {
    active_profile_ = profiles[0];
  } else {
    NOTREACHED();
  }
  active_browser_ = chrome::FindTabbedBrowser(
      active_profile_, /*match_original_profiles=*/false);
}

void WebAppIntegrationBrowserTestBase::TurnSyncOff() {
  delegate_->TurnSyncOff();
}

void WebAppIntegrationBrowserTestBase::TurnSyncOn() {
  delegate_->TurnSyncOn();
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

void WebAppIntegrationBrowserTestBase::UninstallInternal(
    const std::string& action_param) {
  base::Optional<AppState> app_state =
      GetAppByScope(before_action_state_.get(), profile(), action_param);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_param;
  auto app_id = app_state->id;
  WebAppProviderBase* const provider =
      WebAppProviderBase::GetProviderBase(profile());
  base::RunLoop run_loop;

  DCHECK(provider->install_finalizer().CanUserUninstallExternalApp(app_id));
  provider->install_finalizer().UninstallExternalAppByUser(
      app_id, base::BindLambdaForTesting([&](bool uninstalled) {
        EXPECT_TRUE(uninstalled);
        run_loop.Quit();
      }));

  run_loop.Run();
}

void WebAppIntegrationBrowserTestBase::ManifestUpdateDisplay(
    const std::string& action_scope,
    DisplayMode display_mode) {
  // TODO(jarrydg): Create a map of supported manifest updates keyed on scope.
  ASSERT_EQ("site_a", action_scope);
  ASSERT_EQ(blink::mojom::DisplayMode::kMinimalUi, display_mode);
  ForceUpdateManifestContents(action_scope,
                              GetAppURLForManifest(action_scope, display_mode));
}

void WebAppIntegrationBrowserTestBase::UserSigninInternal() {
  delegate_->UserSigninInternal();
}

// Assert Actions
void WebAppIntegrationBrowserTestBase::AssertAppNotLocallyInstalledInternal() {
  DCHECK(after_action_state_);
  base::Optional<AppState> app_state =
      GetStateForAppId(after_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_FALSE(app_state->is_installed_locally);
}

void WebAppIntegrationBrowserTestBase::AssertAppNotInList(
    const std::string& action_param) {
  DCHECK(after_action_state_);
  base::Optional<AppState> app_state =
      GetAppByScope(after_action_state_.get(), profile(), action_param);
  EXPECT_FALSE(app_state.has_value());
}

void WebAppIntegrationBrowserTestBase::AssertInstallable() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  base::Optional<TabState> active_tab =
      GetStateForActiveTab(browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
  EXPECT_TRUE(active_tab->is_installable);
}

void WebAppIntegrationBrowserTestBase::AssertInstallIconShown() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->install_icon_shown);
  EXPECT_TRUE(pwa_install_view()->GetVisible());
}

void WebAppIntegrationBrowserTestBase::AssertInstallIconNotShown() {
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->install_icon_shown);
  EXPECT_FALSE(pwa_install_view()->GetVisible());
}

void WebAppIntegrationBrowserTestBase::AssertLaunchIconShown() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->launch_icon_shown);
}

void WebAppIntegrationBrowserTestBase::AssertLaunchIconNotShown() {
  DCHECK(after_action_state_);
  base::Optional<BrowserState> browser_state =
      GetStateForBrowser(after_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->launch_icon_shown);
}

void WebAppIntegrationBrowserTestBase::AssertManifestDisplayModeInternal(
    DisplayMode display_mode) {
  DCHECK(after_action_state_);
  base::Optional<AppState> app_state =
      GetStateForAppId(after_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(display_mode, app_state->effective_display_mode);
}

void WebAppIntegrationBrowserTestBase::AssertTabCreated() {
  DCHECK(before_action_state_);
  DCHECK(after_action_state_);
  base::Optional<BrowserState> most_recent_browser_state =
      GetStateForBrowser(after_action_state_.get(), profile(), browser());
  base::Optional<BrowserState> previous_browser_state =
      GetStateForBrowser(before_action_state_.get(), profile(), browser());
  ASSERT_TRUE(most_recent_browser_state.has_value());
  ASSERT_TRUE(previous_browser_state.has_value());
  EXPECT_GT(most_recent_browser_state->tabs.size(),
            previous_browser_state->tabs.size());

  base::Optional<TabState> active_tab =
      GetStateForActiveTab(most_recent_browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
}

void WebAppIntegrationBrowserTestBase::AssertUserDisplayModeInternal(
    DisplayMode display_mode) {
  DCHECK(after_action_state_);
  base::Optional<AppState> app_state =
      GetStateForAppId(after_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(display_mode, app_state->user_display_mode);
}

void WebAppIntegrationBrowserTestBase::AssertWindowClosed() {
  DCHECK(before_action_state_);
  DCHECK(after_action_state_);
  base::Optional<ProfileState> after_action_profile =
      GetStateForProfile(after_action_state_.get(), profile());
  base::Optional<ProfileState> before_action_profile =
      GetStateForProfile(before_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  EXPECT_LT(after_action_profile->browsers.size(),
            before_action_profile->browsers.size());
}

void WebAppIntegrationBrowserTestBase::AssertWindowCreated() {
  DCHECK(before_action_state_);
  DCHECK(after_action_state_);
  base::Optional<ProfileState> after_action_profile =
      GetStateForProfile(after_action_state_.get(), profile());
  base::Optional<ProfileState> before_action_profile =
      GetStateForProfile(before_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  EXPECT_GT(after_action_profile->browsers.size(),
            before_action_profile->browsers.size());
}

void WebAppIntegrationBrowserTestBase::AssertWindowDisplayMode(
    blink::mojom::DisplayMode display_mode) {
  DCHECK(app_browser());
  DCHECK(app_browser()->app_controller()->AsWebAppBrowserController());
  base::Optional<AppState> app_state =
      GetStateForAppId(after_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());

  content::WebContents* web_contents =
      app_browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);
  DisplayMode window_display_mode =
      web_contents->GetDelegate()->GetDisplayMode(web_contents);

  if (display_mode == blink::mojom::DisplayMode::kMinimalUi) {
    EXPECT_TRUE(app_browser()->app_controller()->HasMinimalUiButtons());
    EXPECT_EQ(app_state->effective_display_mode,
              blink::mojom::DisplayMode::kMinimalUi);
    EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kMinimalUi);
  } else if (display_mode == blink::mojom::DisplayMode::kStandalone) {
    EXPECT_FALSE(app_browser()->app_controller()->HasMinimalUiButtons());
    EXPECT_EQ(app_state->effective_display_mode,
              blink::mojom::DisplayMode::kStandalone);
    EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kStandalone);
  }
}

// Helpers
std::vector<AppId> WebAppIntegrationBrowserTestBase::GetAppIdsForProfile(
    Profile* profile) {
  return WebAppProvider::Get(profile)->registrar().GetAppIds();
}

GURL WebAppIntegrationBrowserTestBase::GetInstallableAppURL(
    const std::string& action_param) {
  std::string scope = action_param;
  return embedded_test_server()->GetURL(
      base::StringPrintf("/web_apps/%s/basic.html", scope.c_str()));
}

WebAppProvider* WebAppIntegrationBrowserTestBase::GetProviderForProfile(
    Profile* profile) {
  return WebAppProvider::Get(profile);
}

GURL WebAppIntegrationBrowserTestBase::GetNonInstallableAppURL() {
  return embedded_test_server()->GetURL("/web_apps/site_c/basic.html");
}

GURL WebAppIntegrationBrowserTestBase::GetInScopeURL(
    const std::string& action_param) {
  return GetInstallableAppURL(action_param);
}

GURL WebAppIntegrationBrowserTestBase::GetOutOfScopeURL(
    const std::string& action_param) {
  return embedded_test_server()->GetURL("/out_of_scope/index.html");
}

GURL WebAppIntegrationBrowserTestBase::GetURLForScope(
    const std::string& action_param) {
  return embedded_test_server()->GetURL(
      base::StringPrintf("/web_apps/%s/", action_param.c_str()));
}

content::WebContents* WebAppIntegrationBrowserTestBase::GetCurrentTab(
    Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

void WebAppIntegrationBrowserTestBase::ForceUpdateManifestContents(
    const std::string& app_scope,
    GURL app_url_with_manifest_param) {
  base::Optional<AppState> app_state =
      GetAppByScope(before_action_state_.get(), profile(), app_scope);
  ASSERT_TRUE(app_state.has_value());
  auto app_id = app_state->id;

  // Manifest updates must occur as the first navigation after a webapp is
  // installed, otherwise the throttle is tripped.
  ASSERT_FALSE(
      GetProvider()->manifest_update_manager().IsUpdateConsumed(app_id));
  NavigateTabbedBrowserToSite(app_url_with_manifest_param);
}

Browser* WebAppIntegrationBrowserTestBase::browser() {
  Browser* browser = active_browser_
                         ? active_browser_
                         : chrome::FindTabbedBrowser(
                               profile(), /*match_original_profiles=*/false);
  DCHECK(browser);
  if (!browser->tab_strip_model()->count()) {
    delegate_->AddBlankTabAndShow(browser);
  }
  return browser;
}

PageActionIconView* WebAppIntegrationBrowserTestBase::pwa_install_view() {
  PageActionIconView* pwa_install_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kPwaInstall);
  DCHECK(pwa_install_view);
  return pwa_install_view;
}

StateSnapshot WebAppIntegrationBrowserTestBase::ConstructStateSnapshot() {
  base::flat_map<Profile*, ProfileState> profile_state_map;
  for (Profile* profile : delegate_->GetAllProfiles()) {
    base::flat_map<Browser*, BrowserState> browser_state;
    auto* browser_list = BrowserList::GetInstance();
    for (Browser* browser : *browser_list) {
      if (browser->profile() != profile) {
        continue;
      }

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
      content::WebContents* active_tab = tabs->GetActiveWebContents();
      bool is_app_browser = AppBrowserController::IsWebApp(browser);
      bool install_icon_visible = false;
      bool launch_icon_visible = false;
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

    auto* registrar =
        GetProviderForProfile(profile)->registrar().AsWebAppRegistrar();
    auto app_ids = registrar->GetAppIds();
    base::flat_map<AppId, AppState> app_state;
    for (const auto& app_id : app_ids) {
      app_state.emplace(app_id,
                        AppState(app_id, registrar->GetAppShortName(app_id),
                                 registrar->GetAppScope(app_id),
                                 registrar->GetAppEffectiveDisplayMode(app_id),
                                 registrar->GetAppUserDisplayMode(app_id),
                                 registrar->IsLocallyInstalled(app_id)));
    }
    profile_state_map.emplace(
        profile, ProfileState(std::move(browser_state), std::move(app_state)));
  }
  return StateSnapshot(profile_state_map);
}

const net::EmbeddedTestServer*
WebAppIntegrationBrowserTestBase::embedded_test_server() {
  return delegate_->EmbeddedTestServer();
}

GURL WebAppIntegrationBrowserTestBase::GetAppURLForManifest(
    const std::string& action_scope,
    DisplayMode display_mode) {
  std::string str_template = "/web_apps/%s/basic.html";
  if (display_mode == blink::mojom::DisplayMode::kMinimalUi) {
    str_template += "?manifest=manifest_minimal_ui.json";
  }
  return embedded_test_server()->GetURL(
      base::StringPrintf(str_template.c_str(), action_scope.c_str()));
}
}  // namespace web_app
