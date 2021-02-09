// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace web_app {

namespace {
struct LoadedConfigs;
struct ParsedConfigs;
}  // namespace

class PendingAppManager;

// Installs web apps to be preinstalled on the device (AKA default apps) during
// start up. Will keep the apps installed on the device in sync with the set of
// apps configured for preinstall, adding or removing as necessary. Works very
// similar to WebAppPolicyManager.
class ExternalWebAppManager {
 public:
  using ConsumeLoadedConfigs = base::OnceCallback<void(LoadedConfigs)>;
  using ConsumeParsedConfigs = base::OnceCallback<void(ParsedConfigs)>;
  using ConsumeInstallOptions =
      base::OnceCallback<void(std::vector<ExternalInstallOptions>)>;
  using SynchronizeCallback = PendingAppManager::SynchronizeCallback;

  static const char* kHistogramEnabledCount;
  static const char* kHistogramDisabledCount;
  static const char* kHistogramConfigErrorCount;
  static const char* kHistogramInstallResult;
  static const char* kHistogramUninstallAndReplaceCount;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void SkipStartupForTesting();
  static void BypassOfflineManifestRequirementForTesting();
  static void SetConfigDirForTesting(const base::FilePath* config_dir);
  static void SetConfigsForTesting(const std::vector<base::Value>* configs);
  static void SetFileUtilsForTesting(const FileUtilsWrapper* file_utils);

  explicit ExternalWebAppManager(Profile* profile);
  ExternalWebAppManager(const ExternalWebAppManager&) = delete;
  ExternalWebAppManager& operator=(const ExternalWebAppManager&) = delete;
  ~ExternalWebAppManager();

  void SetSubsystems(PendingAppManager* pending_app_manager);

  // Loads the preinstalled app configs and synchronizes them with the device's
  // installed apps.
  void Start();

  void LoadAndSynchronizeForTesting(SynchronizeCallback callback);

  void LoadForTesting(ConsumeInstallOptions callback);

  // Debugging info used by: chrome://internals/web-app
  struct DebugInfo {
    DebugInfo();
    ~DebugInfo();

    bool is_start_up_task_complete = false;
    std::vector<std::string> parse_errors;
    std::vector<ExternalInstallOptions> enabled_configs;
    using DisabledConfigWithReason =
        std::pair<ExternalInstallOptions, std::string>;
    std::vector<DisabledConfigWithReason> disabled_configs;
    std::map<GURL, PendingAppManager::InstallResult> install_results;
    std::map<GURL, bool> uninstall_results;
  };
  const DebugInfo* debug_info() const { return debug_info_.get(); }

 private:
  void LoadAndSynchronize(SynchronizeCallback callback);

  void Load(ConsumeInstallOptions callback);
  void LoadConfigs(ConsumeLoadedConfigs callback);
  void ParseConfigs(ConsumeParsedConfigs callback,
                    LoadedConfigs loaded_configs);
  void PostProcessConfigs(ConsumeInstallOptions callback,
                          ParsedConfigs parsed_configs);

  void Synchronize(SynchronizeCallback callback,
                   std::vector<ExternalInstallOptions>);
  void OnExternalWebAppsSynchronized(
      PendingAppManager::SynchronizeCallback callback,
      std::map<GURL, std::vector<AppId>> desired_uninstall_and_replaces,
      std::map<GURL, PendingAppManager::InstallResult> install_results,
      std::map<GURL, bool> uninstall_results);
  void OnStartUpTaskCompleted(
      std::map<GURL, PendingAppManager::InstallResult> install_results,
      std::map<GURL, bool> uninstall_results);

  // The directory where default web app configs are stored.
  // Empty if not applicable.
  base::FilePath GetConfigDir();

  // Returns whether this is the first time we've deployed default apps on this
  // profile.
  bool IsNewUser();

  // |force_reinstall_for_milestone| is a major version number. See
  // components/version_info/version_info.h.
  bool IsReinstallPastMilestoneNeededSinceLastSync(
      int force_reinstall_for_milestone);

  PendingAppManager* pending_app_manager_ = nullptr;
  Profile* const profile_;

  std::unique_ptr<DebugInfo> debug_info_;

  base::WeakPtrFactory<ExternalWebAppManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_
