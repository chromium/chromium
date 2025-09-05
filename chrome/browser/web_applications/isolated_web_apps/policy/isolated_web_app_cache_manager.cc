// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

namespace {

using SessionType = IwaCacheClient::SessionType;

bool HasManagedGuestSessionInPolicy() {
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
    return !IwaKeyDistributionInfoProvider::GetInstance()
                .IsManagedInstallPermitted(id.id());
  });
  return iwa_ids;
}

std::vector<web_package::SignedWebBundleId> GetPolicyInstalledIwasForKiosk() {
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(ash::CrosSettings::Get());
  std::vector<web_package::SignedWebBundleId> kiosk_iwas;

  for (const policy::DeviceLocalAccount& account : device_local_accounts) {
    if (account.type != policy::DeviceLocalAccountType::kKioskIsolatedWebApp) {
      continue;
    }

    auto kiosk_bundle_id = web_package::SignedWebBundleId::Create(
        account.kiosk_iwa_info.web_bundle_id());

    if (kiosk_bundle_id.has_value()) {
      kiosk_iwas.push_back(kiosk_bundle_id.value());
    } else {
      LOG(ERROR) << "Cannot create SignedWebBundleId for "
                 << account.kiosk_iwa_info.web_bundle_id();
    }
  }
  return kiosk_iwas;
}

std::vector<web_package::SignedWebBundleId>
GetPolicyInstalledIwasForManagedGuestSession(const Profile& profile) {
  std::vector<IsolatedWebAppExternalInstallOptions> iwas_in_policy =
      IsolatedWebAppPolicyManager::GetIwaInstallForceList(profile);
  return base::ToVector(iwas_in_policy,
                        &IsolatedWebAppExternalInstallOptions::web_bundle_id);
}

template <typename T, typename E>
void AddResultToLog(const std::string& key,
                    const base::expected<T, E>& result,
                    base::Value::List& operations_results) {
  std::string value = result.has_value() ? base::ToString(result.value())
                                         : base::ToString(result.error());
  operations_results.Append(base::Value::Dict().Set(key, std::move(value)));
}

}  // namespace

IwaBundleCacheManager::IwaBundleCacheManager(Profile& profile)
    : profile_(profile) {}

IwaBundleCacheManager::~IwaBundleCacheManager() = default;

void IwaBundleCacheManager::Start() {
  CHECK(provider_);

  if (IsIwaBundleCacheFeatureEnabled()) {
    // Remove MGS and Kiosk app cache directories when they are not in the
    // device local account policy anymore. This should be done during any
    // session.
    MaybeRemoveManagedGuestSessionCache();
    RemoveCacheForIwaKioskDeletedFromPolicy();
  }

  if (!content::AreIsolatedWebAppsEnabled(&*profile_) ||
      !IsIwaBundleCacheEnabledInCurrentSession()) {
    return;
  }

  install_manager_observation_.Observe(&provider_->install_manager());
  CleanupManagedGuestSessionOrphanedIwas();
}

void IwaBundleCacheManager::SetProvider(base::PassKey<WebAppProvider>,
                                        WebAppProvider& provider) {
  provider_ = &provider;
}

void IwaBundleCacheManager::OnWebAppInstalled(const webapps::AppId& app_id) {
  ASSIGN_OR_RETURN(const WebApp& iwa,
                   GetIsolatedWebAppById(provider_->registrar_unsafe(), app_id),
                   [](const std::string&) { return; });

  // In ephemeral sessions `IsolatedWebAppUpdateManager` checks for updates
  // before IWAs are installed from cache (without updating IWAs even when the
  // update is available, since only installed IWAs can be updated). Triggering
  // the update check manually here after the IWA installation to avoid waiting
  // for the next scheduled update check.
  TriggerIwaUpdateCheck(iwa);

  // Both update command and remove obsolete versions command take app lock,
  // so it is fine to call them here at the same time.
  RemoveObsoleteIwaVersionsCache(iwa);
}

void IwaBundleCacheManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

base::Value IwaBundleCacheManager::GetDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set(kBundleCacheIsEnabled, IsIwaBundleCacheEnabledInCurrentSession())
          .Set(kOperationsResults, base::Value(operations_results_.Clone())));
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

void IwaBundleCacheManager::RemoveCacheForIwaKioskDeletedFromPolicy() {
  std::vector<web_package::SignedWebBundleId> iwas_to_keep_in_cache =
      FilterAllowlistedIwas(GetPolicyInstalledIwasForKiosk());

  provider_->scheduler().CleanupIsolatedWebAppBundleCache(
      iwas_to_keep_in_cache, SessionType::kKiosk,
      base::BindOnce(
          &IwaBundleCacheManager::OnRemoveCacheForIwaKioskDeletedFromPolicy,
          weak_ptr_factory_.GetWeakPtr()));
}

void IwaBundleCacheManager::OnRemoveCacheForIwaKioskDeletedFromPolicy(
    CleanupBundleCacheResult result) {
  AddResultToLog(kRemoveCacheForIwaKioskDeletedFromPolicy, result,
                 operations_results_);
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
  provider_->iwa_update_manager().MaybeDiscoverUpdatesForApp(iwa.app_id());
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

}  // namespace web_app
