// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <algorithm>
#include <bitset>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/to_string.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_sources.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace web_app {

namespace {

// With Lacros, only system web apps are exposed using the Ash browser.
bool WebAppSourceSupported(const WebApp& web_app) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsWebAppsCrosapiEnabled() && !web_app.IsSystemApp()) {
    return false;
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (web_app.IsSystemApp())
    return false;
#endif
  return true;
}

}  // namespace

WebAppRegistrar::WebAppRegistrar(Profile* profile) : profile_(profile) {}

WebAppRegistrar::~WebAppRegistrar() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnAppRegistrarDestroyed();
}

bool WebAppRegistrar::IsLocallyInstalled(const GURL& start_url) const {
  return IsLocallyInstalled(
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url));
}

blink::ParsedPermissionsPolicy WebAppRegistrar::GetPermissionsPolicy(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->permissions_policy()
                 : blink::ParsedPermissionsPolicy();
}

bool WebAppRegistrar::IsPlaceholderApp(
    const AppId& app_id,
    const WebAppManagement::Type source_type) const {
  if (!base::FeatureList::IsEnabled(
          features::kUseWebAppDBInsteadOfExternalPrefs)) {
    return ExternallyInstalledWebAppPrefs(profile_->GetPrefs())
        .IsPlaceholderApp(app_id);
  }

  CHECK(source_type == WebAppManagement::kPolicy ||
        source_type == WebAppManagement::kKiosk);
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return false;

  const WebApp::ExternalConfigMap& config_map =
      web_app->management_to_external_config_map();
  auto it = config_map.find(source_type);

  if (it == config_map.end()) {
    return false;
  }
  return it->second.is_placeholder;
}

// TODO(crbug.com/1434692): Revert changes back to old code
// once the system starts enforcing a single install URL per
// app_id.
absl::optional<AppId> WebAppRegistrar::LookupPlaceholderAppId(
    const GURL& install_url,
    const WebAppManagement::Type source_type) const {
  if (!base::FeatureList::IsEnabled(
          features::kUseWebAppDBInsteadOfExternalPrefs)) {
    return ExternallyInstalledWebAppPrefs(profile_->GetPrefs())
        .LookupPlaceholderAppId(install_url);
  }

  CHECK(source_type == WebAppManagement::kPolicy ||
        source_type == WebAppManagement::kKiosk);
  for (const WebApp& web_app : GetApps()) {
    const WebApp::ExternalConfigMap& config_map =
        web_app.management_to_external_config_map();
    auto it = config_map.find(source_type);

    if (it == config_map.end()) {
      continue;
    }

    if (base::Contains(it->second.install_urls, install_url) &&
        it->second.is_placeholder) {
      return web_app.app_id();
    }
  }
  return absl::nullopt;
}

void WebAppRegistrar::AddObserver(AppRegistrarObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppRegistrar::RemoveObserver(AppRegistrarObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppRegistrar::NotifyWebAppProtocolSettingsChanged() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppProtocolSettingsChanged();
}

void WebAppRegistrar::NotifyWebAppFileHandlerApprovalStateChanged(
    const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppFileHandlerApprovalStateChanged(app_id);
}

void WebAppRegistrar::NotifyWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppsWillBeUpdatedFromSync(new_apps_state);
}

void WebAppRegistrar::NotifyWebAppDisabledStateChanged(const AppId& app_id,
                                                       bool is_disabled) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppDisabledStateChanged(app_id, is_disabled);
}

void WebAppRegistrar::NotifyWebAppsDisabledModeChanged() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppsDisabledModeChanged();
}

void WebAppRegistrar::NotifyWebAppLastBadgingTimeChanged(
    const AppId& app_id,
    const base::Time& time) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppLastBadgingTimeChanged(app_id, time);
}

void WebAppRegistrar::NotifyWebAppLastLaunchTimeChanged(
    const AppId& app_id,
    const base::Time& time) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppLastLaunchTimeChanged(app_id, time);
}

void WebAppRegistrar::NotifyWebAppInstallTimeChanged(const AppId& app_id,
                                                     const base::Time& time) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppInstallTimeChanged(app_id, time);
}

void WebAppRegistrar::NotifyWebAppProfileWillBeDeleted(const AppId& app_id) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppProfileWillBeDeleted(app_id);
}

void WebAppRegistrar::NotifyWebAppUserDisplayModeChanged(
    const AppId& app_id,
    mojom::UserDisplayMode user_display_mode) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppUserDisplayModeChanged(app_id, user_display_mode);
}

void WebAppRegistrar::NotifyWebAppRunOnOsLoginModeChanged(
    const AppId& app_id,
    RunOnOsLoginMode run_on_os_login_mode) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppRunOnOsLoginModeChanged(app_id, run_on_os_login_mode);
}

void WebAppRegistrar::NotifyWebAppSettingsPolicyChanged() {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnWebAppSettingsPolicyChanged();
}

base::flat_map<AppId, base::flat_set<GURL>>
WebAppRegistrar::GetExternallyInstalledApps(
    ExternalInstallSource install_source) const {
  base::flat_map<AppId, base::flat_set<GURL>> installed_apps;
  if (!base::FeatureList::IsEnabled(
          features::kUseWebAppDBInsteadOfExternalPrefs)) {
    installed_apps = ExternallyInstalledWebAppPrefs::BuildAppIdsMap(
        profile()->GetPrefs(), install_source);
    base::EraseIf(installed_apps,
                  [this](const std::pair<AppId, base::flat_set<GURL>>& app) {
                    return !IsInstalled(app.first);
                  });

    return installed_apps;
  }

  WebAppManagement::Type management_source =
      ConvertExternalInstallSourceToSource(install_source);
  for (const WebApp& web_app : GetApps()) {
    const WebApp::ExternalConfigMap& config_map =
        web_app.management_to_external_config_map();
    auto it = config_map.find(management_source);
    if (it != config_map.end() && !it->second.install_urls.empty())
      installed_apps[web_app.app_id()] = it->second.install_urls;
  }
  return installed_apps;
}

absl::optional<AppId> WebAppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  if (!base::FeatureList::IsEnabled(
          features::kUseWebAppDBInsteadOfExternalPrefs)) {
    return ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .LookupAppId(install_url);
  }

  absl::optional<AppId> app_id = LookUpAppIdByInstallUrl(install_url);
  if (app_id.has_value())
    return app_id;

  return absl::nullopt;
}

bool WebAppRegistrar::HasExternalApp(const AppId& app_id) const {
  if (!base::FeatureList::IsEnabled(
          features::kUseWebAppDBInsteadOfExternalPrefs)) {
    return ExternallyInstalledWebAppPrefs::HasAppId(profile()->GetPrefs(),
                                                    app_id);
  }
  if (!IsInstalled(app_id))
    return false;

  const WebApp* web_app = GetAppById(app_id);
  // If the external config map is filled, then the app was
  // externally installed.
  return web_app && web_app->management_to_external_config_map().size() > 0;
}

bool WebAppRegistrar::HasExternalAppWithInstallSource(
    const AppId& app_id,
    ExternalInstallSource install_source) const {
  if (!base::FeatureList::IsEnabled(
          features::kUseWebAppDBInsteadOfExternalPrefs)) {
    return ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
        profile()->GetPrefs(), app_id, install_source);
  }

  if (!IsInstalled(app_id))
    return false;

  const WebApp* web_app = GetAppById(app_id);
  return web_app &&
         base::Contains(web_app->management_to_external_config_map(),
                        ConvertExternalInstallSourceToSource(install_source));
}

GURL WebAppRegistrar::GetAppLaunchUrl(const AppId& app_id) const {
  const GURL& start_url = GetAppStartUrl(app_id);
  const std::string* launch_query_params = GetAppLaunchQueryParams(app_id);
  if (!start_url.is_valid() || !launch_query_params)
    return start_url;

  GURL::Replacements replacements;
  if (start_url.query_piece().empty()) {
    replacements.SetQueryStr(*launch_query_params);
    return start_url.ReplaceComponents(replacements);
  }

  if (start_url.query_piece().find(*launch_query_params) !=
      base::StringPiece::npos) {
    return start_url;
  }

  std::string query_params = start_url.query() + "&" + *launch_query_params;
  replacements.SetQueryStr(query_params);
  return start_url.ReplaceComponents(replacements);
}

GURL WebAppRegistrar::GetAppScope(const AppId& app_id) const {
  absl::optional<GURL> scope = GetAppScopeInternal(app_id);
  if (scope)
    return *scope;
  return GetAppStartUrl(app_id).GetWithoutFilename();
}

bool WebAppRegistrar::IsUrlInAppScope(const GURL& url,
                                      const AppId& app_id) const {
  return GetUrlInAppScopeScore(url.spec(), app_id) > 0;
}

size_t WebAppRegistrar::GetUrlInAppScopeScore(const std::string& url_spec,
                                              const AppId& app_id) const {
  std::string app_scope = GetAppScope(app_id).spec();

  // The app may have been uninstalled.
  if (app_scope.empty())
    return 0;

  size_t score =
      base::StartsWith(url_spec, app_scope, base::CompareCase::SENSITIVE)
          ? app_scope.size()
          : 0;

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled()) {
    score = std::max(score, ChromeOsWebAppExperiments::GetExtendedScopeScore(
                                app_id, url_spec));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return score;
}

absl::optional<AppId> WebAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  if (!url.is_valid())
    return absl::nullopt;

  const std::string url_spec = url.spec();

  absl::optional<AppId> best_app_id;
  size_t best_score = 0U;
  bool best_app_is_shortcut = true;

  for (const AppId& app_id : GetAppIdsForAppSet(GetApps())) {
    // TODO(crbug.com/910016): Treat shortcuts as PWAs.
    bool app_is_shortcut = IsShortcutApp(app_id);
    if (app_is_shortcut && !best_app_is_shortcut)
      continue;

    size_t score = GetUrlInAppScopeScore(url_spec, app_id);

    if (score > 0 &&
        (score > best_score || (best_app_is_shortcut && !app_is_shortcut))) {
      best_app_id = app_id;
      best_score = score;
      best_app_is_shortcut = app_is_shortcut;
    }
  }

  return best_app_id;
}

bool WebAppRegistrar::DoesScopeContainAnyApp(const GURL& scope) const {
  std::string scope_str = scope.spec();

  for (const auto& app_id : GetAppIds()) {
    if (!IsLocallyInstalled(app_id))
      continue;

    std::string app_scope = GetAppScope(app_id).spec();
    DCHECK(!app_scope.empty());

    if (base::StartsWith(app_scope, scope_str, base::CompareCase::SENSITIVE))
      return true;
  }

  return false;
}

std::vector<AppId> WebAppRegistrar::FindAppsInScope(const GURL& scope) const {
  std::string scope_str = scope.spec();

  std::vector<AppId> in_scope;
  for (const auto& app_id : GetAppIds()) {
    if (!IsLocallyInstalled(app_id))
      continue;

    std::string app_scope = GetAppScope(app_id).spec();
    DCHECK(!app_scope.empty());

    if (!base::StartsWith(app_scope, scope_str, base::CompareCase::SENSITIVE))
      continue;

    in_scope.push_back(app_id);
  }

  return in_scope;
}

absl::optional<AppId> WebAppRegistrar::FindInstalledAppWithUrlInScope(
    const GURL& url,
    bool window_only) const {
  const std::string url_spec = url.spec();

  absl::optional<AppId> best_app_id;
  size_t best_score = 0U;
  bool best_app_is_shortcut = true;

  for (const AppId& app_id : GetAppIds()) {
    // TODO(crbug.com/910016): Treat shortcuts as PWAs.
    bool app_is_shortcut = IsShortcutApp(app_id);
    if (app_is_shortcut && !best_app_is_shortcut)
      continue;

    if (!IsLocallyInstalled(app_id))
      continue;

    if (window_only &&
        GetAppEffectiveDisplayMode(app_id) == DisplayMode::kBrowser) {
      continue;
    }

    size_t score = GetUrlInAppScopeScore(url_spec, app_id);

    if (score > 0 &&
        (score > best_score || (best_app_is_shortcut && !app_is_shortcut))) {
      best_app_id = app_id;
      best_score = score;
      best_app_is_shortcut = app_is_shortcut;
    }
  }

  return best_app_id;
}

bool WebAppRegistrar::IsShortcutApp(const AppId& app_id) const {
  // TODO (crbug/910016): Make app scope always return a value and record this
  //  distinction in some other way.
  return !GetAppScopeInternal(app_id).has_value();
}

bool WebAppRegistrar::IsSystemApp(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->IsSystemApp();
}

DisplayMode WebAppRegistrar::GetAppEffectiveDisplayMode(
    const AppId& app_id) const {
  if (!IsLocallyInstalled(app_id))
    return DisplayMode::kBrowser;

  auto app_display_mode = GetAppDisplayMode(app_id);
  absl::optional<mojom::UserDisplayMode> user_display_mode =
      GetAppUserDisplayMode(app_id);
  if (app_display_mode == DisplayMode::kUndefined ||
      !user_display_mode.has_value()) {
    return DisplayMode::kUndefined;
  }

  std::vector<DisplayMode> display_mode_overrides =
      GetAppDisplayModeOverride(app_id);
  return ResolveEffectiveDisplayMode(app_display_mode, display_mode_overrides,
                                     *user_display_mode, IsIsolated(app_id));
}

DisplayMode WebAppRegistrar::GetEffectiveDisplayModeFromManifest(
    const AppId& app_id) const {
  std::vector<DisplayMode> display_mode_overrides =
      GetAppDisplayModeOverride(app_id);

  if (!display_mode_overrides.empty())
    return display_mode_overrides[0];

  return GetAppDisplayMode(app_id);
}

std::string WebAppRegistrar::GetComputedUnhashedAppId(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? GenerateAppIdUnhashed(web_app->manifest_id(),
                                         web_app->start_url())
                 : std::string();
}

bool WebAppRegistrar::IsTabbedWindowModeEnabled(const AppId& app_id) const {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsTabStrip))
    return false;
  return GetAppEffectiveDisplayMode(app_id) == DisplayMode::kTabbed;
}

GURL WebAppRegistrar::GetAppNewTabUrl(const AppId& app_id) const {
  if (IsTabbedWindowModeEnabled(app_id)) {
    auto* web_app = GetAppById(app_id);
    if (!web_app)
      return GURL::EmptyGURL();

    if (web_app->tab_strip() &&
        absl::holds_alternative<blink::Manifest::NewTabButtonParams>(
            web_app->tab_strip().value().new_tab_button)) {
      absl::optional<GURL> url =
          absl::get<blink::Manifest::NewTabButtonParams>(
              web_app->tab_strip().value().new_tab_button)
              .url;
      if (url.has_value())
        return url.value();
    }
  }
  // Apps with new_tab_button.url set to 'auto' will use the start URL.
  return GetAppStartUrl(app_id);
}

absl::optional<GURL> WebAppRegistrar::GetAppPinnedHomeTabUrl(
    const AppId& app_id) const {
  if (IsTabbedWindowModeEnabled(app_id)) {
    const WebApp* web_app = GetAppById(app_id);
    if (!web_app)
      return absl::nullopt;

    if (web_app->tab_strip() &&
        absl::holds_alternative<blink::Manifest::HomeTabParams>(
            web_app->tab_strip().value().home_tab)) {
      return GetAppStartUrl(app_id);
    }
  }
  // Apps with home_tab set to 'auto' will not have a home tab.
  return absl::nullopt;
}

absl::optional<proto::WebAppOsIntegrationState>
WebAppRegistrar::GetAppCurrentOsIntegrationState(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  return web_app->current_os_integration_states();
}

#if BUILDFLAG(IS_MAC)
bool WebAppRegistrar::AlwaysShowToolbarInFullscreen(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->always_show_toolbar_in_fullscreen() : true;
}

void WebAppRegistrar::NotifyAlwaysShowToolbarInFullscreenChanged(
    const AppId& app_id,
    bool show) {
  for (AppRegistrarObserver& observer : observers_)
    observer.OnAlwaysShowToolbarInFullscreenChanged(app_id, show);
}
#endif

const WebApp* WebAppRegistrar::GetAppById(const AppId& app_id) const {
  if (registry_profile_being_deleted_)
    return nullptr;

  auto it = registry_.find(app_id);
  if (it != registry_.end() && WebAppSourceSupported(*it->second))
    return it->second.get();

  return nullptr;
}

const WebApp* WebAppRegistrar::GetAppByStartUrl(const GURL& start_url) const {
  if (registry_profile_being_deleted_)
    return nullptr;

  for (auto const& it : registry_) {
    if (WebAppSourceSupported(*it.second) &&
        it.second->start_url() == start_url)
      return it.second.get();
  }
  return nullptr;
}

std::vector<AppId> WebAppRegistrar::GetAppsFromSyncAndPendingInstallation()
    const {
  AppSet apps_in_sync_install = AppSet(
      this,
      [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               web_app.is_from_sync_and_pending_installation();
      },
      /*empty=*/registry_profile_being_deleted_);

  std::vector<AppId> app_ids;
  for (const WebApp& app : apps_in_sync_install)
    app_ids.push_back(app.app_id());

  return app_ids;
}

std::vector<AppId> WebAppRegistrar::GetAppsPendingUninstall() const {
  AppSet apps_in_sync_uninstall = AppSet(
      this,
      [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               web_app.is_uninstalling();
      },
      /*empty=*/registry_profile_being_deleted_);

  std::vector<AppId> app_ids;
  for (const WebApp& app : apps_in_sync_uninstall)
    app_ids.push_back(app.app_id());

  return app_ids;
}

bool WebAppRegistrar::AppsExistWithExternalConfigData() const {
  for (const WebApp& web_app : GetApps()) {
    if (web_app.management_to_external_config_map().size() > 0)
      return true;
  }
  return false;
}

void WebAppRegistrar::Start() {
  // Profile manager can be null in unit tests.
  if (ProfileManager* profile_manager = g_browser_process->profile_manager())
    profile_manager_observation_.Observe(profile_manager);
}

void WebAppRegistrar::Shutdown() {
  profile_manager_observation_.Reset();
}

void WebAppRegistrar::SetSubsystems(
    WebAppPolicyManager* policy_manager,
    WebAppTranslationManager* translation_manager) {
  policy_manager_ = policy_manager;
  translation_manager_ = translation_manager;
}

base::WeakPtr<WebAppRegistrar> WebAppRegistrar::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

absl::optional<AppId> WebAppRegistrar::LookUpAppIdByInstallUrl(
    const GURL& install_url) const {
  for (const WebApp& web_app : GetApps()) {
    for (auto it : web_app.management_to_external_config_map()) {
      if (base::Contains(it.second.install_urls, install_url)) {
        return web_app.app_id();
      }
    }
  }
  return absl::nullopt;
}

bool WebAppRegistrar::IsInstalled(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app || web_app->is_uninstalling())
    return false;

  // `is_from_sync_and_pending_installation()` should be treated as 'not
  // installed' only if there are no other sources that have installed the web
  // app.
  WebAppSources sources_except_sync = web_app->GetSources();
  sources_except_sync.set(WebAppManagement::kSync, false);
  return !(web_app->is_from_sync_and_pending_installation() &&
           sources_except_sync.none());
}

bool WebAppRegistrar::IsUninstalling(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->is_uninstalling();
}

bool WebAppRegistrar::IsLocallyInstalled(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app
             ? !web_app->is_uninstalling() && web_app->is_locally_installed()
             : false;
}

bool WebAppRegistrar::IsActivelyInstalled(const AppId& app_id) const {
  if (!IsInstalled(app_id) || !IsLocallyInstalled(app_id))
    return false;

  auto* web_app = GetAppById(app_id);
  DCHECK(web_app);
  return !web_app->HasOnlySource(WebAppManagement::kDefault) ||
         GetAppEffectiveDisplayMode(app_id) != DisplayMode::kBrowser;
}

bool WebAppRegistrar::IsIsolated(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app && web_app->isolation_data().has_value();
}

bool WebAppRegistrar::IsInstalledByDefaultManagement(
    const AppId& app_id) const {
  if (!IsInstalled(app_id))
    return false;

  const WebApp* web_app = GetAppById(app_id);
  DCHECK(web_app);
  return web_app->GetSources().test(WebAppManagement::kDefault);
}

bool WebAppRegistrar::WasInstalledByDefaultOnly(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->HasOnlySource(WebAppManagement::Type::kDefault);
}

bool WebAppRegistrar::WasInstalledByUser(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->WasInstalledByUser();
}

bool WebAppRegistrar::WasInstalledByOem(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->chromeos_data().has_value() &&
         web_app->chromeos_data()->oem_installed;
}

bool WebAppRegistrar::WasInstalledBySubApp(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->IsSubAppInstalledApp();
}

bool WebAppRegistrar::IsAllowedLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app &&
         base::Contains(web_app->allowed_launch_protocols(), protocol_scheme);
}

bool WebAppRegistrar::IsDisallowedLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && base::Contains(web_app->disallowed_launch_protocols(),
                                   protocol_scheme);
}

bool WebAppRegistrar::IsRegisteredLaunchProtocol(
    const AppId& app_id,
    const std::string& protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return false;

  return base::Contains(web_app->protocol_handlers(), protocol_scheme,
                        [](const auto& info) { return info.protocol; });
}

base::flat_set<std::string> WebAppRegistrar::GetAllAllowedLaunchProtocols()
    const {
  base::flat_set<std::string> protocols;
  for (const WebApp& web_app : GetApps()) {
    protocols.insert(web_app.allowed_launch_protocols().begin(),
                     web_app.allowed_launch_protocols().end());
  }
  return protocols;
}

base::flat_set<std::string> WebAppRegistrar::GetAllDisallowedLaunchProtocols()
    const {
  base::flat_set<std::string> protocols;
  for (const WebApp& web_app : GetApps()) {
    protocols.insert(web_app.disallowed_launch_protocols().begin(),
                     web_app.disallowed_launch_protocols().end());
  }
  return protocols;
}

int WebAppRegistrar::CountUserInstalledApps() const {
  int num_user_installed = 0;
  for (const WebApp& app : GetApps()) {
    if (app.is_locally_installed() && app.WasInstalledByUser())
      ++num_user_installed;
  }
  return num_user_installed;
}

std::vector<content::StoragePartitionConfig>
WebAppRegistrar::GetIsolatedWebAppStoragePartitionConfigs(
    const AppId& isolated_web_app_id) const {
  if (!base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return {};
  }

  const WebApp* isolated_web_app = GetAppById(isolated_web_app_id);
  if (!isolated_web_app || !isolated_web_app->is_locally_installed() ||
      !isolated_web_app->isolation_data()) {
    return {};
  }

  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::Create(isolated_web_app->scope());
  if (!url_info.has_value()) {
    LOG(ERROR) << "Invalid Isolated Web App: " << isolated_web_app->app_id()
               << ", " << url_info.error();
    return {};
  }

  // TODO(crbug.com/1311065): Include Controlled Frame StoragePartitions.
  return {url_info->storage_partition_config(profile_)};
}

std::string WebAppRegistrar::GetAppShortName(const AppId& app_id) const {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    std::string translated_name =
        translation_manager_->GetTranslatedName(app_id);
    if (!translated_name.empty()) {
      return translated_name;
    }
  }
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->untranslated_name() : std::string();
}

std::string WebAppRegistrar::GetAppDescription(const AppId& app_id) const {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    std::string translated_description =
        translation_manager_->GetTranslatedDescription(app_id);
    if (!translated_description.empty()) {
      return translated_description;
    }
  }
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->untranslated_description() : std::string();
}

absl::optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppDarkModeThemeColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_theme_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppBackgroundColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->background_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppDarkModeBackgroundColor(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_background_color() : absl::nullopt;
}

const GURL& WebAppRegistrar::GetAppStartUrl(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->start_url() : GURL::EmptyGURL();
}

absl::optional<std::string> WebAppRegistrar::GetAppManifestId(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_id() : absl::nullopt;
}

const std::string* WebAppRegistrar::GetAppLaunchQueryParams(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_query_params() : nullptr;
}

const apps::ShareTarget* WebAppRegistrar::GetAppShareTarget(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return (web_app && web_app->share_target().has_value())
             ? &web_app->share_target().value()
             : nullptr;
}

const apps::FileHandlers* WebAppRegistrar::GetAppFileHandlers(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? &web_app->file_handlers() : nullptr;
}

bool WebAppRegistrar::IsAppFileHandlerPermissionBlocked(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return false;

  return web_app->file_handler_approval_state() ==
         ApiApprovalState::kDisallowed;
}

ApiApprovalState WebAppRegistrar::GetAppFileHandlerApprovalState(
    const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return ApiApprovalState::kDisallowed;

  if (web_app->IsSystemApp())
    return ApiApprovalState::kAllowed;

  // TODO(estade): also consult the policy manager when File Handler policies
  // exist.
  return web_app->file_handler_approval_state();
}

bool WebAppRegistrar::ExpectThatFileHandlersAreRegisteredWithOs(
    const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app) {
    return false;
  }

  // TODO(dibyapal): Add support for the new `current_os_integration_state()`
  // when file handlers are added there. https://crbug.com/1404165.
  return web_app->file_handler_os_integration_state() ==
         OsIntegrationState::kEnabled;
}

absl::optional<GURL> WebAppRegistrar::GetAppScopeInternal(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  // TODO(crbug.com/910016): Treat shortcuts as PWAs.
  // Shortcuts on the WebApp system have empty scopes, while the implementation
  // of IsShortcutApp just checks if the scope is |absl::nullopt|, so make sure
  // we return |absl::nullopt| rather than an empty scope.
  if (web_app->scope().is_empty())
    return absl::nullopt;

  return web_app->scope();
}

DisplayMode WebAppRegistrar::GetAppDisplayMode(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode() : DisplayMode::kUndefined;
}

absl::optional<mojom::UserDisplayMode> WebAppRegistrar::GetAppUserDisplayMode(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (web_app == nullptr) {
    return absl::nullopt;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          features::kPreinstalledWebAppWindowExperiment)) {
    auto it = user_display_mode_overrides_for_experiment_.find(app_id);
    if (it != user_display_mode_overrides_for_experiment_.end()) {
      return it->second;
    }
  }
#endif

  return web_app->user_display_mode();
}

std::vector<DisplayMode> WebAppRegistrar::GetAppDisplayModeOverride(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode_override()
                 : std::vector<DisplayMode>();
}

apps::UrlHandlers WebAppRegistrar::GetAppUrlHandlers(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->url_handlers()
                 : std::vector<apps::UrlHandlerInfo>();
}

base::flat_set<ScopeExtensionInfo> WebAppRegistrar::GetValidatedScopeExtensions(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->validated_scope_extensions()
                 : base::flat_set<ScopeExtensionInfo>();
}

GURL WebAppRegistrar::GetAppManifestUrl(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_url() : GURL::EmptyGURL();
}

base::Time WebAppRegistrar::GetAppLastBadgingTime(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->last_badging_time() : base::Time();
}

base::Time WebAppRegistrar::GetAppLastLaunchTime(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->last_launch_time() : base::Time();
}

base::Time WebAppRegistrar::GetAppInstallTime(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->install_time() : base::Time();
}

absl::optional<webapps::WebappInstallSource>
WebAppRegistrar::GetLatestAppInstallSource(const AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  absl::optional<webapps::WebappInstallSource> value =
      web_app->latest_install_source();

  // If the migration code hasn't run yet, `WebApp::latest_install_source_`
  // may not be populated. After migration code is removed, this branch can be
  // deleted.
  if (!value) {
    absl::optional<int> old_value =
        GetWebAppInstallSourceDeprecated(profile_->GetPrefs(), app_id);
    if (old_value)
      return static_cast<webapps::WebappInstallSource>(*old_value);
  }

  return value;
}

std::vector<apps::IconInfo> WebAppRegistrar::GetAppIconInfos(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_icons() : std::vector<apps::IconInfo>();
}

SortedSizesPx WebAppRegistrar::GetAppDownloadedIconSizesAny(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->downloaded_icon_sizes(IconPurpose::ANY)
                 : SortedSizesPx();
}

std::vector<WebAppShortcutsMenuItemInfo>
WebAppRegistrar::GetAppShortcutsMenuItemInfos(const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->shortcuts_menu_item_infos()
                 : std::vector<WebAppShortcutsMenuItemInfo>();
}

std::vector<IconSizes> WebAppRegistrar::GetAppDownloadedShortcutsMenuIconsSizes(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->downloaded_shortcuts_menu_icons_sizes()
                 : std::vector<IconSizes>();
}

std::vector<AppId> WebAppRegistrar::GetAppIds() const {
  return GetAppIdsForAppSet(GetApps());
}

std::vector<AppId> WebAppRegistrar::GetAllSubAppIds(
    const AppId& parent_app_id) const {
  std::vector<AppId> sub_app_ids;

  for (const WebApp& app : GetApps()) {
    if (app.parent_app_id().has_value() &&
        *app.parent_app_id() == parent_app_id) {
      sub_app_ids.push_back(app.app_id());
    }
  }

  return sub_app_ids;
}

ValueWithPolicy<RunOnOsLoginMode> WebAppRegistrar::GetAppRunOnOsLoginMode(
    const AppId& app_id) const {
  RunOnOsLoginPolicy login_policy =
      policy_manager_->GetUrlRunOnOsLoginPolicy(app_id);

  switch (login_policy) {
    case RunOnOsLoginPolicy::kAllowed: {
      auto* web_app = GetAppById(app_id);
      return {
          web_app ? web_app->run_on_os_login_mode() : RunOnOsLoginMode::kNotRun,
          true};
    }
    case RunOnOsLoginPolicy::kBlocked:
      return {RunOnOsLoginMode::kNotRun, false};
    case RunOnOsLoginPolicy::kRunWindowed:
      return {RunOnOsLoginMode::kWindowed, false};
  }
}

absl::optional<RunOnOsLoginMode>
WebAppRegistrar::GetExpectedRunOnOsLoginOsIntegrationState(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->run_on_os_login_os_integration_state()
                 : absl::nullopt;
}

bool WebAppRegistrar::GetWindowControlsOverlayEnabled(
    const AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->window_controls_overlay_enabled() : false;
}

void WebAppRegistrar::OnProfileMarkedForPermanentDeletion(
    Profile* profile_to_be_deleted) {
  if (profile() != profile_to_be_deleted)
    return;

  for (const AppId& app_id : GetAppIdsForAppSet(GetAppsIncludingStubs())) {
    NotifyWebAppProfileWillBeDeleted(app_id);
  }
  // We can't do registry_.clear() here because it makes in-memory registry
  // diverged from the sync server registry and from the on-disk registry
  // (WebAppDatabase/LevelDB and "Web Applications" profile directory).
  registry_profile_being_deleted_ = true;
}

void WebAppRegistrar::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

WebAppRegistrar::AppSet::AppSet(const WebAppRegistrar* registrar,
                                Filter filter,
                                bool empty)
    : registrar_(registrar),
      filter_(filter),
      empty_(empty)
#if DCHECK_IS_ON()
      ,
      mutations_count_(registrar->mutations_count_)
#endif
{
  DCHECK(filter);
}

WebAppRegistrar::AppSet::~AppSet() {
#if DCHECK_IS_ON()
  DCHECK_EQ(mutations_count_, registrar_->mutations_count_);
#endif
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::begin() {
  if (empty_)
    return end();
  return iterator(registrar_->registry_.begin(), registrar_->registry_.end(),
                  filter_);
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::end() {
  return iterator(registrar_->registry_.end(), registrar_->registry_.end(),
                  filter_);
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::begin() const {
  if (empty_)
    return end();
  return const_iterator(registrar_->registry_.begin(),
                        registrar_->registry_.end(), filter_);
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::end() const {
  return const_iterator(registrar_->registry_.end(),
                        registrar_->registry_.end(), filter_);
}

WebAppRegistrar::AppSet WebAppRegistrar::GetAppsIncludingStubs() const {
  return AppSet(
      this,
      [](const WebApp& web_app) { return WebAppSourceSupported(web_app); },
      /*empty=*/registry_profile_being_deleted_);
}

WebAppRegistrar::AppSet WebAppRegistrar::GetApps() const {
  return AppSet(
      this,
      [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               !web_app.is_uninstalling();
      },
      /*empty=*/registry_profile_being_deleted_);
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppRegistrar::SetUserDisplayModeOverridesForExperiment(
    base::flat_map<AppId, mojom::UserDisplayMode> overrides) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kPreinstalledWebAppWindowExperiment));
  user_display_mode_overrides_for_experiment_ = std::move(overrides);
}
#endif

base::Value WebAppRegistrar::AsDebugValue() const {
  base::Value::Dict root;

  std::vector<const web_app::WebApp*> web_apps;
  for (const web_app::WebApp& web_app : GetAppsIncludingStubs()) {
    web_apps.push_back(&web_app);
  }
  base::ranges::sort(web_apps, {}, &web_app::WebApp::untranslated_name);

  // Prefix with a ! so this appears at the top when serialized.
  base::Value::Dict& index = *root.EnsureDict("!Index");
  for (const web_app::WebApp* web_app : web_apps) {
    const std::string& key = web_app->untranslated_name();
    base::Value* existing_entry = index.Find(key);
    if (!existing_entry) {
      index.Set(key, web_app->app_id());
      continue;
    }
    // If any web apps share identical names then collect a list of app IDs.
    const std::string* existing_id = existing_entry->GetIfString();
    if (existing_id) {
      base::Value::List id_list;
      id_list.Append(*existing_id);
      index.Set(key, std::move(id_list));
    }
    index.FindList(key)->Append(web_app->app_id());
  }

  base::Value::List& web_app_details = *root.EnsureList("Details");
  for (const web_app::WebApp* web_app : web_apps) {
    auto app_id = web_app->app_id();

    base::Value app_debug_value = web_app->AsDebugValue();
    auto& app_debug_dict = app_debug_value.GetDict();

    base::Value::Dict& effective_fields =
        *app_debug_dict.EnsureDict("registrar_evaluated_fields");
    effective_fields.Set(
        "display_mode",
        blink::DisplayModeToString(GetAppEffectiveDisplayMode(app_id)));
    effective_fields.Set("launch_url", base::ToString(GetAppLaunchUrl(app_id)));
    effective_fields.Set("scope", base::ToString(GetAppScope(app_id)));

    base::Value::Dict& run_on_os_login_fields =
        *effective_fields.EnsureDict("run_on_os_login_mode");
    web_app::ValueWithPolicy<web_app::RunOnOsLoginMode> run_on_os_login_mode =
        GetAppRunOnOsLoginMode(app_id);
    run_on_os_login_fields.Set(
        "value", RunOnOsLoginModeToString(run_on_os_login_mode.value));
    run_on_os_login_fields.Set("user_controllable",
                               run_on_os_login_mode.user_controllable);

    web_app_details.Append(std::move(app_debug_value));
  }

  return base::Value(std::move(root));
}

void WebAppRegistrar::SetRegistry(Registry&& registry) {
  registry_ = std::move(registry);
}

void WebAppRegistrar::CountMutation() {
#if DCHECK_IS_ON()
  ++mutations_count_;
#endif
}

WebAppRegistrarMutable::WebAppRegistrarMutable(Profile* profile)
    : WebAppRegistrar(profile) {}

WebAppRegistrarMutable::~WebAppRegistrarMutable() = default;

void WebAppRegistrarMutable::InitRegistry(Registry&& registry) {
  DCHECK(is_empty());
  SetRegistry(std::move(registry));
}

WebApp* WebAppRegistrarMutable::GetAppByIdMutable(const AppId& app_id) {
  return const_cast<WebApp*>(GetAppById(app_id));
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::FilterAppsMutableForTesting(
    Filter filter) {
  return AppSet(this, filter, /*empty=*/registry_profile_being_deleted_);
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::GetAppsIncludingStubsMutable() {
  return AppSet(
      this,
      [](const WebApp& web_app) { return WebAppSourceSupported(web_app); },
      /*empty=*/registry_profile_being_deleted_);
}

WebAppRegistrar::AppSet WebAppRegistrarMutable::GetAppsMutable() {
  return AppSet(
      this,
      [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               !web_app.is_uninstalling();
      },
      /*empty=*/registry_profile_being_deleted_);
}

bool IsRegistryEqual(const Registry& registry, const Registry& registry2) {
  if (registry.size() != registry2.size()) {
    LOG(ERROR) << registry.size() << " != " << registry2.size();
    return false;
  }

  for (auto& kv : registry) {
    const WebApp* web_app = kv.second.get();
    const WebApp* web_app2 = registry2.at(web_app->app_id()).get();
    if (*web_app != *web_app2) {
      LOG(ERROR) << "Web apps are not equal:\n"
                 << *web_app << "\n"
                 << *web_app2;
      return false;
    }
  }

  return true;
}

std::vector<AppId> WebAppRegistrar::GetAppIdsForAppSet(
    const AppSet& app_set) const {
  std::vector<AppId> app_ids;

  for (const WebApp& app : app_set)
    app_ids.push_back(app.app_id());

  return app_ids;
}

}  // namespace web_app
