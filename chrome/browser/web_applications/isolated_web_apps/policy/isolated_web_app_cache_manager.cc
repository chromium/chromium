// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_manager.h"

#include <memory>
#include <optional>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/prefs/pref_service.h"

namespace web_app {

namespace {

using SessionType = IwaCacheClient::SessionType;

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

std::vector<web_package::SignedWebBundleId> GetIwasInPolicy(
    SessionType session_type,
    const Profile& profile) {
  return session_type == SessionType::kKiosk
             ? GetPolicyInstalledIwasForKiosk()
             : GetPolicyInstalledIwasForManagedGuestSession(profile);
}

}  // namespace

IwaBundleCacheManager::IwaBundleCacheManager(Profile& profile)
    : profile_(profile) {}

IwaBundleCacheManager::~IwaBundleCacheManager() = default;

void IwaBundleCacheManager::Start() {
  if (!IsIwaBundleCacheEnabled()) {
    // TODO(crbug.com/388728155): add debug info.
    return;
  }
  CHECK(provider_);

  install_manager_observation_.Observe(&provider_->install_manager());
  CleanCacheForIwasDeletedFromPolicy();
  // TODO(crbug.com/388727598): observe IWA installation to trigger updates.
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
}

void IwaBundleCacheManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void IwaBundleCacheManager::CleanCacheForIwasDeletedFromPolicy() {
  SessionType session_type = IwaCacheClient::GetCurrentSessionType();
  std::vector<web_package::SignedWebBundleId> iwas_in_policy =
      GetIwasInPolicy(session_type, *profile_);

  provider_->scheduler().CleanupIsolatedWebAppBundleCache(
      /*iwas_to_keep_in_cache=*/iwas_in_policy, session_type,
      base::BindOnce(
          &IwaBundleCacheManager::OnCleanCacheForIwasDeletedFromPolicy,
          weak_ptr_factory_.GetWeakPtr()));
}

void IwaBundleCacheManager::OnCleanCacheForIwasDeletedFromPolicy(
    base::expected<CleanupBundleCacheSuccess, CleanupBundleCacheError> result) {
  // TODO(crbug.com/388728155): add result to log.
}

void IwaBundleCacheManager::TriggerIwaUpdateCheck(const WebApp& iwa) {
  CHECK(iwa.isolation_data());
  provider_->iwa_update_manager().MaybeDiscoverUpdatesForApp(iwa.app_id());
  // TODO(crbug.com/388728155): add result to log.
}

}  // namespace web_app
