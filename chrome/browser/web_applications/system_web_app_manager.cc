// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/chromeos/web_applications/camera_system_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/chromeos/web_applications/diagnostics_system_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/help_app_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/media_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/scanning_system_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/terminal_source.h"
#include "chrome/browser/chromeos/web_applications/terminal_system_web_app_info.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "extensions/common/constants.h"

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/chromeos/web_applications/file_manager_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/sample_system_web_app_info.h"
#include "chrome/browser/chromeos/web_applications/telemetry_extension_web_app_info.h"
#endif  // !defined(OFFICIAL_BUILD)

#endif  // defined(OS_CHROMEOS)

namespace web_app {

namespace {

// Copy the origin trial name from runtime_enabled_features.json5, to avoid
// complex dependencies.
const char kFileHandlingOriginTrial[] = "FileHandling";

// Number of attempts to install a given version & locale of the SWAs before
// bailing out.
const int kInstallFailureAttempts = 3;

// Use #if defined to avoid compiler error on unused function.
#if defined(OS_CHROMEOS)

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

#endif  // OS_CHROMEOS

base::flat_map<SystemAppType, SystemAppInfo> CreateSystemWebApps() {
  base::flat_map<SystemAppType, SystemAppInfo> infos;
// TODO(calamity): Split this into per-platform functions.
#if defined(OS_CHROMEOS)
  // SystemAppInfo's |name| field should be defined. These names are persisted
  // to logs and should not be renamed.
  // If new names are added, update tool/metrics/histograms/histograms.xml:
  // "SystemWebAppName"
  if (SystemWebAppManager::IsAppEnabled(SystemAppType::DISCOVER)) {
    infos.emplace(
        SystemAppType::DISCOVER,
        SystemAppInfo("Discover", GURL(chrome::kChromeUIDiscoverURL)));
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::CAMERA)) {
    infos.emplace(SystemAppType::CAMERA,
                  SystemAppInfo("Camera", GURL("chrome://camera-app"),
                                base::BindRepeating(
                                    &CreateWebAppInfoForCameraSystemWebApp)));
    infos.at(SystemAppType::CAMERA).uninstall_and_replace = {
        extension_misc::kCameraAppId};
    // We need "FileHandling" to use File Handling API to set launch directory.
    // And we need "NativeFileSystem2" to use Native File System API.
    infos.at(SystemAppType::CAMERA).enabled_origin_trials =
        OriginTrialsMap({{GetOrigin("chrome://camera-app"),
                          {"FileHandling", "NativeFileSystem2"}}});
    infos.at(SystemAppType::CAMERA).capture_navigations = true;
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::DIAGNOSTICS)) {
    infos.emplace(
        SystemAppType::DIAGNOSTICS,
        SystemAppInfo(
            "Diagnostics", GURL("chrome://diagnostics"),
            base::BindRepeating(&CreateWebAppInfoForDiagnosticsSystemWebApp)));
  }

  infos.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo("OSSettings", GURL("chrome://os-settings/pwa.html")));
  infos.at(SystemAppType::SETTINGS).uninstall_and_replace = {
      chromeos::default_web_apps::kSettingsAppId, ash::kInternalAppIdSettings};
  // Large enough to see the heading text "Settings" in the top-left.
  infos.at(SystemAppType::SETTINGS).minimum_window_size = {300, 100};

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::TERMINAL)) {
    infos.emplace(
        SystemAppType::TERMINAL,
        SystemAppInfo(
            "Terminal", GURL(chrome::kChromeUIUntrustedTerminalURL),
            base::BindRepeating(&CreateWebAppInfoForTerminalSystemWebApp)));
    infos.at(SystemAppType::TERMINAL).single_window = false;
  }

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
        OriginTrialsMap({{GetOrigin("chrome://media-app"),
                          {"FileHandling", "NativeFileSystem2"}}});
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::PRINT_MANAGEMENT)) {
    infos.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(SystemAppType::PRINT_MANAGEMENT),
        std::forward_as_tuple("PrintManagement",
                              GURL("chrome://print-management/pwa.html")));
    infos.at(SystemAppType::PRINT_MANAGEMENT).show_in_launcher = false;
    infos.at(SystemAppType::PRINT_MANAGEMENT).minimum_window_size = {600, 320};
  }

  if (SystemWebAppManager::IsAppEnabled(SystemAppType::SCANNING)) {
    infos.emplace(SystemAppType::SCANNING,
                  SystemAppInfo("Scanning", GURL("chrome://scanning"),
                                base::BindRepeating(
                                    &CreateWebAppInfoForScanningSystemWebApp)));
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
#endif  // !defined(OFFICIAL_BUILD)

#endif  // OS_CHROMEOS

  return infos;
}

bool HasSystemWebAppScheme(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeUIUntrustedScheme);
}

ExternalInstallOptions CreateInstallOptionsForSystemApp(
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

  const auto& search_terms = info.additional_search_terms;
  std::transform(search_terms.begin(), search_terms.end(),
                 std::back_inserter(install_options.additional_search_terms),
                 [](int term) { return l10n_util::GetStringUTF8(term); });
  return install_options;
}

std::set<SystemAppType> GetDisabledSystemWebApps() {
  std::set<SystemAppType> disabled_system_apps;

#if defined(OS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return disabled_system_apps;

  const base::ListValue* disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  if (!disabled_system_features_pref)
    return disabled_system_apps;

  for (const auto& entry : *disabled_system_features_pref) {
    switch (entry.GetInt()) {
      case policy::SystemFeature::CAMERA:
        disabled_system_apps.insert(SystemAppType::CAMERA);
        break;
      case policy::SystemFeature::OS_SETTINGS:
        disabled_system_apps.insert(SystemAppType::SETTINGS);
        break;
      case policy::SystemFeature::SCANNING:
        disabled_system_apps.insert(SystemAppType::SCANNING);
        break;
    }
  }
#endif  // defined(OS_CHROMEOS)

  return disabled_system_apps;
}

}  // namespace

SystemAppInfo::SystemAppInfo(const std::string& internal_name,
                             const GURL& install_url)
    : internal_name(internal_name), install_url(install_url) {}

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

#if defined(OS_CHROMEOS)
  switch (type) {
    case SystemAppType::SETTINGS:
      return true;
    case SystemAppType::DISCOVER:
      return base::FeatureList::IsEnabled(chromeos::features::kDiscoverApp);
    case SystemAppType::CAMERA:
      return base::FeatureList::IsEnabled(
          chromeos::features::kCameraSystemWebApp);
    case SystemAppType::TERMINAL:
      return true;
    case SystemAppType::MEDIA:
      return base::FeatureList::IsEnabled(chromeos::features::kMediaApp);
    case SystemAppType::HELP:
      return true;
    case SystemAppType::PRINT_MANAGEMENT:
      return base::FeatureList::IsEnabled(
          chromeos::features::kPrintJobManagementApp);
    case SystemAppType::SCANNING:
      return base::FeatureList::IsEnabled(chromeos::features::kScanningUI);
    case SystemAppType::DIAGNOSTICS:
      return base::FeatureList::IsEnabled(chromeos::features::kDiagnosticsApp);
#if !defined(OFFICIAL_BUILD)
    case SystemAppType::TELEMETRY:
      return base::FeatureList::IsEnabled(
          chromeos::features::kTelemetryExtension);
    case SystemAppType::FILE_MANAGER:
      return base::FeatureList::IsEnabled(chromeos::features::kFilesSWA);
    case SystemAppType::SAMPLE:
      NOTREACHED();
      return false;
#endif  // !defined(OFFICIAL_BUILD)
  }
#else
  return false;
#endif  // OS_CHROMEOS
}

SystemWebAppManager::SystemWebAppManager(Profile* profile)
    : profile_(profile),
      on_apps_synchronized_(new base::OneShotEvent()),
      install_result_per_profile_histogram_name_(
          std::string(kInstallResultHistogramName) + ".Profiles." +
          GetProfileCategoryForLogging(profile)),
      pref_service_(profile_->GetPrefs()) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    // Always update in tests.
    update_policy_ = UpdatePolicy::kAlwaysUpdate;

    // Return early to avoid populating with real system apps.
    return;
  }

#if defined(OFFICIAL_BUILD)
  // Official builds should trigger updates whenever the version number changes.
  update_policy_ = UpdatePolicy::kOnVersionChange;
#else
  // Dev builds should update every launch.
  update_policy_ = UpdatePolicy::kAlwaysUpdate;
#endif

  system_app_infos_ = CreateSystemWebApps();
}

SystemWebAppManager::~SystemWebAppManager() = default;

void SystemWebAppManager::Shutdown() {
  shutting_down_ = true;
}

void SystemWebAppManager::SetSubsystems(
    PendingAppManager* pending_app_manager,
    AppRegistrar* registrar,
    AppRegistryController* registry_controller,
    WebAppUiManager* ui_manager,
    OsIntegrationManager* os_integration_manager) {
  pending_app_manager_ = pending_app_manager;
  registrar_ = registrar;
  registry_controller_ = registry_controller;
  ui_manager_ = ui_manager;
  os_integration_manager_ = os_integration_manager;
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

#if defined(OS_CHROMEOS)
  // Set up terminal data source. Terminal source is needed for install.
  // TODO(crbug.com/1080384): Move once chrome-untrusted has WebUIControllers.
  if (SystemWebAppManager::IsAppEnabled(SystemAppType::TERMINAL)) {
    content::URLDataSource::Add(profile_,
                                TerminalSource::ForTerminal(profile_));
  }
#endif  // defined(OS_CHROMEOS)

  std::vector<ExternalInstallOptions> install_options_list;
  const bool should_force_install_apps = ShouldForceInstallApps();
  if (should_force_install_apps) {
    UpdateLastAttemptedInfo();
  }

  const auto disabled_system_apps = GetDisabledSystemWebApps();

  for (const auto& app : system_app_infos_) {
    install_options_list.push_back(CreateInstallOptionsForSystemApp(
        app.second, should_force_install_apps,
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
#if defined(OS_CHROMEOS)
  PrefService* const local_state = g_browser_process->local_state();
  if (local_state) {  // Sometimes it's not available in tests.
    local_state_pref_change_registrar_.Init(local_state);

    // Sometimes this function gets called twice in tests.
    if (!local_state_pref_change_registrar_.IsObserved(
            policy::policy_prefs::kSystemFeaturesDisableList)) {
      local_state_pref_change_registrar_.Add(
          policy::policy_prefs::kSystemFeaturesDisableList,
          base::Bind(&SystemWebAppManager::OnAppsPolicyChanged,
                     base::Unretained(this)));
    }
  }
#endif  // defined(OS_CHROMEOS)
}

void SystemWebAppManager::InstallSystemAppsForTesting() {
  on_apps_synchronized_.reset(new base::OneShotEvent());
  system_app_infos_ = CreateSystemWebApps();
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
  auto it = app_id_to_app_type_.find(app_id);
  if (it == app_id_to_app_type_.end())
    return base::nullopt;

  return it->second;
}

std::vector<AppId> SystemWebAppManager::GetAppIds() const {
  std::vector<AppId> app_ids;
  for (const auto& app_id_to_app_type : app_id_to_app_type_) {
    app_ids.push_back(app_id_to_app_type.first);
  }
  return app_ids;
}

bool SystemWebAppManager::IsSystemWebApp(const AppId& app_id) const {
  return app_id_to_app_type_.contains(app_id);
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

  return type;
}

gfx::Size SystemWebAppManager::GetMinimumWindowSize(const AppId& app_id) const {
  auto app_type_it = app_id_to_app_type_.find(app_id);
  if (app_type_it == app_id_to_app_type_.end())
    return gfx::Size();
  const SystemAppType& app_type = app_type_it->second;
  auto app_info_it = system_app_infos_.find(app_type);
  if (app_info_it == system_app_infos_.end())
    return gfx::Size();
  return app_info_it->second.minimum_window_size;
}

void SystemWebAppManager::SetSystemAppsForTesting(
    base::flat_map<SystemAppType, SystemAppInfo> system_apps) {
  system_app_infos_ = std::move(system_apps);
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
    const std::map<GURL, InstallResultCode>& install_results) const {
  // Report install result codes. Exclude kSuccessAlreadyInstalled from metrics.
  // This result means the installation pipeline is a no-op (which happens every
  // time user logs in, and if there hasn't been a version upgrade). This skews
  // the install success rate.
  std::map<GURL, InstallResultCode> results_to_report;
  std::copy_if(install_results.begin(), install_results.end(),
               std::inserter(results_to_report, results_to_report.end()),
               [](const auto& url_and_result) {
                 return url_and_result.second !=
                        InstallResultCode::kSuccessAlreadyInstalled;
               });

  for (const auto& url_and_result : results_to_report) {
    // Record aggregate result.
    base::UmaHistogramEnumeration(
        kInstallResultHistogramName,
        shutting_down_
            ? InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second);

    // Record per-profile result.
    base::UmaHistogramEnumeration(
        install_result_per_profile_histogram_name_,
        shutting_down_
            ? InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second);
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
              : url_and_result->second);
    }
  }
}

void SystemWebAppManager::OnAppsSynchronized(
    bool did_force_install_apps,
    const base::TimeTicks& install_start_time,
    std::map<GURL, InstallResultCode> install_results,
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

  // Build the map from installed app id to app type.
  for (const auto& it : system_app_infos_) {
    const SystemAppType& app_type = it.first;
    base::Optional<AppId> app_id =
        registrar_->LookupExternalAppId(it.second.install_url);
    if (app_id.has_value())
      app_id_to_app_type_[app_id.value()] = app_type;
  }

  // May be called more than once in tests.
  if (!on_apps_synchronized_->is_signaled()) {
    on_apps_synchronized_->Signal();
    OnAppsPolicyChanged();
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

void SystemWebAppManager::OnAppsPolicyChanged() {
#if defined(OS_CHROMEOS)
  if (!on_apps_synchronized_->is_signaled())
    return;

  auto disabled_system_apps = GetDisabledSystemWebApps();

  for (const auto& id_and_type : app_id_to_app_type_) {
    const bool is_disabled =
        base::Contains(disabled_system_apps, id_and_type.second);
    registry_controller_->SetAppIsDisabled(id_and_type.first, is_disabled);
  }
#endif  // defined(OS_CHROMEOS)
}

}  // namespace web_app
