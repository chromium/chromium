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
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
// TODO(b/174811949): Hide behind ChromeOS build flag.
#include "chrome/browser/ash/web_applications/chrome_camera_app_ui_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/ash/web_applications/camera_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/connectivity_diagnostics_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/crosh_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/diagnostics_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/eche_app_info.h"
#include "chrome/browser/ash/web_applications/help_app_web_app_info.h"
#include "chrome/browser/ash/web_applications/media_web_app_info.h"
#include "chrome/browser/ash/web_applications/os_settings_web_app_info.h"
#include "chrome/browser/ash/web_applications/personalization_app_info.h"
#include "chrome/browser/ash/web_applications/print_management_web_app_info.h"
#include "chrome/browser/ash/web_applications/scanning_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/terminal_system_web_app_info.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "chromeos/components/connectivity_diagnostics/url_constants.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/components/personalization_app/personalization_app_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "extensions/common/constants.h"
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ash/web_applications/file_manager_web_app_info.h"
#include "chrome/browser/ash/web_applications/sample_system_web_app_info.h"
#include "chrome/browser/ash/web_applications/telemetry_extension_web_app_info.h"
#endif  // !defined(OFFICIAL_BUILD)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

// Copy the origin trial name from runtime_enabled_features.json5, to avoid
// complex dependencies.
const char kFileHandlingOriginTrial[] = "FileHandling";

// Number of attempts to install a given version & locale of the SWAs before
// bailing out.
const int kInstallFailureAttempts = 3;

// Use #if defined to avoid compiler error on unused function.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// A convenience method to create OriginTrialsMap. Note, we only support simple
// cases for chrome:// and chrome-untrusted:// URLs. We don't support complex
// cases such as about:blank (which inherits origins from the embedding frame).
url::Origin GetOrigin(const char* url) {
  GURL gurl = GURL(url);
  DCHECK(gurl.is_valid());

  url::Origin origin = url::Origin::Create(gurl);
  DCHECK(!origin.opaque());

  return origin;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::flat_map<SystemAppType, SystemAppInfo> CreateSystemWebApps(
    Profile* profile) {
  base::flat_map<SystemAppType, SystemAppInfo> infos;
// TODO(calamity): Split this into per-platform functions.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // SystemAppInfo's |name| field should be defined. These names are persisted
  // to logs and should not be renamed.
  // If new names are added, update tool/metrics/histograms/histograms.xml:
  // "SystemWebAppName"
  if (SystemWebAppManager::IsAppEnabled(SystemAppType::CAMERA)) {
    infos.emplace(
        SystemAppType::CAMERA,
        SystemAppInfo(
            "Camera", GURL("chrome://camera-app/views/main.html"),
            base::BindRepeating(&CreateWebAppInfoForCameraSystemWebApp)));
    if (!profile->GetPrefs()->GetBoolean(
            chromeos::prefs::kHasCameraAppMigratedToSWA)) {
      infos.at(SystemAppType::CAMERA).uninstall_and_replace = {
          extension_misc::kCameraAppId};
    }
    // We need "FileHandling" to use File Handling API to set launch directory.
    infos.at(SystemAppType::CAMERA).enabled_origin_trials =
        OriginTrialsMap({{GetOrigin("chrome://camera-app"),
                          {"FileHandling", "IdleDetection"}}});
    infos.at(SystemAppType::CAMERA).capture_navigations = true;

    // Minimum height +32 for top bar height.
    infos.at(SystemAppType::CAMERA).minimum_window_size = {
        kChromeCameraAppMinimumWidth, kChromeCameraAppMinimumHeight + 32};
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::DIAGNOSTICS)) {
    infos.emplace(
        SystemAppType::DIAGNOSTICS,
        SystemAppInfo(
            "Diagnostics", GURL("chrome://diagnostics"),
            base::BindRepeating(&CreateWebAppInfoForDiagnosticsSystemWebApp)));
    infos.at(SystemAppType::DIAGNOSTICS).minimum_window_size = {600, 390};
    infos.at(SystemAppType::DIAGNOSTICS).show_in_launcher = false;
  }

  infos.emplace(SystemAppType::SETTINGS,
                SystemAppInfo("OSSettings", GURL(chrome::kChromeUISettingsURL),
                              base::BindRepeating(
                                  &CreateWebAppInfoForOSSettingsSystemWebApp)));
  infos.at(SystemAppType::SETTINGS).uninstall_and_replace = {
      kSettingsAppId, ash::kInternalAppIdSettings};
  // Large enough to see the heading text "Settings" in the top-left.
  infos.at(SystemAppType::SETTINGS).minimum_window_size = {300, 100};
  infos.at(SystemAppType::SETTINGS).capture_navigations = true;

  infos.emplace(SystemAppType::CROSH,
                SystemAppInfo("Crosh", GURL(chrome::kChromeUIUntrustedCroshURL),
                              base::BindRepeating(
                                  &CreateWebAppInfoForCroshSystemWebApp)));
  infos.at(SystemAppType::CROSH).single_window = false;
  infos.at(SystemAppType::CROSH).show_in_launcher = false;
  infos.at(SystemAppType::CROSH).show_in_search = false;

  infos.emplace(
      SystemAppType::TERMINAL,
      SystemAppInfo(
          "Terminal", GURL(chrome::kChromeUIUntrustedTerminalURL),
          base::BindRepeating(&CreateWebAppInfoForTerminalSystemWebApp)));
  infos.at(SystemAppType::TERMINAL).single_window = false;

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::HELP)) {
    infos.emplace(
        SystemAppType::HELP,
        SystemAppInfo("Help", GURL("chrome://help-app/pwa.html"),
                      base::BindRepeating(&CreateWebAppInfoForHelpWebApp)));
    infos.at(SystemAppType::HELP).additional_search_terms = {
        IDS_GENIUS_APP_NAME, IDS_HELP_APP_PERKS, IDS_HELP_APP_OFFERS};
    infos.at(SystemAppType::HELP).minimum_window_size = {600, 320};
    infos.at(SystemAppType::HELP).capture_navigations = true;
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::MEDIA)) {
    infos.emplace(
        SystemAppType::MEDIA,
        SystemAppInfo("Media", GURL("chrome://media-app/pwa.html"),
                      base::BindRepeating(&CreateWebAppInfoForMediaWebApp)));
    infos.at(SystemAppType::MEDIA).include_launch_directory = true;
    infos.at(SystemAppType::MEDIA).show_in_launcher = false;
    infos.at(SystemAppType::MEDIA).show_in_search = false;
    infos.at(SystemAppType::MEDIA).enabled_origin_trials =
        OriginTrialsMap({{GetOrigin("chrome://media-app"), {"FileHandling"}}});
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::PRINT_MANAGEMENT)) {
    infos.emplace(
        SystemAppType::PRINT_MANAGEMENT,
        SystemAppInfo(
            "PrintManagement", GURL("chrome://print-management/pwa.html"),
            base::BindRepeating(&CreateWebAppInfoForPrintManagementApp)));
    infos.at(SystemAppType::PRINT_MANAGEMENT).show_in_launcher = false;
    infos.at(SystemAppType::PRINT_MANAGEMENT).minimum_window_size = {600, 320};
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::SCANNING)) {
    infos.emplace(SystemAppType::SCANNING,
                  SystemAppInfo("Scanning", GURL("chrome://scanning"),
                                base::BindRepeating(
                                    &CreateWebAppInfoForScanningSystemWebApp)));
    infos.at(SystemAppType::SCANNING).minimum_window_size = {600, 420};
    infos.at(SystemAppType::SCANNING).capture_navigations = true;
    infos.at(SystemAppType::SCANNING).show_in_launcher = false;
  }

  if (SystemWebAppManager::IsAppEnabled(
          SystemAppType::CONNECTIVITY_DIAGNOSTICS)) {
    infos.emplace(
        SystemAppType::CONNECTIVITY_DIAGNOSTICS,
        SystemAppInfo(
            "ConnectivityDiagnostics",
            GURL(chromeos::kChromeUIConnectivityDiagnosticsUrl),
            base::BindRepeating(
                &CreateWebAppInfoForConnectivityDiagnosticsSystemWebApp)));
    infos.at(SystemAppType::CONNECTIVITY_DIAGNOSTICS).show_in_launcher = false;
    infos.at(SystemAppType::CONNECTIVITY_DIAGNOSTICS).show_in_search = false;
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::ECHE)) {
    infos.emplace(
        SystemAppType::ECHE,
        SystemAppInfo("Eche", GURL("chrome://eche-app"),
                      base::BindRepeating(&CreateWebAppInfoForEcheApp)));
    infos.at(SystemAppType::ECHE).capture_navigations = true;
    infos.at(SystemAppType::ECHE).show_in_launcher = false;
    infos.at(SystemAppType::ECHE).show_in_search = false;
    infos.at(SystemAppType::ECHE).is_resizeable = false;
    infos.at(SystemAppType::ECHE).is_maximizable = false;
    infos.at(SystemAppType::ECHE).should_have_reload_button_in_minimal_ui =
        false;
    infos.at(SystemAppType::ECHE).allow_scripts_to_close_windows = true;
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::PERSONALIZATION)) {
    infos.emplace(
        SystemAppType::PERSONALIZATION,
        SystemAppInfo(
            "Personalization", GURL(chromeos::kChromeUIPersonalizationAppURL),
            base::BindRepeating(&CreateWebAppInfoForPersonalizationApp)));
    auto& personalization_info = infos.at(SystemAppType::PERSONALIZATION);
    personalization_info.capture_navigations = true;
  }

#if !defined(OFFICIAL_BUILD)
  if (SystemWebAppManager::IsAppEnabled(SystemAppType::TELEMETRY)) {
    infos.emplace(
        SystemAppType::TELEMETRY,
        SystemAppInfo(
            "Telemetry", GURL("chrome://telemetry-extension"),
            base::BindRepeating(&CreateWebAppInfoForTelemetryExtension)));
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::FILE_MANAGER)) {
    infos.emplace(
        SystemAppType::FILE_MANAGER,
        SystemAppInfo("File Manager", GURL("chrome://file-manager"),
                      base::BindRepeating(&CreateWebAppInfoForFileManager)));
    infos.at(SystemAppType::FILE_MANAGER).capture_navigations = true;
    infos.at(SystemAppType::FILE_MANAGER).single_window = false;
  }

  infos.emplace(
      SystemAppType::SAMPLE,
      SystemAppInfo(
          "Sample", GURL("chrome://sample-system-web-app/pwa.html"),
          base::BindRepeating(&CreateWebAppInfoForSampleSystemWebApp)));
  // Frobulate is the name for Sample Origin Trial API, and has no impact on the
  // Web App's functionality. Here we use it to demonstrate how to enable origin
  // trials for a System Web App.
  infos.at(SystemAppType::SAMPLE).enabled_origin_trials = OriginTrialsMap(
      {{GetOrigin("chrome://sample-system-web-app"), {"Frobulate"}},
       {GetOrigin("chrome-untrusted://sample-system-web-app"), {"Frobulate"}}});
  infos.at(SystemAppType::SAMPLE).capture_navigations = true;
  infos.at(SystemAppType::SAMPLE).timer_info = SystemAppBackgroundTaskInfo(
      base::TimeDelta::FromSeconds(30),
      GURL("chrome://sample-system-web-app/timer.html"));
#endif  // !defined(OFFICIAL_BUILD)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return infos;
}

bool HasSystemWebAppScheme(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeUIUntrustedScheme);
}

ExternalInstallOptions CreateInstallOptionsForSystemApp(
    const SystemAppType app_type,
    const SystemAppInfo& info,
    bool force_update,
    bool is_disabled) {
  DCHECK(info.install_url.scheme() == content::kChromeUIScheme ||
         info.install_url.scheme() == content::kChromeUIUntrustedScheme);

  ExternalInstallOptions install_options(
      info.install_url, DisplayMode::kStandalone,
      ExternalInstallSource::kSystemInstalled);
  install_options.only_use_app_info_factory = !!info.app_info_factory;
  install_options.app_info_factory = info.app_info_factory;
  install_options.add_to_applications_menu = info.show_in_launcher;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.add_to_search = info.show_in_search;
  install_options.add_to_management = false;
  install_options.is_disabled = is_disabled;
  install_options.bypass_service_worker_check = true;
  install_options.force_reinstall = force_update;
  install_options.uninstall_and_replace = info.uninstall_and_replace;
  install_options.system_app_type = app_type;

  const auto& search_terms = info.additional_search_terms;
  std::transform(search_terms.begin(), search_terms.end(),
                 std::back_inserter(install_options.additional_search_terms),
                 [](int term) { return l10n_util::GetStringUTF8(term); });
  return install_options;
}

}  // namespace

SystemAppInfo::SystemAppInfo(const std::string& internal_name,
                             const GURL& install_url,
                             const WebApplicationInfoFactory& app_info_factory)
    : internal_name(internal_name),
      install_url(install_url),
      app_info_factory(app_info_factory) {}

SystemAppInfo::SystemAppInfo(const SystemAppInfo& other) = default;

SystemAppInfo::~SystemAppInfo() = default;

// static
const char SystemWebAppManager::kInstallResultHistogramName[];
const char SystemWebAppManager::kInstallDurationHistogramName[];

// static
bool SystemWebAppManager::IsAppEnabled(SystemAppType type) {
  if (base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OFFICIAL_BUILD)
  bool install_experimental_apps = true;
#else
  bool install_experimental_apps = false;
#endif

  switch (type) {
    case SystemAppType::SETTINGS:
      return true;
    case SystemAppType::CAMERA:
      return true;
    case SystemAppType::CROSH:
      return true;
    case SystemAppType::TERMINAL:
      return true;
    case SystemAppType::MEDIA:
      return base::FeatureList::IsEnabled(chromeos::features::kMediaApp);
    case SystemAppType::HELP:
      return true;
    case SystemAppType::PRINT_MANAGEMENT:
      return true;
    case SystemAppType::SCANNING:
      return true;
    case SystemAppType::DIAGNOSTICS:
      return base::FeatureList::IsEnabled(chromeos::features::kDiagnosticsApp);
    case SystemAppType::CONNECTIVITY_DIAGNOSTICS:
      return true;
    case SystemAppType::TELEMETRY:
      return install_experimental_apps &&
             base::FeatureList::IsEnabled(
                 chromeos::features::kTelemetryExtension);
    case SystemAppType::FILE_MANAGER:
      return install_experimental_apps &&
             base::FeatureList::IsEnabled(chromeos::features::kFilesSWA);
    case SystemAppType::SAMPLE:
      if (install_experimental_apps)
        NOTREACHED();
      return false;
    case SystemAppType::ECHE:
      return base::FeatureList::IsEnabled(chromeos::features::kEcheSWA);
    case SystemAppType::PERSONALIZATION:
      return chromeos::features::IsWallpaperWebUIEnabled();
  }
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

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
      system_app_infos_ = CreateSystemWebApps(profile_);

    return;
  }

#if defined(OFFICIAL_BUILD)
  // Official builds should trigger updates whenever the version number changes.
  update_policy_ = UpdatePolicy::kOnVersionChange;
#else
  // Dev builds should update every launch.
  update_policy_ = UpdatePolicy::kAlwaysUpdate;
#endif

  system_app_infos_ = CreateSystemWebApps(profile_);
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::StopBackgroundTasks() {
  for (auto& task : tasks_) {
    task->StopTask();
  }
}

void SystemWebAppManager::Shutdown() {
  shutting_down_ = true;
  StopBackgroundTasks();
}

void SystemWebAppManager::SetSubsystems(
    PendingAppManager* pending_app_manager,
    AppRegistrar* registrar,
    AppRegistryController* registry_controller,
    WebAppUiManager* ui_manager,
    OsIntegrationManager* os_integration_manager,
    WebAppPolicyManager* web_app_policy_manager) {
  pending_app_manager_ = pending_app_manager;
  registrar_ = registrar;
  registry_controller_ = registry_controller;
  ui_manager_ = ui_manager;
  os_integration_manager_ = os_integration_manager;
  web_app_policy_manager_ = web_app_policy_manager;
}

void SystemWebAppManager::Start() {
  const base::TimeTicks install_start_time = base::TimeTicks::Now();

#if DCHECK_IS_ON()
  // Check Origin Trials are defined correctly.
  for (const auto& type_and_app_info : system_app_infos_) {
    for (const auto& origin_to_trial_names :
         type_and_app_info.second.enabled_origin_trials) {
      // Only allow force enabled origin trials on chrome:// and
      // chrome-untrusted:// URLs.
      const auto& scheme = origin_to_trial_names.first.scheme();
      DCHECK(scheme == content::kChromeUIScheme ||
             scheme == content::kChromeUIUntrustedScheme);
    }
  }

  // TODO(https://crbug.com/1043843): Find some ways to validate supplied origin
  // trial names. Ideally, construct them from some static const char*.
#endif  // DCHECK_IS_ON()

  std::vector<ExternalInstallOptions> install_options_list;
  const bool should_force_install_apps = ShouldForceInstallApps();
  if (should_force_install_apps) {
    UpdateLastAttemptedInfo();
  }

  const auto& disabled_system_apps =
      web_app_policy_manager_->GetDisabledSystemWebApps();

  for (const auto& app : system_app_infos_) {
    install_options_list.push_back(CreateInstallOptionsForSystemApp(
        app.first, app.second, should_force_install_apps,
        base::Contains(disabled_system_apps, app.first)));
  }

  const bool exceeded_retries = CheckAndIncrementRetryAttempts();
  if (!exceeded_retries) {
    pending_app_manager_->SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kSystemInstalled,
        base::BindOnce(&SystemWebAppManager::OnAppsSynchronized,
                       weak_ptr_factory_.GetWeakPtr(),
                       should_force_install_apps, install_start_time));
  }
}

void SystemWebAppManager::InstallSystemAppsForTesting() {
  on_apps_synchronized_ = std::make_unique<base::OneShotEvent>();
  on_tasks_started_ = std::make_unique<base::OneShotEvent>();
  system_app_infos_ = CreateSystemWebApps(profile_);
  Start();

  // Wait for the System Web Apps to install.
  base::RunLoop run_loop;
  on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

const base::flat_map<SystemAppType, SystemAppInfo>&
SystemWebAppManager::GetRegisteredSystemAppsForTesting() const {
  return system_app_infos_;
}

base::Optional<AppId> SystemWebAppManager::GetAppIdForSystemApp(
    SystemAppType id) const {
  auto app_url_it = system_app_infos_.find(id);

  if (app_url_it == system_app_infos_.end())
    return base::Optional<AppId>();

  return registrar_->LookupExternalAppId(app_url_it->second.install_url);
}

base::Optional<SystemAppType> SystemWebAppManager::GetSystemAppTypeForAppId(
    AppId app_id) const {
  WebAppRegistrar* web_registrar = registrar_->AsWebAppRegistrar();

  if (!web_registrar) {
    return base::nullopt;
  }

  const WebApp* web_app = web_registrar->GetAppById(app_id);
  if (!web_app || !web_app->client_data().system_web_app_data.has_value()) {
    return base::nullopt;
  }

  // The registered system apps can change from previous runs (e.g. flipping a
  // SWA flag). The registry isn't up-to-date until SWA finishes installing, so
  // we could have a invalid type (for current session) during SWA install.
  //
  // This check ensures we return a type that is safe for other methods (avoids
  // crashing when looking up that type).
  SystemAppType proto_type =
      web_app->client_data().system_web_app_data->system_app_type;
  if (system_app_infos_.contains(proto_type)) {
    return proto_type;
  }

  return base::nullopt;
}

std::vector<AppId> SystemWebAppManager::GetAppIds() const {
  std::vector<AppId> app_ids;
  for (const auto& app_type_to_app_info : system_app_infos_) {
    base::Optional<AppId> app_id =
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

bool SystemWebAppManager::IsSingleWindow(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;

  return it->second.single_window;
}

bool SystemWebAppManager::AppShouldReceiveLaunchDirectory(
    SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;
  return it->second.include_launch_directory;
}

const std::vector<std::string>* SystemWebAppManager::GetEnabledOriginTrials(
    const SystemAppType type,
    const GURL& url) {
  const auto& origin_to_origin_trials =
      system_app_infos_.at(type).enabled_origin_trials;
  auto iter_trials = origin_to_origin_trials.find(url::Origin::Create(url));
  if (iter_trials == origin_to_origin_trials.end())
    return nullptr;

  return &iter_trials->second;
}

bool SystemWebAppManager::AppHasFileHandlingOriginTrial(SystemAppType type) {
  const auto& info = system_app_infos_.at(type);
  const std::vector<std::string>* trials =
      GetEnabledOriginTrials(type, info.install_url);
  return trials && base::Contains(*trials, kFileHandlingOriginTrial);
}

void SystemWebAppManager::OnReadyToCommitNavigation(
    const AppId& app_id,
    content::NavigationHandle* navigation_handle) {
  // No need to setup origin trials for intra-document navigation.
  if (navigation_handle->IsSameDocument())
    return;

  const base::Optional<SystemAppType> type = GetSystemAppTypeForAppId(app_id);
  // This function should only be called when an navigation happens inside a
  // System App. So the |app_id| should always have a valid associated System
  // App type.
  DCHECK(type.has_value());

  const std::vector<std::string>* trials =
      GetEnabledOriginTrials(type.value(), navigation_handle->GetURL());
  if (trials)
    navigation_handle->ForceEnableOriginTrials(*trials);
}

std::vector<std::string> SystemWebAppManager::GetAdditionalSearchTerms(
    SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return {};

  const auto& search_terms = it->second.additional_search_terms;

  std::vector<std::string> search_terms_strings;
  std::transform(search_terms.begin(), search_terms.end(),
                 std::back_inserter(search_terms_strings),
                 [](int term) { return l10n_util::GetStringUTF8(term); });
  return search_terms_strings;
}

bool SystemWebAppManager::ShouldShowInLauncher(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;
  return it->second.show_in_launcher;
}

bool SystemWebAppManager::ShouldShowInSearch(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;
  return it->second.show_in_search;
}

bool SystemWebAppManager::IsResizeableWindow(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;

  return it->second.is_resizeable;
}

bool SystemWebAppManager::IsMaximizableWindow(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;

  return it->second.is_maximizable;
}

bool SystemWebAppManager::ShouldHaveReloadButtonInMinimalUi(
    SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;

  return it->second.should_have_reload_button_in_minimal_ui;
}

bool SystemWebAppManager::AllowScriptsToCloseWindows(SystemAppType type) const {
  auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return false;

  return it->second.allow_scripts_to_close_windows;
}

base::Optional<SystemAppType> SystemWebAppManager::GetCapturingSystemAppForURL(
    const GURL& url) const {
  if (!HasSystemWebAppScheme(url))
    return base::nullopt;

  base::Optional<AppId> app_id = registrar_->FindAppWithUrlInScope(url);
  if (!app_id.has_value())
    return base::nullopt;

  base::Optional<SystemAppType> type = GetSystemAppTypeForAppId(app_id.value());
  if (!type.has_value())
    return base::nullopt;

  const auto it = system_app_infos_.find(type);
  if (it == system_app_infos_.end())
    return base::nullopt;

  if (!it->second.capture_navigations)
    return base::nullopt;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (type == SystemAppType::CAMERA &&
      url.spec() != chromeos::kChromeUICameraAppMainURL)
    return base::nullopt;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return type;
}

gfx::Size SystemWebAppManager::GetMinimumWindowSize(const AppId& app_id) const {
  base::Optional<SystemAppType> app_type = GetSystemAppTypeForAppId(app_id);

  if (!app_type.has_value())
    return gfx::Size();
  auto app_type_to_app_info = system_app_infos_.find(app_type.value());

  if (app_type_to_app_info == system_app_infos_.end()) {
    return gfx::Size();
  }

  return app_type_to_app_info->second.minimum_window_size;
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, SystemAppInfo> system_apps) {
  system_app_infos_ = std::move(system_apps);
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
    const std::map<GURL, PendingAppManager::InstallResult>& install_results)
    const {
  // Report install result codes. Exclude kSuccessAlreadyInstalled from metrics.
  // This result means the installation pipeline is a no-op (which happens every
  // time user logs in, and if there hasn't been a version upgrade). This skews
  // the install success rate.
  std::map<GURL, PendingAppManager::InstallResult> results_to_report;
  std::copy_if(install_results.begin(), install_results.end(),
               std::inserter(results_to_report, results_to_report.end()),
               [](const auto& url_and_result) {
                 return url_and_result.second.code !=
                        InstallResultCode::kSuccessAlreadyInstalled;
               });

  for (const auto& url_and_result : results_to_report) {
    // Record aggregate result.
    base::UmaHistogramEnumeration(
        kInstallResultHistogramName,
        shutting_down_
            ? InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second.code);

    // Record per-profile result.
    base::UmaHistogramEnumeration(
        install_result_per_profile_histogram_name_,
        shutting_down_
            ? InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second.code);
  }

  // Record per-app result.
  for (const auto& type_and_app_info : system_app_infos_) {
    const GURL& install_url = type_and_app_info.second.install_url;
    const auto url_and_result = results_to_report.find(install_url);
    if (url_and_result != results_to_report.cend()) {
      const std::string app_histogram_name =
          std::string(kInstallResultHistogramName) + ".Apps." +
          type_and_app_info.second.internal_name;
      base::UmaHistogramEnumeration(
          app_histogram_name,
          shutting_down_
              ? InstallResultCode::kCancelledOnWebAppProviderShuttingDown
              : url_and_result->second.code);
    }
  }
}

void SystemWebAppManager::OnAppsSynchronized(
    bool did_force_install_apps,
    const base::TimeTicks& install_start_time,
    std::map<GURL, PendingAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  // TODO(crbug.com/1053371): Clean up File Handler install. We install SWA file
  // handlers here, because the code that registers file handlers for regular
  // Web Apps, does not run when for apps installed in the background.
  for (const auto& it : system_app_infos_) {
    const SystemAppType& type = it.first;
    base::Optional<AppId> app_id = GetAppIdForSystemApp(type);
    if (!app_id)
      continue;

    if (AppHasFileHandlingOriginTrial(type)) {
      os_integration_manager_->ForceEnableFileHandlingOriginTrial(
          app_id.value());
    } else {
      os_integration_manager_->DisableForceEnabledFileHandlingOriginTrial(
          app_id.value());
    }
  }

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

  for (const auto& it : system_app_infos_) {
    const SystemAppInfo& app_info = it.second;

    if (app_info.timer_info) {
      tasks_.push_back(std::make_unique<SystemAppBackgroundTask>(
          profile_, app_info.timer_info.value()));
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_camera_app_installed =
      system_app_infos_.find(SystemAppType::CAMERA) != system_app_infos_.end();
  profile_->GetPrefs()->SetBoolean(chromeos::prefs::kHasCameraAppMigratedToSWA,
                                   is_camera_app_installed);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
