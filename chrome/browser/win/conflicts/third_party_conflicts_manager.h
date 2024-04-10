// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/win/conflicts/installed_applications.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_updater.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"
#include "chrome/browser/win/conflicts/module_list_component_updater.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"

class IncompatibleApplicationsUpdater;
class InstalledApplications;
class ModuleListFilter;
class PrefRegistrySimple;
struct CertificateInfo;

namespace base {
class FilePath;
class SequencedTaskRunner;
class TaskRunner;
class Version;
}  // namespace base

// This class is responsible for the initialization of the
// IncompatibleApplicationsWarning and ThirdPartyModulesBlocking features. Each
// feature requires a set of dependencies to be initialized on a background
// sequence because their main class can be created
// (IncompatibleApplicationsUpdater and ModuleBlocklistCacheUpdater
// respectively).
//
// Dependencies list
// For both features:
// 1. |exe_certificate_info_| contains info about the certificate of the current
//    executable.
// 2. |module_list_filter_| is used to determine if a module should be blocked
//    or allowed. The Module List component is received from the component
//    update service, which invokes OnModuleListComponentRegistered() and
//    LoadModuleList() when appropriate.
//
// For the IncompatibleApplicationsWarning feature only:
// 3. |installed_applications_| allows to tie a loaded module to an application
//    installed on the computer.
//
// For the ThirdPartyModulesBlocking feature only:
// 4. |initial_blocklisted_modules_| contains the list of modules that were
//    blocklisted at the time the browser was launched. Modifications to that
//    list do not take effect until a restart.
//
class ThirdPartyConflictsManager : public ModuleDatabaseObserver {
 public:
  // |module_database_event_source| must outlive this.
  explicit ThirdPartyConflictsManager(
      ModuleDatabaseEventSource* module_database_event_source);

  ThirdPartyConflictsManager(const ThirdPartyConflictsManager&) = delete;
  ThirdPartyConflictsManager& operator=(const ThirdPartyConflictsManager&) =
      delete;

  ~ThirdPartyConflictsManager() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Explicitely disables the third-party module blocking feature. This is
  // needed because simply turning off the feature using either the Feature List
  // API or via group policy is not sufficient. Disabling the blocking requires
  // the deletion of the module blocklist cache. This task is executed on
  // |background_sequence|.
  static void DisableThirdPartyModuleBlocking(
      base::TaskRunner* background_sequence);

  // Explicitely disables the blocking of third-party modules for the next
  // browser launch and prevent |instance| from reenabling it. Basically calls
  // the above function in the background sequence of |instance| and then
  // deletes that instance.
  static void ShutdownAndDestroy(
      std::unique_ptr<ThirdPartyConflictsManager> instance);

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

  // Invoked when the Third Party Module List component is registered with the
  // component update service. Checks if the component is currently installed or
  // if an update is required.
  void OnModuleListComponentRegistered(std::string_view component_id,
                                       const base::Version& component_version);

  // Loads the |module_list_filter_| using the Module List at |path|.
  void LoadModuleList(const base::FilePath& path);

  void SetInstalledApplicationsForTesting(
      std::unique_ptr<InstalledApplications> installed_applications) {
    installed_applications_ = std::move(installed_applications);
  }

  // Force the initialization of the IncompatibleApplicationsUpdater and the
  // ModuleBlocklistCacheUpdater instances by triggering an update of the module
  // list component, if needed. Immediately invokes
  // |on_initialization_event_callback| if this instance is already in a final
  // state (Failed to initialize or fully initialized). This is only meant to be
  // used when the chrome://conflicts page is opened by the user.
  enum class State {
    // The initialization failed because the Module List component couldn't be
    // used to initialize the ModuleListFilter.
    kModuleListInvalidFailure,
    // The initialization failed because there was no Module List version
    // available to install.
    kNoModuleListAvailableFailure,
    // Only the IncompatibleApplicationsWarning feature is enabled and active.
    kWarningInitialized,
    // Only the ThirdPartyModulesBlocking feature is enabled and active.
    kBlockingInitialized,
    // Both the IncompatibleApplicationsWarning and ThirdPartyModulesBlocking
    // features are enabled and active.
    kWarningAndBlockingInitialized,
    // The instance is about to be deleted.
    kDestroyed,
  };
  using OnInitializationCompleteCallback =
      base::OnceCallback<void(State state)>;
  void ForceInitialization(
      OnInitializationCompleteCallback on_initialization_complete_callback);

  // Returns the IncompatibleApplicationsUpdater instance. Returns null if the
  // corresponding feature is disabled (IncompatibleApplicationsWarning).
  IncompatibleApplicationsUpdater* incompatible_applications_updater() {
    return incompatible_applications_updater_.get();
  }

  // Returns the ModuleBlocklistCacheUpdater instance. Returns null if the
  // corresponding feature is disabled (ThirdPartyModulesBlocking).
  ModuleBlocklistCacheUpdater* module_blocklist_cache_updater() {
    return module_blocklist_cache_updater_.get();
  }

  // Disables the analysis of newly found modules.
  void DisableModuleAnalysis();

 private:
  // Called when |module_list_filter_| finishes its initialization.
  void OnModuleListFilterCreated(
      scoped_refptr<ModuleListFilter> module_list_filter);

  // Called when |installed_applications_| finishes its initialization.
  void OnInstalledApplicationsCreated(
      std::unique_ptr<InstalledApplications> installed_applications);

  // Called when |initial_blocklisted_modules_| finishes its initialization.
  void OnInitialBlocklistedModulesRead(
      std::unique_ptr<std::vector<third_party_dlls::PackedListModule>>
          initial_blocklisted_modules);

  // Initializes either or both |incompatible_applications_updater_| and
  // |module_blocklist_cache_updater_| when the exe_certificate_info_, the
  // module_list_filter_ and the installed_applications_ are available.
  void InitializeIfReady();

  // Checks if the |old_md5_digest| matches the expected one from the Local
  // State file, and updates it to |new_md5_digest|.
  void OnModuleBlocklistCacheUpdated(
      const ModuleBlocklistCacheUpdater::CacheUpdateResult& result);

  // Forcibly triggers an update of the Third Party Module List component. Only
  // invoked when ForceInitialization() is called.
  void ForceModuleListComponentUpdate();

  // Callback for when the component update service was not able to download the
  // module list component. Successful updates will cause the LoadModuleList()
  // function to be invoked instead.
  void OnModuleListComponentNotUpdated();

  // Modifies the current state and invokes
  // |on_initialization_complete_callback_|.
  void SetTerminalState(State terminal_state);

  const raw_ptr<ModuleDatabaseEventSource> module_database_event_source_;

  scoped_refptr<base::SequencedTaskRunner> background_sequence_;

  // Indicates if the initial Module List has been received. Used to prevent the
  // creation of multiple ModuleListFilter instances.
  bool module_list_received_;

  // Indicates if the OnModuleDatabaseIdle() function has been called once
  // already. Used to prevent the creation of multiple InstalledApplications
  // instances.
  bool on_module_database_idle_called_;

  // Path to the current executable (expected to be chrome.exe).
  base::FilePath exe_path_;

  // The certificate info of the current executable.
  std::unique_ptr<CertificateInfo> exe_certificate_info_;

  // Holds the id of the Third Party Module List component.
  std::string module_list_component_id_;

  // Remembers if ForceInitialization() was invoked.
  bool initialization_forced_;

  // Indicates if an update to the Module List component is needed to initialize
  // the ModuleListFilter.
  bool module_list_update_needed_;

  // Responsible for forcing an update to the Module List component on the UI
  // thread if none is currently installed.
  ModuleListComponentUpdater::UniquePtr module_list_component_updater_;

  // Filters third-party modules against an allowlist and a blocklist. This
  // instance is ref counted because the |module_blocklist_cache_updater_| must
  // use it on a background sequence.
  scoped_refptr<ModuleListFilter> module_list_filter_;

  // The blocklisted modules contained in the cache used to initialize the
  // blocking in chrome_elf.
  std::unique_ptr<std::vector<third_party_dlls::PackedListModule>>
      initial_blocklisted_modules_;

  // Retrieves the list of installed applications.
  std::unique_ptr<InstalledApplications> installed_applications_;

  // Maintains the module blocklist cache. This member is only initialized when
  // the ThirdPartyModuleBlocking feature is enabled.
  std::unique_ptr<ModuleBlocklistCacheUpdater> module_blocklist_cache_updater_;

  // Maintains the cache of incompatible applications. This member is only
  // initialized when the IncompatibleApplicationsWarning feature is enabled.
  std::unique_ptr<IncompatibleApplicationsUpdater>
      incompatible_applications_updater_;

  // The final state of this instance.
  std::optional<State> terminal_state_;

  // The callback that is invoked when |state_| changes.
  OnInitializationCompleteCallback on_initialization_complete_callback_;

  // Indicates if the analysis of newly found modules is disabled. Used as a
  // workaround for https://crbug.com/892294.
  bool module_analysis_disabled_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ThirdPartyConflictsManager> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_H_
