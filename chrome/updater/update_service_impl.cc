// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/auto_run_on_os_upgrade_task.h"
#include "chrome/updater/change_owners_task.h"
#include "chrome/updater/check_for_updates_task.h"
#include "chrome/updater/cleanup_task.h"
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
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/win/installer_api.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater {

// The functions below are various adaptors between |update_client| and
// |UpdateService| types.

namespace internal {
UpdateService::Result ToResult(update_client::Error error) {
  switch (error) {
    case update_client::Error::NONE:
      return UpdateService::Result::kSuccess;
    case update_client::Error::UPDATE_IN_PROGRESS:
      return UpdateService::Result::kUpdateInProgress;
    case update_client::Error::UPDATE_CANCELED:
      return UpdateService::Result::kUpdateCanceled;
    case update_client::Error::RETRY_LATER:
      return UpdateService::Result::kRetryLater;
    case update_client::Error::SERVICE_ERROR:
      return UpdateService::Result::kServiceFailed;
    case update_client::Error::UPDATE_CHECK_ERROR:
      return UpdateService::Result::kUpdateCheckFailed;
    case update_client::Error::CRX_NOT_FOUND:
      return UpdateService::Result::kAppNotFound;
    case update_client::Error::INVALID_ARGUMENT:
    case update_client::Error::BAD_CRX_DATA_CALLBACK:
      return UpdateService::Result::kInvalidArgument;
    case update_client::Error::MAX_VALUE:
      NOTREACHED();
      return UpdateService::Result::kInvalidArgument;
  }
}
}  // namespace internal

namespace {

update_client::Callback MakeUpdateClientCallback(
    UpdateService::Callback callback) {
  return base::BindOnce(
      [](UpdateService::Callback callback, update_client::Error error) {
        std::move(callback).Run(internal::ToResult(error));
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

    case update_client::ComponentState::kPingOnly:
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
    scoped_refptr<PersistedData> persisted_data,
    const bool new_install,
    UpdateService::StateChangeCallback callback) {
  return base::BindRepeating(
      [](scoped_refptr<update_client::Configurator> config,
         scoped_refptr<PersistedData> persisted_data, const bool new_install,
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
        if (crx_update_item.installer_result) {
          update_state.error_code =
              crx_update_item.installer_result->original_error;
          update_state.installer_cmd_line =
              crx_update_item.installer_result->installer_cmd_line;
          update_state.installer_text =
              crx_update_item.installer_result->installer_text;
        }

        if (update_state.state == UpdateService::UpdateState::State::kUpdated ||
            update_state.state ==
                UpdateService::UpdateState::State::kUpdateError ||
            update_state.state ==
                UpdateService::UpdateState::State::kNoUpdate) {
          // If a new install encounters an error, the AppId registered in
          // `UpdateServiceImpl::Install` needs to be removed here. Otherwise
          // the updater may remain installed even if there are no other apps to
          // manage, and try to update the app even though the app was not
          // installed.
          if (new_install &&
              (update_state.state ==
                   UpdateService::UpdateState::State::kUpdateError ||
               update_state.state ==
                   UpdateService::UpdateState::State::kNoUpdate)) {
            persisted_data->RemoveApp(update_state.app_id);
          }

          // Commit the prefs values written by |update_client| when the
          // update has completed, such as `pv` and `fingerprint`.
          config->GetPrefService()->CommitPendingWrite();
        }

        callback.Run(update_state);
      },
      config, persisted_data, new_install, callback);
}

std::vector<absl::optional<update_client::CrxComponent>> GetComponents(
    scoped_refptr<Configurator> config,
    scoped_refptr<PersistedData> persisted_data,
    const AppClientInstallData& app_client_install_data,
    const AppInstallDataIndex& app_install_data_index,
    UpdateService::Priority priority,
    bool update_blocked,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    const std::vector<std::string>& ids) {
  VLOG(1) << __func__
          << ". Same version update: " << policy_same_version_update;
  const bool is_foreground = priority == UpdateService::Priority::kForeground;
  std::vector<absl::optional<update_client::CrxComponent>> components;
  for (const auto& id : ids) {
    components.push_back(
        base::MakeRefCounted<Installer>(
            id,
            [&app_client_install_data, &id]() {
              auto it = app_client_install_data.find(id);
              return it != app_client_install_data.end() ? it->second : "";
            }(),
            [&app_install_data_index, &id]() {
              auto it = app_install_data_index.find(id);
              return it != app_install_data_index.end() ? it->second : "";
            }(),
            [&config, &id]() {
              return config->GetPolicyService()->GetTargetChannel(id).policy_or(
                  std::string());
            }(),
            [&config, &id]() {
              return config->GetPolicyService()
                  ->GetTargetVersionPrefix(id)
                  .policy_or(std::string());
            }(),
            [&config, &id]() {
              return config->GetPolicyService()
                  ->IsRollbackToTargetVersionAllowed(id)
                  .policy_or(false);
            }(),
            [&config, &id, &is_foreground, update_blocked]() {
              if (update_blocked) {
                return true;
              }
              PolicyStatus<int> app_updates =
                  config->GetPolicyService()->GetPolicyForAppUpdates(id);
              return app_updates &&
                     (app_updates.policy() == kPolicyDisabled ||
                      (!is_foreground &&
                       app_updates.policy() == kPolicyManualUpdatesOnly) ||
                      (is_foreground &&
                       app_updates.policy() == kPolicyAutomaticUpdatesOnly));
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
          base::MakeRefCounted<PersistedData>(GetUpdaterScope(),
                                              config_->GetPrefService())),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      update_client_(update_client::UpdateClientFactory(config)) {}

void UpdateServiceImpl::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::Version(kUpdaterVersion)));
}

void UpdateServiceImpl::FetchPolicies(base::OnceCallback<void(int)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetUpdaterScope() == UpdaterScope::kUser) {
    VLOG(2) << "Policy fetch skipped for user updater.";
    std::move(callback).Run(0);
  } else {
    config_->GetPolicyService()->FetchPolicies(std::move(callback));
  }
}

void UpdateServiceImpl::RegisterApp(const RegistrationRequest& request,
                                    base::OnceCallback<void(int)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::EqualsCaseInsensitiveASCII(request.app_id, kUpdaterAppId)) {
    persisted_data_->SetHadApps();
  }
  persisted_data_->RegisterApp(request);
  std::move(callback).Run(kRegistrationSuccess);
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
  VLOG(1) << "last_started updated.";

  // The installer should make an updater registration, but in case it halts
  // before it does, synthesize a registration if necessary here.
  const base::Version registered_updater_version =
      persisted_data_->GetProductVersion(kUpdaterAppId);
  if (!registered_updater_version.IsValid() ||
      base::Version(kUpdaterVersion) > registered_updater_version) {
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
      MakeChangeOwnersTask(base::MakeRefCounted<PersistedData>(
                               GetUpdaterScope(), config_->GetPrefService()),
                           GetUpdaterScope()));

  new_tasks.push_back(base::BindOnce(
      [](scoped_refptr<UpdateServiceImpl> update_service_impl,
         base::OnceClosure callback) {
        update_service_impl->FetchPolicies(base::BindOnce(
            [](base::OnceClosure callback, int /* ignore_result */) {
              std::move(callback).Run();
            },
            std::move(callback)));
      },
      base::WrapRefCounted(this)));
  new_tasks.push_back(
      base::BindOnce(&CheckForUpdatesTask::Run,
                     base::MakeRefCounted<CheckForUpdatesTask>(
                         config_, GetUpdaterScope(),
                         base::BindOnce(&UpdateServiceImpl::ForceInstall, this,
                                        base::DoNothing()))));
  new_tasks.push_back(
      base::BindOnce(&CheckForUpdatesTask::Run,
                     base::MakeRefCounted<CheckForUpdatesTask>(
                         config_, GetUpdaterScope(),
                         base::BindOnce(&UpdateServiceImpl::UpdateAll, this,
                                        base::DoNothing()))));
  new_tasks.push_back(
      base::BindOnce(&AutoRunOnOsUpgradeTask::Run,
                     base::MakeRefCounted<AutoRunOnOsUpgradeTask>(
                         GetUpdaterScope(), persisted_data_)));
  new_tasks.push_back(base::BindOnce(
      &CleanupTask::Run, base::MakeRefCounted<CleanupTask>(GetUpdaterScope())));

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
    main_task_runner_->PostTask(FROM_HERE, std::move(tasks_.front()));
  }
}

void UpdateServiceImpl::TaskDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tasks_.pop();
  TaskStart();
}

void UpdateServiceImpl::ForceInstall(StateChangeCallback state_update,
                                     Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PolicyStatus<std::vector<std::string>> force_install_apps_status =
      config_->GetPolicyService()->GetForceInstallApps();
  if (!force_install_apps_status) {
    base::BindPostTask(main_task_runner_, std::move(callback))
        .Run(UpdateService::Result::kSuccess);
    return;
  }
  std::vector<std::string> force_install_apps =
      force_install_apps_status.policy();
  CHECK(!force_install_apps.empty());

  std::vector<std::string> installed_app_ids = persisted_data_->GetAppIds();
  base::ranges::sort(force_install_apps);
  base::ranges::sort(installed_app_ids);

  std::vector<std::string> app_ids_to_install;
  base::ranges::set_difference(force_install_apps, installed_app_ids,
                               std::back_inserter(app_ids_to_install));
  if (app_ids_to_install.empty()) {
    base::BindPostTask(main_task_runner_, std::move(callback))
        .Run(UpdateService::Result::kSuccess);
    return;
  }

  VLOG(1) << __func__ << ": app_ids_to_install: "
          << base::JoinString(app_ids_to_install, " ");

  ShouldBlockUpdateForMeteredNetwork(
      Priority::kBackground,
      base::BindOnce(
          &UpdateServiceImpl::OnShouldBlockForceInstallForMeteredNetwork, this,
          app_ids_to_install, AppClientInstallData(), AppInstallDataIndex(),
          UpdateService::PolicySameVersionUpdate::kNotAllowed, state_update,
          std::move(callback)));
}

void UpdateServiceImpl::CheckForUpdate(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback) {
  VLOG(1) << __func__ << ": " << app_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(app_id, priority, false, policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, false, state_update,
                                 std::move(callback));
    return;
  }
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImpl::OnShouldBlockCheckForUpdateForMeteredNetwork,
          this, app_id, priority, policy_same_version_update, state_update,
          std::move(callback)));
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
  if (IsUpdateDisabledByPolicy(app_id, priority, false, policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, false, state_update,
                                 std::move(callback));
    return;
  }
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImpl::OnShouldBlockUpdateForMeteredNetwork, this,
          std::vector<std::string>{app_id}, AppClientInstallData(),
          AppInstallDataIndex({std::make_pair(app_id, install_data_index)}),
          priority, policy_same_version_update, state_update,
          std::move(callback)));
}

void UpdateServiceImpl::UpdateAll(StateChangeCallback state_update,
                                  Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto app_ids = persisted_data_->GetAppIds();
  CHECK(base::Contains(
      app_ids, base::ToLowerASCII(kUpdaterAppId),
      static_cast<std::string (*)(base::StringPiece)>(&base::ToLowerASCII)));

  const Priority priority = Priority::kBackground;
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImpl::OnShouldBlockUpdateForMeteredNetwork, this,
          app_ids, AppClientInstallData(), AppInstallDataIndex(), priority,
          UpdateService::PolicySameVersionUpdate::kNotAllowed, state_update,
          base::BindOnce(
              [](Callback callback, scoped_refptr<PersistedData> persisted_data,
                 Result result) {
                if (result == Result::kSuccess) {
                  persisted_data->SetLastChecked(
                      base::Time::NowFromSystemTime());
                  VLOG(1) << "last_checked updated.";
                }
                std::move(callback).Run(result);
              },
              std::move(callback), persisted_data_)));
}

void UpdateServiceImpl::Install(const RegistrationRequest& registration,
                                const std::string& client_install_data,
                                const std::string& install_data_index,
                                Priority priority,
                                StateChangeCallback state_update,
                                Callback callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(registration.app_id, priority, true, policy)) {
    HandleUpdateDisabledByPolicy(registration.app_id, policy, true,
                                 state_update, std::move(callback));
    return;
  }
  if (!base::EqualsCaseInsensitiveASCII(registration.app_id, kUpdaterAppId)) {
    persisted_data_->SetHadApps();
  }

  const bool new_install =
      !persisted_data_->GetProductVersion(registration.app_id).IsValid();
  if (new_install) {
    // Pre-register the app if there is no registration for it. This app
    // registration is removed later if the app install encounters an error.
    persisted_data_->RegisterApp(registration);
  } else {
    // Update brand and ap.
    RegistrationRequest request;
    request.app_id = registration.app_id;
    request.brand_code = registration.brand_code;
    request.ap = registration.ap;
    persisted_data_->RegisterApp(request);
  }

  std::multimap<std::string, base::RepeatingClosure>::iterator pos =
      cancellation_callbacks_.emplace(registration.app_id, base::DoNothing());
  pos->second = update_client_->Install(
      registration.app_id,
      base::BindOnce(
          &GetComponents, config_, persisted_data_,
          AppClientInstallData(
              {std::make_pair(registration.app_id, client_install_data)}),
          AppInstallDataIndex(
              {std::make_pair(registration.app_id, install_data_index)}),
          priority,
          /*update_blocked=*/false, PolicySameVersionUpdate::kAllowed),
      MakeUpdateClientCrxStateChangeCallback(config_, persisted_data_,
                                             new_install, state_update),
      MakeUpdateClientCallback(std::move(callback))
          .Then(base::BindOnce(
              [](scoped_refptr<UpdateServiceImpl> self,
                 const std::multimap<std::string,
                                     base::RepeatingClosure>::iterator& pos) {
                self->cancellation_callbacks_.erase(pos);
              },
              base::WrapRefCounted(this), pos)));
}

void UpdateServiceImpl::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  auto range = cancellation_callbacks_.equal_range(app_id);
  base::ranges::for_each(range.first, range.second,
                         [](const auto& i) { i.second.Run(); });
}

void UpdateServiceImpl::RunInstaller(const std::string& app_id,
                                     const base::FilePath& installer_path,
                                     const std::string& install_args,
                                     const std::string& install_data,
                                     const std::string& install_settings,
                                     StateChangeCallback state_update,
                                     Callback callback) {
  VLOG(1) << __func__ << ": " << app_id << ": " << installer_path << ": "
          << install_args << ": " << install_data << ": " << install_settings;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(app_id, Priority::kForeground, true, policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, true, state_update,
                                 std::move(callback));
    return;
  }

  const base::Version pv = persisted_data_->GetProductVersion(app_id);
  AppInfo app_info(
      GetUpdaterScope(), app_id,
      pv.IsValid() ? persisted_data_->GetAP(app_id) : "",
      pv.IsValid() ? persisted_data_->GetBrandCode(app_id) : "", pv,
      pv.IsValid() ? persisted_data_->GetExistenceCheckerPath(app_id)
                   : base::FilePath());

  const base::Version installer_version([&install_settings]() -> std::string {
    std::unique_ptr<base::Value> install_settings_deserialized =
        JSONStringValueDeserializer(install_settings)
            .Deserialize(
                /*error_code=*/nullptr, /*error_message=*/nullptr);
    if (install_settings_deserialized) {
      const base::Value::Dict* install_settings_dict =
          install_settings_deserialized->GetIfDict();
      if (install_settings_dict) {
        const std::string* installer_version_value =
            install_settings_dict->FindString(kInstallerVersion);
        if (installer_version_value) {
          return *installer_version_value;
        }
      }
    }

    return {};
  }());

  // Create a task runner that:
  //   1) has SequencedTaskRunner::CurrentDefaultHandle set, to run
  //      `state_update` callback.
  //   2) may block, since `RunApplicationInstaller` blocks.
  //   3) has `base::WithBaseSyncPrimitives()`, since `RunApplicationInstaller`
  //      waits on process.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const AppInfo& app_info, const base::FilePath& installer_path,
             const std::string& install_args, const std::string& install_data,
             StateChangeCallback state_update, bool usage_stats_enabled) {
            base::ScopedTempDir temp_dir;
            if (!temp_dir.CreateUniqueTempDir()) {
              return InstallerResult(kErrorApplicationInstallerFailed,
                                     kErrorCreatingTempDir);
            }

            return RunApplicationInstaller(
                app_info, installer_path, install_args,
                WriteInstallerDataToTempFile(temp_dir.GetPath(), install_data),
                usage_stats_enabled, kWaitForAppInstaller,
                base::BindRepeating(
                    [](StateChangeCallback state_update,
                       const std::string& app_id, int progress) {
                      VLOG(4) << "Install progress: " << progress;
                      UpdateState state;
                      state.app_id = app_id;
                      state.state = UpdateState::State::kInstalling;
                      state.install_progress = progress;
                      state_update.Run(state);
                    },
                    state_update, app_info.app_id));
          },
          app_info, installer_path, install_args, install_data, state_update,
          persisted_data_->GetUsageStatsEnabled()),
      base::BindOnce(
          [](scoped_refptr<Configurator> config,
             scoped_refptr<PersistedData> persisted_data,
             scoped_refptr<update_client::UpdateClient> update_client,
             const base::Version& installer_version,
             StateChangeCallback state_update, const std::string& app_id,
             const std::string& ap, const std::string& brand, Callback callback,
             const InstallerResult& result) {
            // Final state update after installation completes.
            UpdateState state;
            state.app_id = app_id;
            state.state = result.error == 0 ? UpdateState::State::kUpdated
                                            : UpdateState::State::kUpdateError;

            if (result.error == 0 && installer_version.IsValid()) {
              persisted_data->SetProductVersion(app_id, installer_version);
              config->GetPrefService()->CommitPendingWrite();
            }

            // Handle the offline installer cases similar to the online cases,
            // and get the `error_code` from `original_error`.
            state.error_code =
                result.original_error ? result.original_error : result.error;
            state.extra_code1 = result.extended_error;
            state.installer_text = result.installer_text;
            state.installer_cmd_line = result.installer_cmd_line;
            state_update.Run(state);
            VLOG(1) << app_id
                    << " installation completed: " << state.error_code;

            // Send an install ping. In some environments the ping cannot be
            // sent, so do not wait for it to be sent before calling back the
            // client.
            update_client::CrxComponent install_data;
            install_data.ap = ap;
            install_data.app_id = app_id;
            install_data.brand = brand;
            install_data.requires_network_encryption = false;
            install_data.version = installer_version;
            update_client->SendInstallPing(install_data, result.error == 0,
                                           result.error, result.extended_error,
                                           base::DoNothing());

            std::move(callback).Run(result.error == 0 ? Result::kSuccess
                                                      : Result::kInstallFailed);
          },
          config_, persisted_data_, update_client_, installer_version,
          state_update, app_info.app_id, app_info.ap, app_info.brand,
          std::move(callback)));
}

bool UpdateServiceImpl::IsUpdateDisabledByPolicy(const std::string& app_id,
                                                 Priority priority,
                                                 bool is_install,
                                                 int& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  policy = kPolicyEnabled;

  if (is_install) {
    PolicyStatus<int> app_install_policy_status =
        config_->GetPolicyService()->GetPolicyForAppInstalls(app_id);
    if (app_install_policy_status) {
      policy = app_install_policy_status.policy();
    }
    return app_install_policy_status &&
           (policy == kPolicyDisabled || (config_->IsPerUserInstall() &&
                                          policy == kPolicyEnabledMachineOnly));
  } else {
    PolicyStatus<int> app_update_policy_status =
        config_->GetPolicyService()->GetPolicyForAppUpdates(app_id);
    if (app_update_policy_status) {
      policy = app_update_policy_status.policy();
    }
    return app_update_policy_status &&
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
    bool is_install,
    StateChangeCallback state_update,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateState update_state;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kUpdateError;
  update_state.error_category = UpdateService::ErrorCategory::kUpdateCheck;
  update_state.error_code =
      is_install ? GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY
      : policy != kPolicyAutomaticUpdatesOnly
          ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY
          : GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL;
  update_state.extra_code1 = 0;

  base::BindPostTask(main_task_runner_, state_update).Run(update_state);
  base::BindPostTask(main_task_runner_, std::move(callback))
      .Run(UpdateService::Result::kUpdateCheckFailed);
}

void UpdateServiceImpl::OnShouldBlockCheckForUpdateForMeteredNetwork(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::UpdateClient::CheckForUpdate, update_client_, app_id,
          base::BindOnce(&GetComponents, config_, persisted_data_,
                         AppClientInstallData(), AppInstallDataIndex(),
                         priority, update_blocked, policy_same_version_update),
          MakeUpdateClientCrxStateChangeCallback(config_, persisted_data_,
                                                 /*new_install=*/false,
                                                 state_update),
          priority == Priority::kForeground,
          MakeUpdateClientCallback(std::move(callback))));
}

void UpdateServiceImpl::OnShouldBlockUpdateForMeteredNetwork(
    const std::vector<std::string>& app_ids,
    const AppClientInstallData& app_client_install_data,
    const AppInstallDataIndex& app_install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::UpdateClient::Update, update_client_, app_ids,
          base::BindOnce(&GetComponents, config_, persisted_data_,
                         app_client_install_data, app_install_data_index,
                         priority, update_blocked, policy_same_version_update),
          MakeUpdateClientCrxStateChangeCallback(config_, persisted_data_,
                                                 /*new_install=*/false,
                                                 state_update),
          priority == Priority::kForeground,
          MakeUpdateClientCallback(std::move(callback))));
}

void UpdateServiceImpl::OnShouldBlockForceInstallForMeteredNetwork(
    const std::vector<std::string>& app_ids,
    const AppClientInstallData& app_client_install_data,
    const AppInstallDataIndex& app_install_data_index,
    PolicySameVersionUpdate policy_same_version_update,
    StateChangeCallback state_update,
    Callback callback,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The result from Install is only used for logging. Thus, arbitrarily pick
  // the first non-success result to propagate.
  auto barrier_callback = base::BarrierCallback<Result>(
      app_ids.size(),
      base::BindOnce([](const std::vector<Result>& results) {
        auto error_it = base::ranges::find_if(
            results, [](Result result) { return result != Result::kSuccess; });
        return error_it == std::end(results) ? Result::kSuccess : *error_it;
      }).Then(std::move(callback)));

  for (const std::string& id : app_ids) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&update_client::UpdateClient::Install),
            update_client_, id,
            base::BindOnce(&GetComponents, config_, persisted_data_,
                           app_client_install_data, app_install_data_index,
                           Priority::kBackground, update_blocked,
                           policy_same_version_update),
            MakeUpdateClientCrxStateChangeCallback(config_, persisted_data_,
                                                   /*new_install=*/false,
                                                   state_update),
            MakeUpdateClientCallback(barrier_callback)));
  }
}

UpdateServiceImpl::~UpdateServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
