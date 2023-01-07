// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_system_web_app_delegate_map_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace web_app {

class ManifestUpdateManager::PreUpdateWebContentsObserver
    : public content::WebContentsObserver {
 public:
  PreUpdateWebContentsObserver(base::OnceClosure load_complete_callback,
                               content::WebContents* contents,
                               bool hang_task_callback_for_testing)
      : content::WebContentsObserver(contents),
        load_complete_callback_(std::move(load_complete_callback)),
        hang_task_callback_for_testing_(hang_task_callback_for_testing) {}

 private:
  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host || hang_task_callback_for_testing_)
      return;

    if (render_frame_host->GetParentOrOuterDocument() ||
        !render_frame_host->IsInPrimaryMainFrame())
      return;

    Observe(nullptr);
    if (load_complete_callback_)
      std::move(load_complete_callback_).Run();
  }

  void WebContentsDestroyed() override {
    Observe(nullptr);
    if (load_complete_callback_)
      std::move(load_complete_callback_).Run();
  }

  base::OnceClosure load_complete_callback_;
  bool hang_task_callback_for_testing_;
};

constexpr base::TimeDelta kDelayBetweenChecks = base::Days(1);
constexpr const char kDisableManifestUpdateThrottle[] =
    "disable-manifest-update-throttle";

ManifestUpdateManager::ManifestUpdateManager() = default;

ManifestUpdateManager::~ManifestUpdateManager() = default;

void ManifestUpdateManager::SetSubsystems(
    WebAppInstallManager* install_manager,
    WebAppRegistrar* registrar,
    WebAppIconManager* icon_manager,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* install_finalizer,
    OsIntegrationManager* os_integration_manager,
    WebAppSyncBridge* sync_bridge) {
  install_manager_ = install_manager;
  registrar_ = registrar;
  icon_manager_ = icon_manager;
  ui_manager_ = ui_manager;
  install_finalizer_ = install_finalizer;
  os_integration_manager_ = os_integration_manager;
  sync_bridge_ = sync_bridge;
}

void ManifestUpdateManager::SetSystemWebAppDelegateMap(
    const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map) {
  system_web_apps_delegate_map_ = system_web_apps_delegate_map;
}

void ManifestUpdateManager::Start() {
  install_manager_observation_.Observe(install_manager_.get());

  DCHECK(!started_);
  started_ = true;
}

void ManifestUpdateManager::Shutdown() {
  install_manager_observation_.Reset();

  update_stages_.clear();
  started_ = false;
}

void ManifestUpdateManager::MaybeUpdate(const GURL& url,
                                        const absl::optional<AppId>& app_id,
                                        content::WebContents* web_contents) {
  if (!started_) {
    return;
  }

  if (!app_id.has_value() || !registrar_->IsLocallyInstalled(*app_id)) {
    NotifyResult(url, app_id, ManifestUpdateResult::kNoAppInScope);
    return;
  }

  if (system_web_apps_delegate_map_ &&
      IsSystemWebApp(*registrar_, *system_web_apps_delegate_map_, *app_id)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kAppIsSystemWebApp);
    return;
  }

  if (registrar_->IsPlaceholderApp(*app_id, WebAppManagement::kPolicy) ||
      registrar_->IsPlaceholderApp(*app_id, WebAppManagement::kKiosk)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kAppIsPlaceholder);
    return;
  }

  if (base::Contains(update_stages_, *app_id)) {
    return;
  }

  if (!MaybeConsumeUpdateCheck(url.DeprecatedGetOriginAsURL(), *app_id)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kThrottled);
    return;
  }

  auto load_observer = std::make_unique<PreUpdateWebContentsObserver>(
      base::BindOnce(&ManifestUpdateManager::StartUpdateTaskAfterPageLoad,
                     base::Unretained(this), *app_id,
                     web_contents->GetWeakPtr()),
      web_contents, hang_update_checks_for_testing_);

  update_stages_.emplace(std::piecewise_construct,
                         std::forward_as_tuple(*app_id),
                         std::forward_as_tuple(url, std::move(load_observer)));
}

ManifestUpdateManager::UpdateStage::UpdateStage(
    const GURL& url,
    std::unique_ptr<PreUpdateWebContentsObserver> observer)
    : url(url), observer(std::move(observer)) {}

ManifestUpdateManager::UpdateStage::~UpdateStage() = default;

void ManifestUpdateManager::StartUpdateTaskAfterPageLoad(
    const AppId& app_id,
    base::WeakPtr<content::WebContents> web_contents) {
  auto update_stage_it = update_stages_.find(app_id);
  DCHECK(update_stage_it != update_stages_.end());
  UpdateStage& update_stage = update_stage_it->second;
  GURL url(update_stage.url);
  DCHECK(update_stage.update_task == nullptr &&
         update_stage.observer != nullptr);

  // If web_contents have been destroyed before page load,
  // then no need of running the task.
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    update_stages_.erase(app_id);
    NotifyResult(url, app_id, ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }

  auto manifest_update_task = std::make_unique<ManifestUpdateTask>(
      url, app_id, web_contents->GetWeakPtr(),
      base::BindOnce(&ManifestUpdateManager::OnUpdateStopped,
                     base::Unretained(this)),
      *registrar_, *icon_manager_, ui_manager_, install_finalizer_,
      *os_integration_manager_, sync_bridge_);

  // Swap out the observer for the update task.
  update_stage.observer.reset();
  update_stage.update_task = std::move(manifest_update_task);
}

bool ManifestUpdateManager::IsUpdateConsumed(const AppId& app_id) {
  absl::optional<base::Time> last_check_time = GetLastUpdateCheckTime(app_id);
  base::Time now = time_override_for_testing_.value_or(base::Time::Now());
  if (last_check_time.has_value() &&
      now < *last_check_time + kDelayBetweenChecks &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableManifestUpdateThrottle)) {
    return true;
  }
  return false;
}

bool ManifestUpdateManager::IsUpdateTaskPending(const AppId& app_id) {
  return base::Contains(update_stages_, app_id);
}

// WebAppInstallManager:
void ManifestUpdateManager::OnWebAppWillBeUninstalled(const AppId& app_id) {
  DCHECK(started_);
  auto it = update_stages_.find(app_id);
  if (it != update_stages_.end()) {
    NotifyResult(it->second.url, app_id,
                 ManifestUpdateResult::kAppUninstalling);
    update_stages_.erase(it);
  }
  DCHECK(!base::Contains(update_stages_, app_id));
  last_update_check_.erase(app_id);
}

void ManifestUpdateManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

// Throttling updates to at most once per day is consistent with Android.
// See |UPDATE_INTERVAL| in WebappDataStorage.java.
bool ManifestUpdateManager::MaybeConsumeUpdateCheck(const GURL& origin,
                                                    const AppId& app_id) {
  if (IsUpdateConsumed(app_id))
    return false;

  base::Time now = time_override_for_testing_.value_or(base::Time::Now());
  SetLastUpdateCheckTime(origin, app_id, now);
  return true;
}

absl::optional<base::Time> ManifestUpdateManager::GetLastUpdateCheckTime(
    const AppId& app_id) const {
  auto it = last_update_check_.find(app_id);
  return it != last_update_check_.end() ? absl::optional<base::Time>(it->second)
                                        : absl::nullopt;
}

void ManifestUpdateManager::SetLastUpdateCheckTime(const GURL& origin,
                                                   const AppId& app_id,
                                                   base::Time time) {
  last_update_check_[app_id] = time;
}

void ManifestUpdateManager::OnUpdateStopped(const ManifestUpdateTask& task,
                                            ManifestUpdateResult result) {
  auto update_task_it = update_stages_.find(task.app_id());
  DCHECK(update_task_it != update_stages_.end());
  DCHECK_EQ(&task, update_task_it->second.update_task.get());
  NotifyResult(task.url(), task.app_id(), result);
  update_stages_.erase(task.app_id());
}

void ManifestUpdateManager::SetResultCallbackForTesting(
    ResultCallback callback) {
  DCHECK(result_callback_for_testing_.is_null());
  result_callback_for_testing_ = std::move(callback);
}

void ManifestUpdateManager::NotifyResult(const GURL& url,
                                         const absl::optional<AppId>& app_id,
                                         ManifestUpdateResult result) {
  // Don't log kNoAppInScope because it will be far too noisy (most page loads
  // will hit it).
  if (result != ManifestUpdateResult::kNoAppInScope) {
    base::UmaHistogramEnumeration("Webapp.Update.ManifestUpdateResult", result);
  }
  if (result_callback_for_testing_)
    std::move(result_callback_for_testing_).Run(url, result);
}

void ManifestUpdateManager::ResetManifestThrottleForTesting(
    const AppId& app_id) {
  // Erase the throttle info from the map so that corresponding
  // manifest writes can go through.
  auto it = last_update_check_.find(app_id);
  if (it != last_update_check_.end()) {
    last_update_check_.erase(app_id);
  }
  // Manifest update scheduling can still fail if there are existing tasks.
  // Destroy this to ensure the next load will trigger update.
  update_stages_.erase(app_id);
}

}  // namespace web_app
