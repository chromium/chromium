// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/web_applications/web_app_system_web_app_delegate_map_utils.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

bool IconInfosContainIconURL(const std::vector<apps::IconInfo>& icon_infos,
                             const GURL& url) {
  for (const apps::IconInfo& info : icon_infos) {
    if (info.url.EqualsIgnoringRef(url))
      return true;
  }
  return false;
}

#if BUILDFLAG(IS_CHROMEOS)
void LogIsolatedWebAppInstallResult(
    std::vector<web_app::IsolatedWebAppPolicyManager::EphemeralAppInstallResult>
        result) {
  for (size_t i = 0; i < result.size(); ++i) {
    if (result[i] != web_app::IsolatedWebAppPolicyManager::
                         EphemeralAppInstallResult::kSuccess) {
      DLOG(WARNING) << "Could not force-install IWA number " << i + 1
                    << " failed. Error: " << static_cast<int>(result[i]);
    }
  }
}
#endif

// Policy installed apps are only allowed on:
// 1. ChromeOS guest sessions (current only on Ash).
// 2. All Chrome profiles apart from incognito/guest profiles.
bool AreForceInstalledAppsAllowed(Profile* profile) {
  bool allowed = web_app::AreWebAppsUserInstallable(profile);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  allowed = allowed || user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
            user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
#endif
  return allowed;
}

bool HasPreviouslyMigratedErrorLoadedPolicyApp(PrefService* pref_service) {
  return pref_service->GetBoolean(
      prefs::kErrorLoadedPolicyAppMigrationCompleted);
}

void RecordErrorLoadedPolicyAppsMigrated(PrefService* pref_service) {
  pref_service->SetBoolean(prefs::kErrorLoadedPolicyAppMigrationCompleted,
                           true);
}

bool IsForceUnregistrationPolicyEnabled() {
  return base::FeatureList::IsEnabled(
             web_app::kDesktopPWAsForceUnregisterOSIntegration) &&
         web_app::AreSubManagersExecuteEnabled();
}

}  // namespace

namespace web_app {

BASE_FEATURE(kDesktopPWAsForceUnregisterOSIntegration,
             "DesktopPWAsForceUnregisterOSIntegration",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
);

const char WebAppPolicyManager::kInstallResultHistogramName[];

WebAppPolicyManager::WebAppPolicyManager(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      default_settings_(
          std::make_unique<WebAppPolicyManager::WebAppSetting>()) {}

WebAppPolicyManager::~WebAppPolicyManager() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WebAppPolicyManager::SetSystemWebAppDelegateMap(
    const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map) {
  system_web_apps_delegate_map_ = system_web_apps_delegate_map;
}
#endif

void WebAppPolicyManager::SetProvider(base::PassKey<WebAppProvider>,
                                      WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppPolicyManager::Start(
    base::OnceClosure policy_settings_and_force_installs_applied) {
  DCHECK(policy_settings_and_force_installs_applied_.is_null());

  policy_settings_and_force_installs_applied_ =
      std::move(policy_settings_and_force_installs_applied);
  // When Lacros is enabled, don't run PWA-specific logic in Ash.
  // TODO(crbug.com/1251491): Consider factoring out logic that should only run
  // in Ash into a separate class. This way, when running in Ash, we won't need
  // to construct a WebAppPolicyManager.
  bool enable_pwa_support = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  enable_pwa_support = !IsWebAppsCrosapiEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicy,
                     weak_ptr_factory_.GetWeakPtr(), enable_pwa_support));
}

void WebAppPolicyManager::ReinstallPlaceholderAppIfNecessary(
    const GURL& url,
    ExternallyManagedAppManager::OnceInstallCallback on_complete) {
  const base::Value::List& web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  const auto& web_apps_list = web_apps;

  const auto it = base::ranges::find(
      web_apps_list, url.spec(), [](const base::Value& entry) {
        return CHECK_DEREF(entry.GetDict().FindString(kUrlKey));
      });

  bool is_placeholder_url =
      provider_->registrar_unsafe()
          .LookupPlaceholderAppId(url, WebAppManagement::kPolicy)
          .has_value();

  if (it == web_apps_list.end() || !is_placeholder_url) {
    std::move(on_complete)
        .Run(url, ExternallyManagedAppManager::InstallResult(
                      webapps::InstallResultCode::kFailedPlaceholderUninstall));
    return;
  }

  ExternalInstallOptions install_options =
      ParseInstallPolicyEntry(it->GetDict());

  if (!install_options.install_url.is_valid()) {
    std::move(on_complete)
        .Run(url, ExternallyManagedAppManager::InstallResult(
                      webapps::InstallResultCode::kInstallURLInvalid));
    return;
  }

  // No need to install a placeholder because there should be one already.
  install_options.wait_for_windows_closed = true;

  // If the app is not a placeholder app, ExternallyManagedAppManager will
  // ignore the request.
  provider_->externally_managed_app_manager().InstallNow(
      std::move(install_options), std::move(on_complete));
}

// static
void WebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
  registry->RegisterListPref(prefs::kWebAppSettings);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterListPref(prefs::kIsolatedWebAppInstallForceList);
#endif
}

void WebAppPolicyManager::InitChangeRegistrarAndRefreshPolicy(
    bool enable_pwa_support) {
  pref_change_registrar_.Init(pref_service_);
  if (enable_pwa_support) {
    pref_change_registrar_.Add(
        prefs::kWebAppInstallForceList,
        base::BindRepeating(&WebAppPolicyManager::RefreshPolicyInstalledApps,
                            weak_ptr_factory_.GetWeakPtr()));
    if (base::FeatureList::IsEnabled(
            features::kDesktopPWAsEnforceWebAppSettingsPolicy)) {
      pref_change_registrar_.Add(
          prefs::kWebAppSettings,
          base::BindRepeating(&WebAppPolicyManager::RefreshPolicySettings,
                              weak_ptr_factory_.GetWeakPtr()));

      RefreshPolicySettings();
    }
    RefreshPolicyInstalledApps();

#if BUILDFLAG(IS_CHROMEOS)
    pref_change_registrar_.Add(
        prefs::kIsolatedWebAppInstallForceList,
        base::BindRepeating(
            &WebAppPolicyManager::RefreshPolicyInstalledIsolatedWebApps,
            weak_ptr_factory_.GetWeakPtr()));
    RefreshPolicyInstalledIsolatedWebApps();
#endif
  } else {
    if (policy_settings_and_force_installs_applied_) {
      std::move(policy_settings_and_force_installs_applied_).Run();
    }
  }
  ObserveDisabledSystemFeaturesPolicy();
}

void WebAppPolicyManager::OnDisableListPolicyChanged() {
#if BUILDFLAG(IS_CHROMEOS)
  PopulateDisabledWebAppsIdsLists();
  std::vector<webapps::AppId> app_ids =
      provider_->registrar_unsafe().GetAppIds();
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);
  for (const auto& id : app_ids) {
    const bool is_disabled = base::Contains(disabled_web_apps_, id);
    provider->scheduler().SetAppIsDisabled(id, is_disabled, base::DoNothing());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppPolicyManager::OnSyncPolicySettingsCommandsComplete() {
  provider_->registrar_unsafe().NotifyWebAppSettingsPolicyChanged();
  if (refresh_policy_settings_completed_) {
    std::move(refresh_policy_settings_completed_).Run();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const std::set<ash::SystemWebAppType>&
WebAppPolicyManager::GetDisabledSystemWebApps() const {
  return disabled_system_apps_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const std::set<webapps::AppId>& WebAppPolicyManager::GetDisabledWebAppsIds()
    const {
  return disabled_web_apps_;
}

bool WebAppPolicyManager::IsWebAppInDisabledList(
    const webapps::AppId& app_id) const {
  return base::Contains(GetDisabledWebAppsIds(), app_id);
}

bool WebAppPolicyManager::IsDisabledAppsModeHidden() const {
#if BUILDFLAG(IS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return false;

  std::string disabled_mode =
      local_state->GetString(policy::policy_prefs::kSystemFeaturesDisableMode);
  if (disabled_mode == policy::kHiddenDisableMode)
    return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  return false;
}

void WebAppPolicyManager::RefreshPolicyInstalledApps() {
  if (!AreForceInstalledAppsAllowed(profile_)) {
    OnWebAppForceInstallPolicyParsed();
    return;
  }

  // If this is called again while in progress, we will run it again once the
  // |SynchronizeInstalledApps| call is finished.
  if (is_refreshing_) {
    needs_refresh_ = true;
    return;
  }

  is_refreshing_ = true;
  needs_refresh_ = false;

  custom_manifest_values_by_url_.clear();

  const base::Value::List& web_apps =
      pref_service_->GetList(prefs::kWebAppInstallForceList);
  std::vector<ExternalInstallOptions> install_options_list;
  // No need to validate the types or values of the policy members because we
  // are using a SimpleSchemaValidatingPolicyHandler which should validate them
  // for us.
  for (const base::Value& entry : web_apps) {
    ExternalInstallOptions install_options =
        ParseInstallPolicyEntry(entry.GetDict());

    if (!install_options.install_url.is_valid())
      continue;

    install_options.install_placeholder = true;
    // When the policy gets refreshed, we should try to reinstall placeholder
    // apps but only if they are not being used. In the non-placeholder case, we
    // will not reinstall and there is no need to wait for windows being closed.
    install_options.wait_for_windows_closed =
        provider_->registrar_unsafe()
            .LookupPlaceholderAppId(install_options.install_url,
                                    WebAppManagement::kPolicy)
            .has_value();

    absl::optional<webapps::AppId> app_id =
        provider_->registrar_unsafe().LookupExternalAppId(
            install_options.install_url);

    if (app_id) {
      // If the override name has changed, reinstall:
      if (install_options.override_name &&
          install_options.override_name.value() !=
              provider_->registrar_unsafe().GetAppShortName(app_id.value())) {
        install_options.force_reinstall = true;
      }

      // If the override icon has changed, reinstall:
      if (install_options.override_icon_url &&
          !IconInfosContainIconURL(
              provider_->registrar_unsafe().GetAppIconInfos(app_id.value()),
              install_options.override_icon_url.value())) {
        install_options.force_reinstall = true;
      }

      // TODO(crbug.com/1440946): Remove code in M121.
      if (IsMaybeErrorLoadedPolicyApp(app_id.value(),
                                      install_options.install_url)) {
        install_options.force_reinstall = true;
      }
    }
    install_options_list.push_back(std::move(install_options));
  }
  // We only need to record the error loaded policy app migrations once, since
  // it is guaranteed to be fixed post M115, and a one time migration should
  // effectively fix the erroneous use-cases.
  RecordErrorLoadedPolicyAppsMigrated(profile_->GetPrefs());

  provider_->externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kExternalPolicy,
      base::BindOnce(&WebAppPolicyManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppPolicyManager::RefreshPolicyInstalledIsolatedWebApps() {
  const base::Value::List& isolated_web_apps =
      pref_service_->GetList(prefs::kIsolatedWebAppInstallForceList);
  if (isolated_web_apps.empty()) {
    return;
  }

  if (iwa_policy_manager_) {
    // Isolated web apps have already been processed.
    LOG(WARNING) << "Updating of the IWA is not yet supported.";
    return;
  }

  std::vector<IsolatedWebAppExternalInstallOptions> all_iwa_install_options;
  all_iwa_install_options.reserve(isolated_web_apps.size());
  for (const auto& policy_entry : isolated_web_apps) {
    const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
        options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
            policy_entry);
    if (options.has_value()) {
      all_iwa_install_options.push_back(options.value());
    } else {
      LOG(ERROR) << "Could not interprete IWA force-install policy: "
                 << options.error();
    }
  }

  auto url_loader_factory = profile_->GetURLLoaderFactory();

  WebAppProvider* const web_app_provider =
      web_app::WebAppProvider::GetForWebApps(profile_);
  if (!web_app_provider) {
    LOG(ERROR) << "Can't force-install isolated apps: No web app provider";
    return;
  }
  std::unique_ptr<IsolatedWebAppPolicyManager::IwaInstallCommandWrapper>
      installer = std::make_unique<
          IsolatedWebAppPolicyManager::IwaInstallCommandWrapperImpl>(
          web_app_provider);
  iwa_policy_manager_ = std::make_unique<IsolatedWebAppPolicyManager>(
      profile_->GetPath(), all_iwa_install_options, url_loader_factory,
      std::move(installer), base::BindOnce(&LogIsolatedWebAppInstallResult));
  iwa_policy_manager_->InstallEphemeralApps();
}
#endif

void WebAppPolicyManager::ParsePolicySettings() {
  // No need to validate the types or values of the policy members because we
  // are using a WebAppSettingsPolicyHandler which should validate them for us.
  const base::Value::List& web_apps_list =
      pref_service_->GetList(prefs::kWebAppSettings);

  settings_by_url_.clear();
  default_settings_ = std::make_unique<WebAppPolicyManager::WebAppSetting>();

  // Read default policy, if provided.
  const auto it = base::ranges::find(
      web_apps_list, kWildcard, [](const base::Value& entry) {
        return CHECK_DEREF(entry.GetDict().FindString(kManifestId));
      });

  if (it != web_apps_list.end() && it->is_dict()) {
    if (!default_settings_->Parse(it->GetDict(), true)) {
      SYSLOG(WARNING) << "Malformed default web app management setting.";
      default_settings_->ResetSettings();
    }
  }

  // Read policy for individual web apps
  for (const auto& iter : web_apps_list) {
    const auto& dict = iter.GetDict();
    const std::string* web_app_id_str = dict.FindString(kManifestId);

    if (*web_app_id_str == kWildcard)
      continue;

    GURL url = GURL(*web_app_id_str);
    if (!url.is_valid()) {
      LOG(WARNING) << "Invalid URL: " << *web_app_id_str;
      continue;
    }

    WebAppPolicyManager::WebAppSetting by_url(*default_settings_);
    if (by_url.Parse(dict, /*for_default_settings=*/false)) {
      settings_by_url_[url.spec()] = by_url;
    } else {
      LOG(WARNING) << "Malformed web app settings for " << url;
    }
  }
}

void WebAppPolicyManager::RefreshPolicySettings() {
  ParsePolicySettings();
  ApplyPolicySettings();
}

void WebAppPolicyManager::ApplyPolicySettings() {
  // The number of closures are 2, since we want to wait for 2 things to
  // complete:
  // 1. Applying Run on OS login settings policy.
  // 2. Applying force unregistration settings policy.
  // If for any reason the same app_id is being used for both Run on OS
  // login and force unregistration, it is still safe, since both functions
  // invoke commands, so the Run on OS login will always be scheduled before the
  // force unregistration, and execution will be synchronous.
  auto policy_settings_applied_callback = base::BarrierClosure(
      /*num_closures=*/2,
      base::BindOnce(&WebAppPolicyManager::OnSyncPolicySettingsCommandsComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  ApplyRunOnOsLoginPolicySettings(policy_settings_applied_callback);
  ApplyForceOSUnregistrationPolicySettings(policy_settings_applied_callback);
}

void WebAppPolicyManager::ApplyRunOnOsLoginPolicySettings(
    base::OnceClosure policy_settings_applied_callback) {
  std::vector<webapps::AppId> app_ids_to_sync =
      provider_->registrar_unsafe().GetAppIds();
  auto callback_for_sync_commands = base::BarrierClosure(
      app_ids_to_sync.size(), std::move(policy_settings_applied_callback));
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile_);
  for (const webapps::AppId& app_id : app_ids_to_sync) {
    provider->scheduler().SyncRunOnOsLoginMode(app_id,
                                               callback_for_sync_commands);
  }
}

void WebAppPolicyManager::ApplyForceOSUnregistrationPolicySettings(
    base::OnceClosure policy_settings_applied_callback) {
  if (!IsForceUnregistrationPolicyEnabled()) {
    std::move(policy_settings_applied_callback).Run();
    return;
  }

  base::flat_set<webapps::AppId> app_ids_for_force_unregistration;
  for (const auto& [manifest_string, setting] : settings_by_url_) {
    const GURL manifest_id = GURL(manifest_string);
    if (!manifest_id.is_valid()) {
      continue;
    }

    const webapps::AppId& app_id =
        web_app::GenerateAppIdFromManifestId(manifest_id);
    if (!provider_->registrar_unsafe().IsLocallyInstalled(app_id)) {
      continue;
    }

    if (setting.force_unregister_os_integration) {
      app_ids_for_force_unregistration.insert(app_id);
    }
  }

  if (app_ids_for_force_unregistration.empty()) {
    std::move(policy_settings_applied_callback).Run();
    return;
  }

  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  auto callback_for_synchronize_complete =
      base::BarrierClosure(app_ids_for_force_unregistration.size(),
                           std::move(policy_settings_applied_callback));
  for (const auto& app_id : app_ids_for_force_unregistration) {
    provider_->scheduler().SynchronizeOsIntegration(
        app_id, callback_for_synchronize_complete, options);
  }
}

ExternalInstallOptions WebAppPolicyManager::ParseInstallPolicyEntry(
    const base::Value::Dict& entry) {
  const std::string* install_url = entry.FindString(kUrlKey);
  // url is a required field and is validated by
  // SimpleSchemaValidatingPolicyHandler. It is guaranteed to exist.
  const GURL install_gurl(CHECK_DEREF(install_url));
  const std::string* default_launch_container =
      entry.FindString(kDefaultLaunchContainerKey);
  const absl::optional<bool> create_desktop_shortcut =
      entry.FindBool(kCreateDesktopShortcutKey);
  const std::string* fallback_app_name = entry.FindString(kFallbackAppNameKey);
  const base::Value::List* uninstall_and_replace =
      entry.FindList(kUninstallAndReplaceKey);
  const absl::optional<bool> install_as_shortcut =
      entry.FindBool(kInstallAsShortcut);

  DCHECK(!default_launch_container ||
         (*default_launch_container == kDefaultLaunchContainerWindowValue) ||
         (*default_launch_container == kDefaultLaunchContainerTabValue));

  if (!install_gurl.is_valid()) {
    LOG(WARNING) << "Policy-installed web app has invalid URL " << *install_url;
  }

  mojom::UserDisplayMode user_display_mode;
  if (!default_launch_container) {
    user_display_mode = mojom::UserDisplayMode::kBrowser;
  } else if (*default_launch_container == kDefaultLaunchContainerTabValue) {
    user_display_mode = mojom::UserDisplayMode::kBrowser;
  } else {
    user_display_mode = mojom::UserDisplayMode::kStandalone;
  }

  ExternalInstallOptions install_options{
      install_gurl, user_display_mode, ExternalInstallSource::kExternalPolicy};

  // TODO(dmurph): Store expected os integration state in the database so
  // this doesn't re-apply when we already have it done.
  // https://crbug.com/1295044
  install_options.add_to_applications_menu = true;
  install_options.add_to_desktop = create_desktop_shortcut.value_or(false);
  // Pinning apps to the ChromeOS shelf is done through the PinnedLauncherApps
  // policy.
  install_options.add_to_quick_launch_bar = false;

  // Allow administrators to override the name of the placeholder app, as well
  // as the permanent name for Web Apps without a manifest.
  if (fallback_app_name) {
    install_options.fallback_app_name = *fallback_app_name;
  }

  // Used by default Chrome app policy migration to force install web apps and
  // uninstall the old Chrome app equivalents.
  if (uninstall_and_replace) {
    for (const base::Value& item : *uninstall_and_replace) {
      if (item.is_string()) {
        install_options.uninstall_and_replace.push_back(item.GetString());
      }
    }
  }

  install_options.install_as_shortcut = install_as_shortcut.value_or(false);

  const std::string* custom_name = entry.FindString(kCustomNameKey);
  if (custom_name) {
    install_options.override_name = *custom_name;
    if (install_gurl.is_valid())
      custom_manifest_values_by_url_[install_gurl].SetName(*custom_name);
  }

  const base::Value::Dict* custom_icon = entry.FindDict(kCustomIconKey);
  if (custom_icon && custom_icon) {
    const std::string* icon_url = custom_icon->FindString(kCustomIconURLKey);
    if (icon_url) {
      GURL icon_gurl = GURL(*icon_url);
      if (icon_gurl.SchemeIs(url::kHttpsScheme)) {
        install_options.override_icon_url = icon_gurl;
        if (install_gurl.is_valid())
          custom_manifest_values_by_url_[install_gurl].SetIcon(icon_gurl);
      } else {
        LOG(WARNING) << "Policy-installed web app " << *install_url
                     << " has non-https custom icon URL " << *icon_url
                     << ", ignoring custom icon.";
      }
    }
  }

  return install_options;
}

RunOnOsLoginPolicy WebAppPolicyManager::GetUrlRunOnOsLoginPolicy(
    const webapps::AppId& app_id) const {
  return GetUrlRunOnOsLoginPolicyByManifestId(
      provider_->registrar_unsafe().GetComputedManifestId(app_id).spec());
}

RunOnOsLoginPolicy WebAppPolicyManager::GetUrlRunOnOsLoginPolicyByManifestId(
    const std::string& manifest_id) const {
  auto it = settings_by_url_.find(manifest_id);
  if (it != settings_by_url_.end())
    return it->second.run_on_os_login_policy;
  return default_settings_->run_on_os_login_policy;
}

void WebAppPolicyManager::SetOnAppsSynchronizedCompletedCallbackForTesting(
    base::OnceClosure callback) {
  on_apps_synchronized_for_testing_ = std::move(callback);
}

void WebAppPolicyManager::SetRefreshPolicySettingsCompletedCallbackForTesting(
    base::OnceClosure callback) {
  refresh_policy_settings_completed_ = std::move(callback);
}

void WebAppPolicyManager::RefreshPolicySettingsForTesting() {
  RefreshPolicySettings();
}

void WebAppPolicyManager::OverrideManifest(
    const GURL& custom_values_key,
    blink::mojom::ManifestPtr& manifest) const {
  const CustomManifestValues& custom_values =
      custom_manifest_values_by_url_.at(custom_values_key);
  if (custom_values.name) {
    manifest->name = custom_values.name.value();
  }
  if (custom_values.icons) {
    manifest->icons = custom_values.icons.value();
  }
}

void WebAppPolicyManager::MaybeOverrideManifest(
    content::RenderFrameHost* frame_host,
    blink::mojom::ManifestPtr& manifest) const {
  // This doesn't override the manifest properly on a non primary page since it
  // checks the url from PreRedirectionURLObserver that works only on a primary
  // page.
  if (!frame_host->IsInPrimaryMainFrame())
    return;

  if (!manifest)
    return;

  // For policy-installed apps there are two ways for getting to the manifest:
  // via the policy install URL, or via the manifest-specified identity
  // of an already installed app. Websites without a manifest will use the
  // policy-installed URL as start_url, so they are covered by the first case.
  // Second case first:
  if (manifest->id.is_valid()) {
    const webapps::AppId& app_id = GenerateAppIdFromManifestId(manifest->id);
    // List of policy-installed apps and their install URLs:
    base::flat_map<webapps::AppId, base::flat_set<GURL>> policy_installed_apps =
        provider_->registrar_unsafe().GetExternallyInstalledApps(
            ExternalInstallSource::kExternalPolicy);
    if (base::Contains(policy_installed_apps, app_id)) {
      DCHECK_GT(policy_installed_apps[app_id].size(), 0UL);
      for (const GURL& policy_install_url : policy_installed_apps[app_id]) {
        if (base::Contains(custom_manifest_values_by_url_, policy_install_url))
          OverrideManifest(policy_install_url, manifest);
      }
      return;
    }
  }

  // And now the first case: assume we got here from the policy install URL.
  // We might have been redirected in between, so check where we started
  // the current navigation.
  const webapps::PreRedirectionURLObserver* const pre_redirect =
      webapps::PreRedirectionURLObserver::FromWebContents(
          content::WebContents::FromRenderFrameHost(frame_host));
  if (!pre_redirect)
    return;
  GURL install_url = pre_redirect->last_url();
  if (base::Contains(custom_manifest_values_by_url_, install_url))
    OverrideManifest(install_url, manifest);
}

bool WebAppPolicyManager::IsPreventCloseEnabled(
    const webapps::AppId& app_id) const {
#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          features::kDesktopPWAsEnforceWebAppSettingsPolicy) ||
      !base::FeatureList::IsEnabled(features::kDesktopPWAsPreventClose)) {
    return false;
  }

  const webapps::ManifestId manifest_id =
      provider_->registrar_unsafe().GetComputedManifestId(app_id);
  auto it = settings_by_url_.find(manifest_id.spec());
  if (it != settings_by_url_.end()) {
    return it->second.prevent_close;
  }
  return default_settings_->prevent_close;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppPolicyManager::RefreshPolicyInstalledAppsForTesting() {
  RefreshPolicyInstalledApps();
}

void WebAppPolicyManager::OnAppsSynchronized(
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  is_refreshing_ = false;

  if (!install_results.empty())
    ApplyPolicySettings();

  if (needs_refresh_)
    RefreshPolicyInstalledApps();

  for (const auto& url_and_result : install_results) {
    base::UmaHistogramEnumeration(kInstallResultHistogramName,
                                  url_and_result.second.code);
  }

  OnWebAppForceInstallPolicyParsed();
}

WebAppPolicyManager::WebAppSetting::WebAppSetting() {
  ResetSettings();
}

bool WebAppPolicyManager::WebAppSetting::Parse(const base::Value::Dict& dict,
                                               bool for_default_settings) {
  const std::string* run_on_os_login_str = dict.FindString(kRunOnOsLogin);
  if (run_on_os_login_str) {
    if (*run_on_os_login_str == kAllowed) {
      run_on_os_login_policy = RunOnOsLoginPolicy::kAllowed;
    } else if (*run_on_os_login_str == kBlocked) {
      run_on_os_login_policy = RunOnOsLoginPolicy::kBlocked;
    } else if (!for_default_settings && *run_on_os_login_str == kRunWindowed) {
      run_on_os_login_policy = RunOnOsLoginPolicy::kRunWindowed;
    } else {
      SYSLOG(WARNING) << "Malformed web app run on os login preference.";
      return false;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // The value of "prevent_close" shall only be considered if run-on-os-login
  // is enforced.
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsEnforceWebAppSettingsPolicy) &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsPreventClose) &&
      run_on_os_login_policy == RunOnOsLoginPolicy::kRunWindowed) {
    absl::optional<bool> prevent_close_value = dict.FindBool(kPreventClose);
    if (prevent_close_value && *prevent_close_value) {
      prevent_close = true;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (IsForceUnregistrationPolicyEnabled()) {
    absl::optional<bool> force_unregistration_value =
        dict.FindBool(kForceUnregisterOsIntegration);
    force_unregister_os_integration =
        force_unregistration_value.value_or(false);
  }
  return true;
}

void WebAppPolicyManager::WebAppSetting::ResetSettings() {
  run_on_os_login_policy = RunOnOsLoginPolicy::kAllowed;
  prevent_close = false;
  force_unregister_os_integration = false;
}

WebAppPolicyManager::CustomManifestValues::CustomManifestValues() = default;
WebAppPolicyManager::CustomManifestValues::CustomManifestValues(
    const WebAppPolicyManager::CustomManifestValues&) = default;
WebAppPolicyManager::CustomManifestValues::~CustomManifestValues() = default;

void WebAppPolicyManager::CustomManifestValues::SetName(
    const std::string& utf8_name) {
  name = base::UTF8ToUTF16(utf8_name);
}

void WebAppPolicyManager::CustomManifestValues::SetIcon(const GURL& icon_gurl) {
  blink::Manifest::ImageResource icon;

  icon.src = GURL(icon_gurl);
  icon.sizes.emplace_back(0, 0);  // Represents size "any".
  icon.purpose.push_back(blink::mojom::ManifestImageResource::Purpose::ANY);

  // Initialize icons to only contain icon, possibly resetting icons:
  icons.emplace(1, icon);
}

void WebAppPolicyManager::ObserveDisabledSystemFeaturesPolicy() {
#if BUILDFLAG(IS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state) {  // Sometimes it's not available in tests.
    return;
  }
  local_state_pref_change_registrar_.Init(local_state);

  local_state_pref_change_registrar_.Add(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::BindRepeating(&WebAppPolicyManager::OnDisableListPolicyChanged,
                          base::Unretained(this)));
  local_state_pref_change_registrar_.Add(
      policy::policy_prefs::kSystemFeaturesDisableMode,
      base::BindRepeating(&WebAppPolicyManager::OnDisableModePolicyChanged,
                          base::Unretained(this)));
  // Make sure we get the right disabled mode in case it was changed before
  // policy registration.
  OnDisableModePolicyChanged();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppPolicyManager::OnDisableModePolicyChanged() {
#if BUILDFLAG(IS_CHROMEOS)
  provider_->sync_bridge_unsafe().UpdateAppsDisableMode();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppPolicyManager::PopulateDisabledWebAppsIdsLists() {
  disabled_web_apps_.clear();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  disabled_system_apps_.clear();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return;

  const base::Value::List& disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);

  for (const auto& entry : disabled_system_features_pref) {
    switch (static_cast<policy::SystemFeature>(entry.GetInt())) {
      case policy::SystemFeature::kCanvas:
        disabled_web_apps_.insert(kCanvasAppId);
        break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      case policy::SystemFeature::kCamera:
        disabled_system_apps_.insert(ash::SystemWebAppType::CAMERA);
        break;
      case policy::SystemFeature::kOsSettings:
        disabled_system_apps_.insert(ash::SystemWebAppType::SETTINGS);
        break;
      case policy::SystemFeature::kScanning:
        disabled_system_apps_.insert(ash::SystemWebAppType::SCANNING);
        break;
      case policy::SystemFeature::kExplore:
        disabled_system_apps_.insert(ash::SystemWebAppType::HELP);
        break;
      case policy::SystemFeature::kCrosh:
        disabled_system_apps_.insert(ash::SystemWebAppType::CROSH);
        break;
      case policy::SystemFeature::kTerminal:
        disabled_system_apps_.insert(ash::SystemWebAppType::TERMINAL);
        break;
      case policy::SystemFeature::kGallery:
        disabled_system_apps_.insert(ash::SystemWebAppType::MEDIA);
        break;
#else
      case policy::SystemFeature::kCamera:
      case policy::SystemFeature::kOsSettings:
      case policy::SystemFeature::kScanning:
      case policy::SystemFeature::kExplore:
      case policy::SystemFeature::kCrosh:
      case policy::SystemFeature::kTerminal:
      case policy::SystemFeature::kGallery:
        break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      case policy::SystemFeature::kUnknownSystemFeature:
      case policy::SystemFeature::kBrowserSettings:
      case policy::SystemFeature::kWebStore:
      case policy::SystemFeature::kGoogleNewsDeprecated:
        break;
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(system_web_apps_delegate_map_);
  for (const ash::SystemWebAppType& app_type : disabled_system_apps_) {
    absl::optional<webapps::AppId> app_id =
        GetAppIdForSystemApp(provider_->registrar_unsafe(),
                             *system_web_apps_delegate_map_, app_type);
    if (app_id.has_value()) {
      disabled_web_apps_.insert(app_id.value());
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppPolicyManager::OnWebAppForceInstallPolicyParsed() {
  if (on_apps_synchronized_for_testing_) {
    std::move(on_apps_synchronized_for_testing_).Run();
  }

  // Policy settings have already been applied, as that happens synchronously
  // before force-installs are applied.
  if (policy_settings_and_force_installs_applied_) {
    std::move(policy_settings_and_force_installs_applied_).Run();
  }
}

bool WebAppPolicyManager::IsMaybeErrorLoadedPolicyApp(
    const webapps::AppId& app_id,
    const GURL& policy_install_url) {
  if (!base::FeatureList::IsEnabled(features::kMigrateErrorLoadedPolicyApps)) {
    return false;
  }

  // We want to only run the migration once, since there can be non-installable
  // sites without manifests that can be force installed, and this fix would
  // cause them to be reinstalled over and over again.
  if (HasPreviouslyMigratedErrorLoadedPolicyApp(profile_->GetPrefs())) {
    return false;
  }

  const WebApp* existing_policy_app =
      provider_->registrar_unsafe().GetAppById(app_id);
  CHECK(existing_policy_app);
  return existing_policy_app->start_url() == policy_install_url &&
         !provider_->registrar_unsafe().IsPlaceholderApp(
             app_id, WebAppManagement::kPolicy) &&
         existing_policy_app->manifest_url().is_empty();
}

}  // namespace web_app
