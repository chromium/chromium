// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/common/web_app_id.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#endif

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProvider;

// Documentation: docs/webapps/manifest_update_process.md
//
// Checks for updates to a web app's manifest and triggers a reinstall if the
// current installation is out of date.
//
// Update checks are throttled per app (see MaybeConsumeUpdateCheck()) to avoid
// excessive updating on pathological sites.
//
// Each update check is performed by a |ManifestUpdateCommand|, see that class
// for details about what happens during a check.
//
// TODO(crbug.com/40611449): Replace MaybeUpdate() with a background check
// instead of being triggered by page loads.
class ManifestUpdateManager final : public WebAppInstallManagerObserver {
 public:
  class ScopedBypassWindowCloseWaitingForTesting {
   public:
    ScopedBypassWindowCloseWaitingForTesting();
    ScopedBypassWindowCloseWaitingForTesting(
        const ScopedBypassWindowCloseWaitingForTesting&) = delete;
    ScopedBypassWindowCloseWaitingForTesting& operator=(
        const ScopedBypassWindowCloseWaitingForTesting&) = delete;
    ~ScopedBypassWindowCloseWaitingForTesting();
  };

  using UpdatePendingCallback = base::OnceCallback<void(const GURL& url)>;
  // Sets a |callback| for testing code to get notified when a manifest update
  // is needed and there is a PWA window preventing the update from proceeding.
  // Only called once, iff the update process determines that waiting is needed.
  static void SetUpdatePendingCallbackForTesting(
      UpdatePendingCallback callback);

  using ResultCallback =
      base::OnceCallback<void(const GURL& url, ManifestUpdateResult result)>;
  static void SetResultCallbackForTesting(ResultCallback callback);

  ManifestUpdateManager();
  ~ManifestUpdateManager() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetSystemWebAppDelegateMap(
      const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map);
#endif

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  void MaybeUpdate(const GURL& url,
                   const std::optional<webapps::AppId>& app_id,
                   content::WebContents* web_contents);
  bool IsUpdateConsumed(const webapps::AppId& app_id, base::Time check_time);
  bool IsUpdateCommandPending(const webapps::AppId& app_id);

  // WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  void set_time_override_for_testing(base::Time time_override) {
    time_override_for_testing_ = time_override;
  }

  void hang_update_checks_for_testing() {
    hang_update_checks_for_testing_ = true;
  }

  void ResetManifestThrottleForTesting(const webapps::AppId& app_id);
  // Return whether there are pending updates waiting for the page load to
  // finish.
  bool HasUpdatesPendingLoadFinishForTesting();
  void SetLoadFinishedCallbackForTesting(
      base::OnceClosure load_finished_callback);

  bool IsAppPendingPageAndManifestUrlLoadForTesting(
      const webapps::AppId& app_id);

 private:
  // This class is used to either observe the url loading or web_contents
  // destruction before manifest update tasks can be scheduled. Once any
  // of those events have been fired, observing is stopped.
  class PreUpdateWebContentsObserver;

  // Store information regarding the entire manifest update in different stages.
  // The following steps are followed for the update:
  // 1. The UpdateStage is initialized by passing an observer, who waits till
  // page loading has finished. During the lifetime of the observer,
  // the update_task stays uninitialized.
  // 2. The update_task is initialized as soon as the observer fires a
  // DidFinishLoad and the observer is destructed. This ensures that at any
  // point, either the observer or the update_task exists, but not both. This
  // helps reason about the entire process at different stages of its
  // functionality. This class is owned by the ManifestUpdateManager, and is
  // guaranteed to hold an observer OR an update_task always, but never both.
  struct UpdateStage {
    UpdateStage(const GURL& url,
                std::unique_ptr<PreUpdateWebContentsObserver> observer);
    ~UpdateStage();

    GURL url;
    enum Stage {
      kWaitingForPageLoadAndManifestUrl = 0,
      kCheckingManifestDiff = 1,
    } stage = kWaitingForPageLoadAndManifestUrl;
    std::unique_ptr<PreUpdateWebContentsObserver> observer;
  };

  void StartCheckAfterPageAndManifestUrlLoad(
      const webapps::AppId& app_id,
      base::Time check_time,
      base::WeakPtr<content::WebContents> web_contents);

  void OnManifestCheckAwaitAppWindowClose(
      base::WeakPtr<content::WebContents> contents,
      const GURL& url,
      const webapps::AppId& app_id,
      ManifestUpdateCheckResult check_result,
      std::unique_ptr<WebAppInstallInfo> install_info);

  bool MaybeConsumeUpdateCheck(const GURL& origin,
                               const webapps::AppId& app_id,
                               base::Time check_time);
  std::optional<base::Time> GetLastUpdateCheckTime(
      const webapps::AppId& app_id) const;
  void SetLastUpdateCheckTime(const GURL& origin,
                              const webapps::AppId& app_id,
                              base::Time time);
  void OnUpdateStopped(const GURL& url,
                       const webapps::AppId& app_id,
                       ManifestUpdateResult result);
  void NotifyResult(const GURL& url,
                    const std::optional<webapps::AppId>& app_id,
                    ManifestUpdateResult result);

  static bool& BypassWindowCloseWaitingForTesting();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<const ash::SystemWebAppDelegateMap, DanglingUntriaged>
      system_web_apps_delegate_map_ = nullptr;
#endif
  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  std::map<webapps::AppId, UpdateStage> update_stages_;
  base::flat_map<webapps::AppId, base::Time> last_update_check_;

  std::optional<base::Time> time_override_for_testing_;

  bool started_ = false;
  bool hang_update_checks_for_testing_ = false;

  base::OnceClosure load_finished_callback_;

  base::WeakPtrFactory<ManifestUpdateManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_MANAGER_H_
