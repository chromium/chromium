// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_PERMISSIONS_POLICY_CACHE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_PERMISSIONS_POLICY_CACHE_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "content/public/browser/console_message.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace web_app {

class WebAppProvider;

// This caches the permission policies from within the IWA manifest. The
// lifetime of this cache is the following:
// - It gets populated for a particular IWA on a navigation to it, unless it's
//   already set for it.
// - It gets reset per-IWA when it's updated/uninstalled.
// - It is volatile, so does not persist browser restarts.
//
// The main points of this are to:
// 1) Not save those permission policies persistently in yet another place
//   (outside of the manifest)...
// 2) ...but also not retrieve them from the bundle on every navigation to an
//   IWA.
class IwaPermissionsPolicyCache : public KeyedService,
                                  public WebAppInstallManagerObserver {
 public:
  struct Entry {
    Entry(std::string feature, std::vector<std::string> allowed_origins);
    Entry(const Entry&);
    Entry(Entry&&);
    Entry& operator=(const Entry&);
    ~Entry();

    std::string feature;
    std::vector<std::string> allowed_origins;
  };

  struct Policy {
    explicit Policy(
        const std::vector<Entry>& unfiltered = {},
        const std::vector<Entry>& filtered = {},
        std::optional<IwaVersion> app_version_for_filtering = std::nullopt,
        std::vector<std::string> deprecation_warnings = {});
    Policy(const Policy&);
    Policy(Policy&&);
    Policy& operator=(const Policy&);
    ~Policy();

    std::vector<Entry> unfiltered;
    std::vector<Entry> filtered;
    std::optional<IwaVersion> app_version_for_filtering;
    std::vector<std::string> deprecation_warnings;
  };

  explicit IwaPermissionsPolicyCache(WebAppProvider& provider);
  ~IwaPermissionsPolicyCache() override;

  using CacheEntry = std::vector<Entry>;

  // Gets the cached policy for the given IWA origin.
  // Returns nullptr if not found or not yet cached.
  const CacheEntry* GetPolicy(const IwaOrigin& iwa_origin) const;

  std::vector<content::ConsoleMessage> GetWarningMessages(
      const IwaOrigin& iwa_origin) const;

  // Retrieves IWA manifest, parses it and stores in cache.
  // Callback is queued to run immediately if the cache is already populated.
  void ObtainManifestAndCache(const IwaOrigin& iwa_origin,
                              base::OnceCallback<void(bool success)> callback);

  void SetPolicyForTesting(const IwaOrigin& iwa_origin, CacheEntry policy);

  void SetPolicyWithViolationsForTesting(const IwaOrigin& iwa_origin,
                                         CacheEntry unfiltered_policy,
                                         CacheEntry filtered_policy);

 private:
  // Returns a list of features that were present in the unfiltered policy
  // but removed due to entitlement violations.
  std::vector<content::ConsoleMessage> GetViolationWarningMessages(
      const IwaOrigin& iwa_origin) const;

  std::vector<content::ConsoleMessage> GetDeprecationWarningMessages(
      const IwaOrigin& iwa_origin) const;

  // Stores the policy for the given IWA origin and runs pending callbacks.
  void SetPolicy(const IwaOrigin& iwa_origin,
                 CacheEntry policy,
                 std::vector<std::string> deprecation_warnings);

  // Parses the manifest and stores the policy for the given IWA origin.
  // Returns true if the manifest was parsed successfully (even if the policy
  // is empty), false otherwise.
  bool ParseManifestAndSetPolicy(const IwaOrigin& iwa_origin,
                                 const std::string& manifest_content);

  // Clears the cached policy for the given IWA origin.
  void ClearPolicy(const IwaOrigin& iwa_origin);

  // Re-calculates filtered policy for a specific origin in the cache.
  void UpdateFilteredPolicy(const IwaOrigin& origin, Policy& policy);

  // Re-calculates filtered policies for all entries in the cache.
  void UpdateFilteredPolicies();

  // KeyedService:
  void Shutdown() override;

  // WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  void OnManifestLoaded(const IwaOrigin& iwa_origin,
                        base::OnceCallback<void(bool success)> callback,
                        std::optional<std::string> manifest_content);

  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheBrowserTest,
                           UninstallClearsCache);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheBrowserTest,
                           UpdateClearsCache);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheBrowserTest,
                           ImmediatelyReturnsIfCached);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheBrowserTest,
                           SendsRequestIfNotCached);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheTest,
                           ParseManifestAndSetPolicy_Complex);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheTest,
                           ParseManifestAndSetPolicy_Valid);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheTest,
                           ParseManifestAndSetPolicy_NoPolicy);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheTest,
                           ParseManifestAndSetPolicy_InvalidJson);
  FRIEND_TEST_ALL_PREFIXES(IwaPermissionsPolicyCacheTest,
                           ParseManifestAndSetPolicy_NotDict);
  FRIEND_TEST_ALL_PREFIXES(
      IwaPermissionsPolicyCacheTest,
      ParseManifestAndSetPolicy_InvalidPolicyFormat_NotList);
  FRIEND_TEST_ALL_PREFIXES(
      IwaPermissionsPolicyCacheTest,
      ParseManifestAndSetPolicy_InvalidPolicyFormat_ItemNotString);
  FRIEND_TEST_ALL_PREFIXES(IsolatedWebAppThrottleTest,
                           WebAppProviderInitialized);

  void ClearCacheForApp(const webapps::AppId& app_id);

  // Map from IWA origin to its cache entry (unfiltered and filtered policies).
  base::flat_map<IwaOrigin, Policy> cache_;

  raw_ptr<WebAppProvider> provider_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::CallbackListSubscription runtime_data_subscription_;

  base::WeakPtrFactory<IwaPermissionsPolicyCache> weak_ptr_factory_{this};
};

class IwaPermissionsPolicyCacheFactory
    : public IsolatedWebAppBrowserContextServiceFactory {
 public:
  static IwaPermissionsPolicyCache* GetForProfile(Profile* profile);
  static IwaPermissionsPolicyCacheFactory* GetInstance();

  IwaPermissionsPolicyCacheFactory(const IwaPermissionsPolicyCacheFactory&) =
      delete;
  IwaPermissionsPolicyCacheFactory& operator=(
      const IwaPermissionsPolicyCacheFactory&) = delete;

 private:
  friend class base::NoDestructor<IwaPermissionsPolicyCacheFactory>;

  IwaPermissionsPolicyCacheFactory();
  ~IwaPermissionsPolicyCacheFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_PERMISSIONS_POLICY_CACHE_H_
