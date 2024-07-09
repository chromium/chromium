// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blocklist_cache_updater.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_util.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"

#if !defined(OFFICIAL_BUILD)
#include "base/base_paths.h"
#endif

namespace {

using ModuleBlockingDecision =
    ModuleBlocklistCacheUpdater::ModuleBlockingDecision;

// The maximum number of modules allowed in the cache. This keeps the cache
// from growing indefinitely.
static constexpr size_t kMaxModuleCount = 5000u;

// The maximum amount of time a stale entry is kept in the cache before it is
// deleted.
static constexpr base::TimeDelta kMaxEntryAge = base::Days(180);

// Updates the module blocklist cache asynchronously on a background sequence
// and return a CacheUpdateResult value.
ModuleBlocklistCacheUpdater::CacheUpdateResult UpdateModuleBlocklistCache(
    const base::FilePath& module_blocklist_cache_path,
    scoped_refptr<ModuleListFilter> module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        newly_blocklisted_modules,
    const std::vector<third_party_dlls::PackedListModule>& blocked_modules,
    size_t max_module_count,
    uint32_t min_time_date_stamp) {
  DCHECK(module_list_filter);

  ModuleBlocklistCacheUpdater::CacheUpdateResult result;

  // Read the existing cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  ReadModuleBlocklistCache(module_blocklist_cache_path, &metadata,
                           &blocklisted_modules, &result.old_md5_digest);

  // Update the existing data with |newly_blocklisted_modules| and
  // |blocked_modules|.
  UpdateModuleBlocklistCacheData(
      *module_list_filter, newly_blocklisted_modules, blocked_modules,
      max_module_count, min_time_date_stamp, &metadata, &blocklisted_modules);

  // Then write the updated cache to disk.
  bool write_result =
      WriteModuleBlocklistCache(module_blocklist_cache_path, metadata,
                                blocklisted_modules, &result.new_md5_digest);

  if (write_result) {
    // Write the path of the cache into the registry so that chrome_elf can find
    // it on its own.
    std::wstring cache_path_registry_key =
        install_static::GetRegistryPath().append(
            third_party_dlls::kThirdPartyRegKeyName);
    base::win::RegKey registry_key(
        HKEY_CURRENT_USER, cache_path_registry_key.c_str(), KEY_SET_VALUE);

    registry_key.WriteValue(third_party_dlls::kBlFilePathRegValue,
                            module_blocklist_cache_path.value().c_str());
  }

  return result;
}

// Populates a third_party_dlls::PackedListModule entry from a ModuleInfoKey.
void PopulatePackedListModule(
    const ModuleInfoKey& module_key,
    third_party_dlls::PackedListModule* packed_list_module) {
  // Hash the basename.
  const std::string module_basename = base::UTF16ToUTF8(
      base::i18n::ToLower(module_key.module_path.BaseName().AsUTF16Unsafe()));
  base::span(packed_list_module->basename_hash)
      .copy_from(base::SHA1Hash(base::as_byte_span(module_basename)));

  // Hash the code id.
  const std::string module_code_id = GenerateCodeId(module_key);
  base::span(packed_list_module->code_id_hash)
      .copy_from(base::SHA1Hash(base::as_byte_span(module_code_id)));

  packed_list_module->time_date_stamp =
      CalculateTimeDateStamp(base::Time::Now());
}

// Returns true if a ModuleBlockingDecision means that the module should be
// added to the blocklist cache.
bool ShouldInsertInBlocklistCache(ModuleBlockingDecision blocking_decision) {
  switch (blocking_decision) {
    case ModuleBlockingDecision::kUnknown:
      break;

    // All of these are reasons that allow the module to be loaded.
    case ModuleBlockingDecision::kNotLoaded:
    case ModuleBlockingDecision::kAllowedInProcessType:
    case ModuleBlockingDecision::kAllowedIME:
    case ModuleBlockingDecision::kAllowedSameCertificate:
    case ModuleBlockingDecision::kAllowedSameDirectory:
    case ModuleBlockingDecision::kAllowedMicrosoft:
    case ModuleBlockingDecision::kAllowedAllowlisted:
    case ModuleBlockingDecision::kTolerated:
    case ModuleBlockingDecision::kNotAnalyzed:
      return false;

    // The following are reasons for the module to be blocked.
    case ModuleBlockingDecision::kDisallowedExplicit:
    case ModuleBlockingDecision::kDisallowedImplicit:
      return true;
  }

  NOTREACHED_IN_MIGRATION() << static_cast<int>(blocking_decision);
  return false;
}

}  // namespace

ModuleBlocklistCacheUpdater::ModuleBlocklistCacheUpdater(
    ModuleDatabaseEventSource* module_database_event_source,
    const CertificateInfo& exe_certificate_info,
    scoped_refptr<ModuleListFilter> module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        initial_blocklisted_modules,
    OnCacheUpdatedCallback on_cache_updated_callback,
    bool module_analysis_disabled)
    : module_database_event_source_(module_database_event_source),
      exe_certificate_info_(exe_certificate_info),
      module_list_filter_(std::move(module_list_filter)),
      initial_blocklisted_modules_(initial_blocklisted_modules),
      on_cache_updated_callback_(std::move(on_cache_updated_callback)),
      background_sequence_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      module_analysis_disabled_(module_analysis_disabled),
      weak_ptr_factory_(this) {
  DCHECK(module_list_filter_);
  module_database_event_source_->AddObserver(this);
}

ModuleBlocklistCacheUpdater::~ModuleBlocklistCacheUpdater() {
  module_database_event_source_->RemoveObserver(this);
}

// static
bool ModuleBlocklistCacheUpdater::IsBlockingEnabled() {
  return base::FeatureList::IsEnabled(features::kThirdPartyModulesBlocking);
}

// static
base::FilePath ModuleBlocklistCacheUpdater::GetModuleBlocklistCachePath() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

  return user_data_dir.Append(kModuleListComponentRelativePath)
      .Append(L"bldata");
}

// static
void ModuleBlocklistCacheUpdater::DeleteModuleBlocklistCache() {
  base::DeleteFile(GetModuleBlocklistCachePath());
}

void ModuleBlocklistCacheUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create a "packed list module" entry for this module.
  third_party_dlls::PackedListModule packed_list_module;
  PopulatePackedListModule(module_key, &packed_list_module);

  // This is meant to create the element in the map if it doesn't exist yet.
  ModuleBlockingState& blocking_state = module_blocking_states_[module_key];

  // Determine if the module was in the initial blocklist cache.
  blocking_state.was_in_blocklist_cache =
      std::binary_search(std::begin(*initial_blocklisted_modules_),
                         std::end(*initial_blocklisted_modules_),
                         packed_list_module, internal::ModuleLess());

  // Make note of the fact that the module was blocked. It could be that the
  // module subsequently ends up being loaded, but an earlier load attempt was
  // blocked (ie, the injector actively worked around the blocking logic). This
  // is a one way switch so that it doesn't get reset if the module is analyzed
  // a second time.
  if (module_data.module_properties & ModuleInfoData::kPropertyBlocked)
    blocking_state.was_blocked = true;

  // Make note of the fact that the module was loaded. This is a one-way toggle
  // for the same reasons as above.
  if (module_data.module_properties & ModuleInfoData::kPropertyLoadedModule)
    blocking_state.was_loaded = true;

  // Determine the current blocking decision. This can change at runtime as the
  // module list component changes so re-run this analysis every time through.
  blocking_state.blocking_decision =
      DetermineModuleBlockingDecision(module_key, module_data);

  if (blocking_state.was_blocked)
    blocked_modules_.push_back(packed_list_module);

  if (ShouldInsertInBlocklistCache(blocking_state.blocking_decision)) {
    newly_blocklisted_modules_.push_back(packed_list_module);

    // Signal the module database that this module will be added to the cache.
    // Note that observers that care about this information should register to
    // the Module Database's observer interface after the ModuleBlocklistCache
    // instance.
    // The Module Database can be null during tests.
    auto* module_database = ModuleDatabase::GetInstance();
    if (module_database) {
      module_database->OnModuleAddedToBlocklist(
          module_key.module_path, module_key.module_size,
          module_key.module_time_date_stamp);
    }
  }
}

void ModuleBlocklistCacheUpdater::OnKnownModuleLoaded(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  // Analyze the module again.
  OnNewModuleFound(module_key, module_data);
}

void ModuleBlocklistCacheUpdater::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StartModuleBlocklistCacheUpdate();
}

const ModuleBlocklistCacheUpdater::ModuleBlockingState&
ModuleBlocklistCacheUpdater::GetModuleBlockingState(
    const ModuleInfoKey& module_key) const {
  auto it = module_blocking_states_.find(module_key);
  CHECK(it != module_blocking_states_.end(), base::NotFatalUntil::M130);
  return it->second;
}

void ModuleBlocklistCacheUpdater::DisableModuleAnalysis() {
  module_analysis_disabled_ = true;
}

ModuleBlocklistCacheUpdater::ModuleListState
ModuleBlocklistCacheUpdater::DetermineModuleListState(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (module_list_filter_->IsAllowlisted(module_key, module_data))
    return ModuleListState::kAllowlisted;
  std::unique_ptr<chrome::conflicts::BlocklistAction> blocklist_action =
      module_list_filter_->IsBlocklisted(module_key, module_data);
  if (!blocklist_action)
    return ModuleListState::kUnlisted;
  return blocklist_action->allow_load() ? ModuleListState::kTolerated
                                        : ModuleListState::kBlocklisted;
}

ModuleBlocklistCacheUpdater::ModuleBlockingDecision
ModuleBlocklistCacheUpdater::DetermineModuleBlockingDecision(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't analyze unloaded modules.
  if ((module_data.module_properties & ModuleInfoData::kPropertyLoadedModule) ==
      0) {
    return ModuleBlockingDecision::kNotLoaded;
  }

  // Don't add modules to the blocklist if they were never loaded in a process
  // where blocking is enabled.
  if (!IsBlockingEnabledInProcessTypes(module_data.process_types))
    return ModuleBlockingDecision::kAllowedInProcessType;

  // New modules should not be added to the cache when the module analysis is
  // disabled.
  if (module_analysis_disabled_)
    return ModuleBlockingDecision::kNotAnalyzed;

  // First check if this module is a part of Chrome's installation. This can
  // override explicit directions in the module list. This prevents us from
  // shooting ourselves in the foot by accidentally issuing a blocklisting
  // rule that blocks one of our own modules.

  // Explicitly allowlist modules whose signing cert's Subject field matches the
  // one in the current executable. No attempt is made to check the validity of
  // module signatures or of signing certs.
  if (exe_certificate_info_->type != CertificateInfo::Type::NO_CERTIFICATE &&
      exe_certificate_info_->subject ==
          module_data.inspection_result->certificate_info.subject) {
    return ModuleBlockingDecision::kAllowedSameCertificate;
  }

#if !defined(OFFICIAL_BUILD)
  // For developer builds only, allowlist modules in the same directory as the
  // executable.
  base::FilePath exe_path;
  if (base::PathService::Get(base::DIR_EXE, &exe_path) &&
      exe_path.DirName().IsParent(module_key.module_path)) {
    return ModuleBlockingDecision::kAllowedSameDirectory;
  }
#endif

  // Get the state of the module with respect to the module list component. If
  // there are explicit directions in the list then respect those.
  switch (DetermineModuleListState(module_key, module_data)) {
    case ModuleListState::kUnlisted:
      break;
    case ModuleListState::kAllowlisted:
      return ModuleBlockingDecision::kAllowedAllowlisted;
    case ModuleListState::kTolerated:
      return ModuleBlockingDecision::kTolerated;
    case ModuleListState::kBlocklisted:
      return ModuleBlockingDecision::kDisallowedExplicit;
  }

  // If the module isn't explicitly listed in the module list then it is either
  // implicitly allowlisted or implicitly blocklisted by other policy.

  // Check if the module is seemingly signed by Microsoft. Again, no attempt is
  // made to check the validity of the certificate.
  if (IsMicrosoftModule(
          module_data.inspection_result->certificate_info.subject)) {
    return ModuleBlockingDecision::kAllowedMicrosoft;
  }

  // It is preferable to mark a allowlisted IME as allowed because it is
  // allowlisted, not because it's a shell extension. Thus, check for the module
  // type after. Note that shell extensions are blocked.
  if (module_data.module_properties & ModuleInfoData::kPropertyIme)
    return ModuleBlockingDecision::kAllowedIME;

  // Getting here means that the module is implicitly blocklisted.
  return ModuleBlockingDecision::kDisallowedImplicit;
}

void ModuleBlocklistCacheUpdater::StartModuleBlocklistCacheUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath cache_file_path = GetModuleBlocklistCachePath();
  if (cache_file_path.empty())
    return;

  // Calculate the minimum time date stamp.
  uint32_t min_time_date_stamp =
      CalculateTimeDateStamp(base::Time::Now() - kMaxEntryAge);

  // Update the module blocklist cache on a background sequence.
  background_sequence_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UpdateModuleBlocklistCache, cache_file_path,
                     module_list_filter_, std::move(newly_blocklisted_modules_),
                     std::move(blocked_modules_), kMaxModuleCount,
                     min_time_date_stamp),
      base::BindOnce(
          &ModuleBlocklistCacheUpdater::OnModuleBlocklistCacheUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void ModuleBlocklistCacheUpdater::OnModuleBlocklistCacheUpdated(
    const CacheUpdateResult& result) {
  on_cache_updated_callback_.Run(result);
}
