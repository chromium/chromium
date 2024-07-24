// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/concurrent_closures.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
// TODO(crbug.com/40251079): Remove or at least isolate circular dependencies on
// app service by moving this code to //c/b/web_applications/adjustments, or
// flip entire dependency so web_applications depends on app_service.
#include "chrome/browser/apps/app_service/app_service_proxy.h"  // nogncheck
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"  // nogncheck
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_app_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/version_info/version_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/touchscreen_device.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
// TODO(http://b/333583704): Revert CL which added this include after migration.
#include "chrome/browser/chromeos/echo/echo_util.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

bool g_skip_startup_for_testing_ = false;
bool g_bypass_awaiting_dependencies_for_testing_ = false;
bool g_bypass_offline_manifest_requirement_for_testing_ = false;
bool g_override_previous_user_uninstall_for_testing_ = false;
const base::Value::List* g_configs_for_testing = nullptr;
FileUtilsWrapper* g_file_utils_for_testing = nullptr;

const char kHistogramMigrationDisabledReason[] =
    "WebApp.Preinstalled.DisabledReason";

// These values are reported to UMA, do not modify them.
enum class DisabledReason {
  kNotDisabled = 0,
  kUninstallPreinstalledAppsNotEnabled = 1,
  kUninstallUserTypeNotAllowed = 2,
  kUninstallGatedFeatureNotEnabled = 3,
  kIgnoreGatedFeatureNotEnabled = 4,
  kIgnoreArcAvailable = 5,
  kIgnoreTabletFormFactor = 6,
  kIgnoreNotNewUser = 7,
  kIgnoreNotPreviouslyPreinstalled = 8,
  kUninstallReplacingAppBlockedByPolicy = 9,
  kUninstallReplacingAppForceInstalled = 10,
  kInstallReplacingAppStillInstalled = 11,
  kUninstallDefaultAppAndAppsToReplaceUninstalled = 12,
  kIgnoreReplacingAppUninstalledByUser = 13,
  kIgnoreStylusRequired = 14,
  kInstallOverridePreviousUserUninstall = 15,
  kIgnoreStylusRequiredNoDeviceData = 16,
  kIgnorePreviouslyUninstalledByUser = 17,
  kMaxValue = kIgnorePreviouslyUninstalledByUser
};

struct LoadedConfig {
  base::Value contents;
  base::FilePath file;
};

struct LoadedConfigs {
  std::vector<LoadedConfig> configs;
  std::vector<std::string> errors;
};

#if BUILDFLAG(IS_CHROMEOS)
bool IsArcAvailable() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return arc::IsArcAvailable();
#else
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  return init_params->DeviceProperties() &&
         init_params->DeviceProperties()->is_arc_available;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool IsTabletFormFactor() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::switches::IsTabletFormFactor();
#else
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  return init_params->DeviceProperties() &&
         init_params->DeviceProperties()->is_tablet_form_factor;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::optional<bool> HasStylusEnabledTouchscreen() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()
      ->DeviceProperties()
      ->has_stylus_enabled_touchscreen;
#else
  return DeviceHasStylusEnabledTouchscreen();
#endif
}

LoadedConfigs LoadConfigsBlocking(
    const std::vector<base::FilePath>& config_dirs) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  LoadedConfigs result;
  base::FilePath::StringType extension(FILE_PATH_LITERAL(".json"));

  for (const auto& config_dir : config_dirs) {
    base::FileEnumerator json_files(config_dir,
                                    false,  // Recursive.
                                    base::FileEnumerator::FILES);
    for (base::FilePath file = json_files.Next(); !file.empty();
         file = json_files.Next()) {
      if (!file.MatchesExtension(extension)) {
        continue;
      }

      JSONFileValueDeserializer deserializer(file);
      std::string error_msg;
      std::unique_ptr<base::Value> app_config =
          deserializer.Deserialize(nullptr, &error_msg);
      if (!app_config) {
        result.errors.push_back(base::StrCat(
            {file.AsUTF8Unsafe(), " was not valid JSON: ", error_msg}));
        VLOG(1) << result.errors.back();
        continue;
      }
      result.configs.push_back(
          {.contents = std::move(*app_config), .file = file});
    }
  }
  return result;
}

struct ParsedConfigs {
  std::vector<ExternalInstallOptions> options_list;
  std::vector<std::string> errors;
};

ParsedConfigs ParseConfigsBlocking(LoadedConfigs loaded_configs) {
  ParsedConfigs result;
  result.errors = std::move(loaded_configs.errors);

  scoped_refptr<FileUtilsWrapper> file_utils =
      g_file_utils_for_testing ? base::WrapRefCounted(g_file_utils_for_testing)
                               : base::MakeRefCounted<FileUtilsWrapper>();

  for (const LoadedConfig& loaded_config : loaded_configs.configs) {
    OptionsOrError parse_result =
        ParseConfig(*file_utils, loaded_config.file.DirName(),
                    loaded_config.file, loaded_config.contents);
    if (ExternalInstallOptions* options =
            absl::get_if<ExternalInstallOptions>(&parse_result)) {
      result.options_list.push_back(std::move(*options));
    } else {
      result.errors.push_back(std::move(absl::get<std::string>(parse_result)));
      VLOG(1) << result.errors.back();
    }
  }

  return result;
}

struct SynchronizeDecision {
  enum {
    // Ensures the web app preinstall gets removed.
    kUninstall,

    // Ensures the web app gets preinstalled.
    kInstall,

    // Leaves the web app preinstall state alone.
    // Prefer kIgnore over kUninstall in most cases of disabling a config as
    // uninstalling can have permanent consequences for users when bugs are hit.
    // See crbug.com/1393284 and crbug.com/1363004 for past incidents.
    kIgnore,
  } type;
  // TODO(crbug.com/40253925): Rename DisabledReason to
  // SynchronizeDecisionReason since it applies to every install decision.
  DisabledReason reason;
  std::string log;
};

SynchronizeDecision GetSynchronizeDecision(
    const ExternalInstallOptions& options,
    Profile* profile,
    WebAppRegistrar* registrar,
    bool preinstalled_apps_enabled_in_prefs,
    bool is_new_user,
    const std::string& user_type,
    size_t& corrupt_user_uninstall_prefs_count) {
  DCHECK(registrar);

  // This function encodes the exceptions to the standard preinstalled web app
  // configs; situations in which the preinstall should be removed, added or be
  // left untouched.
  //
  // The priority of these decisions is ordered:
  // kUninstall > kInstall > kIgnore > kInstall (default).

  /////////////////////////
  // kUninstall conditions.
  /////////////////////////

  if (!preinstalled_apps_enabled_in_prefs) {
    return {
        .type = SynchronizeDecision::kUninstall,
        .reason = DisabledReason::kUninstallPreinstalledAppsNotEnabled,
        .log = base::StrCat({options.install_url.spec(),
                             " uninstall by preinstalled_apps pref setting."})};
  }

  // Remove if not applicable to current user type.
  DCHECK_GT(options.user_type_allowlist.size(), 0u);
  if (!base::Contains(options.user_type_allowlist, user_type)) {
    return {.type = SynchronizeDecision::kUninstall,
            .reason = DisabledReason::kUninstallUserTypeNotAllowed,
            .log = base::StrCat({options.install_url.spec(),
                                 " uninstall for user type: ", user_type})};
  }

  // Remove if gated on a disabled feature.
  if (options.gate_on_feature && !IsPreinstalledAppInstallFeatureEnabled(
                                     *options.gate_on_feature, *profile)) {
    return {.type = SynchronizeDecision::kUninstall,
            .reason = DisabledReason::kUninstallGatedFeatureNotEnabled,
            .log = base::StrCat({options.install_url.spec(),
                                 " uninstall because feature is disabled: ",
                                 *options.gate_on_feature})};
  }

  // Remove if any apps to replace are blocked or force installed by admin
  // policy.
  for (const webapps::AppId& app_id : options.uninstall_and_replace) {
    if (extensions::IsExtensionBlockedByPolicy(profile, app_id)) {
      return {.type = SynchronizeDecision::kUninstall,
              .reason = DisabledReason::kUninstallReplacingAppBlockedByPolicy,
              .log = base::StrCat({options.install_url.spec(),
                                   " uninstall due to admin policy blocking "
                                   "replacement Extension."})};
    }
    std::u16string reason;
    if (extensions::IsExtensionForceInstalled(profile, app_id, &reason)) {
      return {
          .type = SynchronizeDecision::kUninstall,
          .reason = DisabledReason::kUninstallReplacingAppForceInstalled,
          .log = base::StrCat(
              {options.install_url.spec(),
               " uninstall due to admin policy force installing replacement "
               "Extension: ",
               base::UTF16ToUTF8(reason)})};
    }
  }
#if !BUILDFLAG(IS_CHROMEOS)
  // Remove if it's a default app and the apps to replace are not installed and
  // default extension apps are not performing new installation.
  if (options.gate_on_feature && !options.uninstall_and_replace.empty() &&
      !extensions::DidPreinstalledAppsPerformNewInstallation(profile)) {
    for (const webapps::AppId& app_id : options.uninstall_and_replace) {
      // First time migration and the app to replace is uninstalled as it passed
      // the last code block. Save the information that the app was
      // uninstalled by user.
      if (!WasMigrationRun(profile, *options.gate_on_feature)) {
        if (extensions::IsPreinstalledAppId(app_id)) {
          MarkPreinstalledAppAsUninstalled(profile, app_id);
          return {.type = SynchronizeDecision::kUninstall,
                  .reason = DisabledReason::
                      kUninstallDefaultAppAndAppsToReplaceUninstalled,
                  .log = base::StrCat(
                      {options.install_url.spec(),
                       "uninstall because its default app and apps to replace ",
                       "were uninstalled."})};
        }
      } else {
        // Not first time migration, can't determine if the app to replace is
        // uninstalled by user as the migration is already run, use the pref
        // saved in first migration.
        if (WasPreinstalledAppUninstalled(profile, app_id)) {
          return {.type = SynchronizeDecision::kUninstall,
                  .reason = DisabledReason::
                      kUninstallDefaultAppAndAppsToReplaceUninstalled,
                  .log = base::StrCat(
                      {options.install_url.spec(),
                       "uninstall because its default app and apps to replace "
                       "were uninstalled."})};
        }
      }
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  ///////////////////////
  // kInstall conditions.
  ///////////////////////

  bool was_previously_uninstalled_by_user =
      UserUninstalledPreinstalledWebAppPrefs(profile->GetPrefs())
          .LookUpAppIdByInstallUrl(options.install_url)
          .has_value();
  if (options.override_previous_user_uninstall &&
      was_previously_uninstalled_by_user) {
    return {
        .type = SynchronizeDecision::kInstall,
        .reason = DisabledReason::kInstallOverridePreviousUserUninstall,
        .log = base::StrCat({options.install_url.spec(),
                             " install overrides previous user uninstall."})};
  }

  // Ensure install if any apps to replace are installed as installation
  // includes uninstall_and_replace-ing the specified apps.
  for (const webapps::AppId& app_id : options.uninstall_and_replace) {
    if (extensions::IsExtensionInstalled(profile, app_id)) {
      return {
          .type = SynchronizeDecision::kInstall,
          .reason = DisabledReason::kInstallReplacingAppStillInstalled,
          .log = base::StrCat({options.install_url.spec(),
                               " install to replace existing Chrome app."})};
    }
  }

  //////////////////////
  // kIgnore conditions.
  //////////////////////

  if (was_previously_uninstalled_by_user) {
    return {.type = SynchronizeDecision::kIgnore,
            .reason = DisabledReason::kIgnorePreviouslyUninstalledByUser,
            .log = base::StrCat(
                {options.install_url.spec(),
                 " ignore because previously uninstalled by user"})};
  }

  // This option means to ignore if the feature flag is not enabled and leave
  // any existing installations alone.
  if (options.gate_on_feature_or_installed &&
      !IsPreinstalledAppInstallFeatureEnabled(
          *options.gate_on_feature_or_installed, *profile)) {
    return {.type = SynchronizeDecision::kIgnore,
            .reason = DisabledReason::kIgnoreGatedFeatureNotEnabled,
            .log = base::StrCat(
                {options.install_url.spec(), " ignore because the feature ",
                 *options.gate_on_feature_or_installed, " is disabled"})};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (options.disable_if_arc_supported && IsArcAvailable()) {
    return {.type = SynchronizeDecision::kIgnore,
            .reason = DisabledReason::kIgnoreArcAvailable,
            .log = base::StrCat({options.install_url.spec(),
                                 " ignore because ARC is available."})};
  }

  if (options.disable_if_tablet_form_factor && IsTabletFormFactor()) {
    return {.type = SynchronizeDecision::kIgnore,
            .reason = DisabledReason::kIgnoreTabletFormFactor,
            .log = base::StrCat({options.install_url.spec(),
                                 " ignore because device is tablet."})};
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (options.only_for_new_users && !is_new_user) {
    return {.type = SynchronizeDecision::kIgnore,
            .reason = DisabledReason::kIgnoreNotNewUser,
            .log = base::StrCat({options.install_url.spec(),
                                 " ignore because user is not new."})};
  }

  // This option means to ignore installations of the config, it came from a
  // time before SynchronizeDecision::kIgnore was added and so is worded
  // differently.
  if (options.only_if_previously_preinstalled) {
    return {.type = SynchronizeDecision::kIgnore,
            .reason = DisabledReason::kIgnoreNotPreviouslyPreinstalled,
            .log = base::StrCat(
                {options.install_url.spec(),
                 " ignore by config (only_if_previously_preinstalled)."})};
  }

  // Ignore if any apps to replace were previously uninstalled.
  for (const webapps::AppId& app_id : options.uninstall_and_replace) {
    if (extensions::IsExternalExtensionUninstalled(profile, app_id)) {
      return {.type = SynchronizeDecision::kIgnore,
              .reason = DisabledReason::kIgnoreReplacingAppUninstalledByUser,
              .log = base::StrCat(
                  {options.install_url.spec(),
                   " ignore because apps to replace were uninstalled."})};
    }
  }

  // Only install if device has a built-in touch screen with stylus support.
  if (options.disable_if_touchscreen_with_stylus_not_supported) {
    std::optional<bool> has_stylus = HasStylusEnabledTouchscreen();

    if (!has_stylus.has_value()) {
      return {.type = SynchronizeDecision::kIgnore,
              .reason = DisabledReason::kIgnoreStylusRequiredNoDeviceData,
              .log = base::StrCat(
                  {options.install_url.spec(),
                   " ignore because touchscreen device information is "
                   "unavailable"})};
    }

    if (!has_stylus.value()) {
      return {.type = SynchronizeDecision::kIgnore,
              .reason = DisabledReason::kIgnoreStylusRequired,
              .log = base::StrCat(
                  {options.install_url.spec(),
                   " ignore because the device does not have a built-in "
                   "touchscreen with stylus support."})};
    }
  }

  ////////////////////
  // Default scenario.
  ////////////////////

  return {
      .type = SynchronizeDecision::kInstall,
      .reason = DisabledReason::kNotDisabled,
      .log = base::StrCat({options.install_url.spec(), " regular install"})};
}

bool IsReinstallPastMilestoneNeededSinceLastSync(
    const PrefService& prefs,
    int force_reinstall_for_milestone) {
  std::string last_preinstall_synchronize_milestone =
      prefs.GetString(prefs::kWebAppsLastPreinstallSynchronizeVersion);

  return IsReinstallPastMilestoneNeeded(last_preinstall_synchronize_milestone,
                                        version_info::GetMajorVersionNumber(),
                                        force_reinstall_for_milestone);
}

bool ShouldForceReinstall(const ExternalInstallOptions& options,
                          const PrefService& prefs,
                          const WebAppRegistrar& registrar) {
  if (options.force_reinstall_for_milestone &&
      IsReinstallPastMilestoneNeededSinceLastSync(
          prefs, options.force_reinstall_for_milestone.value())) {
    return true;
  }

  // TODO(crbug.com/40261748): Add metrics for this event.
  const WebApp* app = registrar.LookUpAppByInstallSourceInstallUrl(
      WebAppManagement::Type::kDefault, options.install_url);
  if (app && LooksLikePlaceholder(*app)) {
    return true;
  }

  return false;
}

}  // namespace

class PreinstalledWebAppManager::DeviceDataInitializedEvent
    : public ui::InputDeviceEventObserver {
 public:
  DeviceDataInitializedEvent() = default;
  DeviceDataInitializedEvent(const DeviceDataInitializedEvent&) = delete;
  DeviceDataInitializedEvent& operator=(const DeviceDataInitializedEvent&) =
      delete;

  // Posts a `task` to be run once ui::DeviceDataManager has complete device
  // lists. If device lists are already complete, or DeviceDataManager is not
  // available, the task will be posted immediately.
  void Post(base::OnceClosure task);

 private:
  // ui::InputDeviceEventObserver:
  void OnDeviceListsComplete() override;

  // Task to run once ui::DeviceDataManager initialization is complete.
  base::OnceClosure initialized_task_;

  base::ScopedObservation<ui::DeviceDataManager, ui::InputDeviceEventObserver>
      device_data_observation_{this};
};

void PreinstalledWebAppManager::DeviceDataInitializedEvent::Post(
    base::OnceClosure task) {
  // DeviceDataManager does not exist on all platforms, but on platforms where
  // it exists, it's always created early in startup, so HasInstance() is a
  // reliable indicator of availability. However, loading device information is
  // asynchronous and may not have completed by this point.
  if (!ui::DeviceDataManager::HasInstance() ||
      ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             std::move(task));
  } else {
    DCHECK(!device_data_observation_.IsObserving());
    device_data_observation_.Observe(ui::DeviceDataManager::GetInstance());
    initialized_task_ = std::move(task);
  }
}

void PreinstalledWebAppManager::DeviceDataInitializedEvent::
    OnDeviceListsComplete() {
  std::move(initialized_task_).Run();
  device_data_observation_.Reset();
}

const char* PreinstalledWebAppManager::kHistogramEnabledCount =
    "WebApp.Preinstalled.EnabledCount";
const char* PreinstalledWebAppManager::kHistogramDisabledCount =
    "WebApp.Preinstalled.DisabledCount";
const char* PreinstalledWebAppManager::kHistogramConfigErrorCount =
    "WebApp.Preinstalled.ConfigErrorCount";
const char*
    PreinstalledWebAppManager::kHistogramCorruptUserUninstallPrefsCount =
        "WebApp.Preinstalled.CorruptUserUninstallPrefsCount";
const char* PreinstalledWebAppManager::kHistogramInstallResult =
    "Webapp.InstallResult.Default";
const char* PreinstalledWebAppManager::kHistogramInstallCount =
    "WebApp.Preinstalled.InstallCount";
const char* PreinstalledWebAppManager::kHistogramUninstallTotalCount =
    "WebApp.Preinstalled.UninstallTotalCount";
const char* PreinstalledWebAppManager::kHistogramUninstallSourceRemovedCount =
    "WebApp.Preinstalled.UninstallSourceRemovedCount";
const char* PreinstalledWebAppManager::kHistogramUninstallAppRemovedCount =
    "WebApp.Preinstalled.UninstallAppRemovedCount";
const char* PreinstalledWebAppManager::kHistogramUninstallAndReplaceCount =
    "WebApp.Preinstalled.UninstallAndReplaceCount";
const char*
    PreinstalledWebAppManager::kHistogramAppToReplaceStillInstalledCount =
        "WebApp.Preinstalled.AppToReplaceStillInstalledCount";
const char* PreinstalledWebAppManager::
    kHistogramAppToReplaceStillDefaultInstalledCount =
        "WebApp.Preinstalled.AppToReplaceStillDefaultInstalledCount";
const char* PreinstalledWebAppManager::
    kHistogramAppToReplaceStillInstalledInShelfCount =
        "WebApp.Preinstalled.AppToReplaceStillInstalledInShelfCount";

void PreinstalledWebAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kWebAppsLastPreinstallSynchronizeVersion,
                               "");
  registry->RegisterListPref(webapps::kWebAppsMigratedPreinstalledApps);
  registry->RegisterListPref(prefs::kWebAppsDidMigrateDefaultChromeApps);
  registry->RegisterListPref(prefs::kWebAppsUninstalledDefaultChromeApps);
}

// static
base::AutoReset<bool> PreinstalledWebAppManager::SkipStartupForTesting() {
  return {&g_skip_startup_for_testing_, true};
}

// static
base::AutoReset<bool>
PreinstalledWebAppManager::BypassAwaitingDependenciesForTesting() {
  return {&g_bypass_awaiting_dependencies_for_testing_, true};
}

// static
base::AutoReset<bool>
PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting() {
  return {&g_bypass_offline_manifest_requirement_for_testing_, true};
}

// static
base::AutoReset<bool>
PreinstalledWebAppManager::OverridePreviousUserUninstallConfigForTesting() {
  return {&g_override_previous_user_uninstall_for_testing_, true};
}

// static
base::AutoReset<const base::Value::List*>
PreinstalledWebAppManager::SetConfigsForTesting(
    const base::Value::List* configs) {
  return {&g_configs_for_testing, configs, nullptr};
}

// static
base::AutoReset<FileUtilsWrapper*>
PreinstalledWebAppManager::SetFileUtilsForTesting(
    FileUtilsWrapper* file_utils) {
  return {&g_file_utils_for_testing, file_utils, nullptr};
}

PreinstalledWebAppManager::PreinstalledWebAppManager(Profile* profile)
    : profile_(profile),
      device_data_initialized_event_(
          std::make_unique<DeviceDataInitializedEvent>()) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    debug_info_ = std::make_unique<DebugInfo>();
  }
}

PreinstalledWebAppManager::~PreinstalledWebAppManager() {
  for (auto& observer : observers_) {
    observer.OnDestroyed();
  }
}

void PreinstalledWebAppManager::SetProvider(base::PassKey<WebAppProvider>,
                                            WebAppProvider& provider) {
  provider_ = &provider;
}

void PreinstalledWebAppManager::Start(base::OnceClosure on_done) {
  DCHECK(provider_);
  if (g_skip_startup_for_testing_ || skip_startup_for_testing_) {  // IN-TEST
    std::move(on_done).Run();                                      // IN-TEST
    return;                                                        // IN-TEST
  }

  LoadAndSynchronize(
      base::BindOnce(&PreinstalledWebAppManager::OnStartUpTaskCompleted,
                     weak_ptr_factory_.GetWeakPtr())
          .Then(std::move(on_done)));
}

void PreinstalledWebAppManager::LoadForTesting(ConsumeInstallOptions callback) {
  Load(std::move(callback));
}

void PreinstalledWebAppManager::AddObserver(
    PreinstalledWebAppManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void PreinstalledWebAppManager::RemoveObserver(
    PreinstalledWebAppManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PreinstalledWebAppManager::SetSkipStartupSynchronizeForTesting(  // IN-TEST
    bool skip_startup) {
  skip_startup_for_testing_ = skip_startup;  // IN-TEST
}

void PreinstalledWebAppManager::LoadAndSynchronizeForTesting(
    SynchronizeCallback callback) {
  LoadAndSynchronize(std::move(callback));
}

void PreinstalledWebAppManager::LoadAndSynchronize(
    SynchronizeCallback callback) {
  base::OnceClosure load_and_synchronize = base::BindOnce(
      &PreinstalledWebAppManager::Load, weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&PreinstalledWebAppManager::Synchronize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  if (g_bypass_awaiting_dependencies_for_testing_) {
    std::move(load_and_synchronize).Run();
    return;
  }

  base::ConcurrentClosures concurrent;
  device_data_initialized_event_->Post(concurrent.CreateClosure());
  // Make sure ExtensionSystem is ready to know if default apps new installation
  // will be performed.
  extensions::OnExtensionSystemReady(profile_, concurrent.CreateClosure());
  std::move(concurrent).Done(std::move(load_and_synchronize));
}

void PreinstalledWebAppManager::Load(ConsumeInstallOptions callback) {
  bool preinstalling_enabled =
      base::FeatureList::IsEnabled(features::kPreinstalledWebAppInstallation);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // With Lacros, web apps are not installed using the Ash browser.
  if (IsWebAppsCrosapiEnabled()) {
    preinstalling_enabled = false;
  }
#endif

  if (!preinstalling_enabled) {
    std::move(callback).Run({});
    return;
  }

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&PreinstalledWebAppManager::LoadDeviceInfo, weak_ptr),
      base::BindOnce(&PreinstalledWebAppManager::CacheDeviceInfo, weak_ptr),
      base::BindOnce(&PreinstalledWebAppManager::LoadConfigs, weak_ptr),
      base::BindOnce(&PreinstalledWebAppManager::ParseConfigs, weak_ptr),
      base::BindOnce(&PreinstalledWebAppManager::PostProcessConfigs, weak_ptr),
      std::move(callback));
}

// TODO(http://b/333583704): Revert CL which added this method after migration.
void PreinstalledWebAppManager::LoadDeviceInfo(ConsumeDeviceInfo callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // This needs to be consistent with echo_private_api to avoid inconsistency
  // between promo offering and eligibility.
  chromeos::echo_util::GetOobeTimestamp(base::BindOnce(
      [](ConsumeDeviceInfo callback, std::optional<base::Time> oobe_timestamp) {
        DeviceInfo device_info;
        device_info.oobe_timestamp = std::move(oobe_timestamp);
        std::move(callback).Run(std::move(device_info));
      },
      std::move(callback)));
#else  // BUILDFLAG(IS_CHROMEOS)
  std::move(callback).Run(DeviceInfo());
#endif
}

// TODO(http://b/333583704): Revert CL which added this method after migration.
void PreinstalledWebAppManager::CacheDeviceInfo(
    CacheDeviceInfoCallback callback,
    DeviceInfo device_info) {
  device_info_ = std::move(device_info);
  std::move(callback).Run();
}

void PreinstalledWebAppManager::LoadConfigs(ConsumeLoadedConfigs callback) {
  if (g_configs_for_testing) {
    LoadedConfigs loaded_configs;
    for (const base::Value& config : *g_configs_for_testing) {
      auto file = base::FilePath(FILE_PATH_LITERAL("test.json"));
      if (GetPreinstalledWebAppConfigDirForTesting()) {
        file = GetPreinstalledWebAppConfigDirForTesting()->Append(file);
      }

      loaded_configs.configs.push_back(
          {.contents = config.Clone(), .file = file});
    }
    std::move(callback).Run(std::move(loaded_configs));
    return;
  }

  if (PreinstalledWebAppsDisabled()) {
    std::move(callback).Run({});
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Don't load configs from /usr/share/google-chrome/extensions/web_apps when
  // preinstalling core apps only.
  if (base::FeatureList::IsEnabled(
          chromeos::features::kPreinstalledWebAppsCoreOnly)) {
    std::move(callback).Run({});
    return;
  }
#endif

  base::FilePath config_dir = GetPreinstalledWebAppConfigDir(profile_);
  if (config_dir.empty()) {
    std::move(callback).Run({});
    return;
  }

  std::vector<base::FilePath> config_dirs = {config_dir};
  base::FilePath extra_config_dir =
      GetPreinstalledWebAppExtraConfigDir(profile_);
  if (!extra_config_dir.empty()) {
    config_dirs.push_back(extra_config_dir);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadConfigsBlocking, std::move(config_dirs)),
      std::move(callback));
}

void PreinstalledWebAppManager::ParseConfigs(ConsumeParsedConfigs callback,
                                             LoadedConfigs loaded_configs) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ParseConfigsBlocking, std::move(loaded_configs)),
      std::move(callback));
}

void PreinstalledWebAppManager::PostProcessConfigs(
    ConsumeInstallOptions callback,
    ParsedConfigs parsed_configs) {
  // Add hard coded configs.
  for (ExternalInstallOptions& options :
       GetPreinstalledWebApps(*profile_, device_info_)) {
    parsed_configs.options_list.push_back(std::move(options));
  }

  // Set common install options.
  for (ExternalInstallOptions& options : parsed_configs.options_list) {
    DCHECK_EQ(options.install_source, ExternalInstallSource::kExternalDefault);

    options.require_manifest = true;

#if BUILDFLAG(IS_CHROMEOS)
    // On Chrome OS the "quick launch bar" is the shelf pinned apps.
    // This is configured in `GetDefaultPinnedAppsForFormFactor()` instead of
    // here to ensure a specific order is deployed.
    options.add_to_quick_launch_bar = false;
#else   // BUILDFLAG(IS_CHROMEOS)
    if (!g_bypass_offline_manifest_requirement_for_testing_) {
      // Non-Chrome OS platforms are not permitted to fetch the web app install
      // URLs during start up.
      DCHECK(options.app_info_factory);
      options.only_use_app_info_factory = true;
    }

    // Preinstalled web apps should not have OS shortcuts of any kind outside of
    // Chrome OS.
    options.add_to_applications_menu = false;
    options.add_to_search = false;
    options.add_to_management = false;
    options.add_to_desktop = false;
    options.add_to_quick_launch_bar = false;
    options.install_without_os_integration = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

    if (g_override_previous_user_uninstall_for_testing_) {
      options.override_previous_user_uninstall = true;
    }
  }

  // TODO(crbug.com/40747215): Move this constant into some shared constants.h
  // file.
  bool preinstalled_apps_enabled_in_prefs =
      profile_->GetPrefs()->GetString(prefs::kPreinstalledApps) == "install";
  bool is_new_user = IsNewUser();
  std::string user_type = apps::DetermineUserType(profile_);
  size_t disabled_count = 0;
  size_t corrupt_user_uninstall_prefs_count = 0;
  std::erase_if(
      parsed_configs.options_list, [&](const ExternalInstallOptions& options) {
        SynchronizeDecision install_decision = GetSynchronizeDecision(
            options, profile_, &provider_->registrar_unsafe(),
            preinstalled_apps_enabled_in_prefs, is_new_user, user_type,
            corrupt_user_uninstall_prefs_count);
        base::UmaHistogramEnumeration(kHistogramMigrationDisabledReason,
                                      install_decision.reason);

        switch (install_decision.type) {
          case SynchronizeDecision::kUninstall:
            VLOG(1) << install_decision.log;
            ++disabled_count;
            if (debug_info_) {
              debug_info_->uninstall_configs.emplace_back(
                  options, std::move(install_decision.log));
            }
            return true;

          case SynchronizeDecision::kInstall:
            if (debug_info_) {
              debug_info_->install_configs.emplace_back(
                  options, std::move(install_decision.log));
            }
            return false;

          case SynchronizeDecision::kIgnore:
            if (debug_info_) {
              debug_info_->ignore_configs.emplace_back(
                  options, std::move(install_decision.log));
            }
            // These configs get passed to SynchronizeInstalledApps() which has
            // no concept of kIgnore, only ensuring installation or
            // uninstallation based on presence/absence of the config. In order
            // for the config to be ignored (as in no installation or
            // uninstallation taking place) the config presence needs to match
            // whether the install_source + install_url is already present in
            // the installed web apps.
            return !provider_->registrar_unsafe()
                        .LookUpAppByInstallSourceInstallUrl(
                            WebAppManagement::Type::kDefault,
                            options.install_url);
        }
      });

  if (debug_info_) {
    debug_info_->parse_errors = parsed_configs.errors;
  }

  for (ExternalInstallOptions& options : parsed_configs.options_list) {
    if (ShouldForceReinstall(options, *profile_->GetPrefs(),
                             provider_->registrar_unsafe())) {
      options.force_reinstall = true;
    }
  }

  base::UmaHistogramCounts100(kHistogramEnabledCount,
                              parsed_configs.options_list.size());
  base::UmaHistogramCounts100(kHistogramDisabledCount, disabled_count);
  base::UmaHistogramCounts100(kHistogramConfigErrorCount,
                              parsed_configs.errors.size());
  base::UmaHistogramCounts100(kHistogramCorruptUserUninstallPrefsCount,
                              corrupt_user_uninstall_prefs_count);

  std::move(callback).Run(parsed_configs.options_list);
}

void PreinstalledWebAppManager::Synchronize(
    ExternallyManagedAppManager::SynchronizeCallback callback,
    std::vector<ExternalInstallOptions> desired_apps_install_options) {
  DCHECK(provider_);

  std::set<InstallUrl> desired_preferred_apps_for_supported_links;
  std::map<InstallUrl, std::vector<webapps::AppId>> desired_uninstalls;
  for (const auto& entry : desired_apps_install_options) {
    if (entry.is_preferred_app_for_supported_links) {
      desired_preferred_apps_for_supported_links.insert(entry.install_url);
    }
    if (!entry.uninstall_and_replace.empty()) {
      desired_uninstalls.emplace(entry.install_url,
                                 entry.uninstall_and_replace);
    }
  }

  provider_->externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalDefault,
      base::BindOnce(&PreinstalledWebAppManager::OnExternalWebAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(desired_preferred_apps_for_supported_links),
                     std::move(desired_uninstalls)));
}

void PreinstalledWebAppManager::OnExternalWebAppsSynchronized(
    ExternallyManagedAppManager::SynchronizeCallback callback,
    std::set<InstallUrl> desired_preferred_apps_for_supported_links,
    std::map<InstallUrl, std::vector<webapps::AppId>> desired_uninstalls,
    std::map<InstallUrl, ExternallyManagedAppManager::InstallResult>
        install_results,
    std::map<InstallUrl, webapps::UninstallResultCode> uninstall_results) {
  // Note that we are storing the Chrome version (milestone number) instead of a
  // "has synchronised" bool in order to do version update specific logic.
  profile_->GetPrefs()->SetString(
      prefs::kWebAppsLastPreinstallSynchronizeVersion,
      version_info::GetMajorVersionNumber());

  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);

  size_t uninstall_and_replace_count = 0;
  size_t app_to_replace_still_installed_count = 0;
  size_t app_to_replace_still_default_installed_count = 0;
  size_t app_to_replace_still_installed_in_shelf_count = 0;

  for (const auto& [url, result] : install_results) {
    base::UmaHistogramEnumeration(kHistogramInstallResult, result.code);
    if (result.did_uninstall_and_replace) {
      ++uninstall_and_replace_count;
    }

    if (!IsSuccess(result.code)) {
      continue;
    }

    DCHECK(result.app_id.has_value());

    // Do not set as the preferred app for supported links if the app is
    // already installed as the user may have already updated their preference.
    if (result.code != webapps::InstallResultCode::kSuccessAlreadyInstalled &&
        desired_preferred_apps_for_supported_links.contains(url)) {
      proxy->SetSupportedLinksPreference(*result.app_id);
    }

    auto iter = desired_uninstalls.find(url);
    if (iter == desired_uninstalls.end()) {
      continue;
    }

    for (const webapps::AppId& replace_id : iter->second) {
      // We mark the app as migrated to a web app as long as the
      // installation was successful, even if the previous app was not
      // installed. This ensures we properly re-install apps if the
      // migration feature is rolled back.
      MarkAppAsMigratedToWebApp(profile_, replace_id, /*was_migrated=*/true);

      // Track whether the app to replace is still present. This is
      // possibly due to getting reinstalled by the user or by Chrome app
      // sync. See https://crbug.com/1266234 for context.
      if (proxy &&
          result.code == webapps::InstallResultCode::kSuccessAlreadyInstalled) {
        bool is_installed = false;
        proxy->AppRegistryCache().ForOneApp(
            replace_id, [&is_installed](const apps::AppUpdate& app) {
              is_installed = apps_util::IsInstalled(app.Readiness());
            });

        if (!is_installed) {
          continue;
        }

        ++app_to_replace_still_installed_count;

        if (extensions::IsExtensionDefaultInstalled(profile_, replace_id)) {
          ++app_to_replace_still_default_installed_count;
        }

        if (provider_->ui_manager().CanAddAppToQuickLaunchBar()) {
          if (provider_->ui_manager().IsAppInQuickLaunchBar(
                  result.app_id.value())) {
            ++app_to_replace_still_installed_in_shelf_count;
          }
        }
      }
    }
  }

  size_t uninstall_source_removed_count = 0;
  size_t uninstall_app_removed_count = 0;

  for (const auto& [url, result] : uninstall_results) {
    if (result == webapps::UninstallResultCode::kInstallSourceRemoved) {
      ++uninstall_source_removed_count;
    } else if (result == webapps::UninstallResultCode::kAppRemoved) {
      ++uninstall_app_removed_count;
    }
  }

  base::UmaHistogramCounts100(kHistogramInstallCount, install_results.size());
  base::UmaHistogramCounts100(kHistogramUninstallTotalCount,
                              uninstall_results.size());
  base::UmaHistogramCounts100(kHistogramUninstallSourceRemovedCount,
                              uninstall_source_removed_count);
  base::UmaHistogramCounts100(kHistogramUninstallAppRemovedCount,
                              uninstall_app_removed_count);
  base::UmaHistogramCounts100(kHistogramUninstallAndReplaceCount,
                              uninstall_and_replace_count);

  base::UmaHistogramCounts100(kHistogramAppToReplaceStillInstalledCount,
                              app_to_replace_still_installed_count);
  base::UmaHistogramCounts100(kHistogramAppToReplaceStillDefaultInstalledCount,
                              app_to_replace_still_default_installed_count);
  base::UmaHistogramCounts100(kHistogramAppToReplaceStillInstalledInShelfCount,
                              app_to_replace_still_installed_in_shelf_count);

  SetMigrationRun(profile_, "MigrateDefaultChromeAppToWebAppsGSuite", true);
  SetMigrationRun(profile_, "MigrateDefaultChromeAppToWebAppsNonGSuite", true);
  if (uninstall_and_replace_count > 0) {
    for (auto& observer : observers_) {
      observer.OnMigrationRun();
    }
  }

  if (callback) {
    std::move(callback).Run(std::move(install_results),
                            std::move(uninstall_results));
  }
}

void PreinstalledWebAppManager::OnStartUpTaskCompleted(
    std::map<InstallUrl, ExternallyManagedAppManager::InstallResult>
        install_results,
    std::map<InstallUrl, webapps::UninstallResultCode> uninstall_results) {
  if (debug_info_) {
    debug_info_->is_start_up_task_complete = true;
    debug_info_->install_results = std::move(install_results);
    debug_info_->uninstall_results = std::move(uninstall_results);
  }
}

bool PreinstalledWebAppManager::IsNewUser() {
  PrefService* prefs = profile_->GetPrefs();
  return prefs->GetString(prefs::kWebAppsLastPreinstallSynchronizeVersion)
      .empty();
}

PreinstalledWebAppManager::DebugInfo::DebugInfo() = default;

PreinstalledWebAppManager::DebugInfo::~DebugInfo() = default;

}  //  namespace web_app
