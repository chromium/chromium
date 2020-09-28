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
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/external_web_app_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_apps.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

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
  int disabled_count = 0;
  int error_count = 0;
};

ParsedConfigs ParseConfigsBlocking(const base::FilePath& config_dir,
                                   const std::string& user_type,
                                   LoadedConfigs loaded_configs) {
  ParsedConfigs result;
  result.error_count = loaded_configs.error_count;

  auto file_utils = g_file_utils_for_testing
                        ? g_file_utils_for_testing->Clone()
                        : std::make_unique<FileUtilsWrapper>();

  for (const LoadedConfig& loaded_config : loaded_configs.configs) {
    ExternalConfigParseResult parse_result =
        ParseConfig(*file_utils, config_dir, loaded_config.file, user_type,
                    loaded_config.contents);
    switch (parse_result.type) {
      case ExternalConfigParseResult::kEnabled:
        result.options_list.push_back(std::move(parse_result.options.value()));
        break;
      case ExternalConfigParseResult::kDisabled:
        ++result.disabled_count;
        break;
      case ExternalConfigParseResult::kError:
        ++result.error_count;
        break;
    }
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
                     apps::DetermineUserType(profile_),
                     std::move(loaded_configs)),
      std::move(callback));
}

void ExternalWebAppManager::PostProcessConfigs(ConsumeInstallOptions callback,
                                               ParsedConfigs parsed_configs) {
  // Add hard coded configs.
  PreinstalledWebApps preinstalled_web_apps = GetPreinstalledWebApps();
  for (ExternalInstallOptions& options : preinstalled_web_apps.options)
    parsed_configs.options_list.push_back(std::move(options));
  parsed_configs.disabled_count += preinstalled_web_apps.disabled_count;

  // Save this as we may remove apps due to user uninstall (not the same as
  // being disabled).
  int enabled_count = parsed_configs.options_list.size();

  // Remove web apps whose replace target was uninstalled.
  if (extensions::ExtensionSystem::Get(profile_)) {
    auto* extension_prefs = extensions::ExtensionPrefs::Get(profile_);
    auto* extension_registry = extensions::ExtensionRegistry::Get(profile_);

    base::EraseIf(
        parsed_configs.options_list,
        [&](const ExternalInstallOptions& options) {
          for (const AppId& app_id : options.uninstall_and_replace) {
            if (extension_registry->GetInstalledExtension(app_id))
              return false;
          }

          for (const AppId& app_id : options.uninstall_and_replace) {
            if (extension_prefs->IsExternalExtensionUninstalled(app_id))
              return true;
          }

          return false;
        });
  }

  base::UmaHistogramCounts100(ExternalWebAppManager::kHistogramEnabledCount,
                              enabled_count);
  base::UmaHistogramCounts100(ExternalWebAppManager::kHistogramDisabledCount,
                              parsed_configs.disabled_count);
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

}  //  namespace web_app
