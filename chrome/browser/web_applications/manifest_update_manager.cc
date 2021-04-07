// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace web_app {

constexpr base::TimeDelta kDelayBetweenChecks = base::TimeDelta::FromDays(1);
constexpr const char kDisableManifestUpdateThrottle[] =
    "disable-manifest-update-throttle";

ManifestUpdateManager::ManifestUpdateManager() = default;

ManifestUpdateManager::~ManifestUpdateManager() = default;

void ManifestUpdateManager::SetSubsystems(
    AppRegistrar* registrar,
    AppIconManager* icon_manager,
    WebAppUiManager* ui_manager,
    InstallManager* install_manager,
    SystemWebAppManager* system_web_app_manager,
    OsIntegrationManager* os_integration_manager) {
  registrar_ = registrar;
  icon_manager_ = icon_manager;
  ui_manager_ = ui_manager;
  install_manager_ = install_manager;
  system_web_app_manager_ = system_web_app_manager;
  os_integration_manager_ = os_integration_manager;
}

void ManifestUpdateManager::Start() {
  registrar_observer_.Add(registrar_);

  DCHECK(!started_);
  started_ = true;
}

void ManifestUpdateManager::Shutdown() {
  registrar_observer_.RemoveAll();

  tasks_.clear();
  started_ = false;
}

void ManifestUpdateManager::MaybeUpdate(const GURL& url,
                                        const AppId& app_id,
                                        content::WebContents* web_contents) {
  if (!started_) {
    return;
  }

  if (app_id.empty() || !registrar_->IsLocallyInstalled(app_id)) {
    NotifyResult(url, app_id, ManifestUpdateResult::kNoAppInScope);
    return;
  }

  if (system_web_app_manager_->IsSystemWebApp(app_id)) {
    NotifyResult(url, app_id, ManifestUpdateResult::kAppIsSystemWebApp);
    return;
  }

  if (registrar_->IsPlaceholderApp(app_id)) {
    NotifyResult(url, app_id, ManifestUpdateResult::kAppIsPlaceholder);
    return;
  }

  if (base::Contains(tasks_, app_id))
    return;

  if (!MaybeConsumeUpdateCheck(url.GetOrigin(), app_id)) {
    NotifyResult(url, app_id, ManifestUpdateResult::kThrottled);
    return;
  }

  tasks_.insert_or_assign(
      app_id, std::make_unique<ManifestUpdateTask>(
                  url, app_id, web_contents,
                  base::BindOnce(&ManifestUpdateManager::OnUpdateStopped,
                                 base::Unretained(this)),
                  hang_update_checks_for_testing_, *registrar_, *icon_manager_,
                  ui_manager_, install_manager_, *os_integration_manager_));
}

bool ManifestUpdateManager::IsUpdateConsumed(const AppId& app_id) {
  base::Optional<base::Time> last_check_time = GetLastUpdateCheckTime(app_id);
  base::Time now = time_override_for_testing_.value_or(base::Time::Now());
  if (last_check_time.has_value() &&
      now < *last_check_time + kDelayBetweenChecks &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableManifestUpdateThrottle)) {
    return true;
  }
  return false;
}

// AppRegistrarObserver:
void ManifestUpdateManager::OnWebAppWillBeUninstalled(const AppId& app_id) {
  DCHECK(started_);

  auto it = tasks_.find(app_id);
  if (it != tasks_.end()) {
    NotifyResult(it->second->url(), app_id,
                 ManifestUpdateResult::kAppUninstalled);
    tasks_.erase(it);
  }
  DCHECK(!tasks_.contains(app_id));
  last_update_check_.erase(app_id);
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

base::Optional<base::Time> ManifestUpdateManager::GetLastUpdateCheckTime(
    const AppId& app_id) const {
  auto it = last_update_check_.find(app_id);
  return it != last_update_check_.end() ? base::Optional<base::Time>(it->second)
                                        : base::nullopt;
}

void ManifestUpdateManager::SetLastUpdateCheckTime(const GURL& origin,
                                                   const AppId& app_id,
                                                   base::Time time) {
  last_update_check_[app_id] = time;
}

void ManifestUpdateManager::OnUpdateStopped(const ManifestUpdateTask& task,
                                            ManifestUpdateResult result) {
  DCHECK_EQ(&task, tasks_[task.app_id()].get());
  NotifyResult(task.url(), task.app_id(), result);
  tasks_.erase(task.app_id());
}

void ManifestUpdateManager::SetResultCallbackForTesting(
    ResultCallback callback) {
  DCHECK(result_callback_for_testing_.is_null());
  result_callback_for_testing_ = std::move(callback);
}

void ManifestUpdateManager::NotifyResult(const GURL& url,
                                         const AppId& app_id,
                                         ManifestUpdateResult result) {
  // Don't log kNoAppInScope because it will be far too noisy (most page loads
  // will hit it).
  if (result != ManifestUpdateResult::kNoAppInScope) {
    base::UmaHistogramEnumeration("Webapp.Update.ManifestUpdateResult", result);
    if (registrar_->HasExternalAppWithInstallSource(
            app_id, ExternalInstallSource::kExternalDefault)) {
      base::UmaHistogramEnumeration(
          "Webapp.Update.ManifestUpdateResult.DefaultApp", result);
    }
  }
  if (result_callback_for_testing_)
    std::move(result_callback_for_testing_).Run(url, result);
}

}  // namespace web_app
