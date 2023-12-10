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
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "url/gurl.h"

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
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnAppRegistrarDestroyed();
  }
}

bool WebAppRegistrar::IsLocallyInstalled(const GURL& start_url) const {
  return IsLocallyInstalled(
      GenerateAppId(/*manifest_id=*/absl::nullopt, start_url));
}

blink::ParsedPermissionsPolicy WebAppRegistrar::GetPermissionsPolicy(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->permissions_policy()
                 : blink::ParsedPermissionsPolicy();
}

bool WebAppRegistrar::IsPlaceholderApp(
    const webapps::AppId& app_id,
    const WebAppManagement::Type source_type) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return false;

  const WebApp::ExternalConfigMap& config_map =
      web_app->management_to_external_config_map();
  auto it = config_map.find(source_type);

  if (it == config_map.end()) {
    return false;
  }
  // Only kiosk and policy sources currently have placeholder apps.
  CHECK(!it->second.is_placeholder ||
        (source_type == WebAppManagement::kPolicy ||
         source_type == WebAppManagement::kKiosk));
  return it->second.is_placeholder;
}

// TODO(crbug.com/1434692): Revert changes back to old code
// once the system starts enforcing a single install URL per
// app_id.
absl::optional<webapps::AppId> WebAppRegistrar::LookupPlaceholderAppId(
    const GURL& install_url,
    const WebAppManagement::Type source_type) const {
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

void WebAppRegistrar::AddObserver(WebAppRegistrarObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppRegistrar::RemoveObserver(WebAppRegistrarObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppRegistrar::NotifyWebAppProtocolSettingsChanged() {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppProtocolSettingsChanged();
  }
}

void WebAppRegistrar::NotifyWebAppFileHandlerApprovalStateChanged(
    const webapps::AppId& app_id) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppFileHandlerApprovalStateChanged(app_id);
  }
}

void WebAppRegistrar::NotifyWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppsWillBeUpdatedFromSync(new_apps_state);
  }
}

void WebAppRegistrar::NotifyWebAppDisabledStateChanged(
    const webapps::AppId& app_id,
    bool is_disabled) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppDisabledStateChanged(app_id, is_disabled);
  }
}

void WebAppRegistrar::NotifyWebAppsDisabledModeChanged() {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppsDisabledModeChanged();
  }
}

void WebAppRegistrar::NotifyWebAppLastBadgingTimeChanged(
    const webapps::AppId& app_id,
    const base::Time& time) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppLastBadgingTimeChanged(app_id, time);
  }
}

void WebAppRegistrar::NotifyWebAppLastLaunchTimeChanged(
    const webapps::AppId& app_id,
    const base::Time& time) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppLastLaunchTimeChanged(app_id, time);
  }
}

void WebAppRegistrar::NotifyWebAppFirstInstallTimeChanged(
    const webapps::AppId& app_id,
    const base::Time& time) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppFirstInstallTimeChanged(app_id, time);
  }
}

void WebAppRegistrar::NotifyWebAppProfileWillBeDeleted(
    const webapps::AppId& app_id) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppProfileWillBeDeleted(app_id);
  }
}

void WebAppRegistrar::NotifyWebAppUserDisplayModeChanged(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppUserDisplayModeChanged(app_id, user_display_mode);
  }
}

void WebAppRegistrar::NotifyWebAppRunOnOsLoginModeChanged(
    const webapps::AppId& app_id,
    RunOnOsLoginMode run_on_os_login_mode) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppRunOnOsLoginModeChanged(app_id, run_on_os_login_mode);
  }
}

void WebAppRegistrar::NotifyWebAppSettingsPolicyChanged() {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppSettingsPolicyChanged();
  }
}

#if !BUILDFLAG(IS_CHROMEOS)
void WebAppRegistrar::NotifyWebAppUserLinkCapturingPreferencesChanged(
    const webapps::AppId& app_id,
    bool is_preferred) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnWebAppUserLinkCapturingPreferencesChanged(app_id, is_preferred);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

base::flat_map<webapps::AppId, base::flat_set<GURL>>
WebAppRegistrar::GetExternallyInstalledApps(
    ExternalInstallSource install_source) const {
  base::flat_map<webapps::AppId, base::flat_set<GURL>> installed_apps;
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

absl::optional<webapps::AppId> WebAppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  absl::optional<webapps::AppId> app_id = LookUpAppIdByInstallUrl(install_url);
  if (app_id.has_value())
    return app_id;

  return absl::nullopt;
}

bool WebAppRegistrar::HasExternalApp(const webapps::AppId& app_id) const {
  if (!IsInstalled(app_id))
    return false;

  const WebApp* web_app = GetAppById(app_id);
  // If the external config map is filled, then the app was
  // externally installed.
  return web_app && web_app->management_to_external_config_map().size() > 0;
}

bool WebAppRegistrar::HasExternalAppWithInstallSource(
    const webapps::AppId& app_id,
    ExternalInstallSource install_source) const {
  if (!IsInstalled(app_id))
    return false;

  const WebApp* web_app = GetAppById(app_id);
  return web_app &&
         base::Contains(web_app->management_to_external_config_map(),
                        ConvertExternalInstallSourceToSource(install_source));
}

GURL WebAppRegistrar::GetAppLaunchUrl(const webapps::AppId& app_id) const {
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

GURL WebAppRegistrar::GetAppScope(const webapps::AppId& app_id) const {
  absl::optional<GURL> scope = GetAppScopeInternal(app_id);
  if (scope)
    return *scope;
  return GetAppStartUrl(app_id).GetWithoutFilename();
}

size_t WebAppRegistrar::GetAppExtendedScopeScore(
    const GURL& url,
    const webapps::AppId& app_id) const {
  if (!url.is_valid()) {
    return 0;
  }

  size_t app_scope = GetUrlInAppScopeScore(url.spec(), app_id);
  if (app_scope > 0) {
    return app_scope;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableScopeExtensions)) {
    return 0;
  }

  const WebApp* app = GetAppById(app_id);
  if (!app || app->validated_scope_extensions().empty()) {
    return 0;
  }

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque() || origin.scheme() != url::kHttpsScheme) {
    return 0;
  }

  absl::optional<std::string> origin_str;

  for (const auto& scope_extension : GetValidatedScopeExtensions(app_id)) {
    if (origin.IsSameOriginWith(scope_extension.origin)) {
      return origin.host().size();
    }

    // Origins with wildcard e.g. *.foo are saved as https://foo.
    // Ensure while matching that the origin ends with '.foo' and not 'foo'.
    if (scope_extension.has_origin_wildcard) {
      if (!origin_str.has_value()) {
        origin_str = origin.Serialize();
      }

      if (base::EndsWith(origin_str.value(), scope_extension.origin.host(),
                         base::CompareCase::SENSITIVE) &&
          origin_str.value().size() > scope_extension.origin.host().size() &&
          origin_str.value()[origin_str.value().size() -
                             scope_extension.origin.host().size() - 1] == '.') {
        return scope_extension.origin.host().size();
      }
    }
  }
  return 0;
}

bool WebAppRegistrar::IsUrlInAppScope(const GURL& url,
                                      const webapps::AppId& app_id) const {
  return GetUrlInAppScopeScore(url.spec(), app_id) > 0;
}

bool WebAppRegistrar::IsUrlInAppExtendedScope(
    const GURL& url,
    const webapps::AppId& app_id) const {
  return GetAppExtendedScopeScore(url, app_id) > 0;
}

size_t WebAppRegistrar::GetUrlInAppScopeScore(
    const std::string& url_spec,
    const webapps::AppId& app_id) const {
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

absl::optional<webapps::AppId> WebAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  if (!url.is_valid())
    return absl::nullopt;

  const std::string url_spec = url.spec();

  absl::optional<webapps::AppId> best_app_id;
  size_t best_score = 0U;
  bool best_app_is_shortcut = true;

  for (const webapps::AppId& app_id : GetAppIdsForAppSet(GetApps())) {
    // TODO(crbug.com/1469482): Consider treating shortcuts differently to PWAs.
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

std::vector<webapps::AppId> WebAppRegistrar::FindAppsInScope(
    const GURL& scope) const {
  std::string scope_str = scope.spec();

  std::vector<webapps::AppId> in_scope;
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

absl::optional<webapps::AppId> WebAppRegistrar::FindInstalledAppWithUrlInScope(
    const GURL& url,
    bool window_only) const {
  const std::string url_spec = url.spec();

  absl::optional<webapps::AppId> best_app_id;
  size_t best_score = 0U;
  bool best_app_is_shortcut = true;

  for (const webapps::AppId& app_id : GetAppIds()) {
    // TODO(crbug.com/1469482): Consider treating shortcuts differently to PWAs.
    bool app_is_shortcut = IsShortcutApp(app_id);
    if (app_is_shortcut && !best_app_is_shortcut) {
      continue;
    }

    if (!IsLocallyInstalled(app_id)) {
      continue;
    }

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

bool WebAppRegistrar::IsNonLocallyInstalledAppWithUrlInScope(
    const GURL& url) const {
  std::string scope_str = url.spec();

  for (const auto& app_id : GetAppIds()) {
    std::string app_scope = GetAppScope(app_id).spec();
    CHECK(!app_scope.empty());

    if (!base::StartsWith(scope_str, app_scope, base::CompareCase::SENSITIVE)) {
      continue;
    }

    if (!IsLocallyInstalled(app_id)) {
      return true;
    }
  }

  return false;
}

bool WebAppRegistrar::IsShortcutApp(const webapps::AppId& app_id) const {
  if (!GetAppById(app_id)) {
    return false;
  }
  // TODO(crbug.com/1469482): Record shortcut distinction explicitly instead of
  // using scope.
#if BUILDFLAG(IS_CHROMEOS)
  return IsShortcutAppChromeOs(app_id);
#else
  return !GetAppScopeInternal(app_id).has_value();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool WebAppRegistrar::IsSystemApp(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->IsSystemApp();
}

DisplayMode WebAppRegistrar::GetAppEffectiveDisplayMode(
    const webapps::AppId& app_id) const {
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
    const webapps::AppId& app_id) const {
  std::vector<DisplayMode> display_mode_overrides =
      GetAppDisplayModeOverride(app_id);

  if (!display_mode_overrides.empty())
    return display_mode_overrides[0];

  return GetAppDisplayMode(app_id);
}

GURL WebAppRegistrar::GetComputedManifestId(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_id() : GURL();
}

bool WebAppRegistrar::IsTabbedWindowModeEnabled(
    const webapps::AppId& app_id) const {
  if (!base::FeatureList::IsEnabled(blink::features::kDesktopPWAsTabStrip)) {
    return false;
  }
  return GetAppEffectiveDisplayMode(app_id) == DisplayMode::kTabbed;
}

GURL WebAppRegistrar::GetAppNewTabUrl(const webapps::AppId& app_id) const {
  if (IsTabbedWindowModeEnabled(app_id)) {
    auto* web_app = GetAppById(app_id);
    if (!web_app)
      return GURL::EmptyGURL();

    if (web_app->tab_strip()) {
      absl::optional<GURL> url =
          web_app->tab_strip().value().new_tab_button.url;
      if (url.has_value())
        return url.value();
    }
  }
  // Apps that don't set a new_tab_button.url will use the start URL.
  return GetAppStartUrl(app_id);
}

absl::optional<GURL> WebAppRegistrar::GetAppPinnedHomeTabUrl(
    const webapps::AppId& app_id) const {
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
WebAppRegistrar::GetAppCurrentOsIntegrationState(
    const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  return web_app->current_os_integration_states();
}

#if BUILDFLAG(IS_MAC)
bool WebAppRegistrar::AlwaysShowToolbarInFullscreen(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->always_show_toolbar_in_fullscreen() : true;
}

void WebAppRegistrar::NotifyAlwaysShowToolbarInFullscreenChanged(
    const webapps::AppId& app_id,
    bool show) {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnAlwaysShowToolbarInFullscreenChanged(app_id, show);
  }
}
#endif

const WebApp* WebAppRegistrar::GetAppById(const webapps::AppId& app_id) const {
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

std::vector<webapps::AppId>
WebAppRegistrar::GetAppsFromSyncAndPendingInstallation() const {
  AppSet apps_in_sync_install = AppSet(
      this,
      [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               web_app.is_from_sync_and_pending_installation();
      },
      /*empty=*/registry_profile_being_deleted_);

  std::vector<webapps::AppId> app_ids;
  for (const WebApp& app : apps_in_sync_install)
    app_ids.push_back(app.app_id());

  return app_ids;
}

std::vector<webapps::AppId> WebAppRegistrar::GetAppsPendingUninstall() const {
  AppSet apps_in_sync_uninstall = AppSet(
      this,
      [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               web_app.is_uninstalling();
      },
      /*empty=*/registry_profile_being_deleted_);

  std::vector<webapps::AppId> app_ids;
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

void WebAppRegistrar::SetProvider(base::PassKey<WebAppProvider>,
                                  WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppRegistrar::Start() {
  // Profile manager can be null in unit tests.
  if (ProfileManager* profile_manager = g_browser_process->profile_manager())
    profile_manager_observation_.Observe(profile_manager);

  int num_user_installed_apps = CountUserInstalledApps();
  int num_non_locally_installed = CountUserInstalledNotLocallyInstalledApps();

  base::UmaHistogramCounts1000("WebApp.InstalledCount.ByUser",
                               num_user_installed_apps);
  base::UmaHistogramCounts1000(
      "WebApp.InstalledCount.ByUserNotLocallyInstalled",
      num_non_locally_installed);

#if BUILDFLAG(IS_MAC)
  auto multi_profile_app_ids =
      AppShimRegistry::Get()->GetAppsInstalledInMultipleProfiles();
  int num_multi_profile_apps = 0;
  for (const auto& app_id : multi_profile_app_ids) {
    const WebApp* app = GetAppById(app_id);
    if (app && app->is_locally_installed() && app->WasInstalledByUser()) {
      num_multi_profile_apps++;
    }
  }
  base::UmaHistogramCounts1000("WebApp.InstalledCount.ByUserInMultipleProfiles",
                               num_multi_profile_apps);
#endif
}

void WebAppRegistrar::Shutdown() {
  profile_manager_observation_.Reset();
}

base::WeakPtr<WebAppRegistrar> WebAppRegistrar::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

absl::optional<webapps::AppId> WebAppRegistrar::LookUpAppIdByInstallUrl(
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

const WebApp* WebAppRegistrar::LookUpAppByInstallSourceInstallUrl(
    WebAppManagement::Type install_source,
    const GURL& install_url) const {
  for (const WebApp& app : GetApps()) {
    const WebApp::ExternalConfigMap& config_map =
        app.management_to_external_config_map();
    auto it = config_map.find(install_source);
    if (it != config_map.end()) {
      if (base::Contains(it->second.install_urls, install_url)) {
        return &app;
      }
    }
  }
  return nullptr;
}

bool WebAppRegistrar::IsInstalled(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app || web_app->is_uninstalling())
    return false;

  // `is_from_sync_and_pending_installation()` should be treated as 'not
  // installed' only if there are no other sources that have installed the web
  // app.
  WebAppManagementTypes sources_except_sync = web_app->GetSources();
  sources_except_sync.Remove(WebAppManagement::kSync);
  return !(web_app->is_from_sync_and_pending_installation() &&
           sources_except_sync.Empty());
}

bool WebAppRegistrar::IsUninstalling(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->is_uninstalling();
}

bool WebAppRegistrar::IsLocallyInstalled(const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app
             ? !web_app->is_uninstalling() && web_app->is_locally_installed()
             : false;
}

bool WebAppRegistrar::IsActivelyInstalled(const webapps::AppId& app_id) const {
  if (!IsInstalled(app_id) || !IsLocallyInstalled(app_id))
    return false;

  auto* web_app = GetAppById(app_id);
  DCHECK(web_app);
  return !web_app->HasOnlySource(WebAppManagement::kDefault) ||
         GetAppEffectiveDisplayMode(app_id) != DisplayMode::kBrowser;
}

bool WebAppRegistrar::IsIsolated(const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app && web_app->isolation_data().has_value();
}

bool WebAppRegistrar::IsInstalledByDefaultManagement(
    const webapps::AppId& app_id) const {
  if (!IsInstalled(app_id))
    return false;

  const WebApp* web_app = GetAppById(app_id);
  DCHECK(web_app);
  return web_app->GetSources().Has(WebAppManagement::kDefault);
}

bool WebAppRegistrar::WasInstalledByDefaultOnly(
    const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->HasOnlySource(WebAppManagement::Type::kDefault);
}

bool WebAppRegistrar::WasInstalledByUser(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->WasInstalledByUser();
}

bool WebAppRegistrar::WasInstalledByOem(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->chromeos_data().has_value() &&
         web_app->chromeos_data()->oem_installed;
}

bool WebAppRegistrar::WasInstalledBySubApp(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->IsSubAppInstalledApp();
}

bool WebAppRegistrar::CanUserUninstallWebApp(
    const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->CanUserUninstallWebApp();
}

bool WebAppRegistrar::IsPreventCloseEnabled(
    const webapps::AppId& app_id) const {
  return provider_->policy_manager().IsPreventCloseEnabled(app_id);
}

bool WebAppRegistrar::IsAllowedLaunchProtocol(
    const webapps::AppId& app_id,
    const std::string& protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app &&
         base::Contains(web_app->allowed_launch_protocols(), protocol_scheme);
}

bool WebAppRegistrar::IsDisallowedLaunchProtocol(
    const webapps::AppId& app_id,
    const std::string& protocol_scheme) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && base::Contains(web_app->disallowed_launch_protocols(),
                                   protocol_scheme);
}

bool WebAppRegistrar::IsRegisteredLaunchProtocol(
    const webapps::AppId& app_id,
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

int WebAppRegistrar::CountUserInstalledNotLocallyInstalledApps() const {
  int num_non_locally_installed = 0;
  for (const WebApp& app : GetApps()) {
    if (!app.is_locally_installed() && app.WasInstalledByUser()) {
      ++num_non_locally_installed;
    }
  }
  return num_non_locally_installed;
}

std::vector<content::StoragePartitionConfig>
WebAppRegistrar::GetIsolatedWebAppStoragePartitionConfigs(
    const webapps::AppId& isolated_web_app_id) const {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(profile_)) {
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

  // Start with IWA's base on-disk partition.
  std::vector<content::StoragePartitionConfig> partitions = {
      url_info->storage_partition_config(profile_)};

  // Get all on-disk Controlled Frame partitions.
  for (const std::string& partition :
       isolated_web_app->isolation_data()->controlled_frame_partitions) {
    partitions.push_back(url_info->GetStoragePartitionConfigForControlledFrame(
        profile_, partition, /*in_memory=*/false));
  }

  // Get all in-memory Controlled Frame partitions.
  auto it = isolated_web_app_in_memory_controlled_frame_partitions_.find(
      isolated_web_app_id);
  if (it != isolated_web_app_in_memory_controlled_frame_partitions_.end()) {
    for (const std::string& partition : it->second) {
      partitions.push_back(
          url_info->GetStoragePartitionConfigForControlledFrame(
              profile_, partition, /*in_memory=*/true));
    }
  }

  return partitions;
}

absl::optional<content::StoragePartitionConfig>
WebAppRegistrar::SaveAndGetInMemoryControlledFramePartitionConfig(
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name) {
  if (!IsInstalled(url_info.app_id())) {
    return absl::nullopt;
  }

  isolated_web_app_in_memory_controlled_frame_partitions_[url_info.app_id()]
      .insert(partition_name);

  return url_info.GetStoragePartitionConfigForControlledFrame(
      profile_, partition_name, true);
}

bool WebAppRegistrar::CanCaptureLinksInScope(
    const webapps::AppId& app_id) const {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsLinkCapturing)) {
    return false;
  }
  if (!IsLocallyInstalled(app_id) || IsShortcutApp(app_id)) {
    return false;
  }
  return true;
}

bool WebAppRegistrar::CapturesLinksInScope(const webapps::AppId& app_id) const {
  if (!CanCaptureLinksInScope(app_id)) {
    return false;
  }

  const WebApp* web_app = GetAppById(app_id);
  CHECK(web_app);
  switch (web_app->user_link_capturing_preference()) {
    case proto::LinkCapturingUserPreference::LINK_CAPTURING_PREFERENCE_DEFAULT:
      if (!features::kLinksCapturedByDefault.Get()) {
        return false;
      }
      break;
    case proto::LinkCapturingUserPreference::CAPTURE_SUPPORTED_LINKS:
      return true;
    case proto::LinkCapturingUserPreference::DO_NOT_CAPTURE_SUPPORTED_LINKS:
      return false;
  }

  // Reaching here means that the default link capturing behavior is 'on' and
  // the current app is 'default'. To resolve,
  // - If any other app shares the scope and has link capturing enabled, return
  //   false.
  // - If there are more than one apps set to 'default', then return one app
  //   deterministically (the earliest installed).
  // Technically, this violates some of the locking practices we have, as this
  // views all apps instead of the one app. However, given the rarity of hitting
  // this, and the difficulty of actually hitting an edge case here, this seems
  // OK.
  std::vector<std::pair<webapps::AppId, base::Time>> app_and_install_time = {
      {app_id, web_app->first_install_time()}};
  for (const webapps::AppId& other_app_id : GetAppIds()) {
    if (!CanCaptureLinksInScope(other_app_id) || other_app_id == app_id) {
      continue;
    }
    if (!AppScopesMatchForUserLinkCapturing(app_id, other_app_id)) {
      continue;
    }
    const WebApp* other_app = GetAppById(other_app_id);
    switch (other_app->user_link_capturing_preference()) {
      case proto::LinkCapturingUserPreference::
          LINK_CAPTURING_PREFERENCE_DEFAULT:
        app_and_install_time.emplace_back(other_app_id,
                                          other_app->first_install_time());
        break;
      case proto::LinkCapturingUserPreference::CAPTURE_SUPPORTED_LINKS:
        return false;
      case proto::LinkCapturingUserPreference::DO_NOT_CAPTURE_SUPPORTED_LINKS:
        continue;
    }
  }

  // Sort by install time so the first installation wins.
  std::sort(app_and_install_time.begin(), app_and_install_time.end(),
            [](auto& left, auto& right) { return left.second < right.second; });
  return app_and_install_time.front().first == app_id;
}

absl::optional<webapps::AppId> WebAppRegistrar::FindAppThatCapturesLinksInScope(
    const GURL& url) const {
  // Nested apps remove that URL space from the parent app, so links from a
  // nested app cannot be captured by a parent app. Even so, there can be
  // multiple apps with the same score, but the only one that matters is the
  // first one that also captures links.
  size_t top_score = 0;
  std::vector<webapps::AppId> top_apps;
  for (const webapps::AppId& app_id : GetAppIds()) {
    if (!CanCaptureLinksInScope(app_id)) {
      continue;
    }
    // TODO(dmurph): Switch to GetAppExtendedScopeScore if the
    // kWebAppEnableScopeExtensions feature is enabled. b/294079334
    size_t score = GetUrlInAppScopeScore(url.spec(), app_id);
    // A score of 0 means it doesn't apply at all.
    if (score == 0 || score < top_score) {
      continue;
    }
    if (score == top_score) {
      top_apps.push_back(app_id);
      continue;
    }
    top_score = score;
    top_apps = {app_id};
  }
  if (top_apps.empty()) {
    return absl::nullopt;
  }
  for (const webapps::AppId& app_id : top_apps) {
    if (CapturesLinksInScope(app_id)) {
      return app_id;
    }
  }
  return absl::nullopt;
}

bool WebAppRegistrar::IsLinkCapturableByApp(const webapps::AppId& app,
                                            const GURL& url) const {
  CHECK(url.is_valid());
  // TODO(dmurph): Switch to GetAppExtendedScopeScore if the
  // kWebAppEnableScopeExtensions feature is enabled. b/294079334
  size_t app_score = GetUrlInAppScopeScore(url.spec(), app);
  if (app_score == 0) {
    return false;
  }
  return base::ranges::none_of(GetAppIds(), [&](const webapps::AppId& app_id) {
    // TODO(b/294079334): Switch to GetAppExtendedScopeScore if the
    // kWebAppEnableScopeExtensions feature is enabled.
    return IsLocallyInstalled(app_id) && !IsShortcutApp(app_id) &&
           GetUrlInAppScopeScore(url.spec(), app_id) > app_score;
  });
}

std::vector<webapps::AppId> WebAppRegistrar::GetOverlappingAppsMatchingScope(
    const webapps::AppId& app_id) const {
  std::vector<webapps::AppId> all_apps_with_supported_links;
  const GURL& required_scope = GetAppScope(app_id);
  if (!IsValidScopeForLinkCapturing(required_scope)) {
    return all_apps_with_supported_links;
  }

  for (const auto& id : GetAppIds()) {
    if (id == app_id) {
      continue;
    }
    if (!CanCaptureLinksInScope(id)) {
      continue;
    }
    if (!AppScopesMatchForUserLinkCapturing(id, app_id)) {
      continue;
    }
    if (!CapturesLinksInScope(id)) {
      continue;
    }
    all_apps_with_supported_links.push_back(id);
  }
  return all_apps_with_supported_links;
}

bool WebAppRegistrar::AppScopesMatchForUserLinkCapturing(
    const webapps::AppId& app_id1,
    const webapps::AppId& app_id2) const {
  if (!IsLocallyInstalled(app_id1) || !IsLocallyInstalled(app_id2)) {
    return false;
  }

  const GURL& app_scope1 = GetAppScope(app_id1);
  const GURL& app_scope2 = GetAppScope(app_id2);
  if (!IsValidScopeForLinkCapturing(app_scope1) ||
      !IsValidScopeForLinkCapturing(app_scope2)) {
    return false;
  }

  return app_scope1 == app_scope2;
}

base::flat_map<webapps::AppId, std::string>
WebAppRegistrar::GetAllAppsControllingUrl(const GURL& url) const {
  base::flat_map<webapps::AppId, std::string> all_controlling_apps;
  for (const webapps::AppId& app_id : GetAppIds()) {
    if (!IsLocallyInstalled(app_id)) {
      continue;
    }

    if (GetAppUserDisplayMode(app_id) == mojom::UserDisplayMode::kBrowser) {
      continue;
    }

    const GURL scope = GetAppScope(app_id);
    if (base::StartsWith(url.spec(), scope.spec(),
                         base::CompareCase::SENSITIVE)) {
      all_controlling_apps.insert_or_assign(app_id, GetAppShortName(app_id));
    }
  }
  return all_controlling_apps;
}

bool WebAppRegistrar::IsPreferredAppForCapturingUrl(
    const GURL& url,
    const webapps::AppId& app_id) {
  const GURL app_scope = GetAppScope(app_id);
  return base::StartsWith(url.spec(), app_scope.spec(),
                          base::CompareCase::SENSITIVE) &&
         CapturesLinksInScope(app_id);
}

std::string WebAppRegistrar::GetAppShortName(
    const webapps::AppId& app_id) const {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    std::string translated_name =
        provider_->translation_manager().GetTranslatedName(app_id);
    if (!translated_name.empty()) {
      return translated_name;
    }
  }
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->untranslated_name() : std::string();
}

std::string WebAppRegistrar::GetAppDescription(
    const webapps::AppId& app_id) const {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    std::string translated_description =
        provider_->translation_manager().GetTranslatedDescription(app_id);
    if (!translated_description.empty()) {
      return translated_description;
    }
  }
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->untranslated_description() : std::string();
}

absl::optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppDarkModeThemeColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_theme_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppBackgroundColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->background_color() : absl::nullopt;
}

absl::optional<SkColor> WebAppRegistrar::GetAppDarkModeBackgroundColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_background_color() : absl::nullopt;
}

const GURL& WebAppRegistrar::GetAppStartUrl(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->start_url() : GURL::EmptyGURL();
}

webapps::ManifestId WebAppRegistrar::GetAppManifestId(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_id() : webapps::ManifestId();
}

const std::string* WebAppRegistrar::GetAppLaunchQueryParams(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->launch_query_params() : nullptr;
}

const apps::ShareTarget* WebAppRegistrar::GetAppShareTarget(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return (web_app && web_app->share_target().has_value())
             ? &web_app->share_target().value()
             : nullptr;
}

const apps::FileHandlers* WebAppRegistrar::GetAppFileHandlers(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? &web_app->file_handlers() : nullptr;
}

bool WebAppRegistrar::IsAppFileHandlerPermissionBlocked(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return false;

  return web_app->file_handler_approval_state() ==
         ApiApprovalState::kDisallowed;
}

ApiApprovalState WebAppRegistrar::GetAppFileHandlerApprovalState(
    const webapps::AppId& app_id) const {
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
    const webapps::AppId& app_id) const {
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
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  // TODO(crbug.com/1469482): Record shortcut distinction explicitly instead of
  // using scope.
  // Shortcuts on the WebApp system have empty scopes, while the implementation
  // of IsShortcutApp just checks if the scope is |absl::nullopt|, so make sure
  // we return |absl::nullopt| rather than an empty scope.
  if (web_app->scope().is_empty())
    return absl::nullopt;

  return web_app->scope();
}

DisplayMode WebAppRegistrar::GetAppDisplayMode(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode() : DisplayMode::kUndefined;
}

absl::optional<mojom::UserDisplayMode> WebAppRegistrar::GetAppUserDisplayMode(
    const webapps::AppId& app_id) const {
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
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode_override()
                 : std::vector<DisplayMode>();
}

apps::UrlHandlers WebAppRegistrar::GetAppUrlHandlers(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->url_handlers()
                 : std::vector<apps::UrlHandlerInfo>();
}

base::flat_set<ScopeExtensionInfo> WebAppRegistrar::GetScopeExtensions(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->scope_extensions()
                 : base::flat_set<ScopeExtensionInfo>();
}

base::flat_set<ScopeExtensionInfo> WebAppRegistrar::GetValidatedScopeExtensions(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->validated_scope_extensions()
                 : base::flat_set<ScopeExtensionInfo>();
}

GURL WebAppRegistrar::GetAppManifestUrl(const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_url() : GURL::EmptyGURL();
}

base::Time WebAppRegistrar::GetAppLastBadgingTime(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->last_badging_time() : base::Time();
}

base::Time WebAppRegistrar::GetAppLastLaunchTime(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->last_launch_time() : base::Time();
}

base::Time WebAppRegistrar::GetAppFirstInstallTime(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->first_install_time() : base::Time();
}

absl::optional<webapps::WebappInstallSource>
WebAppRegistrar::GetLatestAppInstallSource(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return absl::nullopt;

  return web_app->latest_install_source();
}

std::vector<apps::IconInfo> WebAppRegistrar::GetAppIconInfos(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->manifest_icons() : std::vector<apps::IconInfo>();
}

SortedSizesPx WebAppRegistrar::GetAppDownloadedIconSizesAny(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->downloaded_icon_sizes(IconPurpose::ANY)
                 : SortedSizesPx();
}

std::vector<WebAppShortcutsMenuItemInfo>
WebAppRegistrar::GetAppShortcutsMenuItemInfos(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->shortcuts_menu_item_infos()
                 : std::vector<WebAppShortcutsMenuItemInfo>();
}

std::vector<webapps::AppId> WebAppRegistrar::GetAppIds() const {
  return GetAppIdsForAppSet(GetApps());
}

std::vector<webapps::AppId> WebAppRegistrar::GetAllSubAppIds(
    const webapps::AppId& parent_app_id) const {
  std::vector<webapps::AppId> sub_app_ids;

  for (const WebApp& app : GetApps()) {
    if (app.parent_app_id().has_value() &&
        *app.parent_app_id() == parent_app_id) {
      sub_app_ids.push_back(app.app_id());
    }
  }

  return sub_app_ids;
}

base::flat_map<webapps::AppId, webapps::AppId>
WebAppRegistrar::GetSubAppToParentMap() const {
  base::flat_map<webapps::AppId, webapps::AppId> parent_app_ids;

  for (const WebApp& app : GetApps()) {
    if (app.parent_app_id().has_value()) {
      parent_app_ids[app.app_id()] = *app.parent_app_id();
    }
  }

  return parent_app_ids;
}

ValueWithPolicy<RunOnOsLoginMode> WebAppRegistrar::GetAppRunOnOsLoginMode(
    const webapps::AppId& app_id) const {
  RunOnOsLoginPolicy login_policy =
      provider_->policy_manager().GetUrlRunOnOsLoginPolicy(app_id);

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
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->run_on_os_login_os_integration_state()
                 : absl::nullopt;
}

bool WebAppRegistrar::GetWindowControlsOverlayEnabled(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->window_controls_overlay_enabled() : false;
}

void WebAppRegistrar::OnProfileMarkedForPermanentDeletion(
    Profile* profile_to_be_deleted) {
  if (profile() != profile_to_be_deleted)
    return;

  for (const webapps::AppId& app_id :
       GetAppIdsForAppSet(GetAppsIncludingStubs())) {
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
    base::flat_map<webapps::AppId, mojom::UserDisplayMode> overrides) {
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
    run_on_os_login_fields.Set("value",
                               base::ToString(run_on_os_login_mode.value));
    run_on_os_login_fields.Set("user_controllable",
                               run_on_os_login_mode.user_controllable);

    base::Value::List* in_mem_controlled_frame_partitions =
        app_debug_dict.EnsureDict("isolated_data_in_memory")
            ->EnsureList("controlled_frame_partitions (in-memory)");
    auto it = isolated_web_app_in_memory_controlled_frame_partitions_.find(
        web_app->app_id());
    if (it != isolated_web_app_in_memory_controlled_frame_partitions_.end()) {
      for (const std::string& partition : it->second) {
        in_mem_controlled_frame_partitions->Append(partition);
      }
    }

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

WebApp* WebAppRegistrarMutable::GetAppByIdMutable(
    const webapps::AppId& app_id) {
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

std::vector<webapps::AppId> WebAppRegistrar::GetAppIdsForAppSet(
    const AppSet& app_set) const {
  std::vector<webapps::AppId> app_ids;

  for (const WebApp& app : app_set)
    app_ids.push_back(app.app_id());

  return app_ids;
}

#if BUILDFLAG(IS_CHROMEOS)
bool WebAppRegistrar::IsShortcutAppChromeOs(
    const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app) {
    return false;
  }

  // See go/shortstand-prd#bookmark=id.mbe9ojau9umf for detail.
  if (!chromeos::features::IsCrosShortstandEnabled()) {
    return !GetAppScopeInternal(app_id).has_value();
  }

  // Avoid opening Workspace apps in standalone windows if they are set to open
  // in browser.
  // TODO(b/312854225): Remove this special case once Workspace makes use of
  // tabbed web app display mode.
  if (web_app->app_id() == kGoogleDocsAppId ||
      web_app->app_id() == kGoogleSheetsAppId ||
      web_app->app_id() == kGoogleSlidesAppId) {
    return web_app->user_display_mode() == mojom::UserDisplayMode::kBrowser;
  }

  // For policy installed apps/shortcuts, it is a shortcut if admin set to open
  // in browser or install_as_shortcut is set to true.
  if (web_app->IsPolicyInstalledApp()) {
    // TODO(b/304660867): Check the required field for policy installed apps.
    return !GetAppScopeInternal(app_id).has_value();
  }

  // System web apps should always be considered as apps.
  if (web_app->IsSystemApp()) {
    return false;
  }

  // For web apps installed from Chrome Browser and play store by the user,
  // everything is considered as app instead of shortcut.
  if (web_app->WasInstalledByUser() &&
      GetAppScopeInternal(app_id).has_value()) {
    return false;
  }

  // Any default installed apps are considered as apps not shortcut.
  if (web_app->GetSources().Has(WebAppManagement::kDefault) ||
      web_app->GetSources().Has(WebAppManagement::kOem) ||
      web_app->GetSources().Has(WebAppManagement::kApsDefault)) {
    return false;
  }

  // For user created shortcuts via Chrome, we considered whether it is shortcut
  // based on the display mode setting. If will be considered as shortcut only
  // when it is set to open in the browser tab.
  if (web_app->WasInstalledByUser() &&
      !GetAppScopeInternal(app_id).has_value()) {
    return web_app->user_display_mode() == mojom::UserDisplayMode::kBrowser;
  }
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
