// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_METRICS_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_METRICS_H_

#include "base/time/time.h"
#include "components/sync/base/model_type.h"

namespace syncer {

// Logs histograms representing the number of updates that an implementations of
// ModelTypeProcessor receive via OnUpdateReceived().
void LogUpdatesReceivedByProcessorHistogram(ModelType model_type,
                                            bool is_initial_sync,
                                            size_t num_updates);

// Logs histogram representing the staleness of an incoming incremental
// (non-initial) update, when received by a ModelTypeProcessor via
// OnUpdateReceived().
void LogNonReflectionUpdateFreshnessToUma(ModelType type,
                                          base::Time remote_modification_time);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_PROCESSOR_METRICS_H_
