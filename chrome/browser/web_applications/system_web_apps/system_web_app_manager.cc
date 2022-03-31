// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/ash/web_applications/camera_app/camera_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/camera_app/chrome_camera_app_ui_constants.h"
#include "chrome/browser/ash/web_applications/connectivity_diagnostics_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/crosh_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/diagnostics_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/eche_app_info.h"
#include "chrome/browser/ash/web_applications/file_manager_web_app_info.h"
#include "chrome/browser/ash/web_applications/firmware_update_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/help_app/help_app_web_app_info.h"
#include "chrome/browser/ash/web_applications/media_app/media_web_app_info.h"
#include "chrome/browser/ash/web_applications/os_feedback_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/os_flags_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/os_settings_web_app_info.h"
#include "chrome/browser/ash/web_applications/os_url_handler_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_system_app_delegate.h"
#include "chrome/browser/ash/web_applications/print_management_web_app_info.h"
#include "chrome/browser/ash/web_applications/projector_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/scanning_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/shimless_rma_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/shortcut_customization_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/terminal_system_web_app_info.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"  // nogncheck
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ash/web_applications/demo_mode_web_app_info.h"
#include "chrome/browser/ash/web_applications/sample_system_web_app_info.h"
#endif  // !defined(OFFICIAL_BUILD)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

// Number of attempts to install a given version & locale of the SWAs before
// bailing out.
const int kInstallFailureAttempts = 3;

SystemAppDelegateMap CreateSystemWebApps(Profile* profile) {
  std::vector<std::unique_ptr<SystemWebAppDelegate>> info_vec;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1051229): Currently unused, will be hooked up
  // post-migration. We're making delegates for everything, and will then use
  // them in place of SystemAppInfos.
  info_vec.push_back(std::make_unique<CameraSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<DiagnosticsSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<OSSettingsSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<CroshSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<TerminalSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ash::HelpAppSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<MediaSystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<PrintManagementSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ScanningSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ShimlessRMASystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<ConnectivityDiagnosticsSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<EcheSystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<PersonalizationSystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<ShortcutCustomizationSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<OSFeedbackAppDelegate>(profile));
  info_vec.push_back(std::make_unique<FileManagerSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ProjectorSystemWebAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<OsUrlHandlerSystemWebAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<FirmwareUpdateSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<OsFlagsSystemWebAppDelegate>(profile));

#if !defined(OFFICIAL_BUILD)
  info_vec.push_back(std::make_unique<DemoModeSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<SampleSystemAppDelegate>(profile));
#endif  // !defined(OFFICIAL_BUILD)

  SystemAppDelegateMap delegate_map;
  for (auto& info : info_vec) {
    if (info->IsAppEnabled() ||
        base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps)) {
      delegate_map.emplace(info->GetType(), std::move(info));
    }
  }
  return delegate_map;
#else
  return {};
#endif
}

bool HasSystemWebAppScheme(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeUIUntrustedScheme);
}

ExternalInstallOptions CreateInstallOptionsForSystemApp(
    const SystemAppType app_type,

    const SystemWebAppDelegate& delegate,
    bool force_update,

    bool is_disabled) {
  DCHECK(delegate.GetInstallUrl().scheme() == content::kChromeUIScheme ||
         delegate.GetInstallUrl().scheme() ==
             content::kChromeUIUntrustedScheme);

  ExternalInstallOptions install_options(
      delegate.GetInstallUrl(), DisplayMode::kStandalone,
      ExternalInstallSource::kSystemInstalled);
  install_options.only_use_app_info_factory = true;
  // This can be Unretained because it's referring to the delegate owning this
  // factory method. The lifetime of that is the same as the
  // SystemWebAppManager.
  install_options.app_info_factory = base::BindRepeating(
      &SystemWebAppDelegate::GetWebAppInfo, base::Unretained(&delegate));
  install_options.add_to_applications_menu = delegate.ShouldShowInLauncher();
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.add_to_search = delegate.ShouldShowInSearch();
  install_options.add_to_management = false;
  install_options.is_disabled = is_disabled;
  install_options.bypass_service_worker_check = true;
  install_options.force_reinstall = force_update;
  install_options.uninstall_and_replace =
      delegate.GetAppIdsToUninstallAndReplace();
  install_options.system_app_type = app_type;
  install_options.handles_file_open_intents =
      delegate.ShouldHandleFileOpenIntents();

  const auto& search_terms = delegate.GetAdditionalSearchTerms();
  std::transform(search_terms.begin(), search_terms.end(),
                 std::back_inserter(install_options.additional_search_terms),
                 [](int term) { return l10n_util::GetStringUTF8(term); });
  return install_options;
}

}  // namespace

// static
const char SystemWebAppManager::kInstallResultHistogramName[];
const char SystemWebAppManager::kInstallDurationHistogramName[];

SystemWebAppManager::SystemWebAppManager(Profile* profile)
    : profile_(profile),
      on_apps_synchronized_(new base::OneShotEvent()),
      on_tasks_started_(new base::OneShotEvent()),
      install_result_per_profile_histogram_name_(
          std::string(kInstallResultHistogramName) + ".Profiles." +
          GetProfileCategoryForLogging(profile)),
      pref_service_(profile_->GetPrefs()) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    // Always update in tests.
    update_policy_ = UpdatePolicy::kAlwaysUpdate;

    // Populate with real system apps if the test asks for it.
    if (base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps))
      system_app_delegates_ = CreateSystemWebApps(profile_);

    return;
  }

#if defined(OFFICIAL_BUILD)
  // Official builds should trigger updates whenever the version number changes.
  update_policy_ = UpdatePolicy::kOnVersionChange;
#else
  // Dev builds should update every launch.
  update_policy_ = UpdatePolicy::kAlwaysUpdate;
#endif

  system_app_delegates_ = CreateSystemWebApps(profile_);
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::StopBackgroundTasks() {
  for (auto& task : tasks_) {
    task->StopTask();
  }
}

bool SystemWebAppManager::IsAppEnabled(SystemAppType type) {
  if (base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps))
    return true;

  auto it = system_app_delegates_.find(type);
  if (it == system_app_delegates_.end())
    return false;

  return it->second->IsAppEnabled();
}

void SystemWebAppManager::Shutdown() {
  shutting_down_ = true;
  StopBackgroundTasks();
}

void SystemWebAppManager::SetSubsystems(
    ExternallyManagedAppManager* externally_managed_app_manager,
    WebAppRegistrar* registrar,
    WebAppSyncBridge* sync_bridge,
    WebAppUiManager* ui_manager,
    WebAppPolicyManager* web_app_policy_manager) {
  externally_managed_app_manager_ = externally_managed_app_manager;
  registrar_ = registrar;
  sync_bridge_ = sync_bridge;
  ui_manager_ = ui_manager;
  web_app_policy_manager_ = web_app_policy_manager;
}

void SystemWebAppManager::Start() {
  const base::TimeTicks install_start_time = base::TimeTicks::Now();

#if DCHECK_IS_ON()
  // Check Origin Trials are defined correctly.
  for (const auto& type_and_app_info : system_app_delegates_) {
    for (const auto& origin_to_trial_names :
         type_and_app_info.second->GetEnabledOriginTrials()) {
      // Only allow force enabled origin trials on chrome:// and
      // chrome-untrusted:// URLs.
      const auto& scheme = origin_to_trial_names.first.scheme();
      DCHECK(scheme == content::kChromeUIScheme ||
             scheme == content::kChromeUIUntrustedScheme);
      // TODO(https://crbug.com/1043843): Find some ways to validate supplied
      // origin trial names. Ideally, construct them from some static const
      // char*.
    }
  }
#endif  // DCHECK_IS_ON()

  std::vector<ExternalInstallOptions> install_options_list;
  const bool should_force_install_apps = ShouldForceInstallApps();
  if (should_force_install_apps) {
    UpdateLastAttemptedInfo();
  }

  const auto& disabled_system_apps =
      web_app_policy_manager_->GetDisabledSystemWebApps();

  for (const auto& app : system_app_delegates_) {
    install_options_list.push_back(CreateInstallOptionsForSystemApp(
        app.first, *app.second, should_force_install_apps,
        base::Contains(disabled_system_apps, app.first)));
  }

  const bool exceeded_retries = CheckAndIncrementRetryAttempts();
  if (exceeded_retries) {
    LOG(ERROR)
        << "Exceeded SWA install retry attempts.  Skipping installation, will "
           "retry on next OS update or when locale changes.";
    return;
  }

  externally_managed_app_manager_->SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kSystemInstalled,
      base::BindOnce(&SystemWebAppManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr(), should_force_install_apps,
                     install_start_time));
}

void SystemWebAppManager::InstallSystemAppsForTesting() {
  on_apps_synchronized_ = std::make_unique<base::OneShotEvent>();
  on_tasks_started_ = std::make_unique<base::OneShotEvent>();
  system_app_delegates_ = CreateSystemWebApps(profile_);
  Start();

  // Wait for the System Web Apps to install.
  base::RunLoop run_loop;
  on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

const base::flat_map<SystemAppType, std::unique_ptr<SystemWebAppDelegate>>&
SystemWebAppManager::GetRegisteredSystemAppsForTesting() const {
  return system_app_delegates_;
}

absl::optional<AppId> SystemWebAppManager::GetAppIdForSystemApp(
    SystemAppType id) const {
  auto app_url_it = system_app_delegates_.find(id);
  if (app_url_it == system_app_delegates_.end())
    return absl::optional<AppId>();

  return registrar_->LookupExternalAppId(app_url_it->second->GetInstallUrl());
}

absl::optional<SystemAppType> SystemWebAppManager::GetSystemAppTypeForAppId(
    const AppId& app_id) const {
  const WebApp* web_app = registrar_->GetAppById(app_id);
  if (!web_app || !web_app->client_data().system_web_app_data.has_value()) {
    return absl::nullopt;
  }

  // The registered system apps can change from previous runs (e.g. flipping a
  // SWA flag). The registry isn't up-to-date until SWA finishes installing, so
  // we could have a invalid type (for current session) during SWA install.
  //
  // This check ensures we return a type that is safe for other methods (avoids
  // crashing when looking up that type).
  SystemAppType proto_type =
      web_app->client_data().system_web_app_data->system_app_type;
  if (system_app_delegates_.contains(proto_type)) {
    return proto_type;
  }

  return absl::nullopt;
}

const SystemWebAppDelegate* SystemWebAppManager::GetSystemApp(
    SystemAppType type) const {
  auto it = system_app_delegates_.find(type);
  if (it == system_app_delegates_.end())
    return nullptr;

  return it->second.get();
}

std::vector<AppId> SystemWebAppManager::GetAppIds() const {
  std::vector<AppId> app_ids;
  for (const auto& app_type_to_app_info : system_app_delegates_) {
    absl::optional<AppId> app_id =
        GetAppIdForSystemApp(app_type_to_app_info.first);
    if (app_id.has_value()) {
      app_ids.push_back(app_id.value());
    }
  }
  return app_ids;
}

bool SystemWebAppManager::IsSystemWebApp(const AppId& app_id) const {
  return GetSystemAppTypeForAppId(app_id).has_value();
}

const std::vector<std::string>* SystemWebAppManager::GetEnabledOriginTrials(
    const SystemWebAppDelegate* system_app,
    const GURL& url) const {
  DCHECK(system_app);
  const auto& origin_to_origin_trials = system_app->GetEnabledOriginTrials();
  auto iter_trials = origin_to_origin_trials.find(url::Origin::Create(url));

  if (iter_trials == origin_to_origin_trials.end())
    return nullptr;

  return &iter_trials->second;
}

void SystemWebAppManager::OnReadyToCommitNavigation(
    const AppId& app_id,
    content::NavigationHandle* navigation_handle) {
  // No need to setup origin trials for intra-document navigation.
  if (navigation_handle->IsSameDocument())
    return;

  const absl::optional<SystemAppType> type = GetSystemAppTypeForAppId(app_id);
  // This function should only be called when an navigation happens inside a
  // System App. So the |app_id| should always have a valid associated System
  // App type.
  DCHECK(type.has_value());
  auto* system_app = GetSystemApp(type.value());
  DCHECK(system_app);

  const std::vector<std::string>* trials =
      GetEnabledOriginTrials(system_app, navigation_handle->GetURL());
  if (trials) {
    navigation_handle->ForceEnableOriginTrials(*trials);
  }
}

absl::optional<SystemAppType> SystemWebAppManager::GetCapturingSystemAppForURL(
    const GURL& url) const {
  if (!HasSystemWebAppScheme(url))
    return absl::nullopt;

  absl::optional<AppId> app_id = registrar_->FindAppWithUrlInScope(url);
  if (!app_id.has_value())
    return absl::nullopt;

  absl::optional<SystemAppType> type = GetSystemAppTypeForAppId(app_id.value());
  if (!type.has_value())
    return absl::nullopt;

  const auto it = system_app_delegates_.find(type);
  if (it == system_app_delegates_.end())
    return absl::nullopt;

  if (!it->second->ShouldCaptureNavigations())
    return absl::nullopt;

    // TODO(crbug://1051229): Expand ShouldCaptureNavigation to take a GURL, and
    // move this into the camera one.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (type == SystemAppType::CAMERA) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    replacements.ClearRef();
    if (url.ReplaceComponents(replacements).spec() !=
        ash::kChromeUICameraAppMainURL)
      return absl::nullopt;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return type;
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, std::unique_ptr<SystemWebAppDelegate>>
        system_apps) {
  system_app_delegates_ = std::move(system_apps);
}

const std::vector<std::unique_ptr<SystemAppBackgroundTask>>&
SystemWebAppManager::GetBackgroundTasksForTesting() {
  return tasks_;
}

void SystemWebAppManager::SetUpdatePolicyForTesting(UpdatePolicy policy) {
  update_policy_ = policy;
}

void SystemWebAppManager::ResetOnAppsSynchronizedForTesting() {
  on_apps_synchronized_ = std::make_unique<base::OneShotEvent>();
}

// static
void SystemWebAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSystemWebAppLastUpdateVersion, "");
  registry->RegisterStringPref(prefs::kSystemWebAppLastInstalledLocale, "");
  registry->RegisterStringPref(prefs::kSystemWebAppLastAttemptedVersion, "");
  registry->RegisterStringPref(prefs::kSystemWebAppLastAttemptedLocale, "");
  registry->RegisterIntegerPref(prefs::kSystemWebAppInstallFailureCount, 0);
}

const base::Version& SystemWebAppManager::CurrentVersion() const {
  return version_info::GetVersion();
}

const std::string& SystemWebAppManager::CurrentLocale() const {
  return g_browser_process->GetApplicationLocale();
}

void SystemWebAppManager::RecordSystemWebAppInstallDuration(
    const base::TimeDelta& install_duration) const {
  // Install duration should be non-negative. A low resolution clock could
  // result in a |install_duration| of 0.
  DCHECK_GE(install_duration.InMilliseconds(), 0);

  if (!shutting_down_) {
    base::UmaHistogramMediumTimes(kInstallDurationHistogramName,
                                  install_duration);
  }
}

void SystemWebAppManager::RecordSystemWebAppInstallResults(
    const std::map<GURL, ExternallyManagedAppManager::InstallResult>&
        install_results) const {
  // Report install result codes. Exclude kSuccessAlreadyInstalled from metrics.
  // This result means the installation pipeline is a no-op (which happens every
  // time user logs in, and if there hasn't been a version upgrade). This skews
  // the install success rate.
  std::map<GURL, ExternallyManagedAppManager::InstallResult> results_to_report;
  std::copy_if(install_results.begin(), install_results.end(),
               std::inserter(results_to_report, results_to_report.end()),
               [](const auto& url_and_result) {
                 return url_and_result.second.code !=
                        webapps::InstallResultCode::kSuccessAlreadyInstalled;
               });

  for (const auto& url_and_result : results_to_report) {
    // Record aggregate result.
    base::UmaHistogramEnumeration(
        kInstallResultHistogramName,
        shutting_down_
            ? webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second.code);

    // Record per-profile result.
    base::UmaHistogramEnumeration(
        install_result_per_profile_histogram_name_,
        shutting_down_
            ? webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second.code);
  }

  // Record per-app result.
  for (const auto& type_and_app_info : system_app_delegates_) {
    const GURL& install_url = type_and_app_info.second->GetInstallUrl();
    const auto url_and_result = results_to_report.find(install_url);
    if (url_and_result != results_to_report.cend()) {
      const std::string app_histogram_name =
          std::string(kInstallResultHistogramName) + ".Apps." +
          type_and_app_info.second->GetInternalName();
      base::UmaHistogramEnumeration(
          app_histogram_name, shutting_down_
                                  ? webapps::InstallResultCode::
                                        kCancelledOnWebAppProviderShuttingDown
                                  : url_and_result->second.code);
    }
  }
}

void SystemWebAppManager::OnAppsSynchronized(
    bool did_force_install_apps,
    const base::TimeTicks& install_start_time,
    std::map<GURL, ExternallyManagedAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  const base::TimeDelta install_duration =
      base::TimeTicks::Now() - install_start_time;

  // TODO(qjw): Figure out where install_results come from, decide if
  // installation failures need to be handled
  pref_service_->SetString(prefs::kSystemWebAppLastUpdateVersion,
                           CurrentVersion().GetString());
  pref_service_->SetString(prefs::kSystemWebAppLastInstalledLocale,
                           CurrentLocale());
  pref_service_->SetInteger(prefs::kSystemWebAppInstallFailureCount, 0);

  // Report install duration only if the install pipeline actually installs
  // all the apps (e.g. on version upgrade).
  if (did_force_install_apps)
    RecordSystemWebAppInstallDuration(install_duration);

  RecordSystemWebAppInstallResults(install_results);

  for (const auto& it : system_app_delegates_) {
    absl::optional<SystemAppBackgroundTaskInfo> background_info =
        it.second->GetTimerInfo();
    if (background_info && it.second->IsAppEnabled()) {
      tasks_.push_back(std::make_unique<SystemAppBackgroundTask>(
          profile_, background_info.value()));
    }
  }
  // May be called more than once in tests.
  if (!on_apps_synchronized_->is_signaled()) {
    on_apps_synchronized_->Signal();
    web_app_policy_manager_->OnDisableListPolicyChanged();
    // TODO(http://crbug/1173187): Don't create SWA background tasks that are
    // associated with a disabled SWA.
  }

  // Start the tasks async to give any code running in an on_app_synchronized
  // context a chance to finish first.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SystemWebAppManager::StartBackgroundTasks,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SystemWebAppManager::StartBackgroundTasks() const {
  for (const auto& task : tasks_) {
    task->StartTask();
  }
  // This happens as part of synchronize, and can also be called multiple times
  // in testing.
  if (!on_tasks_started_->is_signaled()) {
    on_tasks_started_->Signal();
  }
}

bool SystemWebAppManager::ShouldForceInstallApps() const {
  if (base::FeatureList::IsEnabled(features::kAlwaysReinstallSystemWebApps))
    return true;

  if (update_policy_ == UpdatePolicy::kAlwaysUpdate)
    return true;

  base::Version current_installed_version(
      pref_service_->GetString(prefs::kSystemWebAppLastUpdateVersion));

  const std::string& current_installed_locale(
      pref_service_->GetString(prefs::kSystemWebAppLastInstalledLocale));

  // If Chrome version rolls back for some reason, ensure System Web Apps are
  // always in sync with Chrome version.
  const bool versionIsDifferent = !current_installed_version.IsValid() ||
                                  current_installed_version != CurrentVersion();

  // If system language changes, ensure System Web Apps launcher localization
  // are in sync with current language.
  const bool localeIsDifferent = current_installed_locale != CurrentLocale();

  return versionIsDifferent || localeIsDifferent;
}

void SystemWebAppManager::UpdateLastAttemptedInfo() {
  base::Version last_attempted_version(
      pref_service_->GetString(prefs::kSystemWebAppLastAttemptedVersion));

  const std::string& last_attempted_locale(
      pref_service_->GetString(prefs::kSystemWebAppLastAttemptedLocale));

  const bool is_retry = last_attempted_version.IsValid() &&
                        last_attempted_version == CurrentVersion() &&
                        last_attempted_locale == CurrentLocale();
  if (!is_retry) {
    pref_service_->SetInteger(prefs::kSystemWebAppInstallFailureCount, 0);
  }

  pref_service_->SetString(prefs::kSystemWebAppLastAttemptedVersion,
                           CurrentVersion().GetString());
  pref_service_->SetString(prefs::kSystemWebAppLastAttemptedLocale,
                           CurrentLocale());
  pref_service_->CommitPendingWrite();
}

bool SystemWebAppManager::CheckAndIncrementRetryAttempts() {
  int installation_failures =
      pref_service_->GetInteger(prefs::kSystemWebAppInstallFailureCount);
  bool reached_retry_limit = installation_failures > kInstallFailureAttempts;

  if (!reached_retry_limit) {
    pref_service_->SetInteger(prefs::kSystemWebAppInstallFailureCount,
                              installation_failures + 1);
    pref_service_->CommitPendingWrite();
    return false;
  }
  return true;
}

}  // namespace web_app
