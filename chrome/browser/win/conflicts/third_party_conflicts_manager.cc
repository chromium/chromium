// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/installed_applications.h"
#include "chrome/browser/win/conflicts/module_blacklist_cache_updater.h"
#include "chrome/browser/win/conflicts/module_blacklist_cache_util.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/chrome_elf/third_party_dlls/status_codes.h"
#include "chrome/common/pref_names.h"
#include "chrome/install_static/install_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

scoped_refptr<ModuleListFilter> CreateModuleListFilter(
    const base::FilePath& module_list_path) {
  auto module_list_filter = base::MakeRefCounted<ModuleListFilter>();

  if (!module_list_filter->Initialize(module_list_path))
    return nullptr;

  return module_list_filter;
}

std::unique_ptr<std::vector<third_party_dlls::PackedListModule>>
ReadInitialBlacklistedModules() {
  base::FilePath path =
      ModuleBlacklistCacheUpdater::GetModuleBlacklistCachePath();

  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  ReadResult read_result = ReadModuleBlacklistCache(
      path, &metadata, &blacklisted_modules, &md5_digest);

  // Return an empty vector on failure.
  auto initial_blacklisted_modules =
      std::make_unique<std::vector<third_party_dlls::PackedListModule>>();
  if (read_result == ReadResult::kSuccess)
    *initial_blacklisted_modules = std::move(blacklisted_modules);

  return initial_blacklisted_modules;
}

// Log the initialization status of the chrome_elf component that is responsible
// for blocking third-party DLLs. The status is stored in the
// kStatusCodesRegValue registry key during chrome_elf's initialization.
void LogChromeElfThirdPartyStatus() {
  base::win::RegKey registry_key(
      HKEY_CURRENT_USER,
      base::StringPrintf(L"%ls%ls", install_static::GetRegistryPath().c_str(),
                         third_party_dlls::kThirdPartyRegKeyName)
          .c_str(),
      KEY_QUERY_VALUE);

  // Early return if the registry key can't be opened.
  if (!registry_key.Valid())
    return;

  // Read the status code. Since the data is basically an array of integers, and
  // resets every startups, a starting size of 128 should always be sufficient.
  DWORD size = 128;
  std::vector<uint8_t> buffer(size);
  DWORD key_type;
  DWORD result = registry_key.ReadValue(third_party_dlls::kStatusCodesRegValue,
                                        buffer.data(), &size, &key_type);

  if (result == ERROR_MORE_DATA) {
    buffer.resize(size);
    result = registry_key.ReadValue(third_party_dlls::kStatusCodesRegValue,
                                    buffer.data(), &size, &key_type);
  }

  // Give up if it failed to retrieve the status codes.
  if (result != ERROR_SUCCESS)
    return;

  // The real size of the data is most probably smaller than the initial size.
  buffer.resize(size);

  // Sanity check the type of data.
  if (key_type != REG_BINARY)
    return;

  std::vector<third_party_dlls::ThirdPartyStatus> status_array;
  third_party_dlls::ConvertBufferToStatusCodes(buffer, &status_array);

  for (auto status : status_array)
    UMA_HISTOGRAM_ENUMERATION("ChromeElf.ThirdPartyStatus", status);
}

// Updates the current value of the kModuleBlacklistCacheMD5Digest pref.
void UpdateModuleBlacklistCacheMD5Digest(
    const ModuleBlacklistCacheUpdater::CacheUpdateResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check that the MD5 digest of the old cache matches what was expected. Only
  // used for reporting a metric.
  const PrefService::Preference* preference =
      g_browser_process->local_state()->FindPreference(
          prefs::kModuleBlacklistCacheMD5Digest);
  DCHECK(preference);

  // The first time this is executed, the pref doesn't yet hold a valid MD5
  // digest.
  if (!preference->IsDefaultValue()) {
    const std::string old_md5_string =
        base::MD5DigestToBase16(result.old_md5_digest);
    const std::string& current_md5_string = preference->GetValue()->GetString();
    UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.ExpectedMD5Digest",
                          old_md5_string == current_md5_string);
  }

  // Set the expected MD5 digest for the next time the cache is updated.
  g_browser_process->local_state()->Set(
      prefs::kModuleBlacklistCacheMD5Digest,
      base::Value(base::MD5DigestToBase16(result.new_md5_digest)));
}

void ClearModuleBlacklistCacheMD5Digest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_browser_process->local_state()->ClearPref(
      prefs::kModuleBlacklistCacheMD5Digest);
}

}  // namespace

ThirdPartyConflictsManager::ThirdPartyConflictsManager(
    ModuleDatabaseEventSource* module_database_event_source)
    : module_database_event_source_(module_database_event_source),
      background_sequence_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})),
      module_list_received_(false),
      on_module_database_idle_called_(false),
      initialization_forced_(false),
      module_list_update_needed_(false),
      module_list_component_updater_(nullptr,
                                     base::OnTaskRunnerDeleter(nullptr)),
      weak_ptr_factory_(this) {
  LogChromeElfThirdPartyStatus();

  module_database_event_source_->AddObserver(this);

  // Get the path to the current executable as it will be used to retrieve its
  // associated CertificateInfo from the ModuleDatabase. This shouldn't fail,
  // but it is assumed that without the path, the executable is not signed
  // (hence an empty CertificateInfo).
  if (!base::PathService::Get(base::FILE_EXE, &exe_path_))
    exe_certificate_info_ = std::make_unique<CertificateInfo>();
}

ThirdPartyConflictsManager::~ThirdPartyConflictsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!terminal_state_.has_value())
    SetTerminalState(State::kDestroyed);
  module_database_event_source_->RemoveObserver(this);
}

// static
void ThirdPartyConflictsManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Register the pref that remembers the MD5 digest for the current module
  // blacklist cache. The default value is an invalid MD5 digest.
  registry->RegisterStringPref(prefs::kModuleBlacklistCacheMD5Digest, "");
}

// static
void ThirdPartyConflictsManager::DisableThirdPartyModuleBlocking(
    base::TaskRunner* background_sequence) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delete the module blacklist cache. Since the NtMapViewOfSection hook only
  // blocks if the file is present, this will deactivate third-party modules
  // blocking for the next browser launch.
  background_sequence->PostTask(
      FROM_HERE,
      base::BindOnce(&ModuleBlacklistCacheUpdater::DeleteModuleBlacklistCache));

  // Also clear the MD5 digest since there will no longer be a current module
  // blacklist cache.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ClearModuleBlacklistCacheMD5Digest));
}

// static
void ThirdPartyConflictsManager::ShutdownAndDestroy(
    std::unique_ptr<ThirdPartyConflictsManager> instance) {
  DisableThirdPartyModuleBlocking(instance->background_sequence_.get());
  // |instance| is intentionally destroyed at the end of the function scope.
}

void ThirdPartyConflictsManager::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Keep looking for the CertificateInfo of the current executable as long as
  // it wasn't found yet.
  if (exe_certificate_info_)
    return;

  DCHECK(!exe_path_.empty());

  // The module represent the current executable only if the paths matches.
  if (exe_path_ != module_key.module_path)
    return;

  exe_certificate_info_ = std::make_unique<CertificateInfo>(
      module_data.inspection_result->certificate_info);

  InitializeIfReady();
}

void ThirdPartyConflictsManager::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_module_database_idle_called_)
    return;

  on_module_database_idle_called_ = true;

  // The InstalledApplications instance is only needed for the incompatible
  // applications warning.
  if (IncompatibleApplicationsUpdater::IsWarningEnabled()) {
    base::PostTaskAndReplyWithResult(
        background_sequence_.get(), FROM_HERE, base::BindOnce([]() {
          return std::make_unique<InstalledApplications>();
        }),
        base::BindOnce(
            &ThirdPartyConflictsManager::OnInstalledApplicationsCreated,
            weak_ptr_factory_.GetWeakPtr()));
  }

  // And the initial blacklisted modules are only needed for the third-party
  // modules blocking.
  if (ModuleBlacklistCacheUpdater::IsBlockingEnabled()) {
    base::PostTaskAndReplyWithResult(
        background_sequence_.get(), FROM_HERE,
        base::BindOnce(&ReadInitialBlacklistedModules),
        base::BindOnce(
            &ThirdPartyConflictsManager::OnInitialBlacklistedModulesRead,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ThirdPartyConflictsManager::OnModuleListComponentRegistered(
    base::StringPiece component_id,
    const base::Version& component_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(module_list_component_id_.empty());
  module_list_component_id_ = component_id.as_string();

  if (component_version == base::Version("0.0.0.0")) {
    // The module list component is currently not installed. An update is
    // required to initialize the ModuleListFilter.
    module_list_update_needed_ = true;

    // The update is usually done automatically when the component update
    // service decides to do it. But if the initialization was forced, the
    // component update must also be triggered right now.
    if (initialization_forced_)
      ForceModuleListComponentUpdate();
  }

  // LoadModuleList() will be called if the version is not "0.0.0.0".
}

void ThirdPartyConflictsManager::LoadModuleList(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (module_list_received_)
    return;

  module_list_component_updater_ = nullptr;

  module_list_received_ = true;

  base::PostTaskAndReplyWithResult(
      background_sequence_.get(), FROM_HERE,
      base::BindOnce(&CreateModuleListFilter, path),
      base::BindOnce(&ThirdPartyConflictsManager::OnModuleListFilterCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThirdPartyConflictsManager::ForceInitialization(
    OnInitializationCompleteCallback on_initialization_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  on_initialization_complete_callback_ =
      std::move(on_initialization_complete_callback);

  if (terminal_state_.has_value()) {
    std::move(on_initialization_complete_callback_)
        .Run(terminal_state_.value());
    return;
  }

  // It doesn't make sense to do this twice.
  if (initialization_forced_)
    return;
  initialization_forced_ = true;

  // Nothing to force if we already received a module list.
  if (module_list_received_)
    return;

  // Only force an update if it is needed, because the ModuleListFilter can be
  // initialized with an older version of the Module List component.
  if (module_list_update_needed_)
    ForceModuleListComponentUpdate();
}

void ThirdPartyConflictsManager::DisableModuleAnalysis() {
  module_analysis_disabled_ = true;
  if (incompatible_applications_updater_)
    incompatible_applications_updater_->DisableModuleAnalysis();
  if (module_blacklist_cache_updater_)
    module_blacklist_cache_updater_->DisableModuleAnalysis();
}

void ThirdPartyConflictsManager::OnModuleListFilterCreated(
    scoped_refptr<ModuleListFilter> module_list_filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  module_list_filter_ = std::move(module_list_filter);

  // A valid |module_list_filter_| is critical to the blocking of third-party
  // modules. By returning early here, the |incompatible_applications_updater_|
  // instance never gets created, thus disabling the identification of
  // incompatible applications.
  if (!module_list_filter_) {
    // Mark the module list as not received so that a new one may trigger the
    // creation of a valid filter.
    module_list_received_ = false;
    SetTerminalState(State::kModuleListInvalidFailure);
    return;
  }

  module_list_update_needed_ = false;

  InitializeIfReady();
}

void ThirdPartyConflictsManager::OnInstalledApplicationsCreated(
    std::unique_ptr<InstalledApplications> installed_applications) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  installed_applications_ = std::move(installed_applications);
  InitializeIfReady();
}

void ThirdPartyConflictsManager::OnInitialBlacklistedModulesRead(
    std::unique_ptr<std::vector<third_party_dlls::PackedListModule>>
        initial_blacklisted_modules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initial_blacklisted_modules_ = std::move(initial_blacklisted_modules);
  InitializeIfReady();
}

void ThirdPartyConflictsManager::InitializeIfReady() {
  DCHECK(!terminal_state_.has_value());

  // Check if this instance is ready to initialize. First look at dependencies
  // that both features need.
  if (!exe_certificate_info_ || !module_list_filter_)
    return;

  // Then look at the dependency needed only for the
  // IncompatibleApplicationsWarning feature.
  if (IncompatibleApplicationsUpdater::IsWarningEnabled() &&
      !installed_applications_) {
    return;
  }

  // And the dependency needed only for the ThirdPartyModulesBlocking feature.
  if (ModuleBlacklistCacheUpdater::IsBlockingEnabled() &&
      !initial_blacklisted_modules_) {
    return;
  }

  // Now both features are ready to be initialized.
  if (ModuleBlacklistCacheUpdater::IsBlockingEnabled()) {
    // It is safe to use base::Unretained() since the callback will not be
    // invoked if the updater is freed.
    module_blacklist_cache_updater_ =
        std::make_unique<ModuleBlacklistCacheUpdater>(
            module_database_event_source_, *exe_certificate_info_,
            module_list_filter_, *initial_blacklisted_modules_,
            base::BindRepeating(
                &ThirdPartyConflictsManager::OnModuleBlacklistCacheUpdated,
                base::Unretained(this)),
            module_analysis_disabled_);
  }

  // The |incompatible_applications_updater_| instance must be created last so
  // that it is registered to the Module Database observer's API after the
  // ModuleBlacklistCacheUpdater instance. This way, it knows about which
  // modules were added to the module blacklist cache so that it's possible to
  // not warn about them.
  if (IncompatibleApplicationsUpdater::IsWarningEnabled()) {
    incompatible_applications_updater_ =
        std::make_unique<IncompatibleApplicationsUpdater>(
            module_database_event_source_, *exe_certificate_info_,
            module_list_filter_, *installed_applications_,
            module_analysis_disabled_);
  }

  if (!incompatible_applications_updater_) {
    SetTerminalState(State::kBlockingInitialized);
  } else if (!module_blacklist_cache_updater_) {
    SetTerminalState(State::kWarningInitialized);
  } else {
    SetTerminalState(State::kWarningAndBlockingInitialized);
  }
}

void ThirdPartyConflictsManager::OnModuleBlacklistCacheUpdated(
    const ModuleBlacklistCacheUpdater::CacheUpdateResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&UpdateModuleBlacklistCacheMD5Digest, result));
}

void ThirdPartyConflictsManager::ForceModuleListComponentUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!module_list_component_id_.empty());

  module_list_component_updater_ = ModuleListComponentUpdater::Create(
      module_list_component_id_,
      base::BindRepeating(
          &ThirdPartyConflictsManager::OnModuleListComponentNotUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void ThirdPartyConflictsManager::OnModuleListComponentNotUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // LoadModuleList() was already invoked.
  if (module_list_received_)
    return;

  SetTerminalState(State::kNoModuleListAvailableFailure);
}

void ThirdPartyConflictsManager::SetTerminalState(State terminal_state) {
  DCHECK(!terminal_state_.has_value());
  terminal_state_ = terminal_state;
  if (on_initialization_complete_callback_)
    std::move(on_initialization_complete_callback_)
        .Run(terminal_state_.value());
}
