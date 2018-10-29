// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_storage.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/unacked_invalidation_set.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google/cacheinvalidation/types.pb.h"

namespace {

const char kInvalidatorMaxInvalidationVersions[] =
    "invalidator.max_invalidation_versions";

bool ValueToUnackedInvalidationStorageMap(
    const base::ListValue& value,
    syncer::UnackedInvalidationsMap* map) {
  for (size_t i = 0; i != value.GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (!value.GetDictionary(i, &dict) ||
        !syncer::UnackedInvalidationSet::DeserializeSetIntoMap(*dict, map)) {
      DLOG(WARNING) << "Failed to parse ObjectState at position " << i;
      return false;
    }
  }
  return true;
}

std::unique_ptr<base::ListValue> UnackedInvalidationStorageMapToValue(
    const syncer::UnackedInvalidationsMap& map) {
  std::unique_ptr<base::ListValue> value(new base::ListValue);
  for (auto it = map.begin(); it != map.end(); ++it) {
    value->Append(it->second.ToValue());
  }
  return value;
}

}  // namespace

namespace invalidation {

// static
void InvalidatorStorage::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kInvalidatorSavedInvalidations);
  registry->RegisterStringPref(prefs::kInvalidatorInvalidationState,
                               std::string());
  registry->RegisterStringPref(prefs::kInvalidatorClientId, std::string());

  // This pref is obsolete.  We register it so we can clear it.
  // At some point in the future, it will be safe to remove this.
  registry->RegisterListPref(kInvalidatorMaxInvalidationVersions);
}

// static
void InvalidatorStorage::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kInvalidatorSavedInvalidations);
  registry->RegisterStringPref(prefs::kInvalidatorInvalidationState,
                               std::string());
  registry->RegisterStringPref(prefs::kInvalidatorClientId, std::string());
}

InvalidatorStorage::InvalidatorStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  if (pref_service_->FindPreference(kInvalidatorMaxInvalidationVersions))
    pref_service_->ClearPref(kInvalidatorMaxInvalidationVersions);
}

InvalidatorStorage::~InvalidatorStorage() {
}

void InvalidatorStorage::ClearAndSetNewClientId(const std::string& client_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Clear();  // We can't reuse our old invalidation state if the ID changes.
  pref_service_->SetString(prefs::kInvalidatorClientId, client_id);
}

std::string InvalidatorStorage::GetInvalidatorClientId() const {
  return pref_service_->GetString(prefs::kInvalidatorClientId);
}

void InvalidatorStorage::SetBootstrapData(const std::string& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string base64_data;
  base::Base64Encode(data, &base64_data);
  pref_service_->SetString(prefs::kInvalidatorInvalidationState,
                           base64_data);
}

std::string InvalidatorStorage::GetBootstrapData() const {
  std::string base64_data(
      pref_service_->GetString(prefs::kInvalidatorInvalidationState));
  std::string data;
  base::Base64Decode(base64_data, &data);
  return data;
}

void InvalidatorStorage::SetSavedInvalidations(
      const syncer::UnackedInvalidationsMap& map) {
  std::unique_ptr<base::ListValue> value(
      UnackedInvalidationStorageMapToValue(map));
  pref_service_->Set(prefs::kInvalidatorSavedInvalidations, *value);
}

syncer::UnackedInvalidationsMap
InvalidatorStorage::GetSavedInvalidations() const {
  syncer::UnackedInvalidationsMap map;
  const base::ListValue* value =
      pref_service_->GetList(prefs::kInvalidatorSavedInvalidations);
  if (!ValueToUnackedInvalidationStorageMap(*value, &map))
    return syncer::UnackedInvalidationsMap();
  return map;
}

void InvalidatorStorage::Clear() {
  DCHECK(thread_checker_.CalledOnValidThread());
  pref_service_->ClearPref(prefs::kInvalidatorSavedInvalidations);
  pref_service_->ClearPref(prefs::kInvalidatorClientId);
  pref_service_->ClearPref(prefs::kInvalidatorInvalidationState);
}

}  // namespace invalidation
