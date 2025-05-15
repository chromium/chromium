// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_

#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"

class Profile;

namespace web_app {
class WebAppProvider;

// Controls whether IWA bundle cache directories should be cleaned or not. If
// `IsIwaBundleCacheEnabled()` returns false, this class will not clean up
// anything.
// TODO(crbug.com/388727598): observe IWA installation to trigger updates
// manually.
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

 private:
  // Cleans IWA bundle cache for the current session. Which means if Managed
  // Guest Session is launched, cache will be cleaned only for Managed Guest
  // Session and not for kiosk. And vice versa.
  void CleanCacheForIwasDeletedFromPolicy();
  void OnCleanCacheForIwasDeletedFromPolicy(
      base::expected<CleanupBundleCacheSuccess, CleanupBundleCacheError>
          result);

  void TriggerIwaUpdateCheck(const WebApp& iwa);

  const raw_ref<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::WeakPtrFactory<IwaBundleCacheManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_CACHE_MANAGER_H_
