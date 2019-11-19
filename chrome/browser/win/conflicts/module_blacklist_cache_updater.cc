// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blacklist_cache_updater.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/conflicts/module_blacklist_cache_util.h"
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
    ModuleBlacklistCacheUpdater::ModuleBlockingDecision;

// The maximum number of modules allowed in the cache. This keeps the cache
// from growing indefinitely.
// Note: This value is tied to the "ModuleBlacklistCache.ModuleCount" histogram.
//       Rename the histogram if this value is ever changed.
static constexpr size_t kMaxModuleCount = 5000u;

// The maximum amount of time a stale entry is kept in the cache before it is
// deleted.
static constexpr base::TimeDelta kMaxEntryAge = base::TimeDelta::FromDays(180);

// This enum is used for UMA. Therefore, the values should never change.
enum class BlacklistStatus {
  // A module was marked as blacklisted during the current browser execution.
  kNewlyBlacklisted = 0,
  // A module was blocked when it tried to load into the process.
  kBlocked = 1,
  kMaxValue = kBlocked,
};

// Updates the module blacklist cache asynchronously on a background sequence
// and return a CacheUpdateResult value.
ModuleBlacklistCacheUpdater::CacheUpdateResult UpdateModuleBlacklistCache(
    const base::FilePath& module_blacklist_cache_path,
    scoped_refptr<ModuleListFilter> module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        newly_blacklisted_modules,
    const std::vector<third_party_dlls::PackedListModule>& blocked_modules,
    size_t max_module_count,
    uint32_t min_time_date_stamp) {
  DCHECK(module_list_filter);

  // Emit some UMA metrics about the update.
  for (size_t i = 0; i < newly_blacklisted_modules.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("ModuleBlacklistCache.BlacklistStatus",
                              BlacklistStatus::kNewlyBlacklisted);
  }
  for (size_t i = 0; i < blocked_modules.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("ModuleBlacklistCache.BlacklistStatus",
                              BlacklistStatus::kBlocked);
  }

  ModuleBlacklistCacheUpdater::CacheUpdateResult result;

  // Read the existing cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  ReadResult read_result =
      ReadModuleBlacklistCache(module_blacklist_cache_path, &metadata,
                               &blacklisted_modules, &result.old_md5_digest);
  UMA_HISTOGRAM_ENUMERATION("ModuleBlacklistCache.ReadResult", read_result);

  // Update the existing data with |newly_blacklisted_modules| and
  // |blocked_modules|.
  UpdateModuleBlacklistCacheData(
      *module_list_filter, newly_blacklisted_modules, blocked_modules,
      max_module_count, min_time_date_stamp, &metadata, &blacklisted_modules);
  // Note: This histogram is tied to the current value of kMaxModuleCount.
  //       Rename the histogram if that value is ever changed.
  UMA_HISTOGRAM_CUSTOM_COUNTS("ModuleBlacklistCache.ModuleCount",
                              blacklisted_modules.size(), 1, kMaxModuleCount,
                              50);

  // Then write the updated cache to disk.
  bool write_result =
      WriteModuleBlacklistCache(module_blacklist_cache_path, metadata,
                                blacklisted_modules, &result.new_md5_digest);
  UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.WriteResult", write_result);

  if (write_result) {
    // Write the path of the cache into the registry so that chrome_elf can find
    // it on its own.
    base::string16 cache_path_registry_key =
        install_static::GetRegistryPath().append(
            third_party_dlls::kThirdPartyRegKeyName);
    base::win::RegKey registry_key(
        HKEY_CURRENT_USER, cache_path_registry_key.c_str(), KEY_SET_VALUE);

    bool cache_path_updated = SUCCEEDED(
        registry_key.WriteValue(third_party_dlls::kBlFilePathRegValue,
                                module_blacklist_cache_path.value().c_str()));
    UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.BlacklistPathUpdated",
                          cache_path_updated);
  }

  return result;
}

// Populates a third_party_dlls::PackedListModule entry from a ModuleInfoKey.
void PopulatePackedListModule(
    const ModuleInfoKey& module_key,
    third_party_dlls::PackedListModule* packed_list_module) {
  // Hash the basename.
  const std::string module_basename = base::UTF16ToUTF8(
      base::i18n::ToLower(module_key.module_path.BaseName().value()));
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(module_basename.data()),
                      module_basename.length(),
                      &packed_list_module->basename_hash[0]);

  // Hash the code id.
  const std::string module_code_id = GenerateCodeId(module_key);
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(module_code_id.data()),
                      module_code_id.length(),
                      &packed_list_module->code_id_hash[0]);

  packed_list_module->time_date_stamp =
      CalculateTimeDateStamp(base::Time::Now());
}

// Returns true if a ModuleBlockingDecision means that the module should be
// added to the blacklist cache.
bool ShouldInsertInBlacklistCache(ModuleBlockingDecision blocking_decision) {
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
    case ModuleBlockingDecision::kAllowedWhitelisted:
    case ModuleBlockingDecision::kTolerated:
    case ModuleBlockingDecision::kNotAnalyzed:
      return false;

    // The following are reasons for the module to be blocked.
    case ModuleBlockingDecision::kDisallowedExplicit:
    case ModuleBlockingDecision::kDisallowedImplicit:
      return true;
  }

  NOTREACHED() << static_cast<int>(blocking_decision);
  return false;
}

}  // namespace

ModuleBlacklistCacheUpdater::ModuleBlacklistCacheUpdater(
    ModuleDatabaseEventSource* module_database_event_source,
    const CertificateInfo& exe_certificate_info,
    scoped_refptr<ModuleListFilter> module_list_filter,
    const std::vector<third_party_dlls::PackedListModule>&
        initial_blacklisted_modules,
    OnCacheUpdatedCallback on_cache_updated_callback,
    bool module_analysis_disabled)
    : module_database_event_source_(module_database_event_source),
      exe_certificate_info_(exe_certificate_info),
      module_list_filter_(std::move(module_list_filter)),
      initial_blacklisted_modules_(initial_blacklisted_modules),
      on_cache_updated_callback_(std::move(on_cache_updated_callback)),
      background_sequence_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      module_analysis_disabled_(module_analysis_disabled),
      weak_ptr_factory_(this) {
  DCHECK(module_list_filter_);
  module_database_event_source_->AddObserver(this);
}

ModuleBlacklistCacheUpdater::~ModuleBlacklistCacheUpdater() {
  module_database_event_source_->RemoveObserver(this);
}

// static
bool ModuleBlacklistCacheUpdater::IsBlockingEnabled() {
  return base::win::GetVersion() >= base::win::Version::WIN8 &&
         base::FeatureList::IsEnabled(features::kThirdPartyModulesBlocking);
}

// static
base::FilePath ModuleBlacklistCacheUpdater::GetModuleBlacklistCachePath() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

  return user_data_dir.Append(kModuleListComponentRelativePath)
      .Append(L"bldata");
}

// static
void ModuleBlacklistCacheUpdater::DeleteModuleBlacklistCache() {
  bool delete_result =
      base::DeleteFile(GetModuleBlacklistCachePath(), false /* recursive */);
  UMA_HISTOGRAM_BOOLEAN("ModuleBlacklistCache.DeleteResult", delete_result);
}

void ModuleBlacklistCacheUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create a "packed list module" entry for this module.
  third_party_dlls::PackedListModule packed_list_module;
  PopulatePackedListModule(module_key, &packed_list_module);

  // This is meant to create the element in the map if it doesn't exist yet.
  ModuleBlockingState& blocking_state = module_blocking_states_[module_key];

  // Determine if the module was in the initial blacklist cache.
  blocking_state.was_in_blacklist_cache =
      std::binary_search(std::begin(initial_blacklisted_modules_),
                         std::end(initial_blacklisted_modules_),
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

  if (ShouldInsertInBlacklistCache(blocking_state.blocking_decision)) {
    newly_blacklisted_modules_.push_back(packed_list_module);

    // Signal the module database that this module will be added to the cache.
    // Note that observers that care about this information should register to
    // the Module Database's observer interface after the ModuleBlacklistCache
    // instance.
    // The Module Database can be null during tests.
    auto* module_database = ModuleDatabase::GetInstance();
    if (module_database) {
      module_database->OnModuleAddedToBlacklist(
          module_key.module_path, module_key.module_size,
          module_key.module_time_date_stamp);
    }
  }
}

void ModuleBlacklistCacheUpdater::OnKnownModuleLoaded(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  // Analyze the module again.
  OnNewModuleFound(module_key, module_data);
}

void ModuleBlacklistCacheUpdater::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  StartModuleBlacklistCacheUpdate();
}

const ModuleBlacklistCacheUpdater::ModuleBlockingState&
ModuleBlacklistCacheUpdater::GetModuleBlockingState(
    const ModuleInfoKey& module_key) const {
  auto it = module_blocking_states_.find(module_key);
  DCHECK(it != module_blocking_states_.end());
  return it->second;
}

void ModuleBlacklistCacheUpdater::DisableModuleAnalysis() {
  module_analysis_disabled_ = true;
}

ModuleBlacklistCacheUpdater::ModuleListState
ModuleBlacklistCacheUpdater::DetermineModuleListState(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (module_list_filter_->IsWhitelisted(module_key, module_data))
    return ModuleListState::kWhitelisted;
  std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action =
      module_list_filter_->IsBlacklisted(module_key, module_data);
  if (!blacklist_action)
    return ModuleListState::kUnlisted;
  return blacklist_action->allow_load() ? ModuleListState::kTolerated
                                        : ModuleListState::kBlacklisted;
}

ModuleBlacklistCacheUpdater::ModuleBlockingDecision
ModuleBlacklistCacheUpdater::DetermineModuleBlockingDecision(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't analyze unloaded modules.
  if ((module_data.module_properties & ModuleInfoData::kPropertyLoadedModule) ==
      0) {
    return ModuleBlockingDecision::kNotLoaded;
  }

  // Don't add modules to the blacklist if they were never loaded in a process
  // where blocking is enabled.
  if (!IsBlockingEnabledInProcessTypes(module_data.process_types))
    return ModuleBlockingDecision::kAllowedInProcessType;

  // New modules should not be added to the cache when the module analysis is
  // disabled.
  if (module_analysis_disabled_)
    return ModuleBlockingDecision::kNotAnalyzed;

  // First check if this module is a part of Chrome's installation. This can
  // override explicit directions in the module list. This prevents us from
  // shooting ourselves in the foot by accidentally issuing a blacklisting
  // rule that blocks one of our own modules.

  // Explicitly whitelist modules whose signing cert's Subject field matches the
  // one in the current executable. No attempt is made to check the validity of
  // module signatures or of signing certs.
  if (exe_certificate_info_.type != CertificateInfo::Type::NO_CERTIFICATE &&
      exe_certificate_info_.subject ==
          module_data.inspection_result->certificate_info.subject) {
    return ModuleBlockingDecision::kAllowedSameCertificate;
  }

#if !defined(OFFICIAL_BUILD)
  // For developer builds only, whitelist modules in the same directory as the
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
    case ModuleListState::kWhitelisted:
      return ModuleBlockingDecision::kAllowedWhitelisted;
    case ModuleListState::kTolerated:
      return ModuleBlockingDecision::kTolerated;
    case ModuleListState::kBlacklisted:
      return ModuleBlockingDecision::kDisallowedExplicit;
  }

  // If the module isn't explicitly listed in the module list then it is either
  // implicitly whitelisted or implicitly blacklisted by other policy.

  // Check if the module is seemingly signed by Microsoft. Again, no attempt is
  // made to check the validity of the certificate.
  if (IsMicrosoftModule(
          module_data.inspection_result->certificate_info.subject)) {
    return ModuleBlockingDecision::kAllowedMicrosoft;
  }

  // It is preferable to mark a whitelisted IME as allowed because it is
  // whitelisted, not because it's a shell extension. Thus, check for the module
  // type after. Note that shell extensions are blocked.
  if (module_data.module_properties & ModuleInfoData::kPropertyIme)
    return ModuleBlockingDecision::kAllowedIME;

  // Getting here means that the module is implicitly blacklisted.
  return ModuleBlockingDecision::kDisallowedImplicit;
}

void ModuleBlacklistCacheUpdater::StartModuleBlacklistCacheUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath cache_file_path = GetModuleBlacklistCachePath();
  if (cache_file_path.empty())
    return;

  // Calculate the minimum time date stamp.
  uint32_t min_time_date_stamp =
      CalculateTimeDateStamp(base::Time::Now() - kMaxEntryAge);

  // Update the module blacklist cache on a background sequence.
  base::PostTaskAndReplyWithResult(
      background_sequence_.get(), FROM_HERE,
      base::BindOnce(&UpdateModuleBlacklistCache, cache_file_path,
                     module_list_filter_, std::move(newly_blacklisted_modules_),
                     std::move(blocked_modules_), kMaxModuleCount,
                     min_time_date_stamp),
      base::BindOnce(
          &ModuleBlacklistCacheUpdater::OnModuleBlacklistCacheUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void ModuleBlacklistCacheUpdater::OnModuleBlacklistCacheUpdated(
    const CacheUpdateResult& result) {
  on_cache_updated_callback_.Run(result);
}
