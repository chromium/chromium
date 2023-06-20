// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment.h"
#endif

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace web_app {

namespace {
struct LoadedConfigs;
struct ParsedConfigs;
}  // namespace

class ExternallyManagedAppManager;
class WebAppRegistrar;

// Installs web apps to be preinstalled on the device (AKA default apps) during
// start up. Will keep the apps installed on the device in sync with the set of
// apps configured for preinstall, adding or removing as necessary. Works very
// similar to WebAppPolicyManager.
class PreinstalledWebAppManager {
 public:
  using ConsumeLoadedConfigs = base::OnceCallback<void(LoadedConfigs)>;
  using ConsumeParsedConfigs = base::OnceCallback<void(ParsedConfigs)>;
  using ConsumeInstallOptions =
      base::OnceCallback<void(std::vector<ExternalInstallOptions>)>;
  using SynchronizeCallback = ExternallyManagedAppManager::SynchronizeCallback;
  using InstallUrl = GURL;

  // Observes whether default chrome app migration has completed and
  // triggers MostVisitedHandler to refresh the NTP tiles.
  class Observer : public base::CheckedObserver {
   public:
    // Triggered when preinstalled web app synchronization completes and the
    // pref kWebAppsMigratedPreinstalledApps is filled.
    virtual void OnMigrationRun() = 0;
    // Used to destroy the scoped observation instance if
    // PreinstalledWebAppManager instance is destroyed before the scoped
    // observation, preventing UAF.
    virtual void OnDestroyed() = 0;
  };

  static const char* kHistogramEnabledCount;
  static const char* kHistogramDisabledCount;
  static const char* kHistogramConfigErrorCount;
  static const char* kHistogramCorruptUserUninstallPrefsCount;
  static const char* kHistogramInstallResult;
  static const char* kHistogramUninstallAndReplaceCount;
  static const char* kHistogramAppToReplaceStillInstalledCount;
  static const char* kHistogramAppToReplaceStillDefaultInstalledCount;
  static const char* kHistogramAppToReplaceStillInstalledInShelfCount;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // TODO(crbug.com/1434692): All these should return a base::AutoReset<bool> to
  // avoid leaking override state beyond unit test execution.
  static void SkipStartupForTesting();
  static base::AutoReset<bool> BypassAwaitingDependenciesForTesting();
  static void BypassOfflineManifestRequirementForTesting();

  static void OverridePreviousUserUninstallConfigForTesting();
  static void SetConfigDirForTesting(const base::FilePath* config_dir);

  static void SetConfigsForTesting(const base::Value::List* configs);
  static void SetFileUtilsForTesting(FileUtilsWrapper* file_utils);

  explicit PreinstalledWebAppManager(Profile* profile);
  PreinstalledWebAppManager(const PreinstalledWebAppManager&) = delete;
  PreinstalledWebAppManager& operator=(const PreinstalledWebAppManager&) =
      delete;
  ~PreinstalledWebAppManager();

  void SetSubsystems(
      WebAppRegistrar* registrar,
      const WebAppUiManager* ui_manager,
      ExternallyManagedAppManager* externally_managed_app_manager);

  // Loads the preinstalled app configs and synchronizes them with the device's
  // installed apps.
  void Start(base::OnceClosure on_init_complete);

  void LoadAndSynchronizeForTesting(SynchronizeCallback callback);

  void LoadForTesting(ConsumeInstallOptions callback);

  void AddObserver(PreinstalledWebAppManager::Observer* observer);

  void RemoveObserver(PreinstalledWebAppManager::Observer* observer);

  // Must be called before `Start`. Similar to `SkipStartupForTesting` but not a
  // global setting.
  void SetSkipStartupSynchronizeForTesting(bool skip_startup);

#if BUILDFLAG(IS_CHROMEOS)
  PreinstalledWebAppWindowExperiment& GetWindowExperimentForTesting();
#endif

  // Debugging info used by: chrome://web-app-internals
  struct DebugInfo {
    DebugInfo();
    ~DebugInfo();

    bool is_start_up_task_complete = false;
    std::vector<std::string> parse_errors;
    std::vector<ExternalInstallOptions> enabled_configs;
    using DisabledConfigWithReason =
        std::pair<ExternalInstallOptions, std::string>;
    std::vector<DisabledConfigWithReason> disabled_configs;
    std::map<InstallUrl, ExternallyManagedAppManager::InstallResult>
        install_results;
    std::map<InstallUrl, bool> uninstall_results;
  };
  const DebugInfo* debug_info() const { return debug_info_.get(); }

 private:
  // Helper to delay a task until device information is fully initialized in
  // ui::DeviceDataManager.
  class DeviceDataInitializedEvent;

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
      ExternallyManagedAppManager::SynchronizeCallback callback,
      std::map<InstallUrl, std::vector<AppId>> desired_uninstall_and_replaces,
      std::map<InstallUrl, ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<InstallUrl, bool> uninstall_results);
  void OnStartUpTaskCompleted(
      std::map<InstallUrl, ExternallyManagedAppManager::InstallResult>
          install_results,
      std::map<InstallUrl, bool> uninstall_results);

  // Returns whether this is the first time we've deployed default apps on this
  // profile.
  bool IsNewUser();

  // |force_reinstall_for_milestone| is a major version number. See
  // components/version_info/version_info.h.
  bool IsReinstallPastMilestoneNeededSinceLastSync(
      int force_reinstall_for_milestone);

  raw_ptr<WebAppRegistrar, DanglingUntriaged> registrar_ = nullptr;
  raw_ptr<const WebAppUiManager, DanglingUntriaged> ui_manager_ = nullptr;
  raw_ptr<ExternallyManagedAppManager, DanglingUntriaged>
      externally_managed_app_manager_ = nullptr;
  const raw_ptr<Profile> profile_;

#if BUILDFLAG(IS_CHROMEOS)
  PreinstalledWebAppWindowExperiment preinstalled_web_app_window_experiment_;
#endif

  bool skip_startup_for_testing_ = false;
  std::unique_ptr<DebugInfo> debug_info_;

  std::unique_ptr<DeviceDataInitializedEvent> device_data_initialized_event_;

  base::ObserverList<PreinstalledWebAppManager::Observer, /*check_empty=*/true>
      observers_;

  base::WeakPtrFactory<PreinstalledWebAppManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_MANAGER_H_
