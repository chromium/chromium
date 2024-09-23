// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_PROCESSOR_METRICS_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_PROCESSOR_METRICS_H_

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_mode.h"

namespace syncer {

// Logs histograms representing the duration of the initial setup (, i.e. the
// time from the start of the configuration of sync until the data type receives
// all its sync data and the data is ready for the user.
void LogDataTypeConfigurationTime(DataType data_type,
                                  SyncMode mode,
                                  base::Time configuration_start_time);

// Logs histograms representing the number of updates that an implementations of
// DataTypeProcessor receive via OnUpdateReceived().
void LogUpdatesReceivedByProcessorHistogram(DataType data_type,
                                            bool is_initial_sync,
                                            size_t num_updates);

// Logs histogram representing the staleness of an incoming incremental
// (non-initial) update, when received by a DataTypeProcessor via
// OnUpdateReceived().
void LogNonReflectionUpdateFreshnessToUma(DataType type,
                                          base::Time remote_modification_time);

// Logs calls to processor's ClearMetadataWhileStopped(). `is_delayed_call` is
// true if metadata is cleared from ModelReadyToSync() in the case that
// ClearMetadataWhileStopped() was called before ModelReadyToSync().
void LogClearMetadataWhileStoppedHistogram(DataType data_type,
                                           bool is_delayed_call);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_PROCESSOR_METRICS_H_
