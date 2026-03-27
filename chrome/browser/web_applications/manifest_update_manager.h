// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate_map.h"
#endif

namespace content {
class WebContents;
}

namespace web_app {

struct FetchManifestAndUpdateCompletionInfo;
struct ManifestSilentUpdateCompletionInfo;
class WebAppProvider;
class WebAppTabHelper;

// Documentation: docs/webapps/manifest_update_process.md
//
// Checks for updates to a web app's manifest and triggers a reinstall if the
// current installation is out of date.
class ManifestUpdateManager final : public WebAppInstallManagerObserver {
 public:
  ManifestUpdateManager();
  ~ManifestUpdateManager() override;

#if BUILDFLAG(IS_CHROMEOS)
  void SetSystemWebAppDelegateMap(
      const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map);
#endif

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  // Called by WebAppTabHelper when a developer-specified manifest is seen on
  // the primary page.
  void OnManifestSeenOnPrimaryPage(content::WebContents& web_contents,
                                   const blink::mojom::ManifestPtr& manifest,
                                   base::PassKey<WebAppTabHelper>);
  void TriggerManifestUpdateProcess(content::WebContents& web_contents,
                                    const webapps::AppId& app_id);

  // WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  void OnManifestSilentUpdateComplete(
      base::WeakPtr<content::WebContents> contents,
      const webapps::AppId& app_id,
      ManifestSilentUpdateCompletionInfo completion_info);
  void OnMigrationFetchManifestAndUpdateComplete(
      const webapps::AppId& app_id,
      FetchManifestAndUpdateCompletionInfo completion_info);

#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<const ash::SystemWebAppDelegateMap, DanglingUntriaged>
      system_web_apps_delegate_map_ = nullptr;
#endif
  raw_ptr<WebAppProvider> provider_ = nullptr;
  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};
  // Stores the last time a manifest update was silently made for an app based
  // on the small icon difference as per the new predictable app update
  // algorithm. Used to throttle silent icon updates to once every 24 hours.
  // Please see https://bit.ly/predictable-webapp-updating-prd for more
  // information.
  absl::flat_hash_map<webapps::AppId, base::Time>
      update_check_for_silent_updates_;
  bool started_ = false;
  base::WeakPtrFactory<ManifestUpdateManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
