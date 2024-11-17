// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type_histogram.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/sync/base/data_type.h"

namespace syncer {

namespace {

const char kLegacyModelTypeCountHistogramPrefix[] = "Sync.ModelTypeCount4.";

const char kDataTypeMemoryHistogramPrefix[] = "Sync.DataTypeMemoryKB.";
const char kDataTypeCountHistogramPrefix[] = "Sync.DataTypeCount.";
const char kDataTypeUpdateDropHistogramPrefix[] = "Sync.DataTypeUpdateDrop.";
const char kDataTypeNumUnsyncedEntitiesOnModelReady[] =
    "Sync.DataTypeNumUnsyncedEntitiesOnModelReady.";

const char kEntitySizeWithMetadataHistogramPrefix[] =
    "Sync.EntitySizeOnCommit.Entity.WithMetadata.";
const char kEntitySizeSpecificsOnlyHistogramPrefix[] =
    "Sync.EntitySizeOnCommit.Entity.SpecificsOnly.";
const char kEntitySizeTombstoneHistogramPrefix[] =
    "Sync.EntitySizeOnCommit.Tombstone.";

std::string GetHistogramSuffixForUpdateDropReason(UpdateDropReason reason) {
  switch (reason) {
    case UpdateDropReason::kInconsistentClientTag:
      return "InconsistentClientTag";
    case UpdateDropReason::kCannotGenerateStorageKey:
      return "CannotGenerateStorageKey";
    case UpdateDropReason::kTombstoneInFullUpdate:
      return "TombstoneInFullUpdate";
    case UpdateDropReason::kTombstoneForNonexistentInIncrementalUpdate:
      return "TombstoneForNonexistentInIncrementalUpdate";
    case UpdateDropReason::kDecryptionPending:
      return "DecryptionPending";
    case UpdateDropReason::kDecryptionPendingForTooLong:
      return "DecryptionPendingForTooLong";
    case UpdateDropReason::kFailedToDecrypt:
      return "FailedToDecrypt";
    case UpdateDropReason::kDroppedByBridge:
      return "DroppedByBridge";
  }
}

}  // namespace

void SyncRecordDataTypeUpdateDropReason(UpdateDropReason reason,
                                        DataType type) {
  std::string full_histogram_name =
      kDataTypeUpdateDropHistogramPrefix +
      GetHistogramSuffixForUpdateDropReason(reason);
  base::UmaHistogramEnumeration(full_histogram_name,
                                DataTypeHistogramValue(type));
}

void SyncRecordDataTypeMemoryHistogram(DataType data_type, size_t bytes) {
  std::string type_string = DataTypeToHistogramSuffix(data_type);
  std::string full_histogram_name =
      kDataTypeMemoryHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, bytes / 1024);
}

void SyncRecordDataTypeCountHistogram(DataType data_type, size_t count) {
  std::string type_string = DataTypeToHistogramSuffix(data_type);
  std::string full_histogram_name = kDataTypeCountHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, count);

  // TODO(crbug.com/358120886): Stop recording once alerts are switched to use
  // Sync.DataTypeCount.
  std::string legacy_histogram_name =
      kLegacyModelTypeCountHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(legacy_histogram_name, count);
}

void SyncRecordDataTypeEntitySizeHistogram(DataType data_type,
                                           bool is_tombstone,
                                           size_t specifics_bytes,
                                           size_t total_bytes) {
  std::string type_string = DataTypeToHistogramSuffix(data_type);
  if (is_tombstone) {
    // For tombstones, don't bother recording the `specifics_size` since the
    // specifics is always empty.
    base::UmaHistogramCounts100000(
        kEntitySizeTombstoneHistogramPrefix + type_string, total_bytes);
  } else {
    base::UmaHistogramCounts100000(
        kEntitySizeSpecificsOnlyHistogramPrefix + type_string, specifics_bytes);
    base::UmaHistogramCounts100000(
        kEntitySizeWithMetadataHistogramPrefix + type_string, total_bytes);
  }
}

void SyncRecordDataTypeNumUnsyncedEntitiesOnModelReady(
    DataType data_type,
    size_t num_unsynced_entities) {
  const std::string full_histogram_name =
      std::string(kDataTypeNumUnsyncedEntitiesOnModelReady) +
      DataTypeToHistogramSuffix(data_type);
  base::UmaHistogramCounts1000(full_histogram_name, num_unsynced_entities);
}

void SyncRecordModelClearedOnceHistogram(DataType data_type) {
  base::UmaHistogramEnumeration("Sync.DataTypeClearedOnce",
                                DataTypeHistogramValue(data_type));
}

void RecordSyncToSigninMigrationReadingListStep(ReadingListMigrationStep step) {
  base::UmaHistogramEnumeration(
      "Sync.SyncToSigninMigration.ReadingListMigrationStep", step);
}

}  // namespace syncer
