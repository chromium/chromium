// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_METADATA_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_METADATA_H_

#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

#include "base/profiler/metadata_recorder.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Helper class for maintaining metadata state across samples and generating
// metadata proto messages.
class CallStackProfileMetadata {
 public:
  CallStackProfileMetadata();
  ~CallStackProfileMetadata();

  CallStackProfileMetadata(const CallStackProfileMetadata& other) = delete;
  CallStackProfileMetadata& operator=(const CallStackProfileMetadata& other) =
      delete;

  // Records the metadata for the next sample.
  void RecordMetadata(
      const base::MetadataRecorder::MetadataProvider& metadata_provider);

  // Creates MetadataItems for the currently active metadata, adding new name
  // hashes to |metadata_name_hashes| if necessary. The same
  // |metadata_name_hashes| must be passed to each invocation, and must not be
  // modified outside this function.
  google::protobuf::RepeatedPtrField<CallStackProfile::MetadataItem>
  CreateSampleMetadata(
      google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes);

  // Applies the |item| to the samples between |begin| and |end| in
  // |stack_samples|. Overwrites any existing metadata item with the same key
  // and value that are already applied to the samples.
  void ApplyMetadata(
      const base::MetadataRecorder::Item& item,
      google::protobuf::RepeatedPtrField<
          CallStackProfile::StackSample>::iterator begin,
      google::protobuf::RepeatedPtrField<
          CallStackProfile::StackSample>::iterator end,
      google::protobuf::RepeatedPtrField<CallStackProfile::StackSample>*
          stack_samples,
      google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes);

  // Set the value provided in |src_item| to |dest_item|, where |src_item| is
  // in chromium format, and |dest_item| is in proto format. Intended for
  // setting profile-global metadata items. Setting per-sample metadata should
  // be done via the functions above.
  void SetMetadata(
      const base::MetadataRecorder::Item& src_item,
      CallStackProfile::MetadataItem* dest_item,
      google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes);

 private:
  // Comparison function for the metadata map.
  struct MetadataKey;
  struct MetadataKeyCompare {
    bool operator()(const MetadataKey& a, const MetadataKey& b) const;
  };

  // Definitions for a map-based representation of sample metadata.
  struct MetadataKey {
    MetadataKey(uint64_t name_hash, std::optional<int64_t> key);

    MetadataKey(const MetadataKey& other);
    MetadataKey& operator=(const MetadataKey& other);

    // The name_hash and optional user-specified key uniquely identifies a
    // metadata value. See base::MetadataRecorder for details.
    uint64_t name_hash;
    std::optional<int64_t> key;
  };
  using MetadataMap = std::map<MetadataKey, int64_t, MetadataKeyCompare>;

  // Creates the metadata map from the array of items.
  MetadataMap CreateMetadataMap(base::MetadataRecorder::ItemArray items,
                                size_t item_count);

  // Returns all metadata items with new values in the current sample.
  MetadataMap GetNewOrModifiedMetadataItems(const MetadataMap& current_items,
                                            const MetadataMap& previous_items);

  // Returns all metadata items deleted since the previous sample.
  MetadataMap GetDeletedMetadataItems(const MetadataMap& current_items,
                                      const MetadataMap& previous_items);

  // Appends the |name_hash| to |name_hashes| if it's not already
  // present. Returns its index in |name_hashes|.
  size_t MaybeAppendNameHash(
      uint64_t name_hash,
      google::protobuf::RepeatedField<uint64_t>* metadata_name_hashes);

  // The data provided for the next sample.
  base::MetadataRecorder::ItemArray metadata_items_;
  size_t metadata_item_count_ = 0;

  // The data provided for the previous sample.
  MetadataMap previous_items_;

  // Maps metadata hash to index in |metadata_name_hash| array.
  std::unordered_map<uint64_t, int> metadata_hashes_cache_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_METADATA_H_
