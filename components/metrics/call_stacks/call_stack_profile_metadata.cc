// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_metadata.h"

#include <iterator>
#include <tuple>

#include "base/ranges/algorithm.h"

namespace metrics {

namespace {

class MatchesNameHashIndexAndKey {
 public:
  MatchesNameHashIndexAndKey(int name_hash_index, std::optional<int64_t> key)
      : name_hash_index_(name_hash_index), key_(key) {}

  bool operator()(const CallStackProfile::MetadataItem& item) const {
    std::optional<int64_t> item_key_as_optional =
        item.has_key() ? item.key() : std::optional<int64_t>();
    return item.name_hash_index() == name_hash_index_ &&
           key_ == item_key_as_optional;
  }

 private:
  int name_hash_index_;
  std::optional<int64_t> key_;
};

// Finds the last value for a prior metadata application with |name_hash_index|
// and |key| from |begin| that was still active at |end|. Returns nullopt if no
// such application exists.
std::optional<int64_t> FindLastOpenEndedMetadataValue(
    int name_hash_index,
    std::optional<int64_t> key,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>::iterator
        begin,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>::iterator
        end) {
  // Search the samples backward from the end of the range, looking for an
  // application of the same metadata name hash/key that doesn't have a
  // corresponding removal.
  const auto rbegin = std::make_reverse_iterator(end);
  const auto rend = std::make_reverse_iterator(begin);
  for (auto it = rbegin; it != rend; ++it) {
    auto item = base::ranges::find_if(
        it->metadata(), MatchesNameHashIndexAndKey(name_hash_index, key));

    if (item == it->metadata().end()) {
      // The sample does not contain a matching item.
      continue;
    }

    if (!item->has_value()) {
      // A matching item was previously applied, but stopped being applied
      // before the last sample in the range.
      return std::nullopt;
    }

    // Else, a matching item was applied at this sample.
    return item->value();
  }

  // No matching items were previously applied.
  return std::nullopt;
}

// Clears any existing metadata changes between |begin| and |end|.
void ClearExistingMetadata(
    const int name_hash_index,
    std::optional<int64_t> key,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>::iterator
        begin,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>::iterator
        end) {
  for (auto it = begin; it != end; ++it) {
    google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem>*
        metadata = it->mutable_metadata();
    metadata->erase(
        std::remove_if(metadata->begin(), metadata->end(),
                       MatchesNameHashIndexAndKey(name_hash_index, key)),
        metadata->end());
  }
}

// Sets the state of |item| to the provided values.
void SetMetadataItem(int name_hash_index,
                     std::optional<int64_t> key,
                     std::optional<int64_t> value,
                     CallStackProfile::MetadataItem* item) {
  item->set_name_hash_index(name_hash_index);
  if (key.has_value())
    item->set_key(*key);
  if (value.has_value())
    item->set_value(*value);
}

}  // namespace

CallStackProfileMetadata::CallStackProfileMetadata() = default;

CallStackProfileMetadata::~CallStackProfileMetadata() = default;

// This function is invoked on the profiler thread while the target thread is
// suspended so must not take any locks, including indirectly through use of
// heap allocation, LOG, CHECK, or DCHECK.
void CallStackProfileMetadata::RecordMetadata(
    const base::MetadataRecorder::MetadataProvider& metadata_provider) {
  metadata_item_count_ = metadata_provider.GetItems(&metadata_items_);
}

google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem>
CallStackProfileMetadata::CreateSampleMetadata(
    google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes) {
  DCHECK_EQ(metadata_hashes_cache_.size(),
            static_cast<size_t>(metadata_name_hashes->size()));

  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem>
      metadata_items;
  MetadataMap current_items =
      CreateMetadataMap(metadata_items_, metadata_item_count_);

  for (auto item :
       GetNewOrModifiedMetadataItems(current_items, previous_items_)) {
    size_t name_hash_index =
        MaybeAppendNameHash(item.first.name_hash, metadata_name_hashes);

    CallStackProfile::MetadataItem* profile_item = metadata_items.Add();
    profile_item->set_name_hash_index(name_hash_index);
    if (item.first.key.has_value())
      profile_item->set_key(*item.first.key);
    profile_item->set_value(item.second);
  }

  for (auto item : GetDeletedMetadataItems(current_items, previous_items_)) {
    size_t name_hash_index =
        MaybeAppendNameHash(item.first.name_hash, metadata_name_hashes);

    CallStackProfile::MetadataItem* profile_item = metadata_items.Add();
    profile_item->set_name_hash_index(name_hash_index);
    if (item.first.key.has_value())
      profile_item->set_key(*item.first.key);
    // Leave the value empty to indicate that the item was deleted.
  }

  previous_items_ = std::move(current_items);
  metadata_item_count_ = 0;

  return metadata_items;
}

void CallStackProfileMetadata::ApplyMetadata(
    const base::MetadataRecorder::Item& item,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>::iterator
        begin,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>::iterator
        end,
    google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>*
        stack_samples,
    google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes) {
  if (begin == end)
    return;

  // This function works by finding the previously effective metadata values
  // with the same name hash and key at the begin and end of the range. The
  // range is then cleared of any metadata changes (with the same name hash and
  // key) in preparation for recording the metadata application at the start of
  // the range and the removal at the end of the range.
  //
  // We expect this function to be called at most a few times per collection, so
  // it's written to be readable rather than optimally performant. If
  // performance becomes a concern, the two FindLastOpenEndedMetadataValue()
  // calls can be merged into one to avoid one loop over the samples, or a more
  // efficient representation of when metadata is set/unset could be used to
  // avoid the looping entirely.

  const size_t name_hash_index =
      MaybeAppendNameHash(item.name_hash, metadata_name_hashes);

  // The previously set metadata value immediately prior to begin, or nullopt if
  // none.
  const std::optional<int64_t> previous_value_before_begin =
      FindLastOpenEndedMetadataValue(name_hash_index, item.key,
                                     stack_samples->begin(), begin);

  // The end of the range will be in one of two states: terminating before the
  // last recorded sample, or terminating at the last recorded sample. If it
  // terminates before the last recorded sample then we are able to record the
  // removal of the metadata on the sample following the end of the
  // range. Otherwise we have to wait for the next recorded sample to record the
  // removal of the metadata.
  const bool range_terminates_at_last_sample = end == stack_samples->end();

  // The previously set metadata value at *end (or the one to be set on the next
  // sample if range_terminates_at_last_sample).
  const std::optional<int64_t> previous_value_at_end =
      FindLastOpenEndedMetadataValue(
          name_hash_index, item.key, stack_samples->begin(),
          // If a sample past the end exists check its value as well, since
          // we'll be overwriting that sample's metadata below.
          (range_terminates_at_last_sample ? end : end + 1));

  ClearExistingMetadata(name_hash_index, item.key, begin,
                        (range_terminates_at_last_sample ? end : end + 1));

  // Enable the metadata on the initial sample if different than the previous
  // state.
  if (!previous_value_before_begin.has_value() ||
      *previous_value_before_begin != item.value) {
    SetMetadataItem(name_hash_index, item.key, item.value,
                    begin->mutable_metadata()->Add());
  }

  if (range_terminates_at_last_sample) {
    // We note that the metadata item that was set for the last sample in
    // previous_items_ to enable computing the appropriate deltas from it on
    // the next recorded sample.
    previous_items_[MetadataKey(item.name_hash, item.key)] = item.value;
  } else if (!previous_value_at_end.has_value() ||
             *previous_value_at_end != item.value) {
    // A sample exists past the end of the range so we can use that sample to
    // record the end of the metadata application. We do so if there was no
    // previous value at the end or it was different than what we set at begin.
    SetMetadataItem(name_hash_index, item.key,
                    // If we had a previously set item at the end of the range,
                    // set its value. Otherwise leave the value empty to denote
                    // that it is being unset.
                    previous_value_at_end.has_value()
                        ? *previous_value_at_end
                        : std::optional<int64_t>(),
                    end->mutable_metadata()->Add());
  }
}

void CallStackProfileMetadata::SetMetadata(
    const base::MetadataRecorder::Item& src_item,
    CallStackProfile::MetadataItem* dest_item,
    google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes) {
  const size_t name_hash_index =
      MaybeAppendNameHash(src_item.name_hash, metadata_name_hashes);

  SetMetadataItem(name_hash_index, src_item.key, src_item.value, dest_item);
}

bool CallStackProfileMetadata::MetadataKeyCompare::operator()(
    const MetadataKey& a,
    const MetadataKey& b) const {
  return std::tie(a.name_hash, a.key) < std::tie(b.name_hash, b.key);
}

CallStackProfileMetadata::MetadataKey::MetadataKey(uint64_t name_hash,
                                                   std::optional<int64_t> key)
    : name_hash(name_hash), key(key) {}

CallStackProfileMetadata::MetadataKey::MetadataKey(const MetadataKey& other) =
    default;
CallStackProfileMetadata::MetadataKey& CallStackProfileMetadata::MetadataKey::
operator=(const MetadataKey& other) = default;

CallStackProfileMetadata::MetadataMap
CallStackProfileMetadata::CreateMetadataMap(
    base::MetadataRecorder::ItemArray items,
    size_t item_count) {
  MetadataMap item_map;
  for (size_t i = 0; i < item_count; ++i)
    item_map[MetadataKey{items[i].name_hash, items[i].key}] = items[i].value;
  return item_map;
}

CallStackProfileMetadata::MetadataMap
CallStackProfileMetadata::GetNewOrModifiedMetadataItems(
    const MetadataMap& current_items,
    const MetadataMap& previous_items) {
  MetadataMap new_or_modified_items;
  // Find the new or modified items by subtracting any previous items that are
  // exactly the same as the current items (i.e. equal in key *and* value).
  auto key_and_value_comparator = [](const std::pair<MetadataKey, int64_t>& a,
                                     const std::pair<MetadataKey, int64_t>& b) {
    return std::tie(a.first.name_hash, a.first.key, a.second) <
           std::tie(b.first.name_hash, b.first.key, b.second);
  };
  std::set_difference(
      current_items.begin(), current_items.end(), previous_items.begin(),
      previous_items.end(),
      std::inserter(new_or_modified_items, new_or_modified_items.begin()),
      key_and_value_comparator);
  return new_or_modified_items;
}

CallStackProfileMetadata::MetadataMap
CallStackProfileMetadata::GetDeletedMetadataItems(
    const MetadataMap& current_items,
    const MetadataMap& previous_items) {
  MetadataMap deleted_items;
  // Find the deleted metadata items by subtracting the current items from the
  // previous items, comparing items solely map key (as opposed to map key and
  // value as in GetNewOrModifiedMetadataItems()). Comparing by key is necessary
  // to distinguish modified items from deleted items: subtraction of modified
  // items, which have the same key but different values, should produce the
  // empty set. Deleted items have a key only in |previous_items| so should be
  // retained in the result.
  auto key_comparator = [](const std::pair<MetadataKey, int64_t>& lhs,
                           const std::pair<MetadataKey, int64_t>& rhs) {
    return MetadataKeyCompare()(lhs.first, rhs.first);
  };
  std::set_difference(previous_items.begin(), previous_items.end(),
                      current_items.begin(), current_items.end(),
                      std::inserter(deleted_items, deleted_items.begin()),
                      key_comparator);
  return deleted_items;
}

size_t CallStackProfileMetadata::MaybeAppendNameHash(
    uint64_t name_hash,
    google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes) {
  std::unordered_map<uint64_t, int>::iterator it;
  bool inserted;
  int next_item_index = metadata_name_hashes->size();

  std::tie(it, inserted) =
      metadata_hashes_cache_.emplace(name_hash, next_item_index);
  if (inserted)
    metadata_name_hashes->Add(name_hash);

  return it->second;
}

}  // namespace metrics
