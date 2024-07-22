// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type_histogram.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/sync/base/model_type.h"

namespace syncer {

namespace {
const char kModelTypeMemoryHistogramPrefix[] = "Sync.ModelTypeMemoryKB.";
const char kModelTypeCountHistogramPrefix[] = "Sync.ModelTypeCount4.";
const char kModelTypeUpdateDropHistogramPrefix[] = "Sync.ModelTypeUpdateDrop.";
const char kModelTypeNumUnsyncedEntitiesOnModelReady[] =
    "Sync.ModelTypeNumUnsyncedEntitiesOnModelReady.";

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

void SyncRecordModelTypeUpdateDropReason(UpdateDropReason reason,
                                         ModelType type) {
  std::string full_histogram_name =
      kModelTypeUpdateDropHistogramPrefix +
      GetHistogramSuffixForUpdateDropReason(reason);
  base::UmaHistogramEnumeration(full_histogram_name,
                                ModelTypeHistogramValue(type));
}

void SyncRecordModelTypeMemoryHistogram(ModelType model_type, size_t bytes) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
  std::string full_histogram_name =
      kModelTypeMemoryHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, bytes / 1024);
}

void SyncRecordModelTypeCountHistogram(ModelType model_type, size_t count) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
  std::string full_histogram_name =
      kModelTypeCountHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, count);
}

void SyncRecordModelTypeEntitySizeHistogram(ModelType model_type,
                                            bool is_tombstone,
                                            size_t specifics_bytes,
                                            size_t total_bytes) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
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

void SyncRecordModelTypeNumUnsyncedEntitiesOnModelReady(
    ModelType model_type,
    size_t num_unsynced_entities) {
  const std::string full_histogram_name =
      std::string(kModelTypeNumUnsyncedEntitiesOnModelReady) +
      ModelTypeToHistogramSuffix(model_type);
  base::UmaHistogramCounts1000(full_histogram_name, num_unsynced_entities);
}

void SyncRecordModelClearedOnceHistogram(ModelType model_type) {
  base::UmaHistogramEnumeration("Sync.ModelTypeClearedOnce",
                                ModelTypeHistogramValue(model_type));
}

void RecordSyncToSigninMigrationReadingListStep(ReadingListMigrationStep step) {
  base::UmaHistogramEnumeration(
      "Sync.SyncToSigninMigration.ReadingListMigrationStep", step);
}

}  // namespace syncer
