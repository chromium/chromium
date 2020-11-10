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
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
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
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_util.h"
#endif  // defined(OS_CHROMEOS)

namespace web_app {

namespace {

#if defined(OS_CHROMEOS)
// The sub-directory of the extensions directory in which to scan for external
// web apps (as opposed to external extensions or external ARC apps).
const base::FilePath::CharType kWebAppsSubDirectory[] =
    FILE_PATH_LITERAL("web_apps");
#endif

bool g_skip_startup_for_testing_ = false;
const base::FilePath* g_config_dir_for_testing = nullptr;
const std::vector<base::Value>* g_configs_for_testing = nullptr;
const FileUtilsWrapper* g_file_utils_for_testing = nullptr;

struct LoadedConfig {
  base::Value contents;
  base::FilePath file;
};

struct LoadedConfigs {
  std::vector<LoadedConfig> configs;
  int error_count = 0;
};

LoadedConfigs LoadConfigsBlocking(const base::FilePath& config_dir) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  LoadedConfigs result;
  base::FilePath::StringType extension(FILE_PATH_LITERAL(".json"));
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
      LOG(ERROR) << file.value() << " was not valid JSON: " << error_msg;
      ++result.error_count;
      continue;
    }
    result.configs.push_back(
        {.contents = std::move(*app_config), .file = file});
  }
  return result;
}

struct ParsedConfigs {
  std::vector<ExternalInstallOptions> options_list;
  int error_count = 0;
};

ParsedConfigs ParseConfigsBlocking(const base::FilePath& config_dir,
                                   LoadedConfigs loaded_configs) {
  ParsedConfigs result;
  result.error_count = loaded_configs.error_count;

  auto file_utils = g_file_utils_for_testing
                        ? g_file_utils_for_testing->Clone()
                        : std::make_unique<FileUtilsWrapper>();

  for (const LoadedConfig& loaded_config : loaded_configs.configs) {
    base::Optional<ExternalInstallOptions> parse_result = ParseConfig(
        *file_utils, config_dir, loaded_config.file, loaded_config.contents);
    if (parse_result)
      result.options_list.push_back(std::move(*parse_result));
    else
      ++result.error_count;
  }

  return result;
}

}  // namespace

const char* ExternalWebAppManager::kHistogramEnabledCount =
    "WebApp.Preinstalled.EnabledCount";
const char* ExternalWebAppManager::kHistogramDisabledCount =
    "WebApp.Preinstalled.DisabledCount";
const char* ExternalWebAppManager::kHistogramConfigErrorCount =
    "WebApp.Preinstalled.ConfigErrorCount";

void ExternalWebAppManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kWebAppsLastPreinstallSynchronizeVersion,
                               "");
}

void ExternalWebAppManager::SkipStartupForTesting() {
  g_skip_startup_for_testing_ = true;
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
    : profile_(profile) {}

ExternalWebAppManager::~ExternalWebAppManager() = default;

void ExternalWebAppManager::SetSubsystems(
    PendingAppManager* pending_app_manager) {
  pending_app_manager_ = pending_app_manager;
}

void ExternalWebAppManager::Start() {
  if (!g_skip_startup_for_testing_)
    LoadAndSynchronize({});
}

void ExternalWebAppManager::LoadForTesting(ConsumeInstallOptions callback) {
  Load(std::move(callback));
}

void ExternalWebAppManager::LoadAndSynchronizeForTesting(
    SynchronizeCallback callback) {
  LoadAndSynchronize(std::move(callback));
}

void ExternalWebAppManager::LoadAndSynchronize(SynchronizeCallback callback) {
  Load(base::BindOnce(&ExternalWebAppManager::Synchronize,
                      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
      loaded_configs.configs.push_back(
          {.contents = config.Clone(),
           .file = base::FilePath(FILE_PATH_LITERAL("test.json"))});
    }
    std::move(callback).Run(std::move(loaded_configs));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&LoadConfigsBlocking, GetConfigDir()),
      std::move(callback));
}

void ExternalWebAppManager::ParseConfigs(ConsumeParsedConfigs callback,
                                         LoadedConfigs loaded_configs) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ParseConfigsBlocking, GetConfigDir(),
                     std::move(loaded_configs)),
      std::move(callback));
}

void ExternalWebAppManager::PostProcessConfigs(ConsumeInstallOptions callback,
                                               ParsedConfigs parsed_configs) {
  // Add hard coded configs.
  for (ExternalInstallOptions& options : GetPreinstalledWebApps())
    parsed_configs.options_list.push_back(std::move(options));

  const int total_count = parsed_configs.options_list.size();
  int disabled_count = 0;
  bool is_new_user = IsNewUser();
  std::string user_type = apps::DetermineUserType(profile_);
  base::EraseIf(
      parsed_configs.options_list, [&](const ExternalInstallOptions& options) {
        // Remove if not applicable to current user type.
        DCHECK_GT(options.user_type_allowlist.size(), 0u);
        if (!base::Contains(options.user_type_allowlist, user_type)) {
          ++disabled_count;
          return true;
        }

        // Remove if gated on a disabled feature.
        if (options.gate_on_feature &&
            !IsExternalAppInstallFeatureEnabled(*options.gate_on_feature)) {
          ++disabled_count;
          return true;
        }

#if defined(OS_CHROMEOS)
        // Remove if ARC is supported and app should be disabled.
        if (options.disable_if_arc_supported && arc::IsArcAvailable()) {
          ++disabled_count;
          return true;
        }

        // Remove if device is tablet and app should be disabled.
        if (options.disable_if_tablet_form_factor &&
            chromeos::switches::IsTabletFormFactor()) {
          ++disabled_count;
          return true;
        }
#endif  // defined(OS_CHROMEOS)

        // Remove if only for new users, user isn't new and app was not
        // installed previously.
        if (options.only_for_new_users && !is_new_user) {
          bool was_previously_installed =
              ExternallyInstalledWebAppPrefs(profile_->GetPrefs())
                  .LookupAppId(options.install_url)
                  .has_value();
          if (!was_previously_installed)
            return true;
        }

        // Remove if any apps to replace are blocked by admin policy.
        for (const AppId& app_id : options.uninstall_and_replace) {
          if (extensions::IsExtensionBlockedByPolicy(profile_, app_id)) {
            ++disabled_count;
            return true;
          }
        }

        // Keep if any apps to replace are installed.
        for (const AppId& app_id : options.uninstall_and_replace) {
          if (extensions::IsExtensionInstalled(profile_, app_id))
            return false;
        }

        // Remove if any apps to replace were previously uninstalled.
        for (const AppId& app_id : options.uninstall_and_replace) {
          if (extensions::IsExternalExtensionUninstalled(profile_, app_id))
            return true;
        }

        return false;
      });

  base::UmaHistogramCounts100(ExternalWebAppManager::kHistogramEnabledCount,
                              total_count - disabled_count);
  base::UmaHistogramCounts100(ExternalWebAppManager::kHistogramDisabledCount,
                              disabled_count);
  base::UmaHistogramCounts100(ExternalWebAppManager::kHistogramConfigErrorCount,
                              parsed_configs.error_count);

  std::move(callback).Run(std::move(parsed_configs.options_list));
}

void ExternalWebAppManager::Synchronize(
    PendingAppManager::SynchronizeCallback callback,
    std::vector<ExternalInstallOptions> desired_apps_install_options) {
  DCHECK(pending_app_manager_);

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalDefault,
      base::BindOnce(&ExternalWebAppManager::OnExternalWebAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExternalWebAppManager::OnExternalWebAppsSynchronized(
    PendingAppManager::SynchronizeCallback callback,
    std::map<GURL, InstallResultCode> install_results,
    std::map<GURL, bool> uninstall_results) {
  // Note that we are storing the Chrome version instead of a "has synchronised"
  // bool in order to do version update specific logic in the future.
  profile_->GetPrefs()->SetString(
      prefs::kWebAppsLastPreinstallSynchronizeVersion,
      version_info::GetMajorVersionNumber());

  RecordExternalAppInstallResultCode("Webapp.InstallResult.Default",
                                     install_results);
  if (callback) {
    std::move(callback).Run(std::move(install_results),
                            std::move(uninstall_results));
  }
}

base::FilePath ExternalWebAppManager::GetConfigDir() {
  base::FilePath dir;

#if defined(OS_CHROMEOS)
  // As of mid 2018, only Chrome OS has default/external web apps, and
  // chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX,
  // which includes OS_CHROMEOS.
  if (chromeos::ProfileHelper::IsPrimaryProfile(profile_)) {
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

}  //  namespace web_app
