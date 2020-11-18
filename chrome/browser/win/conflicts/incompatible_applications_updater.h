// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_INCOMPATIBLE_APPLICATIONS_UPDATER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_INCOMPATIBLE_APPLICATIONS_UPDATER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/browser/win/conflicts/installed_applications.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"
#include "chrome/browser/win/conflicts/proto/module_list.pb.h"

class ModuleListFilter;
class PrefRegistrySimple;
struct CertificateInfo;

// Maintains a list of incompatible applications that are installed on the
// machine. These applications cause unwanted DLLs to be loaded into Chrome.
//
// Because the list is expensive to build, it is cached into the Local State
// file so that it is available at startup.
class IncompatibleApplicationsUpdater : public ModuleDatabaseObserver {
 public:
  // The decision that explains why a particular module caused an
  // incompatibility warning or not.
  //
  // Note that this enum is very similar to the ModuleBlockingDecision in
  // ModuleBlacklistCacheUpdater. This is done so that it is easier to keep the
  // 2 features separate, as they can be independently enabled/disabled.
  enum ModuleWarningDecision {
    // No decision was taken yet for the module.
    kUnknown = 0,
    // A shell extension or IME that is not loaded in the process yet.
    kNotLoaded,
    // A module that is loaded into a process type where third-party modules are
    // explicitly allowed.
    kAllowedInProcessType,
    // Input method editors are allowed.
    kAllowedIME,
    // Shell extensions are unwanted, but does not cause trigger a warning.
    kAllowedShellExtension,
    // Allowed because the certificate's subject of the module matches the
    // certificate's subject of the executable. The certificate is not
    // validated.
    kAllowedSameCertificate,
    // Allowed because the path of the executable is the parent of the path of
    // the module.
    kAllowedSameDirectory,
    // Allowed because it is signed by Microsoft. The certificate is not
    // validated.
    kAllowedMicrosoft,
    // Explicitly whitelisted by the Module List component.
    kAllowedWhitelisted,
    // Module analysis was interrupted using DisableModuleAnalysis(). No warning
    // will be emitted for that module.
    kNotAnalyzed,
    // This module is already going to be blocked on next browser launch, so
    // don't warn about it.
    kAddedToBlacklist,
    // Unwanted, but can't tie back to an installed application.
    kNoTiedApplication,
    // An incompatibility warning will be shown because of this module.
    kIncompatible,
  };

  struct IncompatibleApplication {
    IncompatibleApplication(
        InstalledApplications::ApplicationInfo info,
        std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action);
    ~IncompatibleApplication();

    // Needed for std::remove_if().
    IncompatibleApplication(IncompatibleApplication&& incompatible_application);
    IncompatibleApplication& operator=(
        IncompatibleApplication&& incompatible_application);

    InstalledApplications::ApplicationInfo info;
    std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action;
  };

  // Creates an instance of the updater.
  // The parameters must outlive the lifetime of this class.
  IncompatibleApplicationsUpdater(
      ModuleDatabaseEventSource* module_database_event_source,
      const CertificateInfo& exe_certificate_info,
      scoped_refptr<ModuleListFilter> module_list_filter,
      const InstalledApplications& installed_applications,
      bool module_analysis_disabled);
  ~IncompatibleApplicationsUpdater() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns true if the tracking of incompatible applications is enabled. Can
  // be called on any thread. Notably does not check the
  // ThirdPartyBlockingEnabled group policy.
  static bool IsWarningEnabled();

  // Returns true if the cache contains at least one incompatible application.
  // Only call this if IsIncompatibleApplicationsWarningEnabled() returns true.
  static bool HasCachedApplications();

  // Returns all the cached incompatible applications.
  // Only call this if IsIncompatibleApplicationsWarningEnabled() returns true.
  static std::vector<IncompatibleApplication> GetCachedApplications();

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnKnownModuleLoaded(const ModuleInfoKey& module_key,
                           const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

  // Returns the warning decision for a module.
  ModuleWarningDecision GetModuleWarningDecision(
      const ModuleInfoKey& module_key) const;

  // Disables the analysis of newly found modules. This is a one way switch that
  // will apply until Chrome is restarted.
  void DisableModuleAnalysis();

 private:
  ModuleDatabaseEventSource* const module_database_event_source_;

  const CertificateInfo& exe_certificate_info_;
  scoped_refptr<ModuleListFilter> module_list_filter_;
  const InstalledApplications& installed_applications_;

  // Temporarily holds incompatible applications that were recently found.
  std::vector<IncompatibleApplication> incompatible_applications_;

  // Becomes false on the first call to OnModuleDatabaseIdle.
  bool before_first_idle_ = true;

  // Holds the warning decision for all known modules.
  base::flat_map<ModuleInfoKey, ModuleWarningDecision>
      module_warning_decisions_;

  // Indicates if the analysis of newly found modules is disabled. Used as a
  // workaround for https://crbug.com/892294.
  bool module_analysis_disabled_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IncompatibleApplicationsUpdater);
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_INCOMPATIBLE_APPLICATIONS_UPDATER_H_
