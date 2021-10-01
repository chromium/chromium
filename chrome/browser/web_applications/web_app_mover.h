// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MOVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MOVER_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;
namespace content {
class WebContents;
}

namespace web_app {

class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppInstallFinalizer;

// WebAppMover is designed to facilitate a one-off migration for a webapp, from
// one start_url to another.
class WebAppMover final : public syncer::SyncServiceObserver {
 public:
  enum class UninstallMode { kPrefix, kPattern };
  static std::unique_ptr<WebAppMover> CreateIfNeeded(
      Profile* profile,
      WebAppRegistrar* registrar,
      WebAppInstallFinalizer* install_finalizer,
      WebAppInstallManager* install_manager,
      WebAppSyncBridge* sync_bridge);

  static void DisableForTesting();
  static void SkipWaitForSyncForTesting();
  static void SetCompletedCallbackForTesting(base::OnceClosure callback);

  WebAppMover(Profile* profile,
              WebAppRegistrar* registrar,
              WebAppInstallFinalizer* install_finalizer,
              WebAppInstallManager* install_manager,
              WebAppSyncBridge* sync_bridge,
              UninstallMode mode,
              std::string uninstall_url_prefix_or_pattern,
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
  enum class WebAppMoverResult {
    kInvalidConfiguration = 0,
    kInstallAppExists = 1,
    kNoAppsToUninstall = 2,
    kNotInstallable = 3,
    kUninstallFailure = 4,
    kInstallFailure = 5,
    kSuccess = 6,
    kMaxValue = kSuccess
  };

  void WaitForFirstSyncCycle(base::OnceClosure callback);
  void OnFirstSyncCycleComplete();

  void OnInstallManifestFetched(
      base::ScopedClosureRunner complete_callback_runner,
      std::unique_ptr<content::WebContents> web_contents,
      InstallableCheckResult result,
      absl::optional<AppId> app_id);

  void OnAllUninstalled(
      base::ScopedClosureRunner complete_callback_runner,
      std::unique_ptr<content::WebContents> web_contents_for_install,
      scoped_refptr<base::RefCountedData<bool>> success_accumulator);

  void OnInstallCompleted(
      base::ScopedClosureRunner complete_callback_runner,
      std::unique_ptr<content::WebContents> web_contents_for_install,
      const AppId& id,
      InstallResultCode code);

  void RecordResults(WebAppMoverResult result);

  raw_ptr<Profile> profile_;
  raw_ptr<WebAppRegistrar> registrar_;
  raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  raw_ptr<WebAppInstallManager> install_manager_;
  raw_ptr<WebAppSyncBridge> sync_bridge_;

  UninstallMode uninstall_mode_;
  std::string uninstall_url_prefix_or_pattern_;
  GURL install_url_;

  raw_ptr<syncer::SyncService> sync_service_ = nullptr;
  base::OnceClosure sync_ready_callback_;

  bool results_recorded_ = false;
  bool new_app_open_as_window_ = false;
  std::vector<AppId> apps_to_uninstall_;
  std::unique_ptr<ScopedKeepAlive> migration_keep_alive_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};

  base::WeakPtrFactory<WebAppMover> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_MOVER_H_
