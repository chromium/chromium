// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_

#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"

class Profile;

namespace web_app {
class WebAppProvider;

inline constexpr char kBundleCacheIsEnabled[] = "iwa_bundle_cache_is_enabled";
inline constexpr char kOperationsResults[] = "operations_results";
inline constexpr char kRemoveManagedGuestSessionCache[] =
    "remove_managed_guest_session_cache";
inline constexpr char kRemoveCacheForIwaKioskDeletedFromPolicy[] =
    "remove_cache_for_iwa_kiosk_deleted_from_policy";
inline constexpr char kCleanupManagedGuestSessionOrphanedIwas[] =
    "cleanup_managed_guest_session_orphaned_iwas";
inline constexpr char kRemoveObsoleteIwaVersionCache[] =
    "remove_obsolete_iwa_version_cache";

// Controls whether IWA bundle cache directories should be cleaned or not. If
// `IsIwaBundleCacheEnabled()` returns false, this class will not clean up
// anything.
class IwaBundleCacheManager : public WebAppInstallManagerObserver {
 public:
  explicit IwaBundleCacheManager(Profile& profile);

  IwaBundleCacheManager(const IwaBundleCacheManager&) = delete;
  IwaBundleCacheManager& operator=(const IwaBundleCacheManager&) = delete;
  ~IwaBundleCacheManager() override;

  void Start();
  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  // `WebAppInstallManagerObserver`:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  base::Value GetDebugValue() const;

 private:
  // If Managed Guest Session is not in configured on the device anymore, remove
  // all IWA bundle cache for it.
  void MaybeRemoveManagedGuestSessionCache();
  void OnMaybeRemoveManagedGuestSessionCache(CleanupBundleCacheResult result);

  // If some IWA kiosks are not in the policy list anymore, remove their bundles
  // from cache.
  void RemoveCacheForIwaKioskDeletedFromPolicy();
  void OnRemoveCacheForIwaKioskDeletedFromPolicy(
      CleanupBundleCacheResult result);

  // Cleans IWA bundle cache for the IWAs which are not in the policy list for
  // current Managed Guest Session. Does nothing when called outside of the
  // Managed Guest Session.
  void CleanupManagedGuestSessionOrphanedIwas();
  void OnCleanupManagedGuestSessionOrphanedIwas(
      CleanupBundleCacheResult result);

  void TriggerIwaUpdateCheck(const WebApp& iwa);

  // Keep only currently installed version in cache and cleanup all other
  // bundles for `iwa`.
  void RemoveObsoleteIwaVersionsCache(const WebApp& iwa);
  void OnRemoveObsoleteIwaVersionsCache(
      RemoveObsoleteBundleVersionsResult result);

  const raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  // Log all the operations results using `operations_results_` for the debug
  // purpose.
  base::Value::List operations_results_;

  base::WeakPtrFactory<IwaBundleCacheManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_
