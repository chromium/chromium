// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"

#include <memory>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/common/content_features.h"
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

  if (!IsAnyIWAInstalled()) {
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
      continue;
    }
    CreateUpdateApplyWaiter(*url_info);
  }

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks,
                     weak_factory_.GetWeakPtr()));
  update_discovery_timer_.Start(
      FROM_HERE, update_discovery_frequency_, this,
      &IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks);
}

void IsolatedWebAppUpdateManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop all potentially ongoing update discovery tasks and avoid scheduling
  // new tasks.
  install_manager_observation_.Reset();
  update_discovery_timer_.Stop();
  update_discovery_tasks_.clear();
  update_apply_waiters_.clear();
}

base::Value IsolatedWebAppUpdateManager::AsDebugValue() const {
  base::TimeDelta next_update_check_delta =
      update_discovery_timer_.desired_run_time() - base::TimeTicks::Now();
  double next_update_check_delta_in_minutes =
      next_update_check_delta.InSecondsF() / base::Time::kSecondsPerMinute;

  base::Value::List update_discovery_tasks;
  for (const auto& task : update_discovery_tasks_) {
    update_discovery_tasks.Append(task->AsDebugValue());
  }

  base::Value::List update_apply_waiters;
  for (const auto& [app_id, waiter] : update_apply_waiters_) {
    update_apply_waiters.Append(waiter->AsDebugValue());
  }

  return base::Value(
      base::Value::Dict()
          .Set("automatic_updates_enabled", automatic_updates_enabled_)
          .Set("update_discovery_frequency_in_minutes",
               update_discovery_frequency_.InMinutes())
          .Set("update_discovery_timer",
               base::Value::Dict()
                   .Set("running", update_discovery_timer_.IsRunning())
                   .Set("next_update_check_in_minutes",
                        next_update_check_delta_in_minutes))
          .Set("update_discovery_tasks", std::move(update_discovery_tasks))
          .Set("update_discovery_log", update_discovery_results_log_.Clone())
          .Set("update_apply_waiters", std::move(update_apply_waiters)));
}

void IsolatedWebAppUpdateManager::SetEnableAutomaticUpdatesForTesting(
    bool automatic_updates_enabled) {
  CHECK(!has_started_);
  automatic_updates_enabled_ = automatic_updates_enabled;
}

void IsolatedWebAppUpdateManager::OnWebAppInstalled(const AppId& app_id) {
  if (!update_discovery_timer_.IsRunning() && IsAnyIWAInstalled()) {
    update_discovery_timer_.Start(
        FROM_HERE, update_discovery_frequency_, this,
        &IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTasks);
  }
}

void IsolatedWebAppUpdateManager::OnWebAppUninstalled(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  update_apply_waiters_.erase(app_id);
  if (update_discovery_timer_.IsRunning() && !IsAnyIWAInstalled()) {
    update_discovery_timer_.Stop();
  }
}

bool IsolatedWebAppUpdateManager::IsAnyIWAInstalled() {
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

    QueueUpdateDiscoveryTask(url_info, update_manifest_url);
  }

  MaybeStartNextUpdateDiscoveryTask();
}

void IsolatedWebAppUpdateManager::QueueUpdateDiscoveryTask(
    const IsolatedWebAppUrlInfo& url_info,
    const GURL& update_manifest_url) {
  update_discovery_tasks_.push_back(
      std::make_unique<IsolatedWebAppUpdateDiscoveryTask>(
          update_manifest_url, url_info, provider_->scheduler(),
          provider_->registrar_unsafe(), profile_->GetURLLoaderFactory()));
}

void IsolatedWebAppUpdateManager::MaybeStartNextUpdateDiscoveryTask() {
  if (update_discovery_tasks_.empty()) {
    return;
  }

  const std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask>& next_task =
      update_discovery_tasks_.front();
  if (!next_task->has_started()) {
    next_task->Start(base::BindOnce(
        &IsolatedWebAppUpdateManager::OnUpdateDiscoveryTaskCompleted,
        // We can use `base::Unretained` here, because `this` owns
        // `update_discovery_tasks_`.
        base::Unretained(this)));
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

void IsolatedWebAppUpdateManager::OnUpdateDiscoveryTaskCompleted(
    IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) {
  base::Value task_debug_value =
      update_discovery_tasks_.front()->AsDebugValue();
  IsolatedWebAppUrlInfo url_info = update_discovery_tasks_.front()->url_info();
  update_discovery_tasks_.pop_front();

  update_discovery_results_log_.Append(std::move(task_debug_value));
  if (!status.has_value()) {
    LOG(ERROR) << "Isolated Web App update discovery for "
               << url_info.web_bundle_id().id()
               << " failed: " << status.error();
  } else {
    VLOG(1) << "Isolated Web App update discovery for "
            << url_info.web_bundle_id().id()
            << " succeeded: " << status.value();

    if (*status == IsolatedWebAppUpdateDiscoveryTask::Success::
                       kUpdateFoundAndSavedInDatabase) {
      CreateUpdateApplyWaiter(url_info);
    }
  }

  MaybeStartNextUpdateDiscoveryTask();
}

void IsolatedWebAppUpdateManager::OnUpdateApplyWaiterFinished(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive) {
  update_apply_waiters_.erase(url_info.app_id());

  // TODO(cmfcmf): Start task to apply the update here.
}

}  // namespace web_app
