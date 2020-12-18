// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MOVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MOVER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "url/gurl.h"

class Profile;
namespace content {
class WebContents;
}

namespace web_app {

class AppRegistrar;
class AppRegistryController;
class InstallFinalizer;

// WebAppMover is designed to facilitate a one-off migration for a webapp, from
// one start_url to another.
// TODO(dmurph): Finish implementing.
class WebAppMover final : public syncer::SyncServiceObserver {
 public:
  static std::unique_ptr<WebAppMover> CreateIfNeeded(
      Profile* profile,
      AppRegistrar* registrar,
      InstallFinalizer* install_finalizer,
      InstallManager* install_manager,
      AppRegistryController* controller);

  static void DisableForTesting();
  static void SkipWaitForSyncForTesting();
  static void SetCompletedCallbackForTesting(base::OnceClosure callback);

  WebAppMover(Profile* profile,
              AppRegistrar* registrar,
              InstallFinalizer* install_finalizer,
              InstallManager* install_manager,
              AppRegistryController* controller,
              const std::string& uninstall_url_prefix,
              const GURL& install_url);
  WebAppMover(const WebAppMover&) = delete;
  WebAppMover& operator=(const WebAppMover&) = delete;
  ~WebAppMover() final;

  void Start();
  void Shutdown();

  // syncer::SyncServiceObserver:
  void OnSyncCycleCompleted(syncer::SyncService* sync_service) final;
  void OnSyncShutdown(syncer::SyncService* sync_service) final;

 private:
  void WaitForFirstSyncCycle(base::OnceClosure callback);
  void OnFirstSyncCycleComplete();

  void OnInstallManifestFetched(
      base::ScopedClosureRunner complete_callback_runner,
      std::unique_ptr<content::WebContents> web_contents,
      InstallManager::InstallableCheckResult result,
      base::Optional<AppId> app_id);

  void OnAllUninstalled(
      base::ScopedClosureRunner complete_callback_runner,
      std::unique_ptr<content::WebContents> web_contents_for_install,
      scoped_refptr<base::RefCountedData<bool>> success_accumulator);

  void OnInstallCompleted(
      base::ScopedClosureRunner complete_callback_runner,
      std::unique_ptr<content::WebContents> web_contents_for_install,
      const AppId& id,
      InstallResultCode code);

  Profile* profile_;
  AppRegistrar* registrar_;
  InstallFinalizer* install_finalizer_;
  InstallManager* install_manager_;
  AppRegistryController* controller_;

  std::string uninstall_url_prefix_;
  GURL install_url_;

  syncer::SyncService* sync_service_ = nullptr;
  base::OnceClosure sync_ready_callback_;

  bool new_app_open_as_window_ = false;
  std::vector<AppId> apps_to_uninstall_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};

  base::WeakPtrFactory<WebAppMover> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MOVER_H_