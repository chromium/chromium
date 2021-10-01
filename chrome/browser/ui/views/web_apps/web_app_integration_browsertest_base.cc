// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_integration_browsertest_base.h"

#include <ostream>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"

namespace web_app {

namespace {

const base::flat_map<std::string, std::string> site_mode_to_path = {
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

std::ostream& operator<<(std::ostream& os, const StateSnapshot& state) {
  base::Value root(base::Value::Type::DICTIONARY);
  base::Value& profiles_value =
      *root.SetKey("profiles", base::Value(base::Value::Type::DICTIONARY));
  for (const auto& profile_pair : state.profiles) {
    base::Value profile_value(base::Value::Type::DICTIONARY);

    base::Value browsers_value(base::Value::Type::DICTIONARY);
    const ProfileState& profile = profile_pair.second;
    for (const auto& browser_pair : profile.browsers) {
      base::Value browser_value(base::Value::Type::DICTIONARY);
      const BrowserState& browser = browser_pair.second;

      browser_value.SetStringKey("browser",
                                 base::StringPrintf("%p", browser.browser));

      base::Value tab_values(base::Value::Type::DICTIONARY);
      for (const auto& tab_pair : browser.tabs) {
        base::Value tab_value(base::Value::Type::DICTIONARY);
        const TabState& tab = tab_pair.second;
        tab_value.SetStringKey("url", tab.url.spec());
        tab_value.SetBoolKey("is_installable", tab.is_installable);
        tab_values.SetKey(base::StringPrintf("%p", tab_pair.first),
                          std::move(tab_value));
      }
      browser_value.SetKey("tabs", std::move(tab_values));
      browser_value.SetStringKey("active_tab",
                                 base::StringPrintf("%p", browser.active_tab));
      browser_value.SetBoolKey("is_app_browser", browser.is_app_browser);
      browser_value.SetBoolKey("install_icon_shown",
                               browser.install_icon_shown);
      browser_value.SetBoolKey("launch_icon_shown", browser.launch_icon_shown);

      browsers_value.SetKey(base::StringPrintf("%p", browser_pair.first),
                            std::move(browser_value));
    }
    base::Value app_values(base::Value::Type::DICTIONARY);
    for (const auto& app_pair : profile.apps) {
      base::Value app_value(base::Value::Type::DICTIONARY);
      const AppState& app = app_pair.second;

      app_value.SetStringKey("id", app.id);
      app_value.SetStringKey("name", app.name);
      app_value.SetIntKey("effective_display_mode",
                          static_cast<int>(app.effective_display_mode));
      app_value.SetIntKey("user_display_mode",
                          static_cast<int>(app.effective_display_mode));
      app_value.SetBoolKey("is_installed_locally", app.is_installed_locally);

      app_values.SetKey(app_pair.first, std::move(app_value));
    }

    profile_value.SetKey("browsers", std::move(browsers_value));
    profile_value.SetKey("apps", std::move(app_values));
    profiles_value.SetKey(base::StringPrintf("%p", profile_pair.first),
                          std::move(profile_value));
  }
  os << root.DebugString();
  return os;
}

WebAppIntegrationBrowserTestBase::WebAppIntegrationBrowserTestBase(
    TestDelegate* delegate)
    : delegate_(delegate) {}

WebAppIntegrationBrowserTestBase::~WebAppIntegrationBrowserTestBase() = default;

void WebAppIntegrationBrowserTestBase::OnWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  DCHECK_EQ(1ul, delegate_->GetAllProfiles().size())
      << "Manifest update waiting only supported on single profile tests.";
  bool is_waiting = app_ids_with_pending_manifest_updates_.erase(app_id);
  ASSERT_TRUE(is_waiting) << "Received manifest update that was unexpected";
  if (waiting_for_update_id_ && app_id == waiting_for_update_id_.value()) {
    DCHECK(waiting_for_update_run_loop_);
    waiting_for_update_run_loop_->Quit();
    waiting_for_update_id_ = absl::nullopt;
  }
}

// static
absl::optional<AppState> WebAppIntegrationBrowserTestBase::GetAppBySiteMode(
    StateSnapshot* state_snapshot,
    Profile* profile,
    const std::string& site_mode) {
  absl::optional<ProfileState> profile_state =
      GetStateForProfile(state_snapshot, profile);
  if (!profile_state) {
    return absl::nullopt;
  }

  GURL scope = GetURLForSiteMode(site_mode);
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

void WebAppIntegrationBrowserTestBase::SetUp() {
  webapps::TestAppBannerManagerDesktop::SetUp();
}

void WebAppIntegrationBrowserTestBase::SetUpOnMainThread() {
  os_hooks_suppress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();

  // Only support manifest updates on non-sync tests, as the current
  // infrastructure here only supports listening on one profile.
  if (!delegate_->IsSyncTest()) {
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
  MaybeWaitForManifestUpdates();
}

void WebAppIntegrationBrowserTestBase::BeforeStateCheckAction() {
  DCHECK(after_state_change_action_state_);
}

void WebAppIntegrationBrowserTestBase::AfterStateCheckAction() {
  if (!after_state_change_action_state_)
    return;
  DCHECK_EQ(*after_state_change_action_state_, ConstructStateSnapshot());
}

// State change actions implemented before state check actions. Implemented in
// alphabetical order.
void WebAppIntegrationBrowserTestBase::InstallPolicyAppInternal(
    const std::string& site_mode,
    base::Value default_launch_container,
    const bool create_shortcut) {
  GURL url = GetInstallableAppURL(site_mode);
  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
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
  active_app_id_ = observer.Wait();
}

void WebAppIntegrationBrowserTestBase::ClosePwa() {
  DCHECK(app_browser_);
  app_browser_->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser_);
}

void WebAppIntegrationBrowserTestBase::InstallCreateShortcutTabbed(
    const std::string& site_mode) {
  MaybeNavigateTabbedBrowserInScope(site_mode);
  InstallCreateShortcut(/*open_in_window=*/false);
}

void WebAppIntegrationBrowserTestBase::InstallCreateShortcutWindowed(
    const std::string& site_mode) {
  MaybeNavigateTabbedBrowserInScope(site_mode);
  InstallCreateShortcut(/*open_in_window=*/true);
}

void WebAppIntegrationBrowserTestBase::InstallMenuOption(
    const std::string& site_mode) {
  MaybeNavigateTabbedBrowserInScope(site_mode);
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(/*auto_accept=*/true);
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  CHECK(chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA));
  app_loaded_observer.Wait();
  active_app_id_ = install_observer.Wait();
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(/*auto_accept=*/false);
  BrowserList* browser_list = BrowserList::GetInstance();
  app_browser_ = nullptr;
  for (Browser* browser : *browser_list) {
    if (AppBrowserController::IsForWebApp(browser, active_app_id_)) {
      app_browser_ = browser;
      return;
    }
  }
  NOTREACHED() << "Unable to find app browser for app " << active_app_id_;
}

void WebAppIntegrationBrowserTestBase::InstallLocally(
    const std::string& site_mode) {
  MaybeNavigateTabbedBrowserInScope(site_mode);
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

  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  handler.HandleInstallAppLocally(&web_app_ids);
  observer.Wait();
}

void WebAppIntegrationBrowserTestBase::InstallOmniboxIcon(
    const std::string& site_mode) {
  MaybeNavigateTabbedBrowserInScope(site_mode);
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);

  web_app::AppId app_id;
  base::RunLoop run_loop;
  web_app::SetInstalledCallbackForTesting(base::BindLambdaForTesting(
      [&app_id, &run_loop](const web_app::AppId& installed_app_id,
                           web_app::InstallResultCode code) {
        app_id = installed_app_id;
        run_loop.Quit();
      }));
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());

  ASSERT_TRUE(pwa_install_view()->GetVisible());
  WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  pwa_install_view()->ExecuteForTesting();

  run_loop.Run();
  app_loaded_observer.Wait();
  active_app_id_ = install_observer.Wait();
  DCHECK_EQ(app_id, active_app_id_);

  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  auto* browser_list = BrowserList::GetInstance();
  app_browser_ = nullptr;
  for (Browser* browser : *browser_list) {
    if (AppBrowserController::IsForWebApp(browser, active_app_id_)) {
      app_browser_ = browser;
      return;
    }
  }
  NOTREACHED() << "Unable to find app browser for app " << active_app_id_;
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppTabbedNoShortcut(
    const std::string& site_mode) {
  InstallPolicyAppInternal(site_mode,
                           base::Value(kDefaultLaunchContainerTabValue),
                           /*create_shortcut=*/false);
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppTabbedShortcut(
    const std::string& site_mode) {
  InstallPolicyAppInternal(site_mode,
                           base::Value(kDefaultLaunchContainerTabValue),
                           /*create_shortcut=*/true);
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppWindowedNoShortcut(
    const std::string& site_mode) {
  InstallPolicyAppInternal(site_mode,
                           base::Value(kDefaultLaunchContainerWindowValue),
                           /*create_shortcut=*/false);
}

void WebAppIntegrationBrowserTestBase::InstallPolicyAppWindowedShortcut(
    const std::string& site_mode) {
  InstallPolicyAppInternal(site_mode,
                           base::Value(kDefaultLaunchContainerWindowValue),
                           /*create_shortcut=*/true);
}

void WebAppIntegrationBrowserTestBase::LaunchFromChromeApps(
    const std::string& site_mode) {
  absl::optional<AppState> app_state = GetAppBySiteMode(
      before_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for site: " << site_mode;
  auto app_id = app_state->id;
  auto* web_app_provider = GetProvider();
  WebAppRegistrar& app_registrar = web_app_provider->registrar();
  DisplayMode display_mode = app_registrar.GetAppEffectiveDisplayMode(app_id);
  if (display_mode == blink::mojom::DisplayMode::kBrowser) {
    ui_test_utils::UrlLoadObserver url_observer(
        app_registrar.GetAppLaunchUrl(app_id),
        content::NotificationService::AllSources());
    Browser* browser = LaunchBrowserForWebAppInTab(profile(), app_id);
    url_observer.Wait();
    auto* app_banner_manager =
        webapps::TestAppBannerManagerDesktop::FromWebContents(
            GetCurrentTab(browser));
    app_banner_manager->WaitForInstallableCheck();
  } else {
    app_browser_ = LaunchWebAppBrowserAndWait(profile(), app_id);
  }
}

void WebAppIntegrationBrowserTestBase::NavigateTabbedBrowserToSite(
    const GURL& url) {
  DCHECK(browser());
  content::WebContents* web_contents = GetCurrentTab(browser());
  auto* app_banner_manager =
      webapps::TestAppBannerManagerDesktop::FromWebContents(web_contents);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  app_banner_manager->WaitForInstallableCheck();
}

void WebAppIntegrationBrowserTestBase::NavigateBrowser(
    const std::string& site_mode) {
  NavigateTabbedBrowserToSite(GetInScopeURL(site_mode));
}

void WebAppIntegrationBrowserTestBase::SetOpenInTab(
    const std::string& site_mode) {
  absl::optional<AppState> app_state = GetAppBySiteMode(
      before_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for site: " << site_mode;
  auto app_id = app_state->id;
  auto& sync_bridge = WebAppProvider::GetForTest(profile())->sync_bridge();
  sync_bridge.SetAppUserDisplayMode(app_id, blink::mojom::DisplayMode::kBrowser,
                                    true);
}

void WebAppIntegrationBrowserTestBase::SetOpenInWindow(
    const std::string& site_mode) {
  absl::optional<AppState> app_state = GetAppBySiteMode(
      before_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for site: " << site_mode;
  auto app_id = app_state->id;
  auto& sync_bridge = WebAppProvider::GetForTest(profile())->sync_bridge();
  sync_bridge.SetAppUserDisplayMode(
      app_id, blink::mojom::DisplayMode::kStandalone, true);
}

void WebAppIntegrationBrowserTestBase::SwitchProfileClients(
    const std::string& client_mode) {
  std::vector<Profile*> profiles = delegate_->GetAllProfiles();
  ASSERT_EQ(2U, profiles.size())
      << "Cannot switch profile clients if delegate only supports one profile";
  DCHECK(active_profile_);
  if (client_mode == "Client1") {
    active_profile_ = profiles[0];
  } else if (client_mode == "Client2") {
    active_profile_ = profiles[1];
  } else {
    NOTREACHED() << "Unknown client mode " << client_mode;
  }
  active_browser_ = chrome::FindTabbedBrowser(
      active_profile_, /*match_original_profiles=*/false);
  delegate_->AwaitWebAppQuiescence();
}

void WebAppIntegrationBrowserTestBase::SyncTurnOff() {
  delegate_->SyncTurnOff();
}

void WebAppIntegrationBrowserTestBase::SyncTurnOn() {
  delegate_->SyncTurnOn();
}

// TODO(https://crbug.com/1159651): Support this action on CrOS.
void WebAppIntegrationBrowserTestBase::UninstallFromMenu(
    const std::string& site_mode) {
  absl::optional<AppState> app_state = GetAppBySiteMode(
      before_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value())
      << "No app installed for site: " << site_mode;
  auto app_id = app_state->id;
  WebAppTestUninstallObserver observer(profile());
  observer.BeginListening({active_app_id_});

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(app_browser_);
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
  // The |app_menu_model| must be destroyed here, as the |observer| waits
  // until the app is fully uninstalled, which includes closing and deleting
  // the app_browser_.
  app_menu_model.reset();
  app_browser_ = nullptr;
  observer.Wait();
}

void WebAppIntegrationBrowserTestBase::UninstallPolicyApp(
    const std::string& site_mode) {
  GURL url = GetInstallableAppURL(site_mode);
  auto policy_app = GetAppBySiteMode(before_state_change_action_state_.get(),
                                     profile(), site_mode);
  DCHECK(policy_app);
  base::RunLoop run_loop;
  WebAppTestRegistryObserverAdapter observer(profile());
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
    const std::string& site_mode) {
  // TODO(dmurph): Create a map of supported manifest updates keyed on site
  // mode.
  ASSERT_EQ("SiteA", site_mode);
  ForceUpdateManifestContents(
      site_mode,
      GetAppURLForManifest(site_mode, blink::mojom::DisplayMode::kMinimalUi));
}

void WebAppIntegrationBrowserTestBase::CheckAppListEmpty() {
  absl::optional<ProfileState> state =
      GetStateForProfile(after_state_change_action_state_.get(), profile());
  ASSERT_TRUE(state.has_value());
  EXPECT_TRUE(state->apps.empty());
}

void WebAppIntegrationBrowserTestBase::CheckAppInListNotLocallyInstalled(
    const std::string& site_mode) {
  // Note: This is a partially supported action.
  absl::optional<AppState> app_state = GetAppBySiteMode(
      after_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_FALSE(app_state->is_installed_locally);
}

void WebAppIntegrationBrowserTestBase::CheckAppInListTabbed(
    const std::string& site_mode) {
  // Note: This is a partially supported action.
  absl::optional<AppState> app_state = GetAppBySiteMode(
      after_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(app_state->user_display_mode, blink::mojom::DisplayMode::kBrowser);
}

void WebAppIntegrationBrowserTestBase::CheckAppInListWindowed(
    const std::string& site_mode) {
  // Note: This is a partially supported action.
  absl::optional<AppState> app_state = GetAppBySiteMode(
      after_state_change_action_state_.get(), profile(), site_mode);
  ASSERT_TRUE(app_state.has_value());
  EXPECT_EQ(app_state->user_display_mode,
            blink::mojom::DisplayMode::kStandalone);
}

void WebAppIntegrationBrowserTestBase::CheckAppNotInList(
    const std::string& site_mode) {
  absl::optional<AppState> app_state = GetAppBySiteMode(
      after_state_change_action_state_.get(), profile(), site_mode);
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
            before_action_profile->browsers.size())
      << "Before: \n"
      << *before_state_change_action_state_ << "\nAfter:\n"
      << *after_state_change_action_state_;
}

void WebAppIntegrationBrowserTestBase::CheckWindowDisplayMinimal() {
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

  EXPECT_TRUE(app_browser()->app_controller()->HasMinimalUiButtons());
  EXPECT_EQ(app_state->effective_display_mode,
            blink::mojom::DisplayMode::kMinimalUi);
  EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kMinimalUi);
}

void WebAppIntegrationBrowserTestBase::CheckWindowDisplayStandalone() {
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

  EXPECT_FALSE(app_browser()->app_controller()->HasMinimalUiButtons());
  EXPECT_EQ(app_state->effective_display_mode,
            blink::mojom::DisplayMode::kStandalone);
  EXPECT_EQ(window_display_mode, blink::mojom::DisplayMode::kStandalone);
}

GURL WebAppIntegrationBrowserTestBase::GetInstallableAppURL(
    const std::string& site_mode) {
  DCHECK(site_mode_to_path.contains(site_mode));
  auto scope_url_path = site_mode_to_path.find(site_mode)->second;
  return embedded_test_server()->GetURL(
      base::StringPrintf("/web_apps/%s/basic.html", scope_url_path.c_str()));
}

WebAppProvider* WebAppIntegrationBrowserTestBase::GetProviderForProfile(
    Profile* profile) {
  return WebAppProvider::GetForTest(profile);
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
      if (!is_app_browser && active_tab != nullptr) {
        install_icon_visible = pwa_install_view()->GetVisible();
        launch_icon_visible = intent_picker_view()->GetVisible();
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
    const std::string& site_mode,
    DisplayMode display_mode) {
  DCHECK(site_mode_to_path.contains(site_mode));
  auto scope_url_path = site_mode_to_path.find(site_mode)->second;
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
    const std::string& site_mode) {
  return GetInstallableAppURL(site_mode);
}

GURL WebAppIntegrationBrowserTestBase::GetNonInstallableAppURL() {
  return embedded_test_server()->GetURL("/web_apps/site_c/basic.html");
}

GURL WebAppIntegrationBrowserTestBase::GetOutOfScopeURL(
    const std::string& site_mode) {
  return embedded_test_server()->GetURL("/out_of_scope/index.html");
}

GURL WebAppIntegrationBrowserTestBase::GetURLForSiteMode(
    const std::string& site_mode) {
  DCHECK(site_mode_to_path.contains(site_mode));
  auto scope_url_path = site_mode_to_path.find(site_mode)->second;
  return embedded_test_server()->GetURL(
      base::StringPrintf("/web_apps/%s/", scope_url_path.c_str()));
}

void WebAppIntegrationBrowserTestBase::InstallCreateShortcut(
    bool open_in_window) {
  chrome::SetAutoAcceptWebAppDialogForTesting(
      /*auto_accept=*/true,
      /*auto_open_in_window=*/open_in_window);
  WebAppTestInstallWithOsHooksObserver observer(profile());
  observer.BeginListening();
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  CHECK(chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT));
  active_app_id_ = observer.Wait();
  chrome::SetAutoAcceptWebAppDialogForTesting(false, false);
  if (open_in_window) {
    app_loaded_observer.Wait();
    auto* browser_list = BrowserList::GetInstance();
    app_browser_ = nullptr;
    for (Browser* browser : *browser_list) {
      if (AppBrowserController::IsForWebApp(browser, active_app_id_)) {
        app_browser_ = browser;
        return;
      }
    }
    NOTREACHED() << "Unable to find app browser for app " << active_app_id_;
  }
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
    const std::string& site_mode,
    GURL app_url_with_manifest_param) {
  absl::optional<AppState> app_state = GetAppBySiteMode(
      before_state_change_action_state_.get(), profile(), site_mode);
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

void WebAppIntegrationBrowserTestBase::MaybeWaitForManifestUpdates() {
  if (delegate_->GetAllProfiles().size() > 1) {
    return;
  }
  bool continue_checking_for_updates = true;
  while (continue_checking_for_updates) {
    continue_checking_for_updates = false;
    for (const AppId& app_id : app_ids_with_pending_manifest_updates_) {
      if (AreNoAppWindowsOpen(profile(), app_id)) {
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
    const std::string& site_mode) {
  auto browser_url = GetCurrentTab(browser())->GetURL();
  auto dest_url = GetInScopeURL(site_mode);
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

PageActionIconView* WebAppIntegrationBrowserTestBase::intent_picker_view() {
  PageActionIconView* intent_picker_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  DCHECK(intent_picker_view);
  return intent_picker_view;
}

WebAppIntegrationBrowserTest::WebAppIntegrationBrowserTest() : helper_(this) {}
WebAppIntegrationBrowserTest::~WebAppIntegrationBrowserTest() = default;

void WebAppIntegrationBrowserTest::SetUp() {
  helper_.SetUp();
  InProcessBrowserTest::SetUp();
  chrome::SetAutoAcceptAppIdentityUpdateForTesting(false);
}

void WebAppIntegrationBrowserTest::SetUpOnMainThread() {
  helper_.SetUpOnMainThread();
}
void WebAppIntegrationBrowserTest::TearDownOnMainThread() {
  helper_.TearDownOnMainThread();
}

void WebAppIntegrationBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ASSERT_TRUE(embedded_test_server()->Start());
  command_line->AppendSwitchASCII(
      network::switches::kUnsafelyTreatInsecureOriginAsSecure,
      helper_.GetInstallableAppURL("SiteA").GetOrigin().spec());
}

Browser* WebAppIntegrationBrowserTest::CreateBrowser(Profile* profile) {
  return InProcessBrowserTest::CreateBrowser(profile);
}

void WebAppIntegrationBrowserTest::AddBlankTabAndShow(Browser* browser) {
  InProcessBrowserTest::AddBlankTabAndShow(browser);
}

net::EmbeddedTestServer* WebAppIntegrationBrowserTest::EmbeddedTestServer() {
  return embedded_test_server();
}

std::vector<Profile*> WebAppIntegrationBrowserTest::GetAllProfiles() {
  return std::vector<Profile*>{browser()->profile()};
}

bool WebAppIntegrationBrowserTest::IsSyncTest() {
  return false;
}

void WebAppIntegrationBrowserTest::SyncTurnOff() {
  NOTREACHED();
}
void WebAppIntegrationBrowserTest::SyncTurnOn() {
  NOTREACHED();
}
void WebAppIntegrationBrowserTest::AwaitWebAppQuiescence() {
  NOTREACHED();
}

}  // namespace web_app
