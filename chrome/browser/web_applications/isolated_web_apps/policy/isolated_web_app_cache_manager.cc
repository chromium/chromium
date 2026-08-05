// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/types/isolated_web_app_external_install_options.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using SessionType = IwaCacheClient::SessionType;

bool HasManagedGuestSessionInPolicy() {
  // If CrosSettings does not have the device local accounts pref initialized yet
  // (e.g. during early startup or before policy is loaded), assume a Managed Guest
  // Session policy exists to avoid prematurely deleting cached bundles.
  const base::ListValue* list = nullptr;
  if (!ash::CrosSettings::Get()->GetList(ash::kAccountsPrefDeviceLocalAccounts,
                                         &list)) {
    return true;
  }
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(ash::CrosSettings::Get());
  auto managed_gest_session = std::find_if(
      device_local_accounts.begin(), device_local_accounts.end(),
      [](auto& account) {
        return account.type == policy::DeviceLocalAccountType::kPublicSession;
      });

  return managed_gest_session != device_local_accounts.end();
}

// Return only IWAs from `iwa_ids` which are in the IWA allowlist.
std::vector<web_package::SignedWebBundleId> FilterAllowlistedIwas(
    std::vector<web_package::SignedWebBundleId> iwa_ids) {
  std::erase_if(iwa_ids, [](const auto& id) {
    return !IwaRuntimeDataProvider::GetInstance().IsManagedInstallPermitted(
        id.id());
  });
  return iwa_ids;
}

std::vector<web_package::SignedWebBundleId>
GetPolicyInstalledIwasForManagedGuestSession(const Profile& profile) {
  std::vector<IsolatedWebAppExternalInstallOptions> iwas_in_policy =
      ParseIwaInstallForceList(
          profile.GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList));
  return base::ToVector(iwas_in_policy,
                        &IsolatedWebAppExternalInstallOptions::web_bundle_id);
}

template <typename T, typename E>
void AddResultToLog(const std::string& key,
                    const base::expected<T, E>& result,
                    base::ListValue& operations_results) {
  std::string value = result.has_value() ? base::ToString(result.value())
                                         : base::ToString(result.error());
  operations_results.Append(base::DictValue().Set(key, std::move(value)));
}

using KioskIwaInfo = IwaBundleCacheManager::KioskIwaInfo;

base::DictValue KioskIwaInfoToDict(const KioskIwaInfo& info) {
  return base::DictValue()
      .Set("update_manifest_url", info.update_manifest_url)
      .Set("pinned_version", info.pinned_version)
      .Set("allow_downgrades", info.allow_downgrades)
      .Set("update_channel", info.update_channel);
}

std::optional<KioskIwaInfo> DictToKioskIwaInfo(const base::DictValue& dict) {
  const std::string* update_manifest_url =
      dict.FindString("update_manifest_url");
  const std::string* pinned_version = dict.FindString("pinned_version");
  std::optional<bool> allow_downgrades = dict.FindBool("allow_downgrades");
  const std::string* update_channel = dict.FindString("update_channel");

  if (!update_manifest_url || !pinned_version || !allow_downgrades ||
      !update_channel) {
    return std::nullopt;
  }

  return KioskIwaInfo{
      .update_manifest_url = *update_manifest_url,
      .pinned_version = *pinned_version,
      .allow_downgrades = *allow_downgrades,
      .update_channel = *update_channel,
  };
}

}  // namespace

// static
void IwaBundleCacheManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kKioskIwaCachePolicyState);
}

IwaBundleCacheManager::IwaBundleCacheManager(Profile& profile)
    : profile_(profile) {}

IwaBundleCacheManager::~IwaBundleCacheManager() = default;

void IwaBundleCacheManager::Start() {
  CHECK(provider_);

  if (IsIwaBundleCacheFeatureEnabled()) {
    LoadKioskIwaPolicyInfoFromPrefs();

    // Remove MGS and Kiosk app cache directories when they are not in the
    // device local account policy anymore or when policy changed. This should
    // be done during any session.
    if (GetKioskIwaPolicyInfo().empty() &&
        GetPolicyInstalledIwasForManagedGuestSession(*profile_).empty()) {
      MaybeRemoveManagedGuestSessionCache();
      EvictUnnecessaryIwasFromKioskCache();
      RegisterSettingsObserver();
    } else {
      IwaRuntimeDataProvider::GetInstance().OnBestEffortRuntimeDataReady().Post(
          FROM_HERE, base::BindOnce(&IwaBundleCacheManager::OnRuntimeDataReady,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
  }

  if (!content::AreIsolatedWebAppsEnabled(&*profile_) ||
      !IsIwaBundleCacheEnabledInCurrentSession()) {
    return;
  }

  install_manager_observation_.Observe(&provider_->install_manager());
  CleanupManagedGuestSessionOrphanedIwas();
}

void IwaBundleCacheManager::OnRuntimeDataReady() {
  RegisterSettingsObserver();

  // Remove MGS and Kiosk app cache directories when they are not in the
  // device local account policy anymore or when policy changed.
  MaybeRemoveManagedGuestSessionCache();
  EvictUnnecessaryIwasFromKioskCache();
}

void IwaBundleCacheManager::SetProvider(base::PassKey<WebAppProvider>,
                                        WebAppProvider& provider) {
  provider_ = &provider;
}

void IwaBundleCacheManager::OnWebAppInstalled(const webapps::AppId& app_id) {
  const WebApp* iwa = provider_->registrar_unsafe().GetAppById(
      app_id, WebAppFilter::IsIsolatedApp());
  if (!iwa) {
    return;
  }

  // In ephemeral sessions `IsolatedWebAppUpdateManager` checks for updates
  // before IWAs are installed from cache (without updating IWAs even when the
  // update is available, since only installed IWAs can be updated). Triggering
  // the update check manually here after the IWA installation to avoid waiting
  // for the next scheduled update check.
  TriggerIwaUpdateCheck(*iwa);

  // Both update command and remove obsolete versions command take app lock,
  // so it is fine to call them here at the same time.
  RemoveObsoleteIwaVersionsCache(*iwa);
}

void IwaBundleCacheManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

base::Value IwaBundleCacheManager::GetDebugValue() const {
  return base::Value(
      base::DictValue()
          .Set(kBundleCacheIsEnabled, IsIwaBundleCacheEnabledInCurrentSession())
          .Set(kOperationsResults, base::Value(operations_results_.Clone())));
}

void IwaBundleCacheManager::RegisterSettingsObserver() {
  if (cros_settings_subscription_) {
    return;
  }
  if (ash::CrosSettings::IsInitialized()) {
    cros_settings_subscription_ = ash::CrosSettings::Get()->AddSettingsObserver(
        ash::kAccountsPrefDeviceLocalAccounts,
        base::BindRepeating(
            &IwaBundleCacheManager::OnDeviceLocalAccountsChanged,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    CHECK_IS_TEST();
  }
}

void IwaBundleCacheManager::OnDeviceLocalAccountsChanged() {
  MaybeRemoveManagedGuestSessionCache();
  EvictUnnecessaryIwasFromKioskCache();
}

void IwaBundleCacheManager::MaybeRemoveManagedGuestSessionCache() {
  if (HasManagedGuestSessionInPolicy()) {
    // Managed Guest Session is still in the policy, do not clean it's cache.
    return;
  }
  // Delete all IWA cached bundles for Managed Guest Session (MGS).
  provider_->scheduler().CleanupIsolatedWebAppBundleCache(
      /*iwas_to_keep_in_cache=*/{}, SessionType::kManagedGuestSession,
      base::BindOnce(
          &IwaBundleCacheManager::OnMaybeRemoveManagedGuestSessionCache,
          weak_ptr_factory_.GetWeakPtr()));
}

void IwaBundleCacheManager::OnMaybeRemoveManagedGuestSessionCache(
    CleanupBundleCacheResult result) {
  AddResultToLog(kRemoveManagedGuestSessionCache, result, operations_results_);
}

void IwaBundleCacheManager::EvictUnnecessaryIwasFromKioskCache() {
  base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo> new_kiosk_iwas =
      GetKioskIwaPolicyInfo();

  // Only filter allowlisted IWAs if there are Kiosk IWAs in policy and runtime
  // data is ready, to avoid triggering an on-demand update check on startup when
  // policy is empty.
  if (!new_kiosk_iwas.empty() &&
      IwaRuntimeDataProvider::GetInstance()
          .OnBestEffortRuntimeDataReady()
          .is_signaled()) {
    base::EraseIf(new_kiosk_iwas, [](const auto& item) {
      return !IwaRuntimeDataProvider::GetInstance().IsManagedInstallPermitted(
          item.first.id());
    });
  }

  if (kiosk_iwas_.has_value() && *kiosk_iwas_ == new_kiosk_iwas) {
    return;
  }

  std::vector<web_package::SignedWebBundleId> iwas_to_keep;

  for (const auto& [id, info] : new_kiosk_iwas) {
    if (const KioskIwaInfo* cached_info =
            kiosk_iwas_ ? base::FindOrNull(*kiosk_iwas_, id) : nullptr;
        !cached_info || *cached_info == info) {
      iwas_to_keep.push_back(id);
    }
  }

  provider_->scheduler().CleanupIsolatedWebAppBundleCache(
      FilterAllowlistedIwas(std::move(iwas_to_keep)), SessionType::kKiosk,
      base::BindOnce(
          &IwaBundleCacheManager::OnEvictUnnecessaryIwasFromKioskCache,
          weak_ptr_factory_.GetWeakPtr(), new_kiosk_iwas));
}

void IwaBundleCacheManager::OnEvictUnnecessaryIwasFromKioskCache(
    base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo> new_kiosk_iwas,
    CleanupBundleCacheResult result) {
  AddResultToLog(kEvictUnnecessaryIwasFromKioskCache, result,
                 operations_results_);
  if (result) {
    SaveKioskIwaPolicyInfoToPrefs(new_kiosk_iwas);
    kiosk_iwas_ = std::move(new_kiosk_iwas);
  }
}

void IwaBundleCacheManager::CleanupManagedGuestSessionOrphanedIwas() {
  if (IwaCacheClient::GetCurrentSessionType() !=
      SessionType::kManagedGuestSession) {
    return;
  }
  std::vector<web_package::SignedWebBundleId> iwas_to_keep_in_cache =
      FilterAllowlistedIwas(
          GetPolicyInstalledIwasForManagedGuestSession(*profile_));

  provider_->scheduler().CleanupIsolatedWebAppBundleCache(
      iwas_to_keep_in_cache, SessionType::kManagedGuestSession,
      base::BindOnce(
          &IwaBundleCacheManager::OnCleanupManagedGuestSessionOrphanedIwas,
          weak_ptr_factory_.GetWeakPtr()));
}

void IwaBundleCacheManager::OnCleanupManagedGuestSessionOrphanedIwas(
    CleanupBundleCacheResult result) {
  AddResultToLog(kCleanupManagedGuestSessionOrphanedIwas, result,
                 operations_results_);
}

void IwaBundleCacheManager::TriggerIwaUpdateCheck(const WebApp& iwa) {
  CHECK(iwa.isolation_data());
  provider_->isolated_web_app_update_manager().MaybeDiscoverAndPrepareUpdate(
      iwa.app_id());
}

void IwaBundleCacheManager::RemoveObsoleteIwaVersionsCache(const WebApp& iwa) {
  auto url_info = *IsolatedWebAppUrlInfo::Create(iwa.start_url());

  provider_->scheduler().RemoveObsoleteIsolatedWebAppVersionsCache(
      url_info, IwaCacheClient::GetCurrentSessionType(),
      base::BindOnce(&IwaBundleCacheManager::OnRemoveObsoleteIwaVersionsCache,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IwaBundleCacheManager::OnRemoveObsoleteIwaVersionsCache(
    RemoveObsoleteBundleVersionsResult result) {
  AddResultToLog(kRemoveObsoleteIwaVersionCache, result, operations_results_);
}

// static
base::flat_map<web_package::SignedWebBundleId,
               IwaBundleCacheManager::KioskIwaInfo>
IwaBundleCacheManager::GetKioskIwaPolicyInfo() {
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(ash::CrosSettings::Get());
  base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo> infos;

  for (const policy::DeviceLocalAccount& account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccountType::kKioskIsolatedWebApp) {
      continue;
    }

    auto kiosk_bundle_id = web_package::SignedWebBundleId::Create(
        account.kiosk_iwa_info.web_bundle_id());
    if (!kiosk_bundle_id.has_value()) {
      LOG(ERROR) << "Cannot create SignedWebBundleId for "
                 << account.kiosk_iwa_info.web_bundle_id();
      continue;
    }

    infos[*kiosk_bundle_id] = KioskIwaInfo{
        .update_manifest_url = account.kiosk_iwa_info.update_manifest_url(),
        .pinned_version = account.kiosk_iwa_info.pinned_version(),
        .allow_downgrades = account.kiosk_iwa_info.allow_downgrades(),
        .update_channel = account.kiosk_iwa_info.update_channel(),
    };
  }
  return infos;
}

void IwaBundleCacheManager::LoadKioskIwaPolicyInfoFromPrefs() {
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!local_state ||
      !local_state->HasPrefPath(prefs::kKioskIwaCachePolicyState)) {
    return;
  }

  base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo> infos;
  const base::DictValue& dict =
      local_state->GetDict(prefs::kKioskIwaCachePolicyState);
  for (auto [id_str, info_val] : dict) {
    auto bundle_id = web_package::SignedWebBundleId::Create(id_str);
    if (!bundle_id.has_value() || !info_val.is_dict()) {
      continue;
    }
    if (auto info = DictToKioskIwaInfo(info_val.GetDict())) {
      infos[*bundle_id] = std::move(*info);
    }
  }
  kiosk_iwas_ = std::move(infos);
}

void IwaBundleCacheManager::SaveKioskIwaPolicyInfoToPrefs(
    const base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo>& infos) {
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!local_state) {
    return;
  }

  base::DictValue dict;
  for (const auto& [id, info] : infos) {
    dict.Set(id.id(), KioskIwaInfoToDict(info));
  }
  local_state->SetDict(prefs::kKioskIwaCachePolicyState, std::move(dict));
}

}  // namespace web_app
