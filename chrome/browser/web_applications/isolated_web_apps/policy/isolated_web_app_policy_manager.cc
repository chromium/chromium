// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_orphaned_isolated_web_apps_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

constexpr net::BackoffEntry::Policy kInstallRetryBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = 60 * 1000,
    .multiply_factor = 2.0,
    .jitter_factor = 0.0,
    .maximum_backoff_ms = 5 * 60 * 60 * 1000,
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false,
};

constexpr int kIsolatedWebAppForceInstallMaxRetryTreshold = 2;
constexpr base::TimeDelta kIsolatedWebAppForceInstallEmergencyDelay =
    base::Hours(5);

// Remove the install source from the already installed app, possibly
// uninstalling it if no more sources are remaining.
struct AppActionRemoveInstallSource {
  explicit AppActionRemoveInstallSource(WebAppManagement::Type source)
      : source(source) {}

  base::Value::Dict GetDebugValue() const {
    return base::Value::Dict()
        .Set("type", "AppActionRemoveInstallSource")
        .Set("source", base::ToString(source));
  }

  WebAppManagement::Type source;
};

// Install the app. Will error if it is already installed.
struct AppActionInstall {
  explicit AppActionInstall(IsolatedWebAppExternalInstallOptions options)
      : options(std::move(options)) {}

  base::Value::Dict GetDebugValue() const {
    base::Value::Dict debug_value =
        base::Value::Dict()
            .Set("type", "AppActionInstall")
            .Set("update_manifest_url",
                 options.update_manifest_url().possibly_invalid_spec())
            .Set("update_channel", options.update_channel().ToString());
    if (options.pinned_version()) {
      debug_value.Set("pinned_version", options.pinned_version()->GetString());
    }
    return debug_value;
  }

  IsolatedWebAppExternalInstallOptions options;
};

using AppAction = std::variant<AppActionRemoveInstallSource, AppActionInstall>;
using AppActions = base::flat_map<web_package::SignedWebBundleId, AppAction>;

#if BUILDFLAG(IS_CHROMEOS)
bool g_first_policy_processing_delay_recorded = false;

// Records the elapsed time between the first user sign-in and the beginning
// of the actual processing of the IsolatedWebAppInstallForceList policy with
// a lock. Called once per the lifetime of the browser process (we don't need
// to track this more often).
void MaybeRecordFirstPolicyProcessingDelay(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (g_first_policy_processing_delay_recorded ||
      !prefs->HasPrefPath(ash::prefs::kAshLoginSessionStartedTime)) {
    // `ash::prefs::kAshLoginSessionStartedTime` is not defined in tests or
    // linux-chromeos builds.
    return;
  }
  g_first_policy_processing_delay_recorded = true;

  base::UmaHistogramCustomTimes(
      "WebApp.Isolated.FirstPolicyProcessingDelay",
      base::Time::Now() -
          prefs->GetTime(ash::prefs::kAshLoginSessionStartedTime),
      /*min=*/base::Milliseconds(100),
      /*max=*/base::Seconds(20), /*buckets=*/50);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::RepeatingCallback<void(web_package::SignedWebBundleId,
                             IwaInstaller::Result)>&
GetOnInstallTaskCompletedCallbackForTesting() {
  static base::NoDestructor<base::RepeatingCallback<void(
      web_package::SignedWebBundleId, IwaInstaller::Result)>>
      kCallback;
  return *kCallback;
}

bool IsOnDemandComponentUpdateFeatureEnabled() {
  return base::FeatureList::IsEnabled(kIwaPolicyManagerOnDemandComponentUpdate);
}

// If there are potential IWAs to be processed, will run `callback` after we've
// attempted to fetch the latest component data. Otherwise runs it immediately.
void OnComponentDataReady(PrefService* prefs, base::OnceClosure callback) {
  if (!IsOnDemandComponentUpdateFeatureEnabled() ||
      prefs->GetList(prefs::kIsolatedWebAppInstallForceList).empty()) {
    // We don't need the latest component data.
    std::move(callback).Run();
    return;
  }

  IwaKeyDistributionInfoProvider::GetInstance()
      ->OnMaybeDownloadedComponentDataReady()
      .Post(FROM_HERE, std::move(callback));
}

}  // namespace

BASE_FEATURE(kIwaPolicyManagerOnDemandComponentUpdate,
             "IwaPolicyManagerOnDemandComponentUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// static
void IsolatedWebAppPolicyManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kIsolatedWebAppInstallForceList);
  registry->RegisterIntegerPref(
      prefs::kIsolatedWebAppPendingInitializationCount, 0);
}

// static
void IsolatedWebAppPolicyManager::SetOnInstallTaskCompletedCallbackForTesting(
    base::RepeatingCallback<void(web_package::SignedWebBundleId,
                                 IwaInstaller::Result)> callback) {
  CHECK_IS_TEST();
  GetOnInstallTaskCompletedCallbackForTesting() = callback;
}

// static
std::vector<IsolatedWebAppExternalInstallOptions>
IsolatedWebAppPolicyManager::GetIwaInstallForceList(const Profile& profile) {
  std::vector<IsolatedWebAppExternalInstallOptions> iwas_in_policy;

  for (const auto& policy_entry :
       profile.GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList)) {
    const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
        options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
            policy_entry);
    if (options.has_value()) {
      iwas_in_policy.push_back(options.value());
    } else {
      LOG(ERROR) << "Could not interpret IWA force-install policy: "
                 << options.error();
    }
  }

  return iwas_in_policy;
}

IsolatedWebAppPolicyManager::IsolatedWebAppPolicyManager(Profile* profile)
    : profile_(profile),
      install_retry_backoff_entry_(&kInstallRetryBackoffPolicy) {}

IsolatedWebAppPolicyManager::~IsolatedWebAppPolicyManager() = default;

void IsolatedWebAppPolicyManager::Start(base::OnceClosure on_started_callback) {
  if (!content::AreIsolatedWebAppsEnabled(profile_)) {
    std::move(on_started_callback).Run();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsManagedGuestSession() &&
      !base::FeatureList::IsEnabled(
          features::kIsolatedWebAppManagedGuestSessionInstall)) {
    std::move(on_started_callback).Run();
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  auto debug_log =
      base::Value::Dict()
          .Set("start_time",
               base::TimeFormatFriendlyDateAndTime(base::Time::Now()))
          .Set("info", "IsolatedWebAppPolicyManager::Start()");
  IwaKeyDistributionInfoProvider::GetInstance()->WriteComponentMetadata(
      debug_log);
  process_logs_.AppendCompletedStep(std::move(debug_log));

  OnComponentDataReady(profile_->GetPrefs(),
                       base::BindOnce(&IsolatedWebAppPolicyManager::StartImpl,
                                      weak_ptr_factory_.GetWeakPtr()));

  std::move(on_started_callback).Run();
}

void IsolatedWebAppPolicyManager::StartImpl() {
  const int pending_inits_count = GetPendingInitCount();
  SetPendingInitCount(pending_inits_count + 1);
  if (pending_inits_count <= kIsolatedWebAppForceInstallMaxRetryTreshold) {
    ConfigureObserversOnSessionStart();
    CleanupAndProcessPolicyOnSessionStart();
  } else {
    auto configure_observers = base::BindOnce(
        &IsolatedWebAppPolicyManager::ConfigureObserversOnSessionStart,
        weak_ptr_factory_.GetWeakPtr());
    auto cleanup_and_process_policy = base::BindOnce(
        &IsolatedWebAppPolicyManager::CleanupAndProcessPolicyOnSessionStart,
        weak_ptr_factory_.GetWeakPtr());

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        std::move(configure_observers)
            .Then(std::move(cleanup_and_process_policy)),
        kIsolatedWebAppForceInstallEmergencyDelay);
  }
}

void IsolatedWebAppPolicyManager::SetProvider(base::PassKey<WebAppProvider>,
                                              WebAppProvider& provider) {
  provider_ = &provider;
}

base::Value IsolatedWebAppPolicyManager::GetDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set("policy_is_being_processed",
               policy_is_being_processed_
                   ? base::Value(current_process_log_.Clone())
                   : base::Value(false))
          .Set("policy_reprocessing_is_queued", reprocess_policy_needed_)
          .Set("process_logs", process_logs_.ToDebugValue()));
}

void IsolatedWebAppPolicyManager::ProcessPolicy() {
  CHECK(provider_);
  base::Value::Dict process_log;
  process_log.Set("start_time",
                  base::TimeFormatFriendlyDateAndTime(base::Time::Now()));

  // Ensure that only one policy resolution can happen at one time.
  if (policy_is_being_processed_) {
    reprocess_policy_needed_ = true;
    process_log.Set("warning",
                    "policy is already being processed - waiting for "
                    "processing to finish.");
    process_logs_.AppendCompletedStep(std::move(process_log));
    return;
  }

  policy_is_being_processed_ = true;
  current_process_log_ = std::move(process_log);

  provider_->scheduler().ScheduleCallback<AllAppsLock>(
      "IsolatedWebAppPolicyManager::ProcessPolicy", AllAppsLockDescription(),
      base::BindOnce(&IsolatedWebAppPolicyManager::DoProcessPolicy,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/
      initial_policy_processing_finished_cb_
          ? std::move(initial_policy_processing_finished_cb_)
          : base::DoNothing());
}

void IsolatedWebAppPolicyManager::ConfigureObserversOnSessionStart() {
  key_distribution_info_observation_.Observe(
      IwaKeyDistributionInfoProvider::GetInstance());

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIsolatedWebAppInstallForceList,
      base::BindRepeating(&IsolatedWebAppPolicyManager::OnPolicyChanged,
                          base::Unretained(this)));
}

void IsolatedWebAppPolicyManager::CleanupAndProcessPolicyOnSessionStart() {
  base::RepeatingClosure finished_barrier = base::BarrierClosure(
      /*num_closures=*/2u,
      base::BindOnce(&IsolatedWebAppPolicyManager::SetPendingInitCount,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*pending_count=*/0));

  initial_policy_processing_finished_cb_ = finished_barrier;
  CleanupOrphanedBundles(/*finished_closure=*/finished_barrier);
  ProcessPolicy();
}

int IsolatedWebAppPolicyManager::GetPendingInitCount() {
  PrefService& pref_service = CHECK_DEREF(profile_->GetPrefs());
  if (!pref_service.HasPrefPath(
          prefs::kIsolatedWebAppPendingInitializationCount)) {
    pref_service.SetInteger(prefs::kIsolatedWebAppPendingInitializationCount,
                            0);
  }
  return CHECK_DEREF(profile_->GetPrefs())
      .GetUserPrefValue(prefs::kIsolatedWebAppPendingInitializationCount)
      ->GetIfInt()
      .value_or(0);
}

void IsolatedWebAppPolicyManager::SetPendingInitCount(int pending_count) {
  profile_->GetPrefs()->SetInteger(
      prefs::kIsolatedWebAppPendingInitializationCount, pending_count);
}

void IsolatedWebAppPolicyManager::DoProcessPolicy(
    AllAppsLock& lock,
    base::Value::Dict& debug_info) {
#if BUILDFLAG(IS_CHROMEOS)
  MaybeRecordFirstPolicyProcessingDelay(profile_);
#endif  // BUILDFLAG(IS_CHROMEOS)

  IwaKeyDistributionInfoProvider::GetInstance()->WriteComponentMetadata(
      debug_info);

  CHECK(provider_);
  CHECK(install_tasks_.empty());

  std::vector<IsolatedWebAppExternalInstallOptions> apps_in_policy =
      GetIwaInstallForceList(*profile_);
  base::flat_map<web_package::SignedWebBundleId,
                 std::reference_wrapper<const WebApp>>
      installed_iwas = GetInstalledIwas(lock.registrar());

  AppActions app_actions;
  size_t number_of_install_tasks = 0;
  for (const IsolatedWebAppExternalInstallOptions& install_options :
       apps_in_policy) {
    std::reference_wrapper<const WebApp>* maybe_installed_app =
        base::FindOrNull(installed_iwas, install_options.web_bundle_id());
    if (!maybe_installed_app) {
      app_actions.emplace(install_options.web_bundle_id(),
                          AppActionInstall(install_options));
      ++number_of_install_tasks;
      continue;
    }
    const WebApp& installed_app = maybe_installed_app->get();

    static_assert(std::ranges::is_sorted(
        std::vector{WebAppManagement::Type::kIwaShimlessRma,
                    // Add further higher priority IWA sources here and make
                    // sure that the `case` statements below are sorted
                    // appropriately...
                    WebAppManagement::Type::kIwaPolicy,
                    // Add further lower priority IWA sources here and make sure
                    // that the `case` statements below are sorted
                    // appropriately...
                    WebAppManagement::Type::kIwaUserInstalled}));
    switch (installed_app.GetHighestPrioritySource()) {
      case WebAppManagement::kSystem:
      case WebAppManagement::kKiosk:
      case WebAppManagement::kPolicy:
      case WebAppManagement::kOem:
      case WebAppManagement::kSubApp:
      case WebAppManagement::kWebAppStore:
      case WebAppManagement::kOneDriveIntegration:
      case WebAppManagement::kSync:
      case WebAppManagement::kUserInstalled:
      case WebAppManagement::kApsDefault:
      case WebAppManagement::kDefault: {
        NOTREACHED();
      }

      // Do not touch installed apps if they are managed by a higher priority (=
      // lower numerical value) or by the IWA policy source.
      case WebAppManagement::kIwaShimlessRma:
      case WebAppManagement::kIwaPolicy:
        break;

      case WebAppManagement::kIwaUserInstalled:
        if (!CHECK_DEREF(IwaKeyDistributionInfoProvider::GetInstance())
                 .IsManagedInstallPermitted(
                     install_options.web_bundle_id().id())) {
          DLOG(WARNING) << "The IWA " << install_options.web_bundle_id()
                        << " is not in the managed allowlist. ";
          continue;
        }
        // Always fully uninstall user installed apps (dev mode and regular)
        // if they're to be replaced by a policy installation.
        app_actions.emplace(
            install_options.web_bundle_id(),
            AppActionRemoveInstallSource(WebAppManagement::kIwaUserInstalled));

        // We need to reprocess the policy immediately after, so that the then
        // uninstalled app is re-installed.
        reprocess_policy_needed_ = true;
        break;
    }
  }

  for (const auto& [web_bundle_id, _] : installed_iwas) {
    if (!base::Contains(apps_in_policy, web_bundle_id,
                        &IsolatedWebAppExternalInstallOptions::web_bundle_id)) {
      app_actions.emplace(web_bundle_id, AppActionRemoveInstallSource(
                                             WebAppManagement::kIwaPolicy));
    }
  }

  debug_info.Set("apps_in_policy",
                 base::ToValueList(apps_in_policy, [](const auto& options) {
                   return base::ToString(options.web_bundle_id());
                 }));
  debug_info.Set(
      "installed_iwas",
      base::ToValueList(installed_iwas, [](const auto& installed_iwa) {
        const auto& [web_bundle_id, _] = installed_iwa;
        return base::ToString(web_bundle_id);
      }));
  debug_info.Set(
      "app_actions", base::ToValueList(app_actions, [](const auto& entry) {
        const auto& [web_bundle_id, app_action] = entry;
        return base::Value::Dict()
            .Set("web_bundle_id", base::ToString(web_bundle_id))
            .Set("action", std::visit(base::Overloaded{[](const auto& action) {
                                        return action.GetDebugValue();
                                      }},
                                      app_action));
      }));
  current_process_log_.Merge(debug_info.Clone());

  auto action_done_callback = base::BarrierClosure(
      app_actions.size(),
      // Always asynchronously exit this method so that `lock` is released
      // before the next method is called.
      base::BindOnce(
          [](base::WeakPtr<IsolatedWebAppPolicyManager> weak_ptr) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(&IsolatedWebAppPolicyManager::OnPolicyProcessed,
                               std::move(weak_ptr)));
          },
          weak_ptr_factory_.GetWeakPtr()));
  auto install_task_done_callback = base::BarrierCallback<IwaInstaller::Result>(
      number_of_install_tasks,
      base::BindOnce(&IsolatedWebAppPolicyManager::OnAllInstallTasksCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const auto& [web_bundle_id, app_action] : app_actions) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    std::visit(
        base::Overloaded{
            [&](const AppActionRemoveInstallSource& action) {
              auto callback = base::BindOnce(&IsolatedWebAppPolicyManager::
                                                 LogRemoveInstallSourceResult,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             web_bundle_id, action.source)
                                  .Then(action_done_callback);

              provider_->scheduler().RemoveInstallManagementMaybeUninstall(
                  url_info.app_id(), action.source,
                  webapps::WebappUninstallSource::kIwaEnterprisePolicy,
                  std::move(callback));
            },
            [&](const AppActionInstall& action) {
              auto weak_ptr = weak_ptr_factory_.GetWeakPtr();

              auto callback =
                  base::BindOnce(
                      &IsolatedWebAppPolicyManager::OnInstallTaskCompleted,
                      weak_ptr, web_bundle_id, install_task_done_callback)
                      .Then(base::BindOnce(&IsolatedWebAppPolicyManager::
                                               MaybeStartNextInstallTask,
                                           weak_ptr))
                      .Then(action_done_callback);

              auto installer = IwaInstallerFactory::Create(
                  action.options, IwaInstaller::InstallSourceType::kPolicy,
                  profile_->GetURLLoaderFactory(),
                  *current_process_log_.EnsureDict("install_progress")
                       ->EnsureList(base::ToString(web_bundle_id)),
                  provider_, std::move(callback));
              install_tasks_.push(std::move(installer));
            },
        },
        app_action);
  }

  MaybeStartNextInstallTask();
}

void IsolatedWebAppPolicyManager::LogRemoveInstallSourceResult(
    web_package::SignedWebBundleId web_bundle_id,
    WebAppManagement::Type source,
    webapps::UninstallResultCode uninstall_code) {
  if (!webapps::UninstallSucceeded(uninstall_code)) {
    DLOG(WARNING) << "Could not remove install source " << source
                  << " from IWA " << web_bundle_id
                  << ". Error: " << uninstall_code;
  }
  current_process_log_.EnsureDict("remove_install_source_results")
      ->EnsureDict(base::ToString(web_bundle_id))
      ->Set(base::ToString(source), base::ToString(uninstall_code));
}

void IsolatedWebAppPolicyManager::OnInstallTaskCompleted(
    web_package::SignedWebBundleId web_bundle_id,
    base::RepeatingCallback<void(IwaInstaller::Result)> callback,
    IwaInstaller::Result install_result) {
  // Remove the completed task from the queue.
  install_tasks_.pop();

  if (install_result.type() != IwaInstallerResultType::kSuccess) {
    DLOG(WARNING) << "Could not force-install IWA " << web_bundle_id
                  << ". Error: " << install_result.ToDebugValue();
  }
  current_process_log_.EnsureDict("install_results")
      ->Set(base::ToString(web_bundle_id), install_result.ToDebugValue());

  if (auto& testing_callback = GetOnInstallTaskCompletedCallbackForTesting()) {
    CHECK_IS_TEST();
    testing_callback.Run(web_bundle_id, install_result);
  }

  callback.Run(install_result);
}

void IsolatedWebAppPolicyManager::OnAllInstallTasksCompleted(
    std::vector<IwaInstaller::Result> install_results) {
  if (install_results.empty()) {
    return;
  }

  const bool any_app_needs_retry = std::ranges::any_of(
      install_results, [](const IwaInstaller::Result& result) {
        // The component update (allowlist change) triggers reprocessing
        // policy, so do not retry when app rejected because of allowlist.
        return result.type() != IwaInstallerResultType::kSuccess &&
               result.type() != IwaInstallerResultType::kErrorAppNotInAllowlist;
      });

  if (any_app_needs_retry) {
    install_retry_backoff_entry_.InformOfRequest(/*succeeded=*/false);
    CleanupOrphanedBundles(/*finished_closure=*/base::DoNothing());
  } else {
    install_retry_backoff_entry_.Reset();
    return;
  }

  // No retry needed if it was already scheduled --> Exit early.
  if (reprocess_policy_needed_) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IsolatedWebAppPolicyManager::ProcessPolicy,
                     weak_ptr_factory_.GetWeakPtr()),
      install_retry_backoff_entry_.GetTimeUntilRelease());
}

void IsolatedWebAppPolicyManager::MaybeStartNextInstallTask() {
  if (!install_tasks_.empty()) {
    install_tasks_.front()->Start();
  }
}

void IsolatedWebAppPolicyManager::OnPolicyProcessed() {
  process_logs_.AppendCompletedStep(
      std::exchange(current_process_log_, base::Value::Dict()));

  policy_is_being_processed_ = false;

  if (reprocess_policy_needed_) {
    reprocess_policy_needed_ = false;
    ProcessPolicy();
  }
}

void IsolatedWebAppPolicyManager::CleanupOrphanedBundles(
    base::OnceClosure finished_closure) {
  provider_->scheduler().CleanupOrphanedIsolatedApps(
      base::IgnoreArgs<
          base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                         CleanupOrphanedIsolatedWebAppsCommandError>>(
          std::move(finished_closure)));
}

void IsolatedWebAppPolicyManager::OnPolicyChanged() {
  OnComponentDataReady(
      profile_->GetPrefs(),
      base::BindOnce(&IsolatedWebAppPolicyManager::ProcessPolicy,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnComponentUpdateSuccess(
    const base::Version& version,
    bool is_preloaded) {
  ProcessPolicy();
}

IsolatedWebAppPolicyManager::ProcessLogs::ProcessLogs() = default;
IsolatedWebAppPolicyManager::ProcessLogs::~ProcessLogs() = default;

void IsolatedWebAppPolicyManager::ProcessLogs::AppendCompletedStep(
    base::Value::Dict log) {
  log.Set("end_time", base::TimeFormatFriendlyDateAndTime(base::Time::Now()));

  // Keep only the most recent `kMaxEntries`.
  logs_.emplace_front(std::move(log));
  if (logs_.size() > kMaxEntries) {
    logs_.pop_back();
  }
}

base::Value IsolatedWebAppPolicyManager::ProcessLogs::ToDebugValue() const {
  return base::Value(base::ToValueList(logs_, &base::Value::Dict::Clone));
}

}  // namespace web_app
