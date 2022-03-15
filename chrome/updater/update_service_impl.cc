// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/check_for_updates_task.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/remove_uninstalled_apps_task.h"
#include "chrome/updater/update_block_check.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_usage_stats_task.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace updater {

namespace {

// The functions below are various adaptors between |update_client| and
// |UpdateService| types.
update_client::Callback MakeUpdateClientCallback(
    UpdateService::Callback callback) {
  return base::BindOnce(
      [](UpdateService::Callback callback, update_client::Error error) {
        std::move(callback).Run(static_cast<UpdateService::Result>(error));
      },
      std::move(callback));
}

UpdateService::UpdateState::State ToUpdateState(
    update_client::ComponentState component_state) {
  switch (component_state) {
    case update_client::ComponentState::kNew:
      return UpdateService::UpdateState::State::kNotStarted;

    case update_client::ComponentState::kChecking:
      return UpdateService::UpdateState::State::kCheckingForUpdates;

    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDownloadingDiff:
    case update_client::ComponentState::kDownloaded:
      return UpdateService::UpdateState::State::kDownloading;

    case update_client::ComponentState::kCanUpdate:
      return UpdateService::UpdateState::State::kUpdateAvailable;

    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdatingDiff:
      return UpdateService::UpdateState::State::kInstalling;

    case update_client::ComponentState::kUpdated:
      return UpdateService::UpdateState::State::kUpdated;

    case update_client::ComponentState::kUpToDate:
      return UpdateService::UpdateState::State::kNoUpdate;

    case update_client::ComponentState::kUpdateError:
      return UpdateService::UpdateState::State::kUpdateError;

    case update_client::ComponentState::kUninstalled:
    case update_client::ComponentState::kRegistration:
    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      NOTREACHED();
      return UpdateService::UpdateState::State::kUnknown;
  }
}

UpdateService::ErrorCategory ToErrorCategory(
    update_client::ErrorCategory error_category) {
  switch (error_category) {
    case update_client::ErrorCategory::kNone:
      return UpdateService::ErrorCategory::kNone;
    case update_client::ErrorCategory::kDownload:
      return UpdateService::ErrorCategory::kDownload;
    case update_client::ErrorCategory::kUnpack:
      return UpdateService::ErrorCategory::kUnpack;
    case update_client::ErrorCategory::kInstall:
      return UpdateService::ErrorCategory::kInstall;
    case update_client::ErrorCategory::kService:
      return UpdateService::ErrorCategory::kService;
    case update_client::ErrorCategory::kUpdateCheck:
      return UpdateService::ErrorCategory::kUpdateCheck;
  }
}

update_client::UpdateClient::CrxStateChangeCallback
MakeUpdateClientCrxStateChangeCallback(
    scoped_refptr<update_client::Configurator> config,
    UpdateService::StateChangeCallback callback) {
  return base::BindRepeating(
      [](scoped_refptr<update_client::Configurator> config,
         UpdateService::StateChangeCallback callback,
         update_client::CrxUpdateItem crx_update_item) {
        UpdateService::UpdateState update_state;
        update_state.app_id = crx_update_item.id;
        update_state.state = ToUpdateState(crx_update_item.state);
        update_state.next_version = crx_update_item.next_version;
        update_state.downloaded_bytes = crx_update_item.downloaded_bytes;
        update_state.total_bytes = crx_update_item.total_bytes;
        update_state.install_progress = crx_update_item.install_progress;
        update_state.error_category =
            ToErrorCategory(crx_update_item.error_category);
        update_state.error_code = crx_update_item.error_code;
        update_state.extra_code1 = crx_update_item.extra_code1;

        // Commit the prefs values written by |update_client| when the
        // update has completed, such as `pv` and `fingerprint`.
        if (update_state.state == UpdateService::UpdateState::State::kUpdated) {
          config->GetPrefService()->CommitPendingWrite();
        }

        callback.Run(update_state);
      },
      config, callback);
}

std::vector<absl::optional<update_client::CrxComponent>> GetComponents(
    scoped_refptr<Configurator> config,
    scoped_refptr<PersistedData> persisted_data,
    const AppInstallDataIndex& app_install_data_index,
    bool foreground,
    bool update_blocked,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    const std::vector<std::string>& ids) {
  VLOG(1) << __func__
          << ". Same version update: " << policy_same_version_update;

  std::vector<absl::optional<update_client::CrxComponent>> components;
  for (const auto& id : ids) {
    components.push_back(
        base::MakeRefCounted<Installer>(
            id,
            [&app_install_data_index, &id]() {
              auto it = app_install_data_index.find(id);
              return it != app_install_data_index.end() ? it->second : "";
            }(),
            [&config, &id]() {
              std::string component_channel;
              return config->GetPolicyService()->GetTargetChannel(
                         id, nullptr, &component_channel)
                         ? component_channel
                         : std::string();
            }(),
            [&config, &id]() {
              std::string target_version_prefix;
              return config->GetPolicyService()->GetTargetVersionPrefix(
                         id, nullptr, &target_version_prefix)
                         ? target_version_prefix
                         : std::string();
            }(),
            [&config, &id]() {
              bool rollback_allowed;
              return config->GetPolicyService()
                             ->IsRollbackToTargetVersionAllowed(
                                 id, nullptr, &rollback_allowed)
                         ? rollback_allowed
                         : false;
            }(),
            [&config, &id, &foreground, update_blocked]() {
              if (update_blocked)
                return true;
              int policy = kPolicyEnabled;
              return config->GetPolicyService()
                         ->GetEffectivePolicyForAppUpdates(id, nullptr,
                                                           &policy) &&
                     (policy == kPolicyDisabled ||
                      (!foreground && policy == kPolicyManualUpdatesOnly) ||
                      (foreground && policy == kPolicyAutomaticUpdatesOnly));
            }(),
            policy_same_version_update, persisted_data,
            config->GetCrxVerifierFormat())
            ->MakeCrxComponent());
  }
  return components;
}

}  // namespace

UpdateServiceImpl::UpdateServiceImpl(scoped_refptr<Configurator> config)
    : config_(config),
      persisted_data_(
          base::MakeRefCounted<PersistedData>(config_->GetPrefService())),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      update_client_(update_client::UpdateClientFactory(config)) {}

void UpdateServiceImpl::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::Version(kUpdaterVersion)));
}

void UpdateServiceImpl::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(const RegistrationResponse&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request.app_id != kUpdaterAppId) {
    persisted_data_->SetHadApps();
  }
  base::Version current_version =
      persisted_data_->GetProductVersion(request.app_id);
  if (current_version.IsValid() &&
      current_version.CompareTo(request.version) == 1) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       RegistrationResponse(kRegistrationAlreadyRegistered)));
    return;
  }
  persisted_data_->RegisterApp(request);
  update_client::CrxComponent crx_component;
  crx_component.app_id = request.app_id;
  crx_component.version = request.version;
  crx_component.requires_network_encryption = false;
  crx_component.ap = request.ap;
  crx_component.brand = request.brand_code;
  update_client_->SendRegistrationPing(
      crx_component,
      base::BindOnce(
          [](base::OnceCallback<void(const RegistrationResponse&)> callback,
             update_client::Error /*error*/) {
            // Ping failures do not count as registration failures.
            std::move(callback).Run(RegistrationResponse(kRegistrationSuccess));
          },
          std::move(callback)));
}

void UpdateServiceImpl::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::string> app_ids = persisted_data_->GetAppIds();
  std::vector<AppState> apps;
  for (const std::string& app_id : app_ids) {
    AppState app_state;
    app_state.app_id = app_id;
    app_state.version = persisted_data_->GetProductVersion(app_id);
    app_state.ap = persisted_data_->GetAP(app_id);
    app_state.brand_code = persisted_data_->GetBrandCode(app_id);
    app_state.brand_path = persisted_data_->GetBrandPath(app_id);
    app_state.ecp = persisted_data_->GetExistenceCheckerPath(app_id);
    apps.push_back(app_state);
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(apps)));
}

void UpdateServiceImpl::RunPeriodicTasks(base::OnceClosure callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  persisted_data_->SetLastStarted(base::Time::NowFromSystemTime());
  DVLOG(1) << "last_started updated.";

  // The installer should make an updater registration, but in case it halts
  // before it does, synthesize a registration if necessary here.
  if (!base::Contains(persisted_data_->GetAppIds(), kUpdaterAppId)) {
    RegistrationRequest updater_request;
    updater_request.app_id = kUpdaterAppId;
    updater_request.version = base::Version(kUpdaterVersion);
    RegisterApp(updater_request, base::DoNothing());
  }

  std::vector<base::OnceCallback<void(base::OnceClosure)>> new_tasks;
  new_tasks.push_back(
      base::BindOnce(&RemoveUninstalledAppsTask::Run,
                     base::MakeRefCounted<RemoveUninstalledAppsTask>(
                         config_, GetUpdaterScope())));
  new_tasks.push_back(base::BindOnce(&UpdateUsageStatsTask::Run,
                                     base::MakeRefCounted<UpdateUsageStatsTask>(
                                         GetUpdaterScope(), persisted_data_)));
  new_tasks.push_back(
      base::BindOnce(&CheckForUpdatesTask::Run,
                     base::MakeRefCounted<CheckForUpdatesTask>(
                         config_, base::BindOnce(&UpdateServiceImpl::UpdateAll,
                                                 this, base::DoNothing()))));
  const auto barrier_closure =
      base::BarrierClosure(new_tasks.size(), std::move(callback));
  for (auto& task : new_tasks) {
    tasks_.push(base::BindOnce(std::move(task),
                               barrier_closure.Then(base::BindRepeating(
                                   &UpdateServiceImpl::TaskDone, this))));
  }

  if (tasks_.size() == new_tasks.size()) {
    TaskStart();
  }
}

void UpdateServiceImpl::TaskStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tasks_.empty()) {
    std::move(tasks_.front()).Run();
  }
}

void UpdateServiceImpl::TaskDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tasks_.pop();
  TaskStart();
}

void UpdateServiceImpl::UpdateAll(StateChangeCallback state_update,
                                  Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto app_ids = persisted_data_->GetAppIds();
  DCHECK(base::Contains(app_ids, kUpdaterAppId));

  using Callback = base::OnceCallback<void(Result)>;

  const Priority priority = Priority::kBackground;
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImpl::OnShouldBlockUpdateForMeteredNetwork, this,
          state_update,
          base::BindOnce(
              [](Callback callback, scoped_refptr<PersistedData> persisted_data,
                 Result result) {
                if (result == Result::kSuccess) {
                  persisted_data->SetLastChecked(
                      base::Time::NowFromSystemTime());
                  DVLOG(1) << "last_checked updated.";
                }
                std::move(callback).Run(result);
              },
              std::move(callback), persisted_data_),
          base::MakeFlatMap<std::string, std::string>(
              app_ids, {},
              [](const auto& app_id) { return std::make_pair(app_id, ""); }),
          priority, UpdateService::PolicySameVersionUpdate::kNotAllowed));
}

void UpdateServiceImpl::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(app_id, priority, policy_same_version_update,
                               policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, policy_same_version_update,
                                 state_update, std::move(callback));
    return;
  }

  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImpl::OnShouldBlockUpdateForMeteredNetwork, this,
          state_update, std::move(callback),
          AppInstallDataIndex({std::make_pair(app_id, install_data_index)}),
          priority, policy_same_version_update));
}

bool UpdateServiceImpl::IsUpdateDisabledByPolicy(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    int& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  policy = kPolicyEnabled;

  // The Install case is inferred by the presence of
  // `PolicySameVersionUpdate::kAllowed`.
  if (policy_same_version_update == PolicySameVersionUpdate::kAllowed) {
    return config_->GetPolicyService()->GetEffectivePolicyForAppInstalls(
               app_id, nullptr, &policy) &&
           (policy == kPolicyDisabled || (config_->IsPerUserInstall() &&
                                          policy == kPolicyEnabledMachineOnly));
  } else {
    return config_->GetPolicyService()->GetEffectivePolicyForAppUpdates(
               app_id, nullptr, &policy) &&
           (policy == kPolicyDisabled ||
            ((policy == kPolicyManualUpdatesOnly) &&
             (priority != Priority::kForeground)) ||
            ((policy == kPolicyAutomaticUpdatesOnly) &&
             (priority == Priority::kForeground)));
  }
}

void UpdateServiceImpl::HandleUpdateDisabledByPolicy(
    const std::string& app_id,
    int policy,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateState update_state;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kUpdateError;
  update_state.error_category = UpdateService::ErrorCategory::kUpdateCheck;
  update_state.error_code =
      policy_same_version_update == PolicySameVersionUpdate::kAllowed
          ? GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY
          : policy != kPolicyAutomaticUpdatesOnly
                ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY
                : GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL;
  update_state.extra_code1 = 0;

  base::BindPostTask(main_task_runner_, state_update).Run(update_state);
  base::BindPostTask(main_task_runner_, std::move(callback))
      .Run(UpdateService::Result::kUpdateCheckFailed);
}

void UpdateServiceImpl::OnShouldBlockUpdateForMeteredNetwork(
    StateChangeCallback state_update,
    Callback callback,
    const AppInstallDataIndex& app_install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::UpdateClient::Update, update_client_,
          [&app_install_data_index]() {
            std::vector<std::string> app_ids;
            app_ids.reserve(app_install_data_index.size());
            std::transform(app_install_data_index.begin(),
                           app_install_data_index.end(),
                           std::back_inserter(app_ids),
                           [](const auto& param) { return param.first; });
            return app_ids;
          }(),
          base::BindOnce(&GetComponents, config_, persisted_data_,
                         app_install_data_index, false, update_blocked,
                         policy_same_version_update),
          MakeUpdateClientCrxStateChangeCallback(config_, state_update),
          priority == Priority::kForeground,
          MakeUpdateClientCallback(std::move(callback))));
}

void UpdateServiceImpl::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefsCommitPendingWrites(config_->GetPrefService());
}

UpdateServiceImpl::~UpdateServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
