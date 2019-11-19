// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metadata.h"

#include <algorithm>
#include <iterator>
#include <tuple>

namespace metrics {

CallStackProfileMetadata::CallStackProfileMetadata() = default;

CallStackProfileMetadata::~CallStackProfileMetadata() = default;

// This function is invoked on the profiler thread while the target thread is
// suspended so must not take any locks, including indirectly through use of
// heap allocation, LOG, CHECK, or DCHECK.
void CallStackProfileMetadata::RecordMetadata(
    base::ProfileBuilder::MetadataProvider* metadata_provider) {
  metadata_item_count_ = metadata_provider->GetItems(&metadata_items_);
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

bool CallStackProfileMetadata::MetadataKeyCompare::operator()(
    const MetadataKey& a,
    const MetadataKey& b) const {
  return std::tie(a.name_hash, a.key) < std::tie(b.name_hash, b.key);
}

CallStackProfileMetadata::MetadataKey::MetadataKey(uint64_t name_hash,
                                                   base::Optional<int64_t> key)
    : name_hash(name_hash), key(key) {}

CallStackProfileMetadata::MetadataKey::MetadataKey(const MetadataKey& other) =
    default;
CallStackProfileMetadata::MetadataKey& CallStackProfileMetadata::MetadataKey::
operator=(const MetadataKey& other) = default;

CallStackProfileMetadata::MetadataMap
CallStackProfileMetadata::CreateMetadataMap(
    base::ProfileBuilder::MetadataItemArray items,
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
