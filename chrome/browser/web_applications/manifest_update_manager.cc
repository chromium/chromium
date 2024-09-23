// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
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
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/features.h"
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

// TODO(crbug.com/40272003): Also handle DidFinishNavigation() and
// do not start the ManifestUpdateCheckCommand if different origin
// navigation happens.
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
  bool IsInvalidRenderFrameHost(content::RenderFrameHost* render_frame_host) {
    return !render_frame_host || !render_frame_host->IsInPrimaryMainFrame();
  }

  // content::WebContentsObserver:
  // TODO(crbug.com/40873503): Investigate what other functions can be observed
  //  so that for WebAppIntegrationTestDriver::CloseCustomToolbar(), the same
  //  observer can be used.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (IsInvalidRenderFrameHost(render_frame_host)) {
      return;
    }

    page_load_complete_ = true;
    MaybeRunLoadCompleteCallback();
  }

  // This is triggered when the manifest URL gets updated for a page,
  // see WebContentsImpl::OnManifestUrlChanged() for more information.
  void DidUpdateWebManifestURL(content::RenderFrameHost* target_frame,
                               const GURL& manifest_url) override {
    if (IsInvalidRenderFrameHost(target_frame) || !manifest_url.is_valid()) {
      return;
    }

    current_manifest_url_valid_ = true;
    MaybeRunLoadCompleteCallback();
  }

  void WebContentsDestroyed() override {
    Observe(nullptr);
    if (load_complete_callback_) {
      std::move(load_complete_callback_).Run();
    }
  }

  // The final load complete callback is only run once the page has finished
  // loading and the manifest url for the page is valid.
  void MaybeRunLoadCompleteCallback() {
    if (!page_load_complete_ || !current_manifest_url_valid_ ||
        hang_task_callback_for_testing_) {
      return;
    }

    Observe(nullptr);
    if (load_complete_callback_) {
      std::move(load_complete_callback_).Run();
    }
  }

  base::OnceClosure load_complete_callback_;
  bool hang_task_callback_for_testing_;
  bool current_manifest_url_valid_ = false;
  bool page_load_complete_ = false;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ManifestUpdateManager::SetSystemWebAppDelegateMap(
    const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map) {
  system_web_apps_delegate_map_ = system_web_apps_delegate_map;
}
#endif

void ManifestUpdateManager::SetProvider(base::PassKey<WebAppProvider>,
                                        WebAppProvider& provider) {
  provider_ = &provider;
}

void ManifestUpdateManager::Start() {
  install_manager_observation_.Observe(&provider_->install_manager());

  CHECK(!started_);
  started_ = true;
}

void ManifestUpdateManager::Shutdown() {
  install_manager_observation_.Reset();

  update_stages_.clear();
  started_ = false;
}

void ManifestUpdateManager::MaybeUpdate(
    const GURL& url,
    const std::optional<webapps::AppId>& app_id,
    content::WebContents* web_contents) {
  if (!started_) {
    return;
  }

  if (!app_id.has_value() ||
      !provider_->registrar_unsafe().IsInstallState(
          *app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                    proto::INSTALLED_WITH_OS_INTEGRATION})) {
    NotifyResult(url, app_id, ManifestUpdateResult::kNoAppInScope);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (system_web_apps_delegate_map_ &&
      IsSystemWebApp(provider_->registrar_unsafe(),
                     *system_web_apps_delegate_map_, *app_id)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kAppIsSystemWebApp);
    return;
  }
#endif

  if (provider_->registrar_unsafe().IsPlaceholderApp(
          *app_id, WebAppManagement::kPolicy) ||
      provider_->registrar_unsafe().IsPlaceholderApp(
          *app_id, WebAppManagement::kKiosk)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kAppIsPlaceholder);
    return;
  }

  if (provider_->registrar_unsafe().IsIsolated(*app_id)) {
    // Manifests of Isolated Web Apps are only updated when a new version of the
    // app is installed.
    NotifyResult(url, *app_id, ManifestUpdateResult::kAppIsIsolatedWebApp);
    return;
  }

  if (base::Contains(update_stages_, *app_id)) {
    return;
  }

  base::Time check_time =
      time_override_for_testing_.value_or(base::Time::Now());

  if (!MaybeConsumeUpdateCheck(url.DeprecatedGetOriginAsURL(), *app_id,
                               check_time)) {
    NotifyResult(url, *app_id, ManifestUpdateResult::kThrottled);
    return;
  }

  auto load_observer = std::make_unique<PreUpdateWebContentsObserver>(
      base::BindOnce(
          &ManifestUpdateManager::StartCheckAfterPageAndManifestUrlLoad,
          weak_factory_.GetWeakPtr(), *app_id, check_time,
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

void ManifestUpdateManager::StartCheckAfterPageAndManifestUrlLoad(
    const webapps::AppId& app_id,
    base::Time check_time,
    base::WeakPtr<content::WebContents> web_contents) {
  auto update_stage_it = update_stages_.find(app_id);
  CHECK(update_stage_it != update_stages_.end());
  UpdateStage& update_stage = update_stage_it->second;
  GURL url(update_stage.url);
  CHECK(update_stage.observer);
  CHECK_EQ(update_stage.stage,
           UpdateStage::Stage::kWaitingForPageLoadAndManifestUrl);

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

  provider_->scheduler().ScheduleManifestUpdateCheck(
      url, app_id, check_time, web_contents,
      base::BindOnce(&ManifestUpdateManager::OnManifestCheckAwaitAppWindowClose,
                     weak_factory_.GetWeakPtr(), web_contents, url, app_id));
}

void ManifestUpdateManager::OnManifestCheckAwaitAppWindowClose(
    base::WeakPtr<content::WebContents> contents,
    const GURL& url,
    const webapps::AppId& app_id,
    ManifestUpdateCheckResult check_result,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  auto update_stage_it = update_stages_.find(app_id);
  if (update_stage_it == update_stages_.end()) {
    // If the web_app already has already been uninstalled after the
    // manifest update data fetch has happened, then we can early exit.
    return;
  }

  if (check_result ==
      ManifestUpdateCheckResult::kCancelledDueToMainFrameNavigation) {
    update_stages_.erase(app_id);
    NotifyResult(url, app_id,
                 ManifestUpdateResult::kCancelledDueToMainFrameNavigation);
    return;
  }

  if (!contents || contents->IsBeingDestroyed() ||
      !contents->GetBrowserContext()) {
    update_stages_.erase(app_id);
    NotifyResult(url, app_id, ManifestUpdateResult::kWebContentsDestroyed);
    return;
  }

  UpdateStage& update_stage = update_stage_it->second;
  CHECK_EQ(update_stage.stage, UpdateStage::Stage::kCheckingManifestDiff);

  if (check_result != ManifestUpdateCheckResult::kAppUpdateNeeded) {
    OnUpdateStopped(url, app_id,
                    FinalResultFromManifestUpdateCheckResult(check_result));
    return;
  }

  CHECK(install_info);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::APP_MANIFEST_UPDATE, KeepAliveRestartOption::DISABLED);
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive;
  if (!profile->IsOffTheRecord()) {
    profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kWebAppUpdate);
  }

  provider_->scheduler().ScheduleManifestUpdateFinalize(
      url, app_id, std::move(install_info), std::move(keep_alive),
      std::move(profile_keep_alive),
      base::BindOnce(&ManifestUpdateManager::OnUpdateStopped,
                     weak_factory_.GetWeakPtr()));
}

bool ManifestUpdateManager::IsUpdateConsumed(const webapps::AppId& app_id,
                                             base::Time check_time) {
  std::optional<base::Time> last_check_time = GetLastUpdateCheckTime(app_id);
  if (last_check_time.has_value() &&
      check_time < *last_check_time + kDelayBetweenChecks &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableManifestUpdateThrottle)) {
    return true;
  }
  return false;
}

bool ManifestUpdateManager::IsUpdateCommandPending(
    const webapps::AppId& app_id) {
  return base::Contains(update_stages_, app_id);
}

// WebAppInstallManager:
void ManifestUpdateManager::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  CHECK(started_);
  auto it = update_stages_.find(app_id);
  if (it != update_stages_.end()) {
    NotifyResult(it->second.url, app_id,
                 ManifestUpdateResult::kAppUninstalling);
    update_stages_.erase(it);
  }
  CHECK(!base::Contains(update_stages_, app_id));
  last_update_check_.erase(app_id);
}

void ManifestUpdateManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

// Throttling updates to at most once per day is consistent with Android.
// See |UPDATE_INTERVAL| in WebappDataStorage.java.
bool ManifestUpdateManager::MaybeConsumeUpdateCheck(
    const GURL& origin,
    const webapps::AppId& app_id,
    base::Time check_time) {
  if (IsUpdateConsumed(app_id, check_time)) {
    return false;
  }

  SetLastUpdateCheckTime(origin, app_id, check_time);
  return true;
}

std::optional<base::Time> ManifestUpdateManager::GetLastUpdateCheckTime(
    const webapps::AppId& app_id) const {
  auto it = last_update_check_.find(app_id);
  return it != last_update_check_.end() ? std::optional<base::Time>(it->second)
                                        : std::nullopt;
}

void ManifestUpdateManager::SetLastUpdateCheckTime(const GURL& origin,
                                                   const webapps::AppId& app_id,
                                                   base::Time time) {
  last_update_check_[app_id] = time;
}

void ManifestUpdateManager::OnUpdateStopped(const GURL& url,
                                            const webapps::AppId& app_id,
                                            ManifestUpdateResult result) {
  auto update_stage_it = update_stages_.find(app_id);
  // If the app has been uninstalled in the middle of the manifest
  // update, a kAppUninstalled has already been fired.
  if (update_stage_it == update_stages_.end())
    return;
  update_stages_.erase(app_id);
  NotifyResult(url, app_id, result);
}

void ManifestUpdateManager::NotifyResult(
    const GURL& url,
    const std::optional<webapps::AppId>& app_id,
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
    const webapps::AppId& app_id) {
  // Erase the throttle info from the map so that corresponding
  // manifest writes can go through.
  auto it = last_update_check_.find(app_id);
  if (it != last_update_check_.end()) {
    last_update_check_.erase(app_id);
  }
}

bool ManifestUpdateManager::HasUpdatesPendingLoadFinishForTesting() {
  for (const auto& update_data : update_stages_) {
    if (update_data.second.stage ==
        UpdateStage::Stage::kWaitingForPageLoadAndManifestUrl) {
      return true;
    }
  }
  return false;
}

void ManifestUpdateManager::SetLoadFinishedCallbackForTesting(
    base::OnceClosure load_finished_callback) {
  load_finished_callback_ = std::move(load_finished_callback);
}

bool ManifestUpdateManager::IsAppPendingPageAndManifestUrlLoadForTesting(
    const webapps::AppId& app_id) {
  CHECK_IS_TEST();
  auto update_stage_it = update_stages_.find(app_id);
  if (update_stage_it == update_stages_.end()) {
    return false;
  }

  return (update_stage_it->second.stage ==
          UpdateStage::Stage::kWaitingForPageLoadAndManifestUrl);
}

}  // namespace web_app
