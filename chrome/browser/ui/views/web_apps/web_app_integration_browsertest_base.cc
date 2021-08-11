// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"

#include "base/base_paths.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/re2/src/re2/re2.h"

namespace web_app {

namespace {

constexpr char kPlatformName[] =
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    "ChromeOS";
#elif defined(OS_LINUX)
    "Linux";
#elif defined(OS_MAC)
    "Mac";
#elif defined(OS_WIN)
    "Win";
#elif defined(OS_FUCHSIA)
    "Fuchsia";
#else
#error "Unknown platform"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const base::flat_map<std::string, std::string> scope_to_path = {
    {"SiteA", "site_a"},
    {"SiteB", "site_b"},
    {"SiteC", "site_c"},
    {"SiteAFoo", "site_a/foo"},
    {"SiteABar", "site_a/bar"}};

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

void WebAppIntegrationBrowserTestBase::OnWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  bool is_waiting = app_ids_with_pending_manifest_updates_.erase(app_id);
  ASSERT_TRUE(is_waiting) << "Received manifest update that was unexpected";
  if (waiting_for_update_id_ && app_id == waiting_for_update_id_.value()) {
    DCHECK(waiting_for_update_run_loop_);
    waiting_for_update_run_loop_->Quit();
    waiting_for_update_id_ = absl::nullopt;
  }
}

// static
absl::optional<AppState> WebAppIntegrationBrowserTestBase::GetAppByScope(
    StateSnapshot* state_snapshot,
    Profile* profile,
    const std::string& action_mode) {
  absl::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return absl::nullopt;
  }

  GURL scope = GetURLForScope(action_mode);
  auto it =
      std::find_if(profile_state->apps.begin(), profile_state->apps.end(),
                   [scope](std::pair<web_app::AppId, AppState>& app_entry) {
                     return app_entry.second.scope == scope;
                   });

  return it == profile_state->apps.end()
             ? absl::nullopt
             : absl::make_optional<AppState>(it->second);
}

// static
absl::optional<TabState> WebAppIntegrationBrowserTestBase::GetStateForActiveTab(
    BrowserState browser_state) {
  if (!browser_state.active_tab) {
    return absl::nullopt;
  }

  auto it = browser_state.tabs.find(browser_state.active_tab);
  DCHECK(it != browser_state.tabs.end());
  return absl::make_optional<TabState>(it->second);
}

// static
absl::optional<AppState> WebAppIntegrationBrowserTestBase::GetStateForAppId(
    StateSnapshot* state_snapshot,
    Profile* profile,
    web_app::AppId id) {
  absl::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return absl::nullopt;
  }

  auto it = profile_state->apps.find(id);
  return it == profile_state->apps.end()
             ? absl::nullopt
             : absl::make_optional<AppState>(it->second);
}

// static
absl::optional<BrowserState>
WebAppIntegrationBrowserTestBase::GetStateForBrowser(
    StateSnapshot* state_snapshot,
    Profile* profile,
    Browser* browser) {
  absl::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return absl::nullopt;
  }

  auto it = profile_state->browsers.find(browser);
  return it == profile_state->browsers.end()
             ? absl::nullopt
             : absl::make_optional<BrowserState>(it->second);
}

// static
absl::optional<ProfileState>
WebAppIntegrationBrowserTestBase::GetStateForProfile(
    StateSnapshot* state_snapshot,
    Profile* profile) {
  DCHECK(state_snapshot);
  DCHECK(profile);
  auto it = state_snapshot->profiles.find(profile);
  return it == state_snapshot->profiles.end()
             ? absl::nullopt
             : absl::make_optional<ProfileState>(it->second);
}

void WebAppIntegrationBrowserTestBase::SetUp(base::FilePath test_data_dir) {
  webapps::TestAppBannerManagerDesktop::SetUp();
}

void WebAppIntegrationBrowserTestBase::SetUpOnMainThread() {
  os_hooks_suppress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  if (!delegate_->IsSyncTest()) {
    observation_.Reset();
    observation_.Observe(&GetProvider()->registrar());
  }
}

void WebAppIntegrationBrowserTestBase::TearDownOnMainThread() {
  observation_.Reset();
}

void WebAppIntegrationBrowserTestBase::BeforeStateChangeAction() {
  if (after_state_change_action_state_) {
    before_state_change_action_state_ =
        std::move(after_state_change_action_state_);
  } else {
    before_state_change_action_state_ =
        std::make_unique<StateSnapshot>(ConstructStateSnapshot());
  }
}

void WebAppIntegrationBrowserTestBase::AfterStateChangeAction() {
  after_state_change_action_state_ =
      std::make_unique<StateSnapshot>(ConstructStateSnapshot());
  MaybeWaitForManifestUpdates(profile());
}

void WebAppIntegrationBrowserTestBase::BeforeStateCheckAction() {
  DCHECK(after_state_change_action_state_);
}

void WebAppIntegrationBrowserTestBase::AfterStateCheckAction() {
  DCHECK(!after_state_change_action_state_ ||
         *after_state_change_action_state_ == ConstructStateSnapshot());
}

// State change actions implemented before state check actions. Implemented in
// alphabetical order.
void WebAppIntegrationBrowserTestBase::InstallPolicyAppInternal(
    const std::string& action_mode,
    base::Value default_launch_container,
    const bool create_shortcut) {
  GURL url = GetInstallableAppURL(action_mode);
  WebAppRegistrar& web_app_registrar =
      WebAppProvider::Get(profile())->registrar();
  base::RunLoop run_loop;
  WebAppInstallObserver observer(profile());
  observer.SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        bool is_installed = web_app_registrar.IsInstalled(app_id);
        GURL installed_url = web_app_registrar.GetAppStartUrl(app_id);
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
    item.SetKey(kCreateDesktopShortcutKey, base::Value(create_shortcut));
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kWebAppInstallForceList);
    update->Append(item.Clone());
  }
  run_loop.Run();
}

void WebAppIntegrationBrowserTestBase::ClosePwa() {
  DCHECK(app_browser_);
  app_browser_->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser_);
}

void WebAppIntegrationBrowserTestBase::InstallCreateShortcutTabbed(
    const std::string& action_scope) {
  MaybeNavigateTabbedBrowserInScope(action_scope);
  InstallCreateShortcut(/*open_in_window=*/false);
}

void WebAppIntegrationBrowserTestBase::InstallCreateShortcutWindowed(
    const std::string& action_scope) {
  MaybeNavigateTabbedBrowserInScope(action_scope);
  InstallCreateShortcut(/*open_in_window=*/true);
}

void WebAppIntegrationBrowserTestBase::InstallMenuOption(
    const std::string& action_scope) {
  MaybeNavigateTabbedBrowserInScope(action_scope);
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(/*auto_accept=*/true);
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  WebAppInstallObserver observer(profile());
  CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
  active_app_id_ = observer.AwaitNextInstall();
  app_loaded_observer.Wait();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(/*auto_accept=*/false);
  auto* browser_list = BrowserList::GetInstance();
  app_browser_ = browser_list->GetLastActive();
  DCHECK(AppBrowserController::IsWebApp(app_browser_));
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

void WebAppIntegrationBrowserTestBase::InstallOmniboxIcon(
    const std::string& action_scope) {
  MaybeNavigateTabbedBrowserInScope(action_scope);
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);

  web_app::AppId app_id;
  base::RunLoop run_loop;
  web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&app_id, &run_loop](const web_app::AppId& installed_app_id,
                           web_app::InstallResultCode code) {
        app_id = installed_app_id;
        run_loop.Quit();
      }));

  ASSERT_TRUE(pwa_install_view()->GetVisible());
  pwa_install_view()->ExecuteForTesting();

  run_loop.Run();

  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  active_app_id_ = app_id;
  auto* browser_list = BrowserList::GetInstance();
  app_browser_ = browser_list->GetLastActive();
  DCHECK(AppBrowserController::IsWebApp(app_browser_));
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppTabbedNoShortcut(
    const std::string& action_mode) {
  InstallPolicyAppInternal(action_mode,
                           base::Value(kDefaultLaunchContainerTabValue),
                           /*create_shortcut=*/false);
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppTabbedShortcut(
    const std::string& action_mode) {
  InstallPolicyAppInternal(action_mode,
                           base::Value(kDefaultLaunchContainerTabValue),
                           /*create_shortcut=*/true);
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppWindowedNoShortcut(
    const std::string& action_mode) {
  InstallPolicyAppInternal(action_mode,
                           base::Value(kDefaultLaunchContainerWindowValue),
                           /*create_shortcut=*/false);
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppWindowedShortcut(
    const std::string& action_mode) {
  InstallPolicyAppInternal(action_mode,
                           base::Value(kDefaultLaunchContainerWindowValue),
                           /*create_shortcut=*/true);
}

void WebAppIntegrationBrowserTestBase::LaunchInternal(
    const std::string& action_mode) {
  absl::optional<AppState> app_state = GetAppByScope(
      before_state_change_action_state_.get(), profile(), action_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_mode;
  auto app_id = app_state->id;
  auto* web_app_provider = GetProvider();
  WebAppRegistrar& app_registrar = web_app_provider->registrar();
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
  app_ids_ = WebAppProvider::Get(profile())->registrar().GetAppIds();
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

void WebAppIntegrationBrowserTestBase::NavigateBrowser(
    const std::string& scope) {
  NavigateTabbedBrowserToSite(GetInScopeURL(scope));
}

void WebAppIntegrationBrowserTestBase::SetOpenInTab(
    const std::string& action_mode) {
  absl::optional<AppState> app_state = GetAppByScope(
      before_state_change_action_state_.get(), profile(), action_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_mode;
  auto app_id = app_state->id;
  auto& app_registry_controller =
      WebAppProvider::Get(profile())->registry_controller();
  app_registry_controller.SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kBrowser, true);
}

void WebAppIntegrationBrowserTestBase::SetOpenInWindow(
    const std::string& action_mode) {
  absl::optional<AppState> app_state = GetAppByScope(
      before_state_change_action_state_.get(), profile(), action_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for scope: " << action_mode;
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

void WebAppIntegrationBrowserTestBase::SyncTurnOff() {
  delegate_->SyncTurnOff();
}

void WebAppIntegrationBrowserTestBase::SyncTurnOn() {
  delegate_->SyncTurnOn();
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

void WebAppIntegrationBrowserTestBase::UninstallPolicyApp(
    const std::string& action_mode) {
  GURL url = GetInstallableAppURL(action_mode);
  auto policy_app = GetAppByScope(before_state_change_action_state_.get(),
                                  profile(), action_mode);
  DCHECK(policy_app);
  base::RunLoop run_loop;
  WebAppInstallObserver observer(profile());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        if (policy_app->id == app_id) {
          run_loop.Quit();
        }
      }));
  // If there are still install sources, the app might not be fully uninstalled,
  // so this will listen for the removal of the policy install source.
  GetProvider()->install_finalizer().SetRemoveSourceCallbackForTesting(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        if (policy_app->id == app_id)
          run_loop.Quit();
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

void WebAppIntegrationBrowserTestBase::ManifestUpdateDisplayMinimal(
    const std::string& action_scope) {
  // TODO(jarrydg): Create a map of supported manifest updates keyed on scope.
  ASSERT_EQ("SiteA", action_scope);
  ForceUpdateManifestContents(
      action_scope, GetAppURLForManifest(
                        action_scope, blink::mojom::DisplayMode::kMinimalUi));
}

// State Check Actions
void WebAppIntegrationBrowserTestBase::CheckAppLocallyInstalledInternal() {
  absl::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_TRUE(app_state->is_installed_locally);
}

void WebAppIntegrationBrowserTestBase::CheckAppInListNotLocallyInstalled(
    const std::string& action_mode) {
  absl::optional<AppState> app_state = GetAppByScope(
      after_state_change_action_state_.get(), profile(), action_mode);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_FALSE(app_state->is_installed_locally);
}

void WebAppIntegrationBrowserTestBase::CheckAppNotInList(
    const std::string& action_mode) {
  absl::optional<AppState> app_state = GetAppByScope(
      after_state_change_action_state_.get(), profile(), action_mode);
  EXPECT_FALSE(app_state.has_value());
}

void WebAppIntegrationBrowserTestBase::CheckInstallable() {
  absl::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  absl::optional<TabState> active_tab =
      GetStateForActiveTab(browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
  EXPECT_TRUE(active_tab->is_installable);
}

void WebAppIntegrationBrowserTestBase::CheckInstallIconShown() {
  absl::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->install_icon_shown);
  EXPECT_TRUE(pwa_install_view()->GetVisible());
}

void WebAppIntegrationBrowserTestBase::CheckInstallIconNotShown() {
  absl::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->install_icon_shown);
  EXPECT_FALSE(pwa_install_view()->GetVisible());
}

void WebAppIntegrationBrowserTestBase::CheckLaunchIconShown() {
  absl::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_TRUE(browser_state->launch_icon_shown);
}

void WebAppIntegrationBrowserTestBase::CheckLaunchIconNotShown() {
  absl::optional<BrowserState> browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(browser_state.has_value());
  EXPECT_FALSE(browser_state->launch_icon_shown);
}

void WebAppIntegrationBrowserTestBase::CheckManifestDisplayModeInternal(
    DisplayMode display_mode) {
  absl::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(display_mode, app_state->effective_display_mode);
}

void WebAppIntegrationBrowserTestBase::CheckTabCreated() {
  DCHECK(before_state_change_action_state_);
  absl::optional<BrowserState> most_recent_browser_state = GetStateForBrowser(
      after_state_change_action_state_.get(), profile(), browser());
  absl::optional<BrowserState> previous_browser_state = GetStateForBrowser(
      before_state_change_action_state_.get(), profile(), browser());
  ASSERT_TRUE(most_recent_browser_state.has_value());
  ASSERT_TRUE(previous_browser_state.has_value());
  EXPECT_GT(most_recent_browser_state->tabs.size(),
            previous_browser_state->tabs.size());

  absl::optional<TabState> active_tab =
      GetStateForActiveTab(most_recent_browser_state.value());
  ASSERT_TRUE(active_tab.has_value());
}

void WebAppIntegrationBrowserTestBase::CheckUserDisplayModeInternal(
    DisplayMode display_mode) {
  absl::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(display_mode, app_state->user_display_mode);
}

void WebAppIntegrationBrowserTestBase::CheckWindowClosed() {
  DCHECK(before_state_change_action_state_);
  absl::optional<ProfileState> after_action_profile =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  absl::optional<ProfileState> before_action_profile =
      GetStateForProfile(before_state_change_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  EXPECT_LT(after_action_profile->browsers.size(),
            before_action_profile->browsers.size());
}

void WebAppIntegrationBrowserTestBase::CheckWindowCreated() {
  DCHECK(before_state_change_action_state_);
  absl::optional<ProfileState> after_action_profile =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  absl::optional<ProfileState> before_action_profile =
      GetStateForProfile(before_state_change_action_state_.get(), profile());
  ASSERT_TRUE(after_action_profile.has_value());
  ASSERT_TRUE(before_action_profile.has_value());
  EXPECT_GT(after_action_profile->browsers.size(),
            before_action_profile->browsers.size());
}

void WebAppIntegrationBrowserTestBase::CheckWindowDisplayMode(
    blink::mojom::DisplayMode display_mode) {
  DCHECK(app_browser());
  DCHECK(app_browser()->app_controller()->AsWebAppBrowserController());
  absl::optional<AppState> app_state = GetStateForAppId(
      after_state_change_action_state_.get(), profile(), active_app_id_);
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
std::string WebAppIntegrationBrowserTestBase::BuildLogForTest(
    const std::vector<std::string>& testing_actions,
    bool is_sync_test) {
  const std::string test_case = base::JoinString(testing_actions, ", ");
  return base::StringPrintf(
      "Current test case: %s\n"
      "To disable this test, add the following line to "
      "//chrome/test/data/web_apps/TestExpectations (without the quotes):\n"
      "\"crbug.com/XXXXX [ %s ] [ Skip ] %s\"\n"
      "To run this test in isolation, run the following command:\n"
      "out/Default/%s_tests --gtest_filter=\"*%s*\" "
      "--web-app-integration-test-case=%s\n",
      test_case.c_str(), kPlatformName, test_case.c_str(),
      is_sync_test ? "sync_integration" : "browser",
      is_sync_test ? "TwoClientWebAppsIntegrationSyncTest"
                   : "WebAppIntegrationBrowserTest",
      test_case.c_str());
}

std::vector<AppId> WebAppIntegrationBrowserTestBase::GetAppIdsForProfile(
    Profile* profile) {
  return WebAppProvider::Get(profile)->registrar().GetAppIds();
}

GURL WebAppIntegrationBrowserTestBase::GetInstallableAppURL(
    const std::string& scope) {
  DCHECK(scope_to_path.contains(scope));
  auto scope_url_path = scope_to_path.find(scope)->second;
  return embedded_test_server()->GetURL(
      base::StringPrintf("/web_apps/%s/basic.html", scope_url_path.c_str()));
}

WebAppProvider* WebAppIntegrationBrowserTestBase::GetProviderForProfile(
    Profile* profile) {
  return WebAppProvider::Get(profile);
}

void WebAppIntegrationBrowserTestBase::ResetRegistrarObserver() {
  observation_.Reset();
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

    WebAppRegistrar& registrar = GetProviderForProfile(profile)->registrar();
    auto app_ids = registrar.GetAppIds();
    base::flat_map<AppId, AppState> app_state;
    for (const auto& app_id : app_ids) {
      app_state.emplace(app_id,
                        AppState(app_id, registrar.GetAppShortName(app_id),
                                 registrar.GetAppScope(app_id),
                                 registrar.GetAppEffectiveDisplayMode(app_id),
                                 registrar.GetAppUserDisplayMode(app_id),
                                 registrar.IsLocallyInstalled(app_id)));
    }
    profile_state_map.emplace(
        profile, ProfileState(std::move(browser_state), std::move(app_state)));
  }
  return StateSnapshot(profile_state_map);
}

GURL WebAppIntegrationBrowserTestBase::GetAppURLForManifest(
    const std::string& scope,
    DisplayMode display_mode) {
  DCHECK(scope_to_path.contains(scope));
  auto scope_url_path = scope_to_path.find(scope)->second;
  std::string str_template = "/web_apps/%s/basic.html";
  if (display_mode == blink::mojom::DisplayMode::kMinimalUi) {
    str_template += "?manifest=manifest_minimal_ui.json";
  }
  return embedded_test_server()->GetURL(
      base::StringPrintf(str_template.c_str(), scope_url_path.c_str()));
}

content::WebContents* WebAppIntegrationBrowserTestBase::GetCurrentTab(
    Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

GURL WebAppIntegrationBrowserTestBase::GetInScopeURL(
    const std::string& action_mode) {
  return GetInstallableAppURL(action_mode);
}

GURL WebAppIntegrationBrowserTestBase::GetNonInstallableAppURL() {
  return embedded_test_server()->GetURL("/web_apps/site_c/basic.html");
}

GURL WebAppIntegrationBrowserTestBase::GetOutOfScopeURL(
    const std::string& action_mode) {
  return embedded_test_server()->GetURL("/out_of_scope/index.html");
}

GURL WebAppIntegrationBrowserTestBase::GetURLForScope(
    const std::string& scope) {
  DCHECK(scope_to_path.contains(scope));
  auto scope_url_path = scope_to_path.find(scope)->second;
  return embedded_test_server()->GetURL(
      base::StringPrintf("/web_apps/%s/", scope_url_path.c_str()));
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

bool WebAppIntegrationBrowserTestBase::AreNoAppWindowsOpen(
    Profile* profile,
    const AppId& app_id) {
  auto* provider = GetProviderForProfile(profile);
  const GURL& app_scope = provider->registrar().GetAppScope(app_id);
  auto* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    if (browser->IsAttemptingToCloseBrowser()) {
      continue;
    }
    const GURL& browser_url =
        browser->tab_strip_model()->GetActiveWebContents()->GetURL();
    if (AppBrowserController::IsWebApp(browser) &&
        IsInScope(browser_url, app_scope)) {
      return false;
    }
  }
  return true;
}

void WebAppIntegrationBrowserTestBase::ForceUpdateManifestContents(
    const std::string& app_scope,
    GURL app_url_with_manifest_param) {
  absl::optional<AppState> app_state = GetAppByScope(
      before_state_change_action_state_.get(), profile(), app_scope);
  ASSERT_TRUE(app_state.has_value());
  auto app_id = app_state->id;
  active_app_id_ = app_id;

  // Manifest updates must occur as the first navigation after a webapp is
  // installed, otherwise the throttle is tripped.
  ASSERT_FALSE(
      GetProvider()->manifest_update_manager().IsUpdateConsumed(app_id));
  NavigateTabbedBrowserToSite(app_url_with_manifest_param);
  app_ids_with_pending_manifest_updates_.insert(app_id);
}

void WebAppIntegrationBrowserTestBase::MaybeWaitForManifestUpdates(
    Profile* profile) {
  bool continue_checking_for_updates = true;
  while (continue_checking_for_updates) {
    continue_checking_for_updates = false;
    for (const AppId& app_id : app_ids_with_pending_manifest_updates_) {
      if (AreNoAppWindowsOpen(profile, app_id)) {
        waiting_for_update_id_ = absl::make_optional(app_id);
        waiting_for_update_run_loop_ = std::make_unique<base::RunLoop>();
        waiting_for_update_run_loop_->Run();
        waiting_for_update_run_loop_ = nullptr;
        DCHECK(!waiting_for_update_id_);
        // To prevent iteration-during-modification, break and restart
        // the loop.
        continue_checking_for_updates = true;
        break;
      }
    }
  }
}

void WebAppIntegrationBrowserTestBase::MaybeNavigateTabbedBrowserInScope(
    const std::string& scope) {
  auto browser_url = GetCurrentTab(browser())->GetURL();
  auto dest_url = GetInScopeURL(scope);
  if (browser_url.is_empty() || browser_url != dest_url) {
    NavigateTabbedBrowserToSite(dest_url);
  }
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

const net::EmbeddedTestServer*
WebAppIntegrationBrowserTestBase::embedded_test_server() {
  return delegate_->EmbeddedTestServer();
}

PageActionIconView* WebAppIntegrationBrowserTestBase::pwa_install_view() {
  PageActionIconView* pwa_install_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kPwaInstall);
  DCHECK(pwa_install_view);
  return pwa_install_view;
}
}  // namespace web_app
