// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_ids_provider.h"

#include <algorithm>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_features.h"

namespace variations {
namespace {

VariationsIdsProvider* g_instance = nullptr;

base::Lock& GetInstanceLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// Sorts and removes duplicates from the given container. This is useful after
// building ID collections by merging IDs from different sources, as the source
// ID sets may intersect, yielding duplicate and out-of-order IDs.
template <typename Container>
void MakeSortedAndUnique(Container* container) {
  std::sort(container->begin(), container->end());
  container->erase(std::unique(container->begin(), container->end()),
                   container->end());
}

}  // namespace

bool VariationsHeaderKey::operator<(const VariationsHeaderKey& other) const {
  if (is_signed_in != other.is_signed_in) {
    return is_signed_in < other.is_signed_in;
  }
  return web_visibility < other.web_visibility;
}

// Adding/removing headers is implemented by request consumers, and how it is
// implemented depends on the request type.
// There are three cases:
// 1. Subresources request in renderer, it is implemented by
// URLLoader::Context::Start() by adding a VariationsURLLoaderThrottle to a
// content::URLLoaderThrottle vector.
// 2. Navigations/Downloads request in browser, it is implemented in
// ChromeContentBrowserClient::CreateURLLoaderThrottles() which calls
// CreateContentBrowserURLLoaderThrottles which also adds a
// VariationsURLLoaderThrottle to a content::URLLoaderThrottle vector.
// 3. SimpleURLLoader in browser, it is implemented in a SimpleURLLoader wrapper
// function variations::CreateSimpleURLLoaderWithVariationsHeader().

// static
VariationsIdsProvider* VariationsIdsProvider::CreateInstance(
    Mode mode,
    std::unique_ptr<base::Clock> clock) {
  base::AutoLock lock(GetInstanceLock());
  DCHECK(!g_instance);
  g_instance = new VariationsIdsProvider(mode, std::move(clock));
  return g_instance;
}

// static
VariationsIdsProvider* VariationsIdsProvider::GetInstance() {
  base::AutoLock lock(GetInstanceLock());
  DCHECK(g_instance);
  return g_instance;
}

variations::mojom::VariationsHeadersPtr
VariationsIdsProvider::GetClientDataHeaders(bool is_signed_in) {
  if (mode_ == Mode::kIgnoreSignedInState) {
    is_signed_in = true;
  } else if (mode_ == Mode::kDontSendSignedInVariations) {
    is_signed_in = false;
  }

  std::string first_party_header_copy;
  std::string any_context_header_copy;
  {
    base::AutoLock lock(lock_);
    MaybeUpdateVariationIDsAndHeaders();
    first_party_header_copy = GetClientDataHeader(
        is_signed_in, Study_GoogleWebVisibility_FIRST_PARTY);
    any_context_header_copy =
        GetClientDataHeader(is_signed_in, Study_GoogleWebVisibility_ANY);
  }

  if (first_party_header_copy.empty() && any_context_header_copy.empty()) {
    return nullptr;
  }

  base::flat_map<variations::mojom::GoogleWebVisibility, std::string> headers =
      {{variations::mojom::GoogleWebVisibility::FIRST_PARTY,
        std::move(first_party_header_copy)},
       {variations::mojom::GoogleWebVisibility::ANY,
        std::move(any_context_header_copy)}};

  return variations::mojom::VariationsHeaders::New(std::move(headers));
}

std::string VariationsIdsProvider::GetGoogleAppVariationsString() {
  return GetVariationsString({GOOGLE_APP});
}

std::string VariationsIdsProvider::GetTriggerVariationsString() {
  return GetVariationsString({GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
                              GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY});
}

std::string VariationsIdsProvider::GetVariationsString() {
  return GetVariationsString(
      {GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, GOOGLE_WEB_PROPERTIES_FIRST_PARTY});
}

std::vector<VariationID> VariationsIdsProvider::GetVariationsVector(
    const std::set<IDCollectionKey>& keys) {
  return GetVariationsVectorImpl(keys);
}

std::vector<VariationID>
VariationsIdsProvider::GetVariationsVectorForWebPropertiesKeys() {
  const std::set<IDCollectionKey> web_properties_keys{
      GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
      GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
      GOOGLE_WEB_PROPERTIES_SIGNED_IN,
      GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
      GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY,
  };
  return GetVariationsVectorImpl(web_properties_keys);
}

void VariationsIdsProvider::SetLowEntropySourceValue(
    std::optional<int> low_entropy_source_value) {
  // The low entropy source value is an integer that is between 0 and 7999,
  // inclusive. See components/metrics/metrics_state_manager.cc for the logic to
  // generate it.
  if (low_entropy_source_value) {
    DCHECK_GE(low_entropy_source_value.value(), 0);
    DCHECK_LE(low_entropy_source_value.value(), 7999);
  }

  base::AutoLock scoped_lock(lock_);
  low_entropy_source_value_ = low_entropy_source_value;
}

VariationsIdsProvider::ForceIdsResult VariationsIdsProvider::ForceVariationIds(
    base::PassKey<VariationsFieldTrialCreator> pass_key,
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  return ForceVariationIdsImpl(variation_ids, command_line_variation_ids);
}

VariationsIdsProvider::ForceIdsResult VariationsIdsProvider::ForceVariationIds(
    base::PassKey<android_webview::AwBrowserMainParts> pass_key,
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  return ForceVariationIdsImpl(variation_ids, command_line_variation_ids);
}

VariationsIdsProvider::ForceIdsResult
VariationsIdsProvider::ForceVariationIdsForTesting(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  return ForceVariationIdsImpl(variation_ids, command_line_variation_ids);
}

bool VariationsIdsProvider::ForceDisableVariationIds(
    const std::string& command_line_variation_ids) {
  base::AutoLock scoped_lock(lock_);

  force_disabled_ids_set_.clear();
  ResetLastUpdateTime();

  // |should_dedupe| is false here in order to add the IDs specified in
  // |command_line_variation_ids| to |force_disabled_ids_set_| even if they were
  // defined before. The IDs are not marked as active; they are marked as
  // disabled.
  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  /*should_dedupe=*/false,
                                  &force_disabled_ids_set_)) {
    return false;
  }

  // When disabling a variation ID through the command line, ensure it is
  // disabled in every contexts.
  static_assert(
      ID_COLLECTION_COUNT == 6,
      "If you add a new collection key, make sure it can be disabled here.");
  VariationIDEntrySet additional_disabled_ids;
  for (const auto& entry : force_disabled_ids_set_) {
    if (entry.second == GOOGLE_WEB_PROPERTIES_ANY_CONTEXT) {
      additional_disabled_ids.insert(
          VariationIDEntry(entry.first, GOOGLE_WEB_PROPERTIES_SIGNED_IN));
      additional_disabled_ids.insert(
          VariationIDEntry(entry.first, GOOGLE_WEB_PROPERTIES_FIRST_PARTY));
    } else if (entry.second == GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT) {
      additional_disabled_ids.insert(VariationIDEntry(
          entry.first, GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY));
    }
  }
  force_disabled_ids_set_.merge(additional_disabled_ids);

  return true;
}

void VariationsIdsProvider::AddObserver(Observer* observer) {
  base::AutoLock scoped_lock(lock_);
  CHECK(!base::Contains(observer_list_, observer));
  observer_list_.push_back(observer);
}

void VariationsIdsProvider::RemoveObserver(Observer* observer) {
  base::AutoLock scoped_lock(lock_);
  std::erase(observer_list_, observer);
}

void VariationsIdsProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);

  // Stop observing field trials so that it can be restarted when this is
  // re-initialized. Note: This is a no-op if this is not currently observing.
  base::FieldTrialList::RemoveObserver(this);
  is_subscribed_to_field_trial_list_ = false;

  // Reset the remaining cached state.
  low_entropy_source_value_ = std::nullopt;
  last_update_time_ = base::Time::Min();
  next_update_time_ = base::Time::Min();
  active_variation_ids_set_.clear();
  force_enabled_ids_set_.clear();
  synthetic_variation_ids_set_.clear();
  force_disabled_ids_set_.clear();
  variations_headers_map_.clear();
  observer_list_.clear();
}

VariationsIdsProvider::VariationsIdsProvider(Mode mode,
                                             std::unique_ptr<base::Clock> clock)
    : mode_(mode), clock_(std::move(clock)) {
  CHECK(clock_);
}

VariationsIdsProvider::~VariationsIdsProvider() {
  base::FieldTrialList::RemoveObserver(this);
}

// static
VariationsIdsProvider* VariationsIdsProvider::CreateInstanceForTesting(
    Mode mode,
    std::unique_ptr<base::Clock> clock) {
  base::AutoLock lock(GetInstanceLock());
  VariationsIdsProvider* previous_instance = g_instance;
  g_instance = new VariationsIdsProvider(mode, std::move(clock));
  return previous_instance;
}

// static
void VariationsIdsProvider::ResetInstanceForTesting(
    VariationsIdsProvider* previous_instance) {
  base::AutoLock lock(GetInstanceLock());
  delete g_instance;
  g_instance = previous_instance;
}

std::string VariationsIdsProvider::GetVariationsString(
    const std::set<IDCollectionKey>& keys) {
  // Construct a space-separated string with leading and trailing spaces from
  // the VariationIDs set. The IDs in the string are unique and in sorted order.
  std::string ids_string = " ";

  for (const VariationID& id : GetVariationsVector(keys)) {
    ids_string.append(base::NumberToString(id));
    ids_string.push_back(' ');
  }

  return ids_string;
}

void VariationsIdsProvider::OnFieldTrialGroupFinalized(
    const base::FieldTrial& trial,
    const std::string& group_name) {
  base::AutoLock scoped_lock(lock_);
  // The finalized field trial may have caused variation IDs to be added to the
  // active set. Reset the last update time to force an update on the next call
  // to `MaybeUpdateVariationIDsAndHeaders()`.
  ResetLastUpdateTime();
}

void VariationsIdsProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& trials_updated,
    const std::vector<SyntheticTrialGroup>& trials_removed,
    const std::vector<SyntheticTrialGroup>& groups) {
  base::AutoLock scoped_lock(lock_);

  synthetic_variation_ids_set_.clear();
  ResetLastUpdateTime();

  for (const SyntheticTrialGroup& group : groups) {
    VariationID id =
        GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, group.id());
    // TODO(crbug.com/40214121): Handle duplicated IDs in such a way that is
    // visible to developers, but non-intrusive to users. See
    // crrev/c/3628020/comments/e278cd12_2bb863ef for discussions.
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_ANY_CONTEXT));
    }
    id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES_SIGNED_IN, group.id());
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_SIGNED_IN));
    }
    // Google App IDs omitted because they should never be defined
    // synthetically.
  }
}

void VariationsIdsProvider::MaybeUpdateVariationIDsAndHeaders() {
  lock_.AssertAcquired();

  // Check if an update is needed. If not, return early (the currently cached
  // values are still valid), otherwise, update the cached values and notify
  // observers.  See `UpdateIsNeeded()` for more details.
  const base::Time current_time = clock_->Now();
  if (!UpdateIsNeeded(current_time)) {
    return;
  }
  SetLastUpdateTime(current_time);
  SetNextUpdateTime(GetNextTimeWindowEvent(current_time));

  // Subscribe to field trials if not already subscribed.
  if (!is_subscribed_to_field_trial_list_) {
    const bool success = base::FieldTrialList::AddObserver(this);
    CHECK(success);
    is_subscribed_to_field_trial_list_ = true;
  }

  // Clear the active variation IDs set before adding new ones.
  active_variation_ids_set_.clear();

  // Populate the active variation IDs.
  AddActiveVariationIds(current_time);
  AddForceEnabledVariationIds();
  AddSyntheticVariationIds();
  AddLowEntropySourceValue();
  RemoveForceDisabledVariationIds();

  // Generate the header values.
  for (bool is_signed_in : {false, true}) {
    for (auto context : {Study_GoogleWebVisibility_ANY,
                         Study_GoogleWebVisibility_FIRST_PARTY}) {
      variations_headers_map_[VariationsHeaderKey{is_signed_in, context}] =
          GenerateBase64EncodedProto(is_signed_in, context);
    }
  }

  // Notify observers that the variation IDs header has been updated.
  for (auto* observer : observer_list_) {
    observer->VariationIdsHeaderUpdated();
  }
}

VariationsIdsProvider::ForceIdsResult
VariationsIdsProvider::ForceVariationIdsImpl(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  base::AutoLock scoped_lock(lock_);

  force_enabled_ids_set_.clear();
  ResetLastUpdateTime();

  if (!AddVariationIdsToSet(variation_ids, /*should_dedupe=*/true,
                            &force_enabled_ids_set_)) {
    return ForceIdsResult::INVALID_VECTOR_ENTRY;
  }

  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  /*should_dedupe=*/true,
                                  &force_enabled_ids_set_)) {
    return ForceIdsResult::INVALID_SWITCH_ENTRY;
  }
  return ForceIdsResult::SUCCESS;
}

void VariationsIdsProvider::AddActiveVariationIds(base::Time current_time) {
  lock_.AssertAcquired();

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);

  // Add all IDs for the non-forced, timebox-active, groups.
  for (const auto& entry : active_groups) {
    const auto active_group_id =
        MakeActiveGroupId(entry.trial_name, entry.group_name);
    for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
      const IDCollectionKey key = static_cast<IDCollectionKey>(i);
      const VariationID id =
          GetGoogleVariationID(key, active_group_id, current_time);
      if (id != EMPTY_ID) {
        active_variation_ids_set_.emplace(id, key);
      }
    }
  }
}

void VariationsIdsProvider::AddLowEntropySourceValue() {
  lock_.AssertAcquired();

  // Add the low entropy source value, if it exists, which has one of
  // 8000 possible values (between kLowEntropySourceVariationIdRange[Min/Max],
  // ~13 bits). This is the value that has been used for deriving the variation
  // ids included in the X-Client-Data header and therefore does not reveal
  // additional information about the client when there are more than 13
  // variations. A typical Chrome client has more than 13 variation ids
  // reported.
  //
  // The entropy source value is used for retrospective A/A tests to validate
  // that there's no existing bias between two randomized groups of clients for
  // a later A/B study.
  if (low_entropy_source_value_.has_value()) {
    const int source_value = low_entropy_source_value_.value() +
                             internal::kLowEntropySourceVariationIdRangeMin;
    DCHECK_GE(source_value, internal::kLowEntropySourceVariationIdRangeMin);
    DCHECK_LE(source_value, internal::kLowEntropySourceVariationIdRangeMax);
    active_variation_ids_set_.emplace(source_value,
                                      GOOGLE_WEB_PROPERTIES_ANY_CONTEXT);
  }
}

void VariationsIdsProvider::AddForceEnabledVariationIds() {
  lock_.AssertAcquired();
  active_variation_ids_set_.insert(force_enabled_ids_set_.begin(),
                                   force_enabled_ids_set_.end());
}

void VariationsIdsProvider::AddSyntheticVariationIds() {
  lock_.AssertAcquired();
  active_variation_ids_set_.insert(synthetic_variation_ids_set_.begin(),
                                   synthetic_variation_ids_set_.end());
}

void VariationsIdsProvider::RemoveForceDisabledVariationIds() {
  lock_.AssertAcquired();
  for (const auto& entry : force_disabled_ids_set_) {
    active_variation_ids_set_.erase(entry);
  }
}

std::string VariationsIdsProvider::GenerateBase64EncodedProto(
    bool is_signed_in,
    Study_GoogleWebVisibility context) {
  lock_.AssertAcquired();
  const bool is_first_party_context =
      context == Study_GoogleWebVisibility_FIRST_PARTY;
  ClientVariations proto;
  for (const VariationIDEntry& entry : active_variation_ids_set_) {
    switch (entry.second) {
      case GOOGLE_WEB_PROPERTIES_SIGNED_IN:
        if (is_signed_in) {
          proto.add_variation_id(entry.first);
        }
        break;
      case GOOGLE_WEB_PROPERTIES_ANY_CONTEXT:
        proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_FIRST_PARTY:
        if (is_first_party_context) {
          proto.add_variation_id(entry.first);
        }
        break;
      case GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT:
        proto.add_trigger_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY:
        if (is_first_party_context) {
          proto.add_trigger_variation_id(entry.first);
        }
        break;
      case GOOGLE_APP:
        // These IDs should not be added into Google Web headers.
        break;
      case ID_COLLECTION_COUNT:
        // This case included to get full enum coverage for switch, so that
        // new enums introduce compiler warnings. Nothing to do for this.
        break;
    }
  }

  // Sort the ids and remove duplicates. This makes the representation of any
  // given combination of variation ids deterministic.
  MakeSortedAndUnique(proto.mutable_variation_id());
  MakeSortedAndUnique(proto.mutable_trigger_variation_id());

  const size_t total_id_count =
      proto.variation_id_size() + proto.trigger_variation_id_size();

  if (total_id_count == 0) {
    return std::string();
  }

  // This is the bottleneck for the creation of the header, so validate the size
  // here. Force a hard maximum on the ID count in case the Variations server
  // returns too many IDs and DOSs receiving servers with large requests.
  DCHECK_LE(total_id_count, 75U);
  base::UmaHistogramCounts100("Variations.Headers.ExperimentCount",
                              total_id_count);
  if (total_id_count > 100) {
    return std::string();
  }

  std::string serialized;
  proto.SerializeToString(&serialized);
  return base::Base64Encode(serialized);
}

bool VariationsIdsProvider::AddVariationIdsToSet(
    const std::vector<std::string>& variation_id_strings,
    bool should_dedupe,
    VariationIDEntrySet* target_set) {
  lock_.AssertAcquired();
  for (const std::string& entry_string : variation_id_strings) {
    std::string_view entry = entry_string;
    if (entry.empty()) {
      target_set->clear();
      return false;
    }
    const bool is_trigger_id =
        base::StartsWith(entry, "t", base::CompareCase::SENSITIVE);
    // Remove the "t" prefix if it's there.
    std::string_view trimmed_entry = is_trigger_id ? entry.substr(1) : entry;

    int variation_id = 0;
    if (!base::StringToInt(trimmed_entry, &variation_id)) {
      target_set->clear();
      return false;
    }

    if (should_dedupe && IsDuplicateId(variation_id)) {
      DVLOG(1) << "Invalid variation ID specified: " << entry
               << " (it is already in use)";
      target_set->clear();
      return false;
    }
    target_set->emplace(
        variation_id, is_trigger_id ? GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT
                                    : GOOGLE_WEB_PROPERTIES_ANY_CONTEXT);
  }
  return true;
}

bool VariationsIdsProvider::ParseVariationIdsParameter(
    const std::string& command_line_variation_ids,
    bool should_dedupe,
    VariationIDEntrySet* target_set) {
  lock_.AssertAcquired();
  if (command_line_variation_ids.empty()) {
    return true;
  }
  return AddVariationIdsToSet(
      base::SplitString(command_line_variation_ids, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL),
      should_dedupe, target_set);
}

std::string VariationsIdsProvider::GetClientDataHeader(
    bool is_signed_in,
    Study_GoogleWebVisibility web_visibility) {
  lock_.AssertAcquired();

  auto it = variations_headers_map_.find(
      VariationsHeaderKey{is_signed_in, web_visibility});

  if (it == variations_headers_map_.end()) {
    return std::string();
  }
  // Deliberately return a copy.
  return it->second;
}

std::vector<VariationID> VariationsIdsProvider::GetVariationsVectorImpl(
    const std::set<IDCollectionKey>& keys) {
  // Get all the active variation ids while holding the lock.
  std::vector<VariationID> result;
  {
    base::AutoLock scoped_lock(lock_);
    MaybeUpdateVariationIDsAndHeaders();

    // Copy the requested variations to the output vector.
    result.reserve(active_variation_ids_set_.size());
    for (const auto& entry : active_variation_ids_set_) {
      if (keys.find(entry.second) != keys.end()) {
        result.push_back(entry.first);
      }
    }
  }

  // Sort the ids and remove duplicates. This makes the representation of any
  // given combination of variation ids deterministic.
  MakeSortedAndUnique(&result);
  return result;
}

bool VariationsIdsProvider::IsDuplicateId(VariationID id) {
  lock_.AssertAcquired();
  for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
    const IDCollectionKey key = static_cast<IDCollectionKey>(i);
    // GOOGLE_APP ids may be duplicated. Further validation is done in
    // GroupMapAccessor::ValidateID().
    if (key == GOOGLE_APP) {
      continue;
    }

    const VariationIDEntry entry(id, key);
    if (base::Contains(force_enabled_ids_set_, entry) ||
        base::Contains(synthetic_variation_ids_set_, entry)) {
      return true;
    }
  }
  return false;
}

void VariationsIdsProvider::SetLastUpdateTime(base::Time time) {
  lock_.AssertAcquired();
  last_update_time_ = time;
}

void VariationsIdsProvider::ResetLastUpdateTime() {
  SetLastUpdateTime(base::Time::Min());
}

void VariationsIdsProvider::SetNextUpdateTime(base::Time time) {
  lock_.AssertAcquired();
  next_update_time_ = time;
}

bool VariationsIdsProvider::UpdateIsNeeded(base::Time current_time) const {
  // The `last_update_time_` and prospective `next_update_time_` are cached
  // whenever an update is performed. See `MaybeUpdateVariationIDsAndHeaders()`.
  //
  // When a new field trial is activated, `last_update_time_` is reset to
  // `base::Time::Min()`. See `OnFieldTrialGroupFinalized()`. Similarly, if the
  // force-enabled, force-disabled or synthetic trial ids are  updated,
  // `last_update_time_` is reset to `base::Time::Min()`. This forces a
  // re-calculation of the cached header values. We do this because any new
  // ids may be time-boxed with a window that starts not only before the
  // `current_time`, but also before the `last_update_time_`.
  //
  // Otherwise, the active field trials have not changed, and we can rely on
  // the cached `next_update_time_` to determine whether an update is needed.
  lock_.AssertAcquired();
  return last_update_time_ == base::Time::Min() ||
         current_time >= next_update_time_;
}

}  // namespace variations
