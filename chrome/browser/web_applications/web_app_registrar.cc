// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_registrar.h"

#include <algorithm>
#include <bitset>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/proto_helpers.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace web_app {
namespace {
using InstallStateSet = base::EnumSet<proto::InstallState,
                                      proto::InstallState_MIN,
                                      proto::InstallState_MAX>;

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

bool IsLinkCapturingDisabledByDefaultBasedOnFlagState() {
  return features::kNavigationCapturingDefaultState.Get() ==
             features::CapturingState::kDefaultOff ||
         features::kNavigationCapturingDefaultState.Get() ==
             features::CapturingState::kReimplDefaultOff;
}

}  // namespace

WebAppRegistrar::WebAppRegistrar(Profile* profile) : profile_(profile) {}

WebAppRegistrar::~WebAppRegistrar() {
  for (WebAppRegistrarObserver& observer : observers_) {
    observer.OnAppRegistrarDestroyed();
  }
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

// TODO(crbug.com/40264854): Revert changes back to old code
// once the system starts enforcing a single install URL per
// app_id.
std::optional<webapps::AppId> WebAppRegistrar::LookupPlaceholderAppId(
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
  return std::nullopt;
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

std::optional<webapps::AppId> WebAppRegistrar::LookupExternalAppId(
    const GURL& install_url) const {
  std::optional<webapps::AppId> app_id = LookUpAppIdByInstallUrl(install_url);
  if (app_id.has_value())
    return app_id;

  return std::nullopt;
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
      std::string_view::npos) {
    return start_url;
  }

  std::string query_params = start_url.query() + "&" + *launch_query_params;
  replacements.SetQueryStr(query_params);
  return start_url.ReplaceComponents(replacements);
}

GURL WebAppRegistrar::GetAppScope(const webapps::AppId& app_id) const {
  std::optional<GURL> scope = GetAppScopeInternal(app_id);
  if (scope)
    return *scope;
  return GetAppStartUrl(app_id).GetWithoutFilename();
}

int WebAppRegistrar::GetAppExtendedScopeScore(
    const GURL& url,
    const webapps::AppId& app_id) const {
  if (!url.is_valid()) {
    return 0;
  }

  int app_scope = GetUrlInAppScopeScore(url.spec(), app_id);
  if (app_scope > 0) {
    return app_scope;
  }

  const WebApp* app = GetAppById(app_id);
  if (!app || app->validated_scope_extensions().empty()) {
    return 0;
  }

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque() || origin.scheme() != url::kHttpsScheme) {
    return 0;
  }

  std::optional<std::string> origin_str;

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

int WebAppRegistrar::GetUrlInAppScopeScore(const std::string& url_spec,
                                           const webapps::AppId& app_id) const {
  std::string app_scope = GetAppScope(app_id).spec();

  // The app may have been uninstalled.
  if (app_scope.empty())
    return 0;

  int score =
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

std::optional<webapps::AppId> WebAppRegistrar::FindAppWithUrlInScope(
    const GURL& url) const {
  return FindBestAppWithUrlInScope(
      url, {
               proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
               proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
               proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
           });
}

bool WebAppRegistrar::DoesScopeContainAnyApp(const GURL& scope) const {
  return DoesScopeContainAnyApp(
      scope, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
              proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION});
}

std::vector<webapps::AppId> WebAppRegistrar::FindAppsInScope(
    const GURL& scope) const {
  return FindAllAppsNestedInUrl(
      scope, {
                 proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
                 proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
             });
}

std::optional<webapps::AppId> WebAppRegistrar::FindInstalledAppWithUrlInScope(
    const GURL& url,
    bool window_only,
    bool exclude_diy_apps) const {
  return FindBestAppWithUrlInScope(
      url,
      {
          proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
          proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
      },
      {.include_open_in_browser_tab = !window_only,
       .include_diy = !exclude_diy_apps});
}

bool WebAppRegistrar::IsNonLocallyInstalledAppWithUrlInScope(
    const GURL& url) const {
  return FindBestAppWithUrlInScope(
             url, {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE})
      .has_value();
}

bool WebAppRegistrar::IsShortcutApp(const webapps::AppId& app_id) const {
  if (!GetAppById(app_id)) {
    return false;
  }
  // TODO(crbug.com/40277513): Record shortcut distinction explicitly instead of
  // using scope.
  return !GetAppScopeInternal(app_id).has_value();
}

bool WebAppRegistrar::IsSystemApp(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->IsSystemApp();
}

DisplayMode WebAppRegistrar::GetAppEffectiveDisplayMode(
    const webapps::AppId& app_id) const {
  if (!IsInstallState(app_id,
                      {proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                       proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
    return DisplayMode::kBrowser;
  }

  auto app_display_mode = GetAppDisplayMode(app_id);
  std::optional<mojom::UserDisplayMode> user_display_mode =
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
    if (!web_app) {
      return GURL();
    }

    if (web_app->tab_strip()) {
      std::optional<GURL> url = web_app->tab_strip().value().new_tab_button.url;
      if (url.has_value())
        return url.value();
    }
  }
  // Apps that don't set a new_tab_button.url will use the start URL.
  return GetAppStartUrl(app_id);
}

std::optional<GURL> WebAppRegistrar::GetAppPinnedHomeTabUrl(
    const webapps::AppId& app_id) const {
  if (IsTabbedWindowModeEnabled(app_id)) {
    const WebApp* web_app = GetAppById(app_id);
    if (!web_app)
      return std::nullopt;

    if (web_app->tab_strip() &&
        absl::holds_alternative<blink::Manifest::HomeTabParams>(
            web_app->tab_strip().value().home_tab)) {
      return GetAppStartUrl(app_id);
    }
  }
  // Apps with home_tab set to 'auto' will not have a home tab.
  return std::nullopt;
}

std::optional<proto::WebAppOsIntegrationState>
WebAppRegistrar::GetAppCurrentOsIntegrationState(
    const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return std::nullopt;

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
  auto it = registry_.find(app_id);
  if (it != registry_.end() && WebAppSourceSupported(*it->second))
    return it->second.get();

  return nullptr;
}

const WebApp* WebAppRegistrar::GetAppByStartUrl(const GURL& start_url) const {
  for (auto const& it : registry_) {
    if (WebAppSourceSupported(*it.second) &&
        it.second->start_url() == start_url)
      return it.second.get();
  }
  return nullptr;
}

std::vector<webapps::AppId>
WebAppRegistrar::GetAppsFromSyncAndPendingInstallation() const {
  AppSet apps_in_sync_install =
      AppSet(this, [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               web_app.is_from_sync_and_pending_installation();
      });

  std::vector<webapps::AppId> app_ids;
  for (const WebApp& app : apps_in_sync_install)
    app_ids.push_back(app.app_id());

  return app_ids;
}

std::vector<webapps::AppId> WebAppRegistrar::GetAppsPendingUninstall() const {
  AppSet apps_in_sync_uninstall =
      AppSet(this, [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               web_app.is_uninstalling();
      });

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
  int num_user_installed_apps =
      std::get<InstallableAppCount>(CountTotalUserInstalledAppsIncludingDiy())
          .value();
  int num_user_installed_diy_apps =
      std::get<DiyAppCount>(CountTotalUserInstalledAppsIncludingDiy()).value();
  int num_non_locally_installed = CountUserInstalledNotLocallyInstalledApps();

  base::UmaHistogramCounts1000("WebApp.InstalledCount.ByUser",
                               num_user_installed_apps);
  base::UmaHistogramCounts1000(
      "WebApp.InstalledCount.ByUserNotLocallyInstalled",
      num_non_locally_installed);
  base::UmaHistogramCounts1000("WebApp.DiyAppsInstalledCount.ByUser",
                               num_user_installed_diy_apps);

#if BUILDFLAG(IS_MAC)
  auto multi_profile_app_ids =
      AppShimRegistry::Get()->GetAppsInstalledInMultipleProfiles();
  int num_multi_profile_apps = 0;
  for (const auto& app_id : multi_profile_app_ids) {
    const WebApp* app = GetAppById(app_id);
    if (app && app->install_state() == proto::INSTALLED_WITH_OS_INTEGRATION &&
        app->WasInstalledByUser()) {
      num_multi_profile_apps++;
    }
  }
  base::UmaHistogramCounts1000("WebApp.InstalledCount.ByUserInMultipleProfiles",
                               num_multi_profile_apps);
#endif
}

base::WeakPtr<WebAppRegistrar> WebAppRegistrar::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::optional<webapps::AppId> WebAppRegistrar::LookUpAppIdByInstallUrl(
    const GURL& install_url) const {
  for (const WebApp& web_app : GetApps()) {
    for (auto it : web_app.management_to_external_config_map()) {
      if (base::Contains(it.second.install_urls, install_url)) {
        return web_app.app_id();
      }
    }
  }
  return std::nullopt;
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

bool WebAppRegistrar::IsNotInRegistrar(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app || web_app->is_uninstalling()) {
    return true;
  }

  // `is_from_sync_and_pending_installation()` should be treated as 'not
  // installed' only if there are no other sources that have installed the web
  // app.
  WebAppManagementTypes sources_except_sync = web_app->GetSources();
  sources_except_sync.Remove(WebAppManagement::kSync);
  if (web_app->is_from_sync_and_pending_installation() &&
      sources_except_sync.empty()) {
    return true;
  }
  return false;
}

std::optional<proto::InstallState> WebAppRegistrar::GetInstallState(
    const webapps::AppId& app_id) const {
  if (IsNotInRegistrar(app_id)) {
    return std::nullopt;
  }
  const WebApp* web_app = GetAppById(app_id);
  CHECK(web_app);
  return web_app->install_state();
}

bool WebAppRegistrar::IsInstallState(
    const webapps::AppId& app_id,
    std::initializer_list<proto::InstallState> states) const {
  CHECK_NE(states.size(), 0ul);
  std::optional<proto::InstallState> install_state = GetInstallState(app_id);
  if (!install_state) {
    return false;
  }
  InstallStateSet state_set(states);
  return state_set.Has(install_state.value());
}

std::optional<webapps::AppId> WebAppRegistrar::FindBestAppWithUrlInScope(
    const GURL& url,
    std::initializer_list<proto::InstallState> states) const {
  return FindBestAppWithUrlInScope(url, states, AppFilterOptions());
}

std::optional<webapps::AppId> WebAppRegistrar::FindBestAppWithUrlInScope(
    const GURL& url,
    std::initializer_list<proto::InstallState> states,
    AppFilterOptions options) const {
  CHECK_NE(states.size(), 0ul);
  if (!url.is_valid()) {
    return std::nullopt;
  }

  const std::string url_spec = url.spec();

  std::optional<webapps::AppId> best_app_id;
  int best_score = 0;
  bool best_app_is_shortcut = true;

  for (const webapps::AppId& app_id :
       GetAppIdsForAppSet(GetAppsIncludingStubs())) {
    if (!IsInstallState(app_id, states)) {
      continue;
    }
    if (!options.include_open_in_browser_tab &&
        GetAppEffectiveDisplayMode(app_id) == DisplayMode::kBrowser) {
      continue;
    }

    if (!options.include_diy && IsDiyApp(app_id)) {
      continue;
    }
    // TODO(crbug.com/40277513): Consider treating shortcuts differently to
    // PWAs.
    // TODO(crbug.com/341316725): Remove shortcut apps.
    bool app_is_shortcut = IsShortcutApp(app_id);
    if (app_is_shortcut && !best_app_is_shortcut) {
      continue;
    }

    int score;
    // TODO(crbug.com/341337420): Audit call sites and ideally have scope
    // extensions be considered by default.
    if (options.include_extended_scope) {
      score = GetAppExtendedScopeScore(url, app_id);
    } else {
      score = GetUrlInAppScopeScore(url_spec, app_id);
    }

    if (score > 0 &&
        (score > best_score || (best_app_is_shortcut && !app_is_shortcut))) {
      best_app_id = app_id;
      best_score = score;
      best_app_is_shortcut = app_is_shortcut;
    }
  }
  return best_app_id;
}

// Returns all apps that have the given `url` in scope and are in one of the
// given `states`.
std::vector<webapps::AppId> WebAppRegistrar::FindAllAppsWithUrlInScope(
    const GURL& url,
    std::initializer_list<proto::InstallState> states) const {
  CHECK_NE(states.size(), 0ul);
  if (!url.is_valid()) {
    return {};
  }
  std::string url_spec = url.spec();

  std::vector<webapps::AppId> apps_with_url_in_scope;
  for (const auto& app_id : GetAppIdsForAppSet(GetAppsIncludingStubs())) {
    if (!IsInstallState(app_id, states)) {
      continue;
    }

    std::string app_scope = GetAppScope(app_id).spec();
    DCHECK(!app_scope.empty());

    if (!base::StartsWith(app_scope, url_spec, base::CompareCase::SENSITIVE)) {
      continue;
    }

    apps_with_url_in_scope.push_back(app_id);
  }

  return apps_with_url_in_scope;
}

// Returns all apps that have the given `url` in scope and are in one of the
// given `states`.
std::vector<webapps::AppId> WebAppRegistrar::FindAllAppsNestedInUrl(
    const GURL& outer_scope,
    std::initializer_list<proto::InstallState> states) const {
  CHECK_NE(states.size(), 0ul);
  if (!outer_scope.is_valid()) {
    return {};
  }
  std::string outer_scope_spec = outer_scope.spec();

  std::vector<webapps::AppId> apps_in_outer_scope;
  for (const auto& app_id : GetAppIdsForAppSet(GetAppsIncludingStubs())) {
    if (!IsInstallState(app_id, states)) {
      continue;
    }

    std::string app_scope = GetAppScope(app_id).spec();
    DCHECK(!app_scope.empty());

    if (!base::StartsWith(app_scope, outer_scope_spec,
                          base::CompareCase::SENSITIVE)) {
      continue;
    }

    apps_in_outer_scope.push_back(app_id);
  }

  return apps_in_outer_scope;
}

bool WebAppRegistrar::DoesScopeContainAnyApp(
    const GURL& scope,
    std::initializer_list<proto::InstallState> allowed_states) const {
  std::string scope_str = scope.spec();

  for (const auto& app_id : GetAppIdsForAppSet(GetAppsIncludingStubs())) {
    if (!IsInstallState(app_id, allowed_states)) {
      continue;
    }

    std::string app_scope = GetAppScope(app_id).spec();
    CHECK(!app_scope.empty());

    if (base::StartsWith(app_scope, scope_str, base::CompareCase::SENSITIVE)) {
      return true;
    }
    // TODO(crbug.com/341337420): Support scope extensions.
  }
  return false;
}

bool WebAppRegistrar::IsInstalled(const webapps::AppId& app_id) const {
  return IsInstallState(
      app_id, {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
               proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
               proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION});
}

bool WebAppRegistrar::IsUninstalling(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->is_uninstalling();
}

bool WebAppRegistrar::IsActivelyInstalled(const webapps::AppId& app_id) const {
  return IsInstallState(app_id,
                        {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION});
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

bool WebAppRegistrar::IsInstalledByPolicy(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app) {
    return false;
  }

  WebAppManagementTypes sources = web_app->GetSources();
  if (web_app->isolation_data().has_value()) {
    return sources.Has(WebAppManagement::Type::kIwaPolicy);
  }
  return sources.Has(WebAppManagement::Type::kPolicy);
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
  return std::get<InstallableAppCount>(
             CountTotalUserInstalledAppsIncludingDiy())
      .value();
}

int WebAppRegistrar::CountUserInstalledDiyApps() const {
  return std::get<DiyAppCount>(CountTotalUserInstalledAppsIncludingDiy())
      .value();
}

std::vector<content::StoragePartitionConfig>
WebAppRegistrar::GetIsolatedWebAppStoragePartitionConfigs(
    const webapps::AppId& isolated_web_app_id) const {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(profile_)) {
    return {};
  }
  const WebApp* isolated_web_app = GetAppById(isolated_web_app_id);
  if (!isolated_web_app) {
    return {};
  }
  // Note: This function is called after is_installed is set to true, so regular
  // helper functions cannot be used.
  if (isolated_web_app->install_state() !=
          proto::INSTALLED_WITH_OS_INTEGRATION &&
      isolated_web_app->install_state() !=
          proto::INSTALLED_WITHOUT_OS_INTEGRATION) {
    return {};
  }

  if (!isolated_web_app->isolation_data()) {
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
       isolated_web_app->isolation_data()->controlled_frame_partitions()) {
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

std::optional<content::StoragePartitionConfig>
WebAppRegistrar::SaveAndGetInMemoryControlledFramePartitionConfig(
    const IsolatedWebAppUrlInfo& url_info,
    const std::string& partition_name) {
  if (!IsInstalled(url_info.app_id())) {
    return std::nullopt;
  }

  isolated_web_app_in_memory_controlled_frame_partitions_[url_info.app_id()]
      .insert(partition_name);

  return url_info.GetStoragePartitionConfigForControlledFrame(
      profile_, partition_name, true);
}

bool WebAppRegistrar::CanCaptureLinksInScope(
    const webapps::AppId& app_id) const {
  if (!base::FeatureList::IsEnabled(features::kPwaNavigationCapturing)) {
    return false;
  }
  if (!IsInstallState(app_id,
                      {proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                       proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}) ||
      IsShortcutApp(app_id)) {
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
      if (IsLinkCapturingDisabledByDefaultBasedOnFlagState()) {
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

std::optional<webapps::AppId> WebAppRegistrar::FindAppThatCapturesLinksInScope(
    const GURL& url) const {
  // Nested apps remove that URL space from the parent app, so links from a
  // nested app cannot be captured by a parent app. Even so, there can be
  // multiple apps with the same score, but the only one that matters is the
  // first one that also captures links.
  int top_score = 0;
  std::vector<webapps::AppId> top_apps;
  for (const webapps::AppId& app_id : GetAppIds()) {
    if (!CanCaptureLinksInScope(app_id)) {
      continue;
    }
    int score;
    if (base::FeatureList::IsEnabled(
            features::kPwaNavigationCapturingWithScopeExtensions)) {
      score = GetAppExtendedScopeScore(url, app_id);
    } else {
      score = GetUrlInAppScopeScore(url.spec(), app_id);
    }
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
    return std::nullopt;
  }
  for (const webapps::AppId& app_id : top_apps) {
    if (CapturesLinksInScope(app_id)) {
      return app_id;
    }
  }
  return std::nullopt;
}

bool WebAppRegistrar::IsLinkCapturableByApp(const webapps::AppId& app,
                                            const GURL& url) const {
  CHECK(url.is_valid());
  int app_score;
  if (base::FeatureList::IsEnabled(
          features::kPwaNavigationCapturingWithScopeExtensions)) {
    app_score = GetAppExtendedScopeScore(url, app);
  } else {
    app_score = GetUrlInAppScopeScore(url.spec(), app);
  }
  if (app_score == 0) {
    return false;
  }
  return base::ranges::none_of(GetAppIds(), [&](const webapps::AppId& app_id) {
    int other_score;
    if (base::FeatureList::IsEnabled(
            features::kPwaNavigationCapturingWithScopeExtensions)) {
      other_score = GetAppExtendedScopeScore(url, app_id);

    } else {
      other_score = GetUrlInAppScopeScore(url.spec(), app_id);
    }
    return IsInstallState(
               app_id, {proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                        proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}) &&
           !IsShortcutApp(app_id) && other_score > app_score;
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
  if (!IsInstallState(app_id1, {proto::INSTALLED_WITH_OS_INTEGRATION,
                                proto::INSTALLED_WITHOUT_OS_INTEGRATION}) ||
      !IsInstallState(app_id2, {proto::INSTALLED_WITH_OS_INTEGRATION,
                                proto::INSTALLED_WITHOUT_OS_INTEGRATION})) {
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
    if (!IsInstallState(app_id,
                        {proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                         proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
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

bool WebAppRegistrar::IsDiyApp(const webapps::AppId& app_id) const {
  if (!IsInstalled(app_id)) {
    return false;
  }
  const WebApp* web_app = GetAppById(app_id);
  return web_app && web_app->is_diy_app();
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

std::optional<SkColor> WebAppRegistrar::GetAppThemeColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->theme_color() : std::nullopt;
}

std::optional<SkColor> WebAppRegistrar::GetAppDarkModeThemeColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_theme_color() : std::nullopt;
}

std::optional<SkColor> WebAppRegistrar::GetAppBackgroundColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->background_color() : std::nullopt;
}

std::optional<SkColor> WebAppRegistrar::GetAppDarkModeBackgroundColor(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->dark_mode_background_color() : std::nullopt;
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
  auto state = GetAppCurrentOsIntegrationState(app_id);
  if (!state.has_value()) {
    return false;
  }

  return state->has_file_handling();
}

std::optional<GURL> WebAppRegistrar::GetAppScopeInternal(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (!web_app)
    return std::nullopt;

  // TODO(crbug.com/40277513): Record shortcut distinction explicitly instead of
  // using scope.
  // Shortcuts on the WebApp system have empty scopes, while the implementation
  // of IsShortcutApp just checks if the scope is |std::nullopt|, so make sure
  // we return |std::nullopt| rather than an empty scope.
  // TODO(crbug.com/341316725): Remove shortcut apps.
  if (web_app->scope().is_empty())
    return std::nullopt;

  return web_app->scope();
}

DisplayMode WebAppRegistrar::GetAppDisplayMode(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->display_mode() : DisplayMode::kUndefined;
}

std::optional<mojom::UserDisplayMode> WebAppRegistrar::GetAppUserDisplayMode(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  if (web_app == nullptr) {
    return std::nullopt;
  }

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
  return web_app ? web_app->manifest_url() : GURL();
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

std::optional<webapps::WebappInstallSource>
WebAppRegistrar::GetLatestAppInstallSource(const webapps::AppId& app_id) const {
  const WebApp* web_app = GetAppById(app_id);
  if (!web_app)
    return std::nullopt;

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

bool WebAppRegistrar::GetWindowControlsOverlayEnabled(
    const webapps::AppId& app_id) const {
  auto* web_app = GetAppById(app_id);
  return web_app ? web_app->window_controls_overlay_enabled() : false;
}

WebAppRegistrar::AppSet::AppSet(const WebAppRegistrar* registrar, Filter filter)
    : registrar_(registrar),
      filter_(filter)
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
  return iterator(registrar_->registry_.begin(), registrar_->registry_.end(),
                  filter_);
}

WebAppRegistrar::AppSet::iterator WebAppRegistrar::AppSet::end() {
  return iterator(registrar_->registry_.end(), registrar_->registry_.end(),
                  filter_);
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::begin() const {
  return const_iterator(registrar_->registry_.begin(),
                        registrar_->registry_.end(), filter_);
}

WebAppRegistrar::AppSet::const_iterator WebAppRegistrar::AppSet::end() const {
  return const_iterator(registrar_->registry_.end(),
                        registrar_->registry_.end(), filter_);
}

WebAppRegistrar::AppSet WebAppRegistrar::GetAppsIncludingStubs() const {
  return AppSet(this, [](const WebApp& web_app) {
    return WebAppSourceSupported(web_app);
  });
}

WebAppRegistrar::AppSet WebAppRegistrar::GetApps() const {
  return AppSet(
      this, [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               !web_app.is_uninstalling();
      });
}

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

WebAppRegistrar::AppSet WebAppRegistrarMutable::GetAppsMutable() {
  return AppSet(
      this, [](const WebApp& web_app) {
        return WebAppSourceSupported(web_app) &&
               !web_app.is_from_sync_and_pending_installation() &&
               !web_app.is_uninstalling();
      });
}

bool IsRegistryEqual(const Registry& registry,
                     const Registry& registry2,
                     bool exclude_current_os_integration) {
  if (registry.size() != registry2.size()) {
    LOG(ERROR) << registry.size() << " != " << registry2.size();
    return false;
  }

  for (auto& kv : registry) {
    // Copy to allow clearing the os integration.
    WebApp web_app = WebApp(*kv.second);
    if (!registry2.contains(web_app.app_id())) {
      LOG(ERROR) << "Registry does not contain app: " << web_app;
      return false;
    }
    WebApp web_app2 = WebApp(*registry2.at(web_app.app_id()));
    if (exclude_current_os_integration) {
      web_app.SetCurrentOsIntegrationStates(proto::WebAppOsIntegrationState());
      web_app2.SetCurrentOsIntegrationStates(proto::WebAppOsIntegrationState());
      // Tests that want to ignore current os integration state usually also
      // want to ignore the presence/absece of the "user installed" source, as
      // that is something else that is not synced across.
      // TODO(https://crbug.com/372062068): Figure out a better way to handle
      // differences in installed state.
      web_app.RemoveSource(WebAppManagement::kUserInstalled);
      web_app2.RemoveSource(WebAppManagement::kUserInstalled);
    }
    if (web_app != web_app2) {
      LOG(ERROR) << "Web apps are not equal:\n" << web_app << "\n" << web_app2;
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

int WebAppRegistrar::CountUserInstalledNotLocallyInstalledApps() const {
  int num_non_locally_installed = 0;
  for (const WebApp& app : GetApps()) {
    if (app.install_state() ==
            proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE &&
        app.WasInstalledByUser()) {
      ++num_non_locally_installed;
    }
  }
  return num_non_locally_installed;
}

std::tuple<DiyAppCount, InstallableAppCount>
WebAppRegistrar::CountTotalUserInstalledAppsIncludingDiy() const {
  InstallableAppCount num_user_installed(0);
  DiyAppCount num_diy_apps_user_installed(0);
  for (const WebApp& app : GetApps()) {
    if ((app.install_state() == proto::INSTALLED_WITH_OS_INTEGRATION ||
         app.install_state() == proto::INSTALLED_WITHOUT_OS_INTEGRATION) &&
        app.WasInstalledByUser()) {
      if (app.is_diy_app()) {
        ++num_diy_apps_user_installed.value();
      }
      ++num_user_installed.value();
    }
  }
  return std::make_tuple(num_diy_apps_user_installed, num_user_installed);
}

}  // namespace web_app
