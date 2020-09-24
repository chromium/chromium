// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"

namespace base {
class FilePath;
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

  static void SkipStartupForTesting();
  static void SetConfigDirForTesting(const base::FilePath* config_dir);
  static void SetConfigsForTesting(const std::vector<base::Value>* configs);
  static void SetFileUtilsForTesting(const FileUtilsWrapper* file_utils);

  explicit ExternalWebAppManager(Profile* profile);
  ~ExternalWebAppManager();

  void SetSubsystems(PendingAppManager* pending_app_manager);

  // Loads the preinstalled app configs and synchronizes them with the device's
  // installed apps.
  void Start();

  void LoadAndSynchronizeForTesting(SynchronizeCallback callback);

  void LoadForTesting(ConsumeInstallOptions callback);

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
      std::map<GURL, InstallResultCode> install_results,
      std::map<GURL, bool> uninstall_results);

  base::FilePath GetConfigDir();

  PendingAppManager* pending_app_manager_ = nullptr;
  Profile* const profile_;

  base::WeakPtrFactory<ExternalWebAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_WEB_APP_MANAGER_H_
