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
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/web_applications/web_app_system_web_app_delegate_map_utils.h"
#endif

class Profile;

namespace web_app {

namespace {

// Returns a shared instance of UpdatePendingCallback.
ManifestUpdateManager::UpdatePendingCallback*
GetUpdatePendingCallbackMutableForTesting() {
  static base::NoDestructor<ManifestUpdateManager::UpdatePendingCallback>
      g_update_pending_callback;
  return g_update_pending_callback.get();
}

ManifestUpdateManager::ResultCallback* GetResultCallbackMutableForTesting() {
  static base::NoDestructor<ManifestUpdateManager::ResultCallback>
      g_result_callback;
  return g_result_callback.get();
}

}  // namespace

ManifestUpdateManager::ScopedBypassWindowCloseWaitingForTesting::
    ScopedBypassWindowCloseWaitingForTesting() {
  BypassWindowCloseWaitingForTesting() = true;  // IN-TEST
}

ManifestUpdateManager::ScopedBypassWindowCloseWaitingForTesting::
    ~ScopedBypassWindowCloseWaitingForTesting() {
  BypassWindowCloseWaitingForTesting() = false;  // IN-TEST
}

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
  // TODO(crbug.com/1376155): Investigate what other functions can be observed
  //  so that for WebAppIntegrationTestDriver::CloseCustomToolbar(), the same
  //  observer can be used.
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

// static
void ManifestUpdateManager::SetUpdatePendingCallbackForTesting(
    UpdatePendingCallback callback) {
  *GetUpdatePendingCallbackMutableForTesting() =  // IN-TEST
      std::move(callback);
}

// static
void ManifestUpdateManager::SetResultCallbackForTesting(
    ResultCallback callback) {
  *GetResultCallbackMutableForTesting() =  // IN-TEST
      std::move(callback);
}

// static
bool& ManifestUpdateManager::BypassWindowCloseWaitingForTesting() {
  static bool bypass_window_close_waiting_for_testing_ = false;
  return bypass_window_close_waiting_for_testing_;
}

ManifestUpdateManager::ManifestUpdateManager() = default;

ManifestUpdateManager::~ManifestUpdateManager() = default;

void ManifestUpdateManager::SetSubsystems(
    WebAppInstallManager* install_manager,
    WebAppRegistrar* registrar,
    WebAppUiManager* ui_manager,
    WebAppCommandScheduler* command_scheduler) {
  install_manager_ = install_manager;
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  command_scheduler_ = command_scheduler;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ManifestUpdateManager::SetSystemWebAppDelegateMap(
    const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map) {
  system_web_apps_delegate_map_ = system_web_apps_delegate_map;
}
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_web_apps_delegate_map_ &&
      IsSystemWebApp(*registrar_, *system_web_apps_delegate_map_, *app_id)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kAppIsSystemWebApp);
    return;
  }
#endif

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
      base::BindOnce(&ManifestUpdateManager::StartManifestCheckAfterPageLoad,
                     weak_factory_.GetWeakPtr(), *app_id,
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

void ManifestUpdateManager::StartManifestCheckAfterPageLoad(
    const AppId& app_id,
    base::WeakPtr<content::WebContents> web_contents) {
  auto update_stage_it = update_stages_.find(app_id);
  DCHECK(update_stage_it != update_stages_.end());
  UpdateStage& update_stage = update_stage_it->second;
  GURL url(update_stage.url);
  DCHECK(update_stage.observer);
  DCHECK_EQ(update_stage.stage, UpdateStage::Stage::kWaitingForPageLoad);

  // If web_contents have been destroyed before page load,
  // then no need of running the command.
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    OnUpdateStopped(url, app_id, ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }

  // The observer's task is done, the other stages is used to keep track of the
  // 2 manifest update commands. See ManifestUpdateDataFetchCommand and
  // ManifestUpdateFinalizeCommand for more details.
  update_stage.observer.reset();
  update_stage.stage = UpdateStage::Stage::kCheckingManifestDiff;

  if (load_finished_callback_)
    std::move(load_finished_callback_).Run();

  command_scheduler_->ScheduleManifestUpdateCheck(
      url, app_id, web_contents,
      base::BindOnce(&ManifestUpdateManager::OnManifestCheckAwaitAppWindowClose,
                     weak_factory_.GetWeakPtr(), web_contents, url, app_id));
}

void ManifestUpdateManager::OnManifestCheckAwaitAppWindowClose(
    base::WeakPtr<content::WebContents> contents,
    const GURL& url,
    const AppId& app_id,
    ManifestUpdateCheckResult check_result,
    absl::optional<WebAppInstallInfo> install_info) {
  auto update_stage_it = update_stages_.find(app_id);
  if (update_stage_it == update_stages_.end()) {
    // If the web_app already has already been uninstalled after the
    // manifest update data fetch has happened, then we can early exit.
    return;
  }

  if (!contents || contents->IsBeingDestroyed() ||
      !contents->GetBrowserContext()) {
    update_stages_.erase(app_id);
    NotifyResult(url, app_id, ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }

  UpdateStage& update_stage = update_stage_it->second;
  DCHECK_EQ(update_stage.stage, UpdateStage::Stage::kCheckingManifestDiff);
  update_stage.stage = UpdateStage::Stage::kPendingAppWindowClose;

  if (check_result != ManifestUpdateCheckResult::kAppUpdateNeeded) {
    OnUpdateStopped(url, app_id,
                    FinalResultFromManifestUpdateCheckResult(check_result));
    return;
  }

  DCHECK(install_info.has_value());

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::APP_MANIFEST_UPDATE, KeepAliveRestartOption::DISABLED);
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  if (!profile->IsOffTheRecord()) {
    profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kWebAppUpdate);
  }

  if (base::FeatureList::IsEnabled(
          features::kWebAppManifestImmediateUpdating) ||
      BypassWindowCloseWaitingForTesting()) {
    StartManifestWriteAfterWindowsClosed(url, app_id, std::move(keep_alive),
                                         std::move(profile_keep_alive),
                                         std::move(install_info.value()));
  } else {
    ui_manager_->NotifyOnAllAppWindowsClosed(
        app_id,
        base::BindOnce(
            &ManifestUpdateManager::StartManifestWriteAfterWindowsClosed,
            weak_factory_.GetWeakPtr(), url, app_id, std::move(keep_alive),
            std::move(profile_keep_alive), std::move(install_info.value())));
    UpdatePendingCallback* callback =
        GetUpdatePendingCallbackMutableForTesting();  // IN-TEST
    if (!callback->is_null())
      std::move(*callback).Run(url);
  }
}

void ManifestUpdateManager::StartManifestWriteAfterWindowsClosed(
    const GURL& url,
    const AppId& app_id,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    WebAppInstallInfo install_info) {
  auto update_stage_it = update_stages_.find(app_id);
  if (update_stage_it == update_stages_.end()) {
    // If the web_app already has already been uninstalled after the
    // manifest update data fetch has happened, then we can early exit.
    return;
  }

  UpdateStage& update_stage = update_stage_it->second;
  DCHECK_EQ(update_stage.stage, UpdateStage::Stage::kPendingAppWindowClose);

  command_scheduler_->ScheduleManifestUpdateFinalize(
      url, app_id, std::move(install_info), std::move(keep_alive),
      std::move(profile_keep_alive),
      base::BindOnce(&ManifestUpdateManager::OnUpdateStopped,
                     weak_factory_.GetWeakPtr()));
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

bool ManifestUpdateManager::IsUpdateCommandPending(const AppId& app_id) {
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

void ManifestUpdateManager::OnUpdateStopped(const GURL& url,
                                            const AppId& app_id,
                                            ManifestUpdateResult result) {
  auto update_stage_it = update_stages_.find(app_id);
  // If the app has been uninstalled in the middle of the manifest
  // update, a kAppUninstalled has already been fired.
  if (update_stage_it == update_stages_.end())
    return;
  update_stages_.erase(app_id);
  NotifyResult(url, app_id, result);
}

void ManifestUpdateManager::NotifyResult(const GURL& url,
                                         const absl::optional<AppId>& app_id,
                                         ManifestUpdateResult result) {
  // Don't log kNoAppInScope because it will be far too noisy (most page loads
  // will hit it).
  if (result != ManifestUpdateResult::kNoAppInScope) {
    base::UmaHistogramEnumeration("Webapp.Update.ManifestUpdateResult", result);
  }
  if (*GetResultCallbackMutableForTesting()) {
    std::move(*GetResultCallbackMutableForTesting()).Run(url, result);
  }
}

void ManifestUpdateManager::ResetManifestThrottleForTesting(
    const AppId& app_id) {
  // Erase the throttle info from the map so that corresponding
  // manifest writes can go through.
  auto it = last_update_check_.find(app_id);
  if (it != last_update_check_.end()) {
    last_update_check_.erase(app_id);
  }
}

bool ManifestUpdateManager::HasUpdatesPendingLoadFinishForTesting() {
  for (const auto& update_data : update_stages_) {
    if (update_data.second.stage == UpdateStage::Stage::kWaitingForPageLoad)
      return true;
  }
  return false;
}

void ManifestUpdateManager::SetLoadFinishedCallbackForTesting(
    base::OnceClosure load_finished_callback) {
  load_finished_callback_ = std::move(load_finished_callback);
}

base::flat_set<AppId>
ManifestUpdateManager::GetAppsPendingWindowsClosingForTesting() {
  base::flat_set<AppId> apps_pending_window_closed;
  for (const auto& data : update_stages_) {
    if (data.second.stage == UpdateStage::Stage::kPendingAppWindowClose) {
      apps_pending_window_closed.emplace(data.first);
    }
  }
  return apps_pending_window_closed;
}

}  // namespace web_app
