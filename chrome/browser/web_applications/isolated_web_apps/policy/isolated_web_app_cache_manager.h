// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
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

class PrefRegistrySimple;
class Profile;

namespace web_app {
class WebAppProvider;

inline constexpr char kBundleCacheIsEnabled[] = "iwa_bundle_cache_is_enabled";
inline constexpr char kOperationsResults[] = "operations_results";
inline constexpr char kRemoveManagedGuestSessionCache[] =
    "remove_managed_guest_session_cache";
inline constexpr char kEvictUnnecessaryIwasFromKioskCache[] =
    "evict_unnecessary_iwas_from_kiosk_cache";
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

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  base::Value GetDebugValue() const;

  // Stores policy metadata for Kiosk IWAs. If any of these values change for a
  // bundle ID on policy refresh, the associated bundle is evicted from the
  // cache.
  struct KioskIwaInfo {
    std::string update_manifest_url;
    std::string pinned_version;
    bool allow_downgrades = false;
    std::string update_channel;

    bool operator==(const KioskIwaInfo& other) const = default;
  };

 private:
  static base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo>
  GetKioskIwaPolicyInfo();

  void LoadKioskIwaPolicyInfoFromPrefs();
  void SaveKioskIwaPolicyInfoToPrefs(
      const base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo>&
          infos);

  // If Managed Guest Session is not in configured on the device anymore, remove
  // all IWA bundle cache for it.
  void MaybeRemoveManagedGuestSessionCache();
  void OnMaybeRemoveManagedGuestSessionCache(CleanupBundleCacheResult result);

  // Cleans IWA bundle cache for Kiosk IWAs which are no longer in the policy
  // list or whose policy configuration / pinned version changed.
  void EvictUnnecessaryIwasFromKioskCache();
  void OnEvictUnnecessaryIwasFromKioskCache(
      base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo>
          new_kiosk_iwas,
      CleanupBundleCacheResult result);

  void RegisterSettingsObserver();
  void OnDeviceLocalAccountsChanged();
  void OnRuntimeDataReady();

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
  base::ListValue operations_results_;

  // Tracks the Kiosk IWAs that should be cached given the current policy state
  // (from the last policy evaluation), rather than the physical list of IWAs
  // currently installed in the cache on disk. Used to determine which bundles
  // changed or were removed when device local account policies update.
  std::optional<base::flat_map<web_package::SignedWebBundleId, KioskIwaInfo>>
      kiosk_iwas_;

  base::CallbackListSubscription cros_settings_subscription_;
  base::CallbackListSubscription runtime_data_subscription_;

  base::WeakPtrFactory<IwaBundleCacheManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_
