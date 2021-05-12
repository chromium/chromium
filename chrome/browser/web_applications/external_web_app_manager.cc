// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_manager.h"

#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/external_web_app_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/arc/arc_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The sub-directory of the extensions directory in which to scan for external
// web apps (as opposed to external extensions or external ARC apps).
const base::FilePath::CharType kWebAppsSubDirectory[] =
    FILE_PATH_LITERAL("web_apps");
#endif

bool g_skip_startup_for_testing_ = false;
bool g_bypass_offline_manifest_requirement_for_testing_ = false;
const base::FilePath* g_config_dir_for_testing = nullptr;
const std::vector<base::Value>* g_configs_for_testing = nullptr;
const FileUtilsWrapper* g_file_utils_for_testing = nullptr;

struct LoadedConfig {
  base::Value contents;
  base::FilePath file;
};

struct LoadedConfigs {
  std::vector<LoadedConfig> configs;
  std::vector<std::string> errors;
};

LoadedConfigs LoadConfigsBlocking(std::vector<base::FilePath> config_dirs) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  LoadedConfigs result;
  base::FilePath::StringType extension(FILE_PATH_LITERAL(".json"));

  for (auto config_dir : config_dirs) {
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

  auto file_utils = g_file_utils_for_testing
                        ? g_file_utils_for_testing->Clone()
                        : std::make_unique<FileUtilsWrapper>();

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

base::Optional<std::string> GetDisableReason(
    const ExternalInstallOptions& options,
    Profile* profile,
    bool default_apps_enabled_in_prefs,
    bool is_new_user,
    const std::string& user_type) {
  if (!default_apps_enabled_in_prefs) {
    return options.install_url.spec() +
           " disabled by default_apps pref setting.";
  }

  // Remove if not applicable to current user type.
  DCHECK_GT(options.user_type_allowlist.size(), 0u);
  if (!base::Contains(options.user_type_allowlist, user_type)) {
    return options.install_url.spec() + " disabled for user type: " + user_type;
  }

  // Remove if gated on a disabled feature.
  if (options.gate_on_feature &&
      !IsExternalAppInstallFeatureEnabled(*options.gate_on_feature, *profile)) {
    return options.install_url.spec() +
           " disabled because feature is disabled: " + *options.gate_on_feature;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Remove if ARC is supported and app should be disabled.
  if (options.disable_if_arc_supported && arc::IsArcAvailable()) {
    return options.install_url.spec() + " disabled because ARC is available.";
  }

  // Remove if device is tablet and app should be disabled.
  if (options.disable_if_tablet_form_factor &&
      chromeos::switches::IsTabletFormFactor()) {
    return options.install_url.spec() + " disabled because device is tablet.";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Remove if only for new users, user isn't new and app was not
  // installed previously.
  if (options.only_for_new_users && !is_new_user) {
    bool was_previously_installed =
        ExternallyInstalledWebAppPrefs(profile->GetPrefs())
            .LookupAppId(options.install_url)
            .has_value();
    if (!was_previously_installed) {
      return options.install_url.spec() +
             " disabled because user was not new when config was added.";
    }
  }

  // Remove if any apps to replace are blocked by admin policy.
  for (const AppId& app_id : options.uninstall_and_replace) {
    if (extensions::IsExtensionBlockedByPolicy(profile, app_id)) {
      return options.install_url.spec() +
             " disabled due to admin policy blocking replacement "
             "Extension.";
    }
  }

  // Keep if any apps to replace are installed.
  for (const AppId& app_id : options.uninstall_and_replace) {
    if (extensions::IsExtensionInstalled(profile, app_id)) {
      return base::nullopt;
    }
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Remove if it's a default app and the apps to replace are not installed and
  // default extension apps are not performing new installation.
  if (options.gate_on_feature && !options.uninstall_and_replace.empty() &&
      !extensions::DidDefaultAppsPerformNewInstallation(profile)) {
    for (const AppId& app_id : options.uninstall_and_replace) {
      // First time migration and the app to replace is uninstalled as it passed
      // the last code block. Save the information that the app was
      // uninstalled by user.
      if (!WasMigrationRun(profile, *options.gate_on_feature)) {
        if (extensions::IsDefaultAppId(app_id)) {
          MarkDefaultAppAsUninstalled(profile, app_id);
          return options.install_url.spec() +
                 "disabled because it's default app and apps to replace were "
                 "uninstalled.";
        }
      } else {
        // Not first time migration, can't determine if the app to replace is
        // uninstalled by user as the migration is already run, use the pref
        // saved in first migration.
        if (WasDefaultAppUninstalled(profile, app_id)) {
          return options.install_url.spec() +
                 "disabled because it's default app and apps to replace were "
                 "uninstalled.";
        }
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Remove if any apps to replace were previously uninstalled.
  for (const AppId& app_id : options.uninstall_and_replace) {
    if (extensions::IsExternalExtensionUninstalled(profile, app_id)) {
      return options.install_url.spec() +
             " disabled because apps to replace were uninstalled.";
    }
  }

  return base::nullopt;
}

std::string GetExtraConfigSubdirectory() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      chromeos::switches::kExtraWebAppsDir);
#else
  return std::string();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

const char* ExternalWebAppManager::kHistogramEnabledCount =
    "WebApp.Preinstalled.EnabledCount";
const char* ExternalWebAppManager::kHistogramDisabledCount =
    "WebApp.Preinstalled.DisabledCount";
const char* ExternalWebAppManager::kHistogramConfigErrorCount =
    "WebApp.Preinstalled.ConfigErrorCount";
const char* ExternalWebAppManager::kHistogramInstallResult =
    "Webapp.InstallResult.Default";
const char* ExternalWebAppManager::kHistogramUninstallAndReplaceCount =
    "WebApp.Preinstalled.UninstallAndReplaceCount";

void ExternalWebAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kWebAppsLastPreinstallSynchronizeVersion,
                               "");
  registry->RegisterListPref(prefs::kWebAppsMigratedDefaultApps);
  registry->RegisterListPref(prefs::kWebAppsDidMigrateDefaultChromeApps);
  registry->RegisterListPref(prefs::kWebAppsUninstalledDefaultChromeApps);
}

void ExternalWebAppManager::SkipStartupForTesting() {
  g_skip_startup_for_testing_ = true;
}

void ExternalWebAppManager::BypassOfflineManifestRequirementForTesting() {
  g_bypass_offline_manifest_requirement_for_testing_ = true;
}

void ExternalWebAppManager::SetConfigDirForTesting(
    const base::FilePath* config_dir) {
  g_config_dir_for_testing = config_dir;
}

void ExternalWebAppManager::SetConfigsForTesting(
    const std::vector<base::Value>* configs) {
  g_configs_for_testing = configs;
}

void ExternalWebAppManager::SetFileUtilsForTesting(
    const FileUtilsWrapper* file_utils) {
  g_file_utils_for_testing = file_utils;
}

ExternalWebAppManager::ExternalWebAppManager(Profile* profile)
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    debug_info_ = std::make_unique<DebugInfo>();
  }
}

ExternalWebAppManager::~ExternalWebAppManager() = default;

void ExternalWebAppManager::SetSubsystems(
    PendingAppManager* pending_app_manager) {
  pending_app_manager_ = pending_app_manager;
}

void ExternalWebAppManager::Start() {
  if (!g_skip_startup_for_testing_) {
    LoadAndSynchronize(
        base::BindOnce(&ExternalWebAppManager::OnStartUpTaskCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ExternalWebAppManager::LoadForTesting(ConsumeInstallOptions callback) {
  Load(std::move(callback));
}

void ExternalWebAppManager::LoadAndSynchronizeForTesting(
    SynchronizeCallback callback) {
  LoadAndSynchronize(std::move(callback));
}

void ExternalWebAppManager::LoadAndSynchronize(SynchronizeCallback callback) {
  // Make sure ExtensionSystem is ready to know if default apps new installation
  // will be performed.
  extensions::OnExtensionSystemReady(
      profile_,
      base::BindOnce(
          &ExternalWebAppManager::Load, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&ExternalWebAppManager::Synchronize,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ExternalWebAppManager::Load(ConsumeInstallOptions callback) {
  if (!base::FeatureList::IsEnabled(features::kDefaultWebAppInstallation)) {
    std::move(callback).Run({});
    return;
  }

  LoadConfigs(base::BindOnce(
      &ExternalWebAppManager::ParseConfigs, weak_ptr_factory_.GetWeakPtr(),
      base::BindOnce(&ExternalWebAppManager::PostProcessConfigs,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ExternalWebAppManager::LoadConfigs(ConsumeLoadedConfigs callback) {
  if (g_configs_for_testing) {
    LoadedConfigs loaded_configs;
    for (const base::Value& config : *g_configs_for_testing) {
      auto file = base::FilePath(FILE_PATH_LITERAL("test.json"));
      if (g_config_dir_for_testing) {
        file = g_config_dir_for_testing->Append(file);
      }

      loaded_configs.configs.push_back(
          {.contents = config.Clone(), .file = file});
    }
    std::move(callback).Run(std::move(loaded_configs));
    return;
  }

  base::FilePath config_dir = GetConfigDir();
  if (config_dir.empty()) {
    std::move(callback).Run({});
    return;
  }

  std::vector<base::FilePath> config_dirs = {config_dir};
  std::string extra_config_subdir = GetExtraConfigSubdirectory();
  if (!extra_config_subdir.empty()) {
    config_dirs.push_back(config_dir.AppendASCII(extra_config_subdir));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadConfigsBlocking, std::move(config_dirs)),
      std::move(callback));
}

void ExternalWebAppManager::ParseConfigs(ConsumeParsedConfigs callback,
                                         LoadedConfigs loaded_configs) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ParseConfigsBlocking, std::move(loaded_configs)),
      std::move(callback));
}

void ExternalWebAppManager::PostProcessConfigs(ConsumeInstallOptions callback,
                                               ParsedConfigs parsed_configs) {
  // Add hard coded configs.
  for (ExternalInstallOptions& options : GetPreinstalledWebApps())
    parsed_configs.options_list.push_back(std::move(options));

  // Set common install options.
  for (ExternalInstallOptions& options : parsed_configs.options_list) {
    ALLOW_UNUSED_LOCAL(options);
    DCHECK_EQ(options.install_source, ExternalInstallSource::kExternalDefault);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  // TODO(crbug.com/1175196): Move this constant into some shared constants.h
  // file.
  bool default_apps_enabled_in_prefs =
      profile_->GetPrefs()->GetString(prefs::kDefaultApps) == "install";
  bool is_new_user = IsNewUser();
  std::string user_type = apps::DetermineUserType(profile_);
  size_t disabled_count = 0;
  base::EraseIf(
      parsed_configs.options_list, [&](const ExternalInstallOptions& options) {
        base::Optional<std::string> disable_reason =
            GetDisableReason(options, profile_, default_apps_enabled_in_prefs,
                             is_new_user, user_type);
        if (disable_reason) {
          VLOG(1) << *disable_reason;
          ++disabled_count;
          if (debug_info_) {
            debug_info_->disabled_configs.emplace_back(
                std::move(options), std::move(*disable_reason));
          }
          return true;
        }
        return false;
      });

  if (debug_info_) {
    debug_info_->parse_errors = parsed_configs.errors;
    debug_info_->enabled_configs = parsed_configs.options_list;
  }

  // Triggers |force_reinstall| in PendingAppManager if milestone increments
  // across |force_reinstall_for_milestone|.
  for (ExternalInstallOptions& options : parsed_configs.options_list) {
    if (options.force_reinstall_for_milestone &&
        IsReinstallPastMilestoneNeededSinceLastSync(
            options.force_reinstall_for_milestone.value())) {
      options.force_reinstall = true;
    }
  }

  UMA_HISTOGRAM_COUNTS_100(kHistogramEnabledCount,
                           parsed_configs.options_list.size());
  UMA_HISTOGRAM_COUNTS_100(kHistogramDisabledCount, disabled_count);
  UMA_HISTOGRAM_COUNTS_100(kHistogramConfigErrorCount,
                           parsed_configs.errors.size());

  std::move(callback).Run(parsed_configs.options_list);
}

void ExternalWebAppManager::Synchronize(
    PendingAppManager::SynchronizeCallback callback,
    std::vector<ExternalInstallOptions> desired_apps_install_options) {
  DCHECK(pending_app_manager_);

  std::map<GURL, std::vector<AppId>> desired_uninstalls;
  for (const auto& entry : desired_apps_install_options) {
    if (!entry.uninstall_and_replace.empty())
      desired_uninstalls.emplace(entry.install_url,
                                 entry.uninstall_and_replace);
  }
  pending_app_manager_->SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalDefault,
      base::BindOnce(&ExternalWebAppManager::OnExternalWebAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(desired_uninstalls)));
}

void ExternalWebAppManager::OnExternalWebAppsSynchronized(
    PendingAppManager::SynchronizeCallback callback,
    std::map<GURL, std::vector<AppId>> desired_uninstalls,
    std::map<GURL, PendingAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  // Note that we are storing the Chrome version (milestone number) instead of a
  // "has synchronised" bool in order to do version update specific logic.
  profile_->GetPrefs()->SetString(
      prefs::kWebAppsLastPreinstallSynchronizeVersion,
      version_info::GetMajorVersionNumber());

  size_t uninstall_and_replace_count = 0;
  for (const auto& url_and_result : install_results) {
    UMA_HISTOGRAM_ENUMERATION(kHistogramInstallResult,
                              url_and_result.second.code);
    if (url_and_result.second.did_uninstall_and_replace) {
      ++uninstall_and_replace_count;
    }
    // We mark the app as migrated to a web app as long as the installation
    // was successful, even if the previous app was not installed. This ensures
    // we properly re-install apps if the migration feature is rolled back.
    if (IsSuccess(url_and_result.second.code)) {
      auto iter = desired_uninstalls.find(url_and_result.first);
      if (iter != desired_uninstalls.end()) {
        for (const auto& uninstalled_id : iter->second) {
          MarkAppAsMigratedToWebApp(profile_, uninstalled_id, true);
        }
      }
    }
  }
  UMA_HISTOGRAM_COUNTS_100(kHistogramUninstallAndReplaceCount,
                           uninstall_and_replace_count);

  SetMigrationRun(profile_, kMigrateDefaultChromeAppToWebAppsGSuite.name,
                  IsExternalAppInstallFeatureEnabled(
                      kMigrateDefaultChromeAppToWebAppsGSuite.name, *profile_));
  SetMigrationRun(
      profile_, kMigrateDefaultChromeAppToWebAppsNonGSuite.name,
      IsExternalAppInstallFeatureEnabled(
          kMigrateDefaultChromeAppToWebAppsNonGSuite.name, *profile_));

  if (callback) {
    std::move(callback).Run(std::move(install_results),
                            std::move(uninstall_results));
  }
}

void ExternalWebAppManager::OnStartUpTaskCompleted(
    std::map<GURL, PendingAppManager::InstallResult> install_results,
    std::map<GURL, bool> uninstall_results) {
  if (debug_info_) {
    debug_info_->is_start_up_task_complete = true;
    debug_info_->install_results = std::move(install_results);
    debug_info_->uninstall_results = std::move(uninstall_results);
  }
}

base::FilePath ExternalWebAppManager::GetConfigDir() {
  base::FilePath dir;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // As of mid 2018, only Chrome OS has default/external web apps, and
  // chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX,
  // which includes OS_CHROMEOS.
  if (chromeos::ProfileHelper::IsRegularProfile(profile_)) {
    if (g_config_dir_for_testing) {
      dir = *g_config_dir_for_testing;
    } else {
      // For manual testing, you can change s/STANDALONE/USER/, as writing to
      // "$HOME/.config/chromium/test-user/.config/chromium/External
      // Extensions/web_apps" does not require root ACLs, unlike
      // "/usr/share/chromium/extensions/web_apps".
      if (!base::PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                                  &dir)) {
        LOG(ERROR) << "base::PathService::Get failed";
      } else {
        dir = dir.Append(kWebAppsSubDirectory);
      }
    }
  }
#endif

  return dir;
}

bool ExternalWebAppManager::IsNewUser() {
  PrefService* prefs = profile_->GetPrefs();
  std::string last_version =
      prefs->GetString(prefs::kWebAppsLastPreinstallSynchronizeVersion);
  if (!last_version.empty())
    return false;
  // It's not enough to check whether the last_version string has been set
  // because users have been around before this pref was introduced (M88). We
  // distinguish those users via the presence of any
  // ExternallyInstalledWebAppPrefs which would have been set by past default
  // app installs. Remove this after a few Chrome versions have passed.
  return ExternallyInstalledWebAppPrefs(prefs).HasNoApps();
}

bool ExternalWebAppManager::IsReinstallPastMilestoneNeededSinceLastSync(
    int force_reinstall_for_milestone) {
  PrefService* prefs = profile_->GetPrefs();
  std::string last_preinstall_synchronize_milestone =
      prefs->GetString(prefs::kWebAppsLastPreinstallSynchronizeVersion);

  return IsReinstallPastMilestoneNeeded(last_preinstall_synchronize_milestone,
                                        version_info::GetMajorVersionNumber(),
                                        force_reinstall_for_milestone);
}

ExternalWebAppManager::DebugInfo::DebugInfo() = default;

ExternalWebAppManager::DebugInfo::~DebugInfo() = default;

}  //  namespace web_app
