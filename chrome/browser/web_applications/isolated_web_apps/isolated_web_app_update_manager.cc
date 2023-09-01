// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <memory>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace web_app {

IsolatedWebAppUpdateManager::IsolatedWebAppUpdateManager(
    Profile& profile,
    base::TimeDelta update_discovery_frequency)
    : profile_(profile),
      automatic_updates_enabled_(
          content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(&profile) &&
          base::FeatureList::IsEnabled(
              features::kIsolatedWebAppAutomaticUpdates)),
      update_discovery_frequency_(std::move(update_discovery_frequency)) {}

IsolatedWebAppUpdateManager::~IsolatedWebAppUpdateManager() = default;

void IsolatedWebAppUpdateManager::SetProvider(base::PassKey<WebAppProvider>,
                                              WebAppProvider& provider) {
  provider_ = &provider;
}

void IsolatedWebAppUpdateManager::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  has_started_ = true;
  if (!automatic_updates_enabled_) {
    return;
  }
  install_manager_observation_.Observe(&provider_->install_manager());

  if (!IsAnyIwaInstalled()) {
    // If no IWA is installed, then we do not need to regularly check for
    // updates and can therefore be a little more efficient.
    // `install_manager_observation_` will take care of starting the timer once
    // an IWA is installed.
    return;
  }

  for (WebApp web_app : provider_->registrar_unsafe().GetApps()) {
    if (!web_app.isolation_data().has_value() ||
        !web_app.isolation_data()->pending_update_info().has_value()) {
      continue;
    }
    auto url_info = IsolatedWebAppUrlInfo::Create(web_app.start_url());
    if (!url_info.has_value()) {
      LOG(ERROR) << "Unable to calculate IsolatedWebAppUrlInfo from "
                 << web_app.start_url();
      continue;
    }
    CreateUpdateApplyWaiter(*url_info);
  }

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&IsolatedWebAppUpdateManager::DelayedStart,
                                weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUpdateManager::DelayedStart() {
  // Kick-off task processing. The task queue can already contain
  // `IsolatedWebAppUpdateApplyTask`s for updates that are pending from the last
  // browser session and were created in `IsolatedWebAppUpdateManager::Start`.
  MaybeStartNextTask();

  QueueUpdateDiscoveryTasks();
  MaybeStartUpdateDiscoveryTimer();
}

void IsolatedWebAppUpdateManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop all potentially ongoing tasks and avoid scheduling new tasks.
  install_manager_observation_.Reset();
  update_discovery_timer_.Stop();
  update_discovery_tasks_.clear();
  update_apply_waiters_.clear();
  update_apply_tasks_.clear();
}

base::Value IsolatedWebAppUpdateManager::AsDebugValue() const {
  base::TimeDelta next_update_check =
      update_discovery_timer_.desired_run_time() - base::TimeTicks::Now();
  double next_update_check_in_minutes =
      next_update_check.InSecondsF() / base::Time::kSecondsPerMinute;

  base::Value::List update_discovery_tasks;
  for (const auto& task : update_discovery_tasks_) {
    update_discovery_tasks.Append(task->AsDebugValue());
  }

  base::Value::List update_apply_waiters;
  for (const auto& [app_id, waiter] : update_apply_waiters_) {
    update_apply_waiters.Append(waiter->AsDebugValue());
  }

  base::Value::List update_apply_tasks;
  for (const auto& task : update_apply_tasks_) {
    update_apply_tasks.Append(task->AsDebugValue());
  }

  return base::Value(
      base::Value::Dict()
          .Set("automatic_updates_enabled", automatic_updates_enabled_)
          .Set("update_discovery_frequency_in_minutes",
               update_discovery_frequency_.InSecondsF() /
                   base::Time::kSecondsPerMinute)
          .Set("update_discovery_timer",
               base::Value::Dict()
                   .Set("running", update_discovery_timer_.IsRunning())
                   .Set("next_update_check_in_minutes",
                        next_update_check_in_minutes))
          .Set("update_discovery_tasks", std::move(update_discovery_tasks))
          .Set("update_discovery_log", update_discovery_results_log_.Clone())
          .Set("update_apply_waiters", std::move(update_apply_waiters))
          .Set("update_apply_tasks", std::move(update_apply_tasks))
          .Set("update_apply_log", update_apply_results_log_.Clone()));
}

void IsolatedWebAppUpdateManager::SetEnableAutomaticUpdatesForTesting(
    bool automatic_updates_enabled) {
  CHECK(!has_started_);
  automatic_updates_enabled_ = automatic_updates_enabled;
}

void IsolatedWebAppUpdateManager::OnWebAppInstalled(const AppId& app_id) {
  MaybeStartUpdateDiscoveryTimer();
}

void IsolatedWebAppUpdateManager::OnWebAppUninstalled(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  update_apply_waiters_.erase(app_id);
  MaybeStopUpdateDiscoveryTimer();
}

void IsolatedWebAppUpdateManager::DiscoverUpdatesNowForTesting() {
  QueueUpdateDiscoveryTasks();
}

bool IsolatedWebAppUpdateManager::IsAnyIwaInstalled() {
  for (const WebApp& app : provider_->registrar_unsafe().GetApps()) {
    if (app.isolation_data().has_value()) {
      return true;
    }
  }
  return false;
}

base::flat_map<web_package::SignedWebBundleId, GURL>
IsolatedWebAppUpdateManager::GetForceInstalledBundleIdToUpdateManifestUrlMap() {
  base::flat_map<web_package::SignedWebBundleId, GURL>
      id_to_update_manifest_map;

  const base::Value::List& iwa_force_install_list =
      profile_->GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList);
  for (const base::Value& policy_entry : iwa_force_install_list) {
    base::expected<IsolatedWebAppExternalInstallOptions, std::string> options =
        IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_entry);
    if (!options.has_value()) {
      LOG(ERROR) << "IsolatedWebAppUpdateManager: "
                 << "Could not parse IWA force-install policy: "
                 << options.error();
      continue;
    }

    id_to_update_manifest_map.emplace(options->web_bundle_id(),
                                      options->update_manifest_url());
  }

  return id_to_update_manifest_map;
}

void IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks() {
  // Clear the log of previously finished update discovery tasks when queueing
  // new tasks so that it doesn't grow forever.
  update_discovery_results_log_.clear();

  base::flat_map<web_package::SignedWebBundleId, GURL>
      id_to_update_manifest_map =
          GetForceInstalledBundleIdToUpdateManifestUrlMap();

  // TODO(crbug.com/1459160): In the future, we also need to automatically
  // update IWAs not installed via policy.
  for (const auto& [web_bundle_id, update_manifest_url] :
       id_to_update_manifest_map) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    const WebApp* web_app =
        provider_->registrar_unsafe().GetAppById(url_info.app_id());
    if (!web_app) {
      continue;
    }
    const absl::optional<WebApp::IsolationData>& isolation_data =
        web_app->isolation_data();
    if (!isolation_data) {
      continue;
    }
    if (!absl::holds_alternative<InstalledBundle>(isolation_data->location)) {
      // Never automatically update IWAs installed in dev mode. Updates for dev
      // mode apps will be triggerable manually from the upcoming dev mode
      // browser UI.
      continue;
    }

    update_discovery_tasks_.push_back(
        std::make_unique<IsolatedWebAppUpdateDiscoveryTask>(
            update_manifest_url, url_info, provider_->scheduler(),
            provider_->registrar_unsafe(), profile_->GetURLLoaderFactory()));
  }

  MaybeStartNextTask();
}

void IsolatedWebAppUpdateManager::MaybeStartNextTask() {
  if (IsAnyTaskRunning()) {
    return;
  }

  if (!update_apply_tasks_.empty()) {
    update_apply_tasks_.front()->Start(
        base::BindOnce(&IsolatedWebAppUpdateManager::OnUpdateApplyTaskCompleted,
                       // We can use `base::Unretained` here, because `this`
                       // owns `update_apply_tasks_`.
                       base::Unretained(this)));
    return;
  }

  if (!update_discovery_tasks_.empty()) {
    update_discovery_tasks_.front()->Start(base::BindOnce(
        &IsolatedWebAppUpdateManager::OnUpdateDiscoveryTaskCompleted,
        // We can use `base::Unretained` here, because `this` owns
        // `update_discovery_tasks_`.
        base::Unretained(this)));
    return;
  }
}

void IsolatedWebAppUpdateManager::MaybeStartUpdateDiscoveryTimer() {
  if (!update_discovery_timer_.IsRunning() && IsAnyIwaInstalled()) {
    update_discovery_timer_.Start(
        FROM_HERE, update_discovery_frequency_, this,
        &IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks);
  }
}

void IsolatedWebAppUpdateManager::MaybeStopUpdateDiscoveryTimer() {
  if (update_discovery_timer_.IsRunning() && !IsAnyIwaInstalled()) {
    update_discovery_timer_.Stop();
  }
}

void IsolatedWebAppUpdateManager::CreateUpdateApplyWaiter(
    const IsolatedWebAppUrlInfo& url_info) {
  const AppId& app_id = url_info.app_id();
  if (update_apply_waiters_.contains(app_id)) {
    return;
  }
  auto [it, was_insertion] = update_apply_waiters_.emplace(
      app_id, std::make_unique<IsolatedWebAppUpdateApplyWaiter>(
                  url_info, provider_->ui_manager()));
  CHECK(was_insertion);
  it->second->Wait(
      &*profile_,
      base::BindOnce(&IsolatedWebAppUpdateManager::OnUpdateApplyWaiterFinished,
                     weak_factory_.GetWeakPtr(), url_info));
}

bool IsolatedWebAppUpdateManager::IsAnyTaskRunning() const {
  return (!update_apply_tasks_.empty() &&
          update_apply_tasks_.front()->has_started()) ||
         (!update_discovery_tasks_.empty() &&
          update_discovery_tasks_.front()->has_started());
}

void IsolatedWebAppUpdateManager::OnUpdateDiscoveryTaskCompleted(
    IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) {
  base::Value task_debug_value =
      update_discovery_tasks_.front()->AsDebugValue();
  IsolatedWebAppUrlInfo url_info = update_discovery_tasks_.front()->url_info();
  update_discovery_tasks_.pop_front();

  update_discovery_results_log_.Append(task_debug_value.Clone());
  if (!status.has_value()) {
    LOG(ERROR) << "Isolated Web App update discovery for "
               << url_info.web_bundle_id().id() << " failed: " << status.error()
               << " debug log: " << task_debug_value;
  } else {
    VLOG(1) << "Isolated Web App update discovery for "
            << url_info.web_bundle_id().id()
            << " succeeded: " << status.value();

    if (*status == IsolatedWebAppUpdateDiscoveryTask::Success::
                       kUpdateFoundAndSavedInDatabase) {
      CreateUpdateApplyWaiter(url_info);
    }
  }

  MaybeStartNextTask();
}

void IsolatedWebAppUpdateManager::OnUpdateApplyWaiterFinished(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive) {
  update_apply_waiters_.erase(url_info.app_id());

  update_apply_tasks_.push_back(std::make_unique<IsolatedWebAppUpdateApplyTask>(
      url_info, std::move(keep_alive), std::move(profile_keep_alive),
      provider_->scheduler()));

  MaybeStartNextTask();
}

void IsolatedWebAppUpdateManager::OnUpdateApplyTaskCompleted(
    IsolatedWebAppUpdateApplyTask::CompletionStatus status) {
  base::Value task_debug_value = update_apply_tasks_.front()->AsDebugValue();
  IsolatedWebAppUrlInfo url_info = update_apply_tasks_.front()->url_info();
  update_apply_tasks_.pop_front();

  update_apply_results_log_.Append(task_debug_value.Clone());
  if (status.has_value()) {
    VLOG(1) << "Applying an Isolated Web App update for "
            << url_info.web_bundle_id().id() << " succeeded.";
  } else {
    LOG(ERROR) << "Applying an Isolated Web App update for "
               << url_info.web_bundle_id().id() << " failed: " << status.error()
               << " debug log: " << task_debug_value;
  }

  MaybeStartNextTask();
}

}  // namespace web_app
