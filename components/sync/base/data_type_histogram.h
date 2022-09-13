// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
#define COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_

#include "base/metrics/histogram_macros.h"
#include "components/sync/base/model_type.h"

namespace syncer {

// The enum values are used for histogram suffixes. When adding a new type here,
// extend also the "SyncModelTypeUpdateDrop" <histogram_suffixes> in
// histograms.xml.
enum class UpdateDropReason {
  kInconsistentClientTag,
  kCannotGenerateStorageKey,
  kTombstoneInFullUpdate,
  kTombstoneForNonexistentInIncrementalUpdate,
  kDecryptionPending,
  kDecryptionPendingForTooLong,
  kFailedToDecrypt
};

// Records that a remote update of an entity of type |type| got dropped into a
// |reason| related histogram.
void SyncRecordModelTypeUpdateDropReason(UpdateDropReason reason,
                                         ModelType type);

// Converts memory size |bytes| into kilobytes and records it into |model_type|
// related histogram for memory footprint of sync data.
void SyncRecordModelTypeMemoryHistogram(ModelType model_type, size_t bytes);

// Records |count| into a |model_type| related histogram for count of sync
// entities.
void SyncRecordModelTypeCountHistogram(ModelType model_type, size_t count);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
