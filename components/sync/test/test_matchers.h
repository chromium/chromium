// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_MATCHERS_H_
#define COMPONENTS_SYNC_TEST_TEST_MATCHERS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

// Matcher for absl::optional<ModelError>: verifies that it contains no error.
MATCHER(NoModelError, "") {
  if (arg.has_value()) {
    *result_listener << "which represents error: " << arg->ToString();
    return false;
  }
  return true;
}

// Matcher for MetadataBatch: verifies that it is empty (i.e. contains neither
// entity metadata nor global model state.
MATCHER(IsEmptyMetadataBatch, "") {
  return arg != nullptr &&
         sync_pb::ModelTypeState().SerializeAsString() ==
             arg->GetModelTypeState().SerializeAsString() &&
         arg->TakeAllMetadata().empty();
}

// Matcher for MetadataBatch: general purpose verification given two matchers,
// of type Matcher<ModelTypeState> and Matcher<EntityMetadataMap> respectively.
MATCHER_P2(MetadataBatchContains, state, entities, "") {
  if (arg == nullptr) {
    *result_listener << "which is null";
    return false;
  }
  if (!ExplainMatchResult(testing::Matcher<sync_pb::ModelTypeState>(state),
                          arg->GetModelTypeState(), result_listener)) {
    return false;
  }

  // We need to convert the map values to non-pointers in order to make them
  // copyable and use gmock.
  std::map<std::string, std::unique_ptr<sync_pb::EntityMetadata>>
      metadata_by_storage_key = arg->TakeAllMetadata();
  std::map<std::string, sync_pb::EntityMetadata> copyable_metadata;
  for (auto& [storage_key, metadata] : metadata_by_storage_key) {
    copyable_metadata[storage_key] = std::move(*metadata);
  }

  return ExplainMatchResult(
      testing::Matcher<std::map<std::string, sync_pb::EntityMetadata>>(
          entities),
      copyable_metadata, result_listener);
}

// Matcher for sync_pb::ModelTypeState: verifies that field
// |encryption_key_name| is equal to the provided string |expected_key_name|.
MATCHER_P(HasEncryptionKeyName, expected_key_name, "") {
  return arg.encryption_key_name() == expected_key_name;
}

// Matcher for sync_pb::ModelTypeState: verifies that initial sync is done.
MATCHER(HasInitialSyncDone, "") {
  return arg.initial_sync_done();
}

// Matcher for sync_pb::ModelTypeState: verifies that initial sync is not done.
MATCHER(HasNotInitialSyncDone, "") {
  return !arg.initial_sync_done();
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_MATCHERS_H_
