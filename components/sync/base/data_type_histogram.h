// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
#define COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_

#include "components/sync/base/data_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {

// The enum values are used for histogram suffixes. When adding a new type here,
// extend also the "Sync.DataTypeUpdateDrop" histogram in histograms.xml.
enum class UpdateDropReason {
  kInconsistentClientTag,
  kCannotGenerateStorageKey,
  kTombstoneInFullUpdate,
  kTombstoneForNonexistentInIncrementalUpdate,
  kDecryptionPending,
  kDecryptionPendingForTooLong,
  kFailedToDecrypt,
  // This should effectively replace kCannotGenerateStorageKey in the long run.
  kDroppedByBridge
};

// LINT.IfChange(UnsyncedDataRecordingEvent)
enum class UnsyncedDataRecordingEvent {
  // Upon `DataTypeLocalChangeProcessor::ModelReadyToSync()` call.
  kOnModelReady,
  // When the user initiates a signout flow (but has not confirmed yet).
  // And is in pending state.
  kOnSignoutConfirmationFromPendingState,
  // And is not in pending state.
  kOnSignoutConfirmation,
  // Right after the user reauthenticates and fixes their signin pending state.
  // Only recorded in transport mode. Not recorded for sync-the-feature (aka
  // "Sync paused").
  kOnReauthFromPendingState,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sync/histograms.xml:UnsyncedDataRecordingEventVariants)

// Records that a remote update of an entity of type `type` got dropped into a
// `reason` related histogram.
void SyncRecordDataTypeUpdateDropReason(UpdateDropReason reason, DataType type);

// Converts memory size `bytes` into kilobytes and records it into `data_type`
// related histogram for memory footprint of sync data.
void SyncRecordDataTypeMemoryHistogram(DataType data_type, size_t bytes);

// Records `count` into a `data_type` related histogram for count of sync
// entities.
void SyncRecordDataTypeCountHistogram(DataType data_type, size_t count);

// Records the serialized byte size of a sync entity from `data_type`, both
// with and without sync metadata (`total_bytes` and `specifics_bytes`
// respectively). Meant to be called when the entity is committed.
void SyncRecordDataTypeEntitySizeHistogram(DataType data_type,
                                           bool is_tombstone,
                                           size_t specifics_bytes,
                                           size_t total_bytes);

// Records the amount of unsynced entities for the given `unsynced_data`.
void SyncRecordDataTypeNumUnsyncedEntitiesFromDataCounts(
    UnsyncedDataRecordingEvent event,
    absl::flat_hash_map<DataType, size_t> unsynced_data);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncToSigninMigrationReadingListStep)
enum class ReadingListMigrationStep {
  kMigrationRequested = 0,
  kMigrationStarted = 1,
  kMigrationFailed = 2,
  kMigrationFinishedAndPrefCleared = 3,
  kMaxValue = kMigrationFinishedAndPrefCleared
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncToSigninMigrationReadingListStep)
void RecordSyncToSigninMigrationReadingListStep(ReadingListMigrationStep step);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
