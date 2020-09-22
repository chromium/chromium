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
#include "base/no_destructor.h"
#include "base/path_service.h"
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

std::vector<ExternalInstallOptions> LoadInstallOptionsBlocking(
    std::unique_ptr<FileUtilsWrapper> file_utils,
    const base::FilePath& dir,
    const std::string& user_type) {
  std::vector<ExternalInstallOptions> install_options_list;
  if (!base::FeatureList::IsEnabled(features::kDefaultWebAppInstallation))
    return install_options_list;

  for (ExternalInstallOptions& options : GetPreinstalledWebApps())
    install_options_list.push_back(std::move(options));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath::StringType extension(FILE_PATH_LITERAL(".json"));
  base::FileEnumerator json_files(dir,
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
      continue;
    }
    base::Optional<ExternalInstallOptions> install_options =
        ParseConfig(*file_utils, dir, file, user_type, *app_config);
    if (install_options.has_value())
      install_options_list.push_back(std::move(*install_options));
  }

  return install_options_list;
}

base::FilePath DetermineLoadDir(const Profile* profile) {
  base::FilePath dir;
#if defined(OS_CHROMEOS)
  // As of mid 2018, only Chrome OS has default/external web apps, and
  // chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS is only defined for OS_LINUX,
  // which includes OS_CHROMEOS.

  if (chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    // For manual testing, you can change s/STANDALONE/USER/, as writing to
    // "$HOME/.config/chromium/test-user/.config/chromium/External
    // Extensions/web_apps" does not require root ACLs, unlike
    // "/usr/share/chromium/extensions/web_apps".
    if (!base::PathService::Get(chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
                                &dir)) {
      LOG(ERROR) << "ExternalWebAppManager::LoadInstallOptions: "
                    "base::PathService::Get failed";
    } else {
      dir = dir.Append(kWebAppsSubDirectory);
    }
  }

#endif
  return dir;
}

void OnExternalWebAppsSynchronized(
    std::map<GURL, InstallResultCode> install_results,
    std::map<GURL, bool> uninstall_results) {
  RecordExternalAppInstallResultCode("Webapp.InstallResult.Default",
                                     install_results);
}

std::vector<ExternalInstallOptions> SynchronizeAppsBlockingForTesting(
    std::unique_ptr<FileUtilsWrapper> file_utils,
    std::vector<std::string> app_configs,
    const std::string& user_type) {
  std::vector<ExternalInstallOptions> install_options_list;

  for (const std::string& app_config_string : app_configs) {
    base::Optional<base::Value> app_config =
        base::JSONReader::Read(app_config_string);
    DCHECK(app_config);

    base::Optional<ExternalInstallOptions> install_options =
        ParseConfig(*file_utils, base::FilePath(FILE_PATH_LITERAL("test_dir")),
                    base::FilePath(FILE_PATH_LITERAL("test_dir/test.json")),
                    user_type, *app_config);
    if (install_options)
      install_options_list.push_back(std::move(*install_options));
  }

  // TODO(crbug.com/1128801): Dedupe this with LoadInstallOptionsBlocking().
  for (ExternalInstallOptions& options : GetPreinstalledWebApps())
    install_options_list.push_back(std::move(options));

  return install_options_list;
}

}  // namespace

ExternalWebAppManager::ExternalWebAppManager(Profile* profile)
    : profile_(profile) {}

ExternalWebAppManager::~ExternalWebAppManager() = default;

void ExternalWebAppManager::SetSubsystems(
    PendingAppManager* pending_app_manager) {
  pending_app_manager_ = pending_app_manager;
}

void ExternalWebAppManager::Start() {
  if (!g_skip_startup_for_testing_) {
    LoadInstallOptions(base::BindOnce(
        &ExternalWebAppManager::SynchronizeExternalInstallOptions,
        weak_ptr_factory_.GetWeakPtr(),
        base::BindOnce(&OnExternalWebAppsSynchronized)));
  }
}

// static
std::vector<ExternalInstallOptions>
ExternalWebAppManager::ReloadInstallOptionsForTesting(
    std::unique_ptr<FileUtilsWrapper> file_utils,
    const base::FilePath& dir,
    Profile* profile) {
  return LoadInstallOptionsBlocking(std::move(file_utils), dir,
                                    apps::DetermineUserType(profile));
}

void ExternalWebAppManager::LoadInstallOptions(LoadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Do a two-part callback dance, across different TaskRunners.
  //
  // 1. Schedule LoadInstallOptionsBlocking to happen on a background thread, so
  // that we don't block the UI thread. When that's done,
  // base::PostTaskAndReplyWithResult will bounce us back to the originating
  // thread (the UI thread).
  //
  // 2. In |callback|, forward the vector of ExternalInstallOptions on to the
  // pending_app_manager_, which can only be called on the UI thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &LoadInstallOptionsBlocking, std::make_unique<FileUtilsWrapper>(),
          DetermineLoadDir(profile_), apps::DetermineUserType(profile_)),
      std::move(callback));
}

void ExternalWebAppManager::SkipStartupForTesting() {
  g_skip_startup_for_testing_ = true;
}

void ExternalWebAppManager::SynchronizeAppsForTesting(
    std::unique_ptr<FileUtilsWrapper> file_utils,
    std::vector<std::string> app_configs,
    PendingAppManager::SynchronizeCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&SynchronizeAppsBlockingForTesting, std::move(file_utils),
                     std::move(app_configs), apps::DetermineUserType(profile_)),
      base::BindOnce(&ExternalWebAppManager::SynchronizeExternalInstallOptions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ExternalWebAppManager::SynchronizeExternalInstallOptions(
    PendingAppManager::SynchronizeCallback callback,
    std::vector<ExternalInstallOptions> desired_apps_install_options) {
  DCHECK(pending_app_manager_);

  pending_app_manager_->SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalDefault, std::move(callback));
}

}  //  namespace web_app
