// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/pup_data.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/pup_data/uws_catalog.h"

namespace chrome_cleaner {

namespace {

// TODO(csharp): Handle the SysWOW64 case if needed.
const wchar_t kGroupPolicyPath[] = L"%SystemRoot%\\system32\\GroupPolicy";
const wchar_t kMachinePolicyFolder[] = L"Machine";
const wchar_t kUserPolicyFolder[] = L"User";

}  // namespace

// static
const wchar_t PUPData::kCommaDelimiter[] = L",";
const size_t PUPData::kCommaDelimiterLength =
    base::size(PUPData::kCommaDelimiter) - 1;
const wchar_t PUPData::kCommonDelimiters[] = L" ,\0";
const size_t PUPData::kCommonDelimitersLength =
    base::size(PUPData::kCommonDelimiters) - 1;

// The escape character used for registry key name and value is an unused
// unicode character (see: http://en.wikipedia.org/wiki/Private_Use_Areas).
const wchar_t PUPData::kRegistryPatternEscapeCharacter = L'\uFFFF';
// TODO(csharp): Find a way to add a compile assert making sure that the \uFFFF
// used here is the same as the one used in ESCAPE_REGISTRY_STR(str).

// static
PUPData::PUPDataMap* PUPData::cached_pup_map_ = nullptr;
std::vector<UwSId>* PUPData::cached_uws_ids_ = nullptr;
PUPData::UwSCatalogs* PUPData::last_uws_catalogs_ = nullptr;

PUPData::RegistryFootprint::RegistryFootprint()
    : rule(REGISTRY_VALUE_MATCH_INVALID) {}

PUPData::RegistryFootprint::RegistryFootprint(
    const RegKeyPath& key_path,
    const base::string16& value_name,
    const base::string16& value_substring,
    RegistryMatchRule rule)
    : key_path(key_path),
      value_name(value_name),
      value_substring(value_substring),
      rule(rule) {}

PUPData::RegistryFootprint::RegistryFootprint(const RegistryFootprint& other) =
    default;

PUPData::RegistryFootprint::~RegistryFootprint() = default;

PUPData::RegistryFootprint& PUPData::RegistryFootprint::operator=(
    const PUPData::RegistryFootprint& other) = default;

bool PUPData::RegistryFootprint::operator==(
    const RegistryFootprint& other) const {
  return key_path == other.key_path && value_name == other.value_name &&
         value_substring == other.value_substring && rule == other.rule;
}

PUPData::FileInfo::FileInfo() = default;
PUPData::FileInfo::FileInfo(const PUPData::FileInfo&) = default;
PUPData::FileInfo::~FileInfo() = default;

PUPData::FileInfo::FileInfo(const std::set<UwS::TraceLocation>& found_in)
    : found_in(found_in) {}

bool PUPData::FileInfo::operator==(const PUPData::FileInfo& other) const {
  return found_in == other.found_in;
}

PUPData::PUP::PUP() : signature_(nullptr) {}

PUPData::PUP::PUP(const UwSSignature* const signature)
    : signature_(signature) {}

PUPData::PUP::PUP(const PUPData::PUP& other) = default;

PUPData::PUP::~PUP() = default;

PUPData::PUP& PUPData::PUP::operator=(const PUPData::PUP& other) = default;

bool PUPData::PUP::AddDiskFootprint(const base::FilePath& file_path) {
  return expanded_disk_footprints.Insert(file_path);
}

void PUPData::PUP::ClearDiskFootprints() {
  expanded_disk_footprints.clear();
}

void PUPData::PUP::AddDiskFootprintTraceLocation(const base::FilePath& path,
                                                 UwS::TraceLocation location) {
  PUPData::FileInfo* existing_info = disk_footprints_info.Find(path);
  if (existing_info)
    existing_info->found_in.insert(location);
  else
    disk_footprints_info.Insert(path, PUPData::FileInfo({location}));
}

void PUPData::PUP::MergeFrom(const PUPData::PUP& other) {
  expanded_disk_footprints.CopyFrom(other.expanded_disk_footprints);
  expanded_registry_footprints.insert(
      expanded_registry_footprints.end(),
      other.expanded_registry_footprints.begin(),
      other.expanded_registry_footprints.end());
  expanded_scheduled_tasks.insert(expanded_scheduled_tasks.end(),
                                  other.expanded_scheduled_tasks.begin(),
                                  other.expanded_scheduled_tasks.end());

  for (auto path_locations_it = other.disk_footprints_info.map().begin();
       path_locations_it != other.disk_footprints_info.map().end();
       ++path_locations_it) {
    const base::FilePath& path = path_locations_it->first;
    const PUPData::FileInfo& other_info = path_locations_it->second;
    PUPData::FileInfo* existing_locations = disk_footprints_info.Find(path);
    if (existing_locations) {
      existing_locations->found_in.insert(other_info.found_in.begin(),
                                          other_info.found_in.end());
    } else {
      disk_footprints_info.Insert(path, other_info);
    }
  }
}

PUPData::PUPData() = default;

PUPData::~PUPData() = default;

// static
bool PUPData::IsKnownPUP(UwSId uws_id) {
  DCHECK(cached_pup_map_);
  return cached_pup_map_->find(uws_id) != cached_pup_map_->end();
}

// static
PUPData::PUP* PUPData::GetPUP(UwSId uws_id) {
  DCHECK(cached_pup_map_);

  auto it = cached_pup_map_->find(uws_id);
  // Because of this check, tests that override PUP data will break if |uws_id|
  // corresponds to a deprecated UwS.
  // TODO(csharp): Make tests more resilient to the existence of PUP IDs.
  if (it == cached_pup_map_->end())
    NOTREACHED() << "Unknown pup id requested: " << uws_id;

  return it->second.get();
}

// static
void PUPData::InitializePUPData(const UwSCatalogs& uws_catalogs) {
  // Reinitialize the caches if they already exist.
  delete cached_pup_map_;
  cached_pup_map_ = new PUPDataMap();

  delete cached_uws_ids_;
  cached_uws_ids_ = new std::vector<UwSId>;

  if (!last_uws_catalogs_)
    last_uws_catalogs_ = new UwSCatalogs();
  last_uws_catalogs_->clear();

  for (const UwSCatalog* uws_catalog : uws_catalogs) {
    last_uws_catalogs_->push_back(uws_catalog);
    for (const UwSId uws_id : uws_catalog->GetUwSIds()) {
      if (!uws_catalog->IsEnabledForScanning(uws_id))
        continue;
      PUPData::AddPUPToMap(uws_catalog->CreatePUPForId(uws_id));
    }
  }
}

// static
const std::vector<UwSId>* PUPData::GetUwSIds() {
  DCHECK(cached_uws_ids_);
  return cached_uws_ids_;
}

// static
const char* PUPData::GetPUPName(const PUP* pup) {
  DCHECK(pup);
  return pup->signature().name ? pup->signature().name : R"(???)";
}

// static
bool PUPData::HasReportOnlyFlag(Flags flags) {
  return !HasRemovalFlag(flags);
}

// static
bool PUPData::HasRemovalFlag(Flags flags) {
  return (flags & FLAGS_ACTION_REMOVE) != 0;
}

// static
bool PUPData::HasRebootFlag(Flags flags) {
  return (flags & FLAGS_REMOVAL_FORCE_REBOOT) != 0;
}

// static
bool PUPData::HasConfirmedUwSFlag(Flags flags) {
  return (flags & FLAGS_STATE_CONFIRMED_UWS) != 0;
}

// static
bool PUPData::IsReportOnlyUwS(UwSId uws_id) {
  return HasReportOnlyFlag(GetPUP(uws_id)->signature().flags);
}

// static
bool PUPData::IsRemovable(UwSId uws_id) {
  return HasRemovalFlag(GetPUP(uws_id)->signature().flags);
}

// static
bool PUPData::IsConfirmedUwS(UwSId uws_id) {
  return HasConfirmedUwSFlag(GetPUP(uws_id)->signature().flags);
}

// static
void PUPData::ChoosePUPs(const std::vector<UwSId>& input_pup_list,
                         bool (*chooser)(Flags),
                         std::vector<UwSId>* output) {
  DCHECK(output);
  DCHECK(chooser);
  for (const auto& uws_id : input_pup_list) {
    const PUPData::UwSSignature& signature = GetPUP(uws_id)->signature();

    if (chooser(signature.flags))
      output->push_back(signature.id);
  }
}

// static
bool PUPData::HasFlaggedPUP(const std::vector<UwSId>& input_pup_list,
                            bool (*chooser)(Flags)) {
  DCHECK(chooser);
  for (const auto& uws_id : input_pup_list) {
    const auto* pup = GetPUP(uws_id);

    if (chooser(pup->signature().flags))
      return true;
  }
  return false;
}

// static
bool PUPData::GetRootKeyFromRegistryRoot(RegistryRoot registry_root,
                                         HKEY* key,
                                         base::FilePath* policy_file) {
  DCHECK(key);

  HKEY hkroot = nullptr;
  switch (registry_root) {
    case (REGISTRY_ROOT_LOCAL_MACHINE):
      hkroot = HKEY_LOCAL_MACHINE;
      break;
    case (REGISTRY_ROOT_USERS):
      hkroot = HKEY_CURRENT_USER;
      break;
    case (REGISTRY_ROOT_CLASSES):
      hkroot = HKEY_CLASSES_ROOT;
      break;
    case (REGISTRY_ROOT_MACHINE_GROUP_POLICY):
      hkroot = HKEY_LOCAL_MACHINE;
      if (policy_file) {
        *policy_file =
            base::FilePath(kGroupPolicyPath).Append(kMachinePolicyFolder);
      }
      break;
    case (REGISTRY_ROOT_USERS_GROUP_POLICY):
      hkroot = HKEY_CURRENT_USER;
      if (policy_file) {
        *policy_file =
            base::FilePath(kGroupPolicyPath).Append(kUserPolicyFolder);
      }
      break;
    case (REGISTRY_ROOT_INVALID):
      return false;
  }

  *key = hkroot;
  return true;
}

// static.
void PUPData::DeleteRegistryKey(const RegKeyPath& key_path, PUPData::PUP* pup) {
  DCHECK(pup);

  RegistryFootprint footprint = {};
  footprint.key_path = key_path;
  footprint.rule = REGISTRY_VALUE_MATCH_KEY;

  pup->expanded_registry_footprints.push_back(footprint);
}

// static.
void PUPData::DeleteRegistryKeyIfPresent(const RegKeyPath& key_path,
                                         PUPData::PUP* pup) {
  if (key_path.Exists())
    PUPData::DeleteRegistryKey(key_path, pup);
}

// static.
void PUPData::DeleteRegistryValue(const RegKeyPath& key_path,
                                  const wchar_t* value_name,
                                  PUPData::PUP* pup) {
  DCHECK(value_name);
  DCHECK(pup);

  RegistryFootprint footprint = {};
  footprint.key_path = key_path;
  footprint.value_name = value_name;
  footprint.rule = REGISTRY_VALUE_MATCH_VALUE_NAME;

  pup->expanded_registry_footprints.push_back(footprint);
}

// static.
void PUPData::UpdateRegistryValue(const RegKeyPath& key_path,
                                  const wchar_t* value_name,
                                  const wchar_t* value_substring,
                                  RegistryMatchRule rule,
                                  PUPData::PUP* pup) {
  DCHECK(pup);

  RegistryFootprint footprint = {};
  footprint.key_path = key_path;
  DCHECK(value_name);
  footprint.value_name = value_name;
  DCHECK(value_substring);
  footprint.value_substring = value_substring;
  footprint.rule = rule;

  pup->expanded_registry_footprints.push_back(footprint);
}

// static.
void PUPData::DeleteScheduledTask(const wchar_t* task_name, PUPData::PUP* pup) {
  DCHECK(task_name);
  DCHECK(pup);
  pup->expanded_scheduled_tasks.push_back(task_name);
}

// static
const PUPData::PUPDataMap* PUPData::GetAllPUPs() {
  DCHECK(cached_pup_map_);
  return cached_pup_map_;
}

// static
void PUPData::UpdateCachedUwSForTesting() {
  DCHECK(cached_pup_map_);
  DCHECK(cached_uws_ids_);
  DCHECK(last_uws_catalogs_);

  // For each signature in each catalog, create a PUPData::PUP object and add
  // it to cached_pup_map_ if none exists for that ID, or update the existing
  // PUP object for that ID to point to the signature.
  for (const UwSCatalog* catalog : *last_uws_catalogs_) {
    for (const UwSId id : catalog->GetUwSIds()) {
      std::unique_ptr<PUPData::PUP> pup = catalog->CreatePUPForId(id);
      auto cached_pup_iter = cached_pup_map_->find(id);
      if (cached_pup_iter == cached_pup_map_->end()) {
        AddPUPToMap(std::move(pup));
      } else {
        // Copy the signature into the existing PUP, then throw away the newly
        // created object.
        cached_pup_iter->second->signature_ = pup->signature_;
      }
    }
  }
}

// static
void PUPData::AddPUPToMap(std::unique_ptr<PUPData::PUP> pup) {
  const UwSId id = pup->signature().id;
  DCHECK(cached_pup_map_->find(id) == cached_pup_map_->end());
  cached_pup_map_->insert(std::make_pair(id, std::move(pup)));
  cached_uws_ids_->push_back(id);
}

// static
Engine::Name PUPData::GetEngine(UwSId id) {
  // These values were used by the deprecated Urza engine and shouldn't be used
  // anymore.
  if ((0 <= id && id <= 340) || id == 9001 || id == 9002) {
    NOTREACHED() << "Deprecated ID from Urza engine used";
    return Engine::DEPRECATED_URZA;
  }
  if (id == kGoogleTestAUwSID || id == kGoogleTestBUwSID ||
      id == kGoogleTestCUwSID) {
    return Engine::TEST_ONLY;
  }

  return Engine::ESET;
}
}  // namespace chrome_cleaner
