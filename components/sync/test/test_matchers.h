// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_MATCHERS_H_
#define COMPONENTS_SYNC_TEST_TEST_MATCHERS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/deletion_origin.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {
using ::testing::_;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::IsEmpty;

// Matcher for std::optional<ModelError>: verifies that it contains no error.
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
         sync_pb::DataTypeState().SerializeAsString() ==
             arg->GetDataTypeState().SerializeAsString() &&
         arg->TakeAllMetadata().empty();
}

// Matcher for MetadataBatch: general purpose verification given two matchers,
// of type Matcher<DataTypeState> and Matcher<EntityMetadataMap> respectively.
MATCHER_P2(MetadataBatchContains, state, entities, "") {
  if (arg == nullptr) {
    *result_listener << "which is null";
    return false;
  }
  if (!ExplainMatchResult(testing::Matcher<sync_pb::DataTypeState>(state),
                          arg->GetDataTypeState(), result_listener)) {
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

// Matcher for sync_pb::DataTypeState: verifies that field
// `encryption_key_name` is equal to the provided string `expected_key_name`.
MATCHER_P(HasEncryptionKeyName, expected_key_name, "") {
  return arg.encryption_key_name() == expected_key_name;
}

// Matcher for sync_pb::DataTypeState: verifies that initial sync is done.
MATCHER(HasInitialSyncDone, "") {
  return arg.initial_sync_state() ==
         sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE;
}

// Matcher for sync_pb::DataTypeState: verifies that initial sync is not done.
MATCHER(HasNotInitialSyncDone, "") {
  return arg.initial_sync_state() ==
         sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED;
}

MATCHER_P2(MatchesDeletionOrigin, expected_version, expected_location, "") {
  const sync_pb::DeletionOrigin& actual_origin = arg;
  if (actual_origin.chromium_version() != expected_version) {
    *result_listener << "Expected version " << expected_version << " but got "
                     << actual_origin.chromium_version();
    return false;
  }
  if (actual_origin.file_name_hash() !=
      base::PersistentHash(expected_location.file_name())) {
    *result_listener << "Unexpected file name hash: "
                     << actual_origin.file_name_hash();
    return false;
  }
  if (actual_origin.file_line_number() != expected_location.line_number()) {
    *result_listener << "Unexpected line number: "
                     << actual_origin.file_line_number();
    return false;
  }
  return true;
}

// Checks whether the item matches a syncer::LocalDataItemModel.
MATCHER_P4(MatchesLocalDataItemModel, id, icon, title, subtitle, "") {
  return ExplainMatchResult(Field(&syncer::LocalDataItemModel::id, id), arg,
                            result_listener) &&
         ExplainMatchResult(Field(&syncer::LocalDataItemModel::icon, icon), arg,
                            result_listener) &&
         ExplainMatchResult(Field(&syncer::LocalDataItemModel::title, title),
                            arg, result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataItemModel::subtitle, subtitle), arg,
             result_listener);
}

// Checks whether the description matches a syncer::LocalDataDescription.
MATCHER_P5(MatchesLocalDataDescription,
           type,
           local_data_models,
           item_count,
           domains,
           domain_count,
           "") {
  return ExplainMatchResult(Field(&syncer::LocalDataDescription::type, type),
                            arg, result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::local_data_models,
                   local_data_models),
             arg, result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::item_count, item_count), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::domains, domains), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::domain_count, domain_count),
             arg, result_listener);
}

MATCHER(IsEmptyLocalDataDescription, "") {
  return ExplainMatchResult(
      MatchesLocalDataDescription(_, IsEmpty(), Eq(0u), IsEmpty(), Eq(0u)), arg,
      result_listener);
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_MATCHERS_H_
