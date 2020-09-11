// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_ids_provider.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/variations_client.h"

namespace variations {

// Adding/removing headers is implemented by request consumers, and how it is
// implemented depends on the request type.
// There are three cases:
// 1. Subresources request in renderer, it is implemented by
// WebURLLoaderImpl::Context::Start() by adding a VariationsURLLoaderThrottle
// to a content::URLLoaderThrottle vector.
// 2. Navigations/Downloads request in browser, it is implemented in
// ChromeContentBrowserClient::CreateURLLoaderThrottles() which calls
// CreateContentBrowserURLLoaderThrottles which also adds a
// VariationsURLLoaderThrottle to a content::URLLoaderThrottle vector.
// 3. SimpleURLLoader in browser, it is implemented in a SimpleURLLoader wrapper
// function variations::CreateSimpleURLLoaderWithVariationsHeader().

// static
VariationsIdsProvider* VariationsIdsProvider::GetInstance() {
  return base::Singleton<VariationsIdsProvider>::get();
}

std::string VariationsIdsProvider::GetClientDataHeader(bool is_signed_in) {
  // Lazily initialize the header, if not already done, before attempting to
  // transmit it.
  InitVariationIDsCacheIfNeeded();

  std::string variation_ids_header_copy;
  {
    base::AutoLock scoped_lock(lock_);
    variation_ids_header_copy = is_signed_in
                                    ? cached_variation_ids_header_signed_in_
                                    : cached_variation_ids_header_;
  }
  return variation_ids_header_copy;
}

std::string VariationsIdsProvider::GetVariationsString(IDCollectionKey key) {
  InitVariationIDsCacheIfNeeded();

  // Construct a space-separated string with leading and trailing spaces from
  // the variations set. Note: The ids in it will be in sorted order per the
  // std::set contract.
  std::string ids_string = " ";
  {
    base::AutoLock scoped_lock(lock_);
    for (const VariationIDEntry& entry : GetAllVariationIds()) {
      if (entry.second == key) {
        ids_string.append(base::NumberToString(entry.first));
        ids_string.push_back(' ');
      }
    }
  }
  return ids_string;
}

std::string VariationsIdsProvider::GetGoogleAppVariationsString() {
  return GetVariationsString(GOOGLE_APP);
}

std::string VariationsIdsProvider::GetVariationsString() {
  return GetVariationsString(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT);
}

std::vector<VariationID> VariationsIdsProvider::GetVariationsVector(
    IDCollectionKey key) {
  return GetVariationsVectorImpl(std::set<IDCollectionKey>{key});
}

std::vector<VariationID>
VariationsIdsProvider::GetVariationsVectorForWebPropertiesKeys() {
  const std::set<IDCollectionKey> web_properties_keys{
      variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
      variations::GOOGLE_WEB_PROPERTIES_SIGNED_IN,
      variations::GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT};
  return GetVariationsVectorImpl(web_properties_keys);
}

VariationsIdsProvider::ForceIdsResult VariationsIdsProvider::ForceVariationIds(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  default_variation_ids_set_.clear();

  if (!AddVariationIdsToSet(variation_ids, &default_variation_ids_set_))
    return ForceIdsResult::INVALID_VECTOR_ENTRY;

  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  &default_variation_ids_set_)) {
    return ForceIdsResult::INVALID_SWITCH_ENTRY;
  }
  if (variation_ids_cache_initialized_) {
    // Update the cached variation ids header value after cache initialization,
    // otherwise the change won't be in the cache.
    base::AutoLock scoped_lock(lock_);
    UpdateVariationIDsHeaderValue();
  }
  return ForceIdsResult::SUCCESS;
}

bool VariationsIdsProvider::ForceDisableVariationIds(
    const std::string& command_line_variation_ids) {
  force_disabled_ids_set_.clear();
  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  &force_disabled_ids_set_)) {
    return false;
  }
  if (variation_ids_cache_initialized_) {
    // Update the cached variation ids header value after cache initialization,
    // otherwise the change won't be in the cache.
    base::AutoLock scoped_lock(lock_);
    UpdateVariationIDsHeaderValue();
  }
  return true;
}

void VariationsIdsProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VariationsIdsProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VariationsIdsProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);

  // Stop observing field trials so that it can be restarted when this is
  // re-inited. Note: This is a no-op if this is not currently observing.
  base::FieldTrialList::RemoveObserver(this);
  variation_ids_cache_initialized_ = false;
  variation_ids_set_.clear();
  default_variation_ids_set_.clear();
  synthetic_variation_ids_set_.clear();
  force_disabled_ids_set_.clear();
  cached_variation_ids_header_.clear();
  cached_variation_ids_header_signed_in_.clear();
}

VariationsIdsProvider::VariationsIdsProvider()
    : variation_ids_cache_initialized_(false) {}

VariationsIdsProvider::~VariationsIdsProvider() {
  base::FieldTrialList::RemoveObserver(this);
}

void VariationsIdsProvider::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  base::AutoLock scoped_lock(lock_);
  const size_t old_size = variation_ids_set_.size();
  CacheVariationsId(trial_name, group_name, GOOGLE_WEB_PROPERTIES_ANY_CONTEXT);
  CacheVariationsId(trial_name, group_name, GOOGLE_WEB_PROPERTIES_SIGNED_IN);
  CacheVariationsId(trial_name, group_name,
                    GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT);
  CacheVariationsId(trial_name, group_name, GOOGLE_APP);
  if (variation_ids_set_.size() != old_size)
    UpdateVariationIDsHeaderValue();
}

void VariationsIdsProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& groups) {
  base::AutoLock scoped_lock(lock_);

  synthetic_variation_ids_set_.clear();
  for (const SyntheticTrialGroup& group : groups) {
    VariationID id = GetGoogleVariationIDFromHashes(
        GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, group.id);
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_ANY_CONTEXT));
    }
    id = GetGoogleVariationIDFromHashes(GOOGLE_WEB_PROPERTIES_SIGNED_IN,
                                        group.id);
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_SIGNED_IN));
    }
    // Google App IDs omitted because they should never be defined
    // synthetically.
  }
  UpdateVariationIDsHeaderValue();
}

void VariationsIdsProvider::InitVariationIDsCacheIfNeeded() {
  base::AutoLock scoped_lock(lock_);
  if (variation_ids_cache_initialized_)
    return;

  // Register for additional cache updates. This is done first to avoid a race
  // that could cause registered FieldTrials to be missed.
  bool success = base::FieldTrialList::AddObserver(this);
  DCHECK(success);

  base::FieldTrial::ActiveGroups initial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&initial_groups);

  for (const auto& entry : initial_groups) {
    CacheVariationsId(entry.trial_name, entry.group_name,
                      GOOGLE_WEB_PROPERTIES_ANY_CONTEXT);
    CacheVariationsId(entry.trial_name, entry.group_name,
                      GOOGLE_WEB_PROPERTIES_SIGNED_IN);
    CacheVariationsId(entry.trial_name, entry.group_name,
                      GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT);
    CacheVariationsId(entry.trial_name, entry.group_name, GOOGLE_APP);
  }
  UpdateVariationIDsHeaderValue();

  variation_ids_cache_initialized_ = true;
}

void VariationsIdsProvider::CacheVariationsId(const std::string& trial_name,
                                              const std::string& group_name,
                                              IDCollectionKey key) {
  const VariationID id = GetGoogleVariationID(key, trial_name, group_name);
  if (id != EMPTY_ID)
    variation_ids_set_.insert(VariationIDEntry(id, key));
}

void VariationsIdsProvider::UpdateVariationIDsHeaderValue() {
  lock_.AssertAcquired();

  // The header value is a serialized protobuffer of Variation IDs which is
  // base64 encoded before transmitting as a string.
  cached_variation_ids_header_.clear();
  cached_variation_ids_header_signed_in_.clear();

  // If successful, swap the header value with the new one.
  // Note that the list of IDs and the header could be temporarily out of sync
  // if IDs are added as the header is recreated. The receiving servers are OK
  // with such discrepancies.
  cached_variation_ids_header_ = GenerateBase64EncodedProto(false);
  cached_variation_ids_header_signed_in_ = GenerateBase64EncodedProto(true);

  for (auto& observer : observer_list_) {
    observer.VariationIdsHeaderUpdated();
  }
}

std::string VariationsIdsProvider::GenerateBase64EncodedProto(
    bool is_signed_in) {
  std::set<VariationIDEntry> all_variation_ids_set = GetAllVariationIds();

  ClientVariations proto;
  for (const VariationIDEntry& entry : all_variation_ids_set) {
    switch (entry.second) {
      case GOOGLE_WEB_PROPERTIES_SIGNED_IN:
        if (is_signed_in)
          proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_ANY_CONTEXT:
        proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT:
        proto.add_trigger_variation_id(entry.first);
        break;
      case GOOGLE_APP:
        // These IDs should not be added into Google Web headers.
        break;
      case GOOGLE_WEB_PROPERTIES_FIRST_PARTY:
      case GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY:
        // TODO(crbug/1094303): Add support for the above IDCollectionKeys.
        break;
      case ID_COLLECTION_COUNT:
        // This case included to get full enum coverage for switch, so that
        // new enums introduce compiler warnings. Nothing to do for this.
        break;
    }
  }

  const size_t total_id_count =
      proto.variation_id_size() + proto.trigger_variation_id_size();

  if (total_id_count == 0)
    return std::string();

  // This is the bottleneck for the creation of the header, so validate the size
  // here. Force a hard maximum on the ID count in case the Variations server
  // returns too many IDs and DOSs receiving servers with large requests.
  DCHECK_LE(total_id_count, 35U);
  UMA_HISTOGRAM_COUNTS_100("Variations.Headers.ExperimentCount",
                           total_id_count);
  if (total_id_count > 50)
    return std::string();

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string hashed;
  base::Base64Encode(serialized, &hashed);
  return hashed;
}

// static
bool VariationsIdsProvider::AddVariationIdsToSet(
    const std::vector<std::string>& variation_ids,
    std::set<VariationIDEntry>* target_set) {
  for (const std::string& entry : variation_ids) {
    if (entry.empty()) {
      target_set->clear();
      return false;
    }
    bool trigger_id =
        base::StartsWith(entry, "t", base::CompareCase::SENSITIVE);
    // Remove the "t" prefix if it's there.
    std::string trimmed_entry = trigger_id ? entry.substr(1) : entry;

    int variation_id = 0;
    if (!base::StringToInt(trimmed_entry, &variation_id)) {
      target_set->clear();
      return false;
    }
    target_set->insert(VariationIDEntry(
        variation_id, trigger_id ? GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT
                                 : GOOGLE_WEB_PROPERTIES_ANY_CONTEXT));
  }
  return true;
}

// static
bool VariationsIdsProvider::ParseVariationIdsParameter(
    const std::string& command_line_variation_ids,
    std::set<VariationIDEntry>* target_set) {
  if (command_line_variation_ids.empty())
    return true;

  std::vector<std::string> variation_ids_from_command_line =
      base::SplitString(command_line_variation_ids, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  return AddVariationIdsToSet(variation_ids_from_command_line, target_set);
}

std::set<VariationsIdsProvider::VariationIDEntry>
VariationsIdsProvider::GetAllVariationIds() {
  lock_.AssertAcquired();

  std::set<VariationIDEntry> all_variation_ids_set = default_variation_ids_set_;
  for (const VariationIDEntry& entry : variation_ids_set_) {
    all_variation_ids_set.insert(entry);
  }
  for (const VariationIDEntry& entry : synthetic_variation_ids_set_) {
    all_variation_ids_set.insert(entry);
  }
  for (const VariationIDEntry& entry : force_disabled_ids_set_) {
    all_variation_ids_set.erase(entry);
  }
  return all_variation_ids_set;
}

std::vector<VariationID> VariationsIdsProvider::GetVariationsVectorImpl(
    const std::set<IDCollectionKey>& keys) {
  InitVariationIDsCacheIfNeeded();

  // Get all the active variation ids while holding the lock.
  std::set<VariationIDEntry> all_variation_ids;
  {
    base::AutoLock scoped_lock(lock_);
    all_variation_ids = GetAllVariationIds();
  }

  // Copy the requested variations to the output vector.
  std::vector<VariationID> result;
  result.reserve(all_variation_ids.size());
  for (const VariationIDEntry& entry : all_variation_ids) {
    if (keys.find(entry.second) != keys.end())
      result.push_back(entry.first);
  }

  // Make sure each enry is unique. As a side-effect, the output will be sorted.
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

}  // namespace variations
