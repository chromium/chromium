// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type_histogram.h"

#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/sync/base/data_type.h"

namespace syncer {

namespace {

constexpr char kDataTypeMemoryHistogramPrefix[] = "Sync.DataTypeMemoryKB.";
constexpr char kDataTypeCountHistogramPrefix[] = "Sync.DataTypeCount.";
constexpr char kDataTypeUpdateDropHistogramPrefix[] = "Sync.DataTypeUpdateDrop.";
constexpr char kDataTypeNumUnsyncedEntities[] = "Sync.DataTypeNumUnsyncedEntities";

// Suffixes for `kDataTypeNumUnsyncedEntities`:
constexpr char kDataTypeNumUnsyncedEntitiesOnModelReady[] = "OnModelReady";
constexpr char
    kDataTypeNumUnsyncedEntitiesOnSignoutConfirmationFromPendingState[] =
        "OnSignoutConfirmationFromPendingState";
constexpr char kDataTypeNumUnsyncedEntitiesOnSignoutConfirmation[] =
    "OnSignoutConfirmation";
constexpr char kDataTypeNumUnsyncedEntitiesOnReauthFromPendingState[] =
    "OnReauthFromPendingState";

constexpr char kEntitySizeWithMetadataHistogramPrefix[] =
    "Sync.EntitySizeOnCommit.Entity.WithMetadata.";
constexpr char kEntitySizeSpecificsOnlyHistogramPrefix[] =
    "Sync.EntitySizeOnCommit.Entity.SpecificsOnly.";
constexpr char kEntitySizeTombstoneHistogramPrefix[] =
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

// Returns the suffix  for the histograms recording the number of unsynced
// entities.
const char* SyncGetNumUnsyncedEntitiesHistogramSuffix(
    UnsyncedDataRecordingEvent event) {
  switch (event) {
    case UnsyncedDataRecordingEvent::kOnModelReady:
      return kDataTypeNumUnsyncedEntitiesOnModelReady;
    case UnsyncedDataRecordingEvent::kOnSignoutConfirmationFromPendingState:
      return kDataTypeNumUnsyncedEntitiesOnSignoutConfirmationFromPendingState;
    case UnsyncedDataRecordingEvent::kOnSignoutConfirmation:
      return kDataTypeNumUnsyncedEntitiesOnSignoutConfirmation;
    case UnsyncedDataRecordingEvent::kOnReauthFromPendingState:
      return kDataTypeNumUnsyncedEntitiesOnReauthFromPendingState;
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

void SyncRecordDataTypeNumUnsyncedEntitiesFromDataCounts(
    UnsyncedDataRecordingEvent event,
    absl::flat_hash_map<DataType, size_t> unsynced_data) {
  for (const auto& [type, count] : unsynced_data) {
    base::UmaHistogramCounts1000(
        base::StrCat({kDataTypeNumUnsyncedEntities,
                      SyncGetNumUnsyncedEntitiesHistogramSuffix(event), ".",
                      DataTypeToHistogramSuffix(type)}),
        count);
  }
}

void RecordSyncToSigninMigrationReadingListStep(ReadingListMigrationStep step) {
  base::UmaHistogramEnumeration(
      "Sync.SyncToSigninMigration.ReadingListMigrationStep", step);
}

}  // namespace syncer
