// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_data_type_controller.h"

#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"

namespace autofill {

AutofillValuableDataTypeController::AutofillValuableDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
        delegate_for_transport_mode)
    : syncer::DataTypeController(type,
                                 std::move(delegate_for_full_sync_mode),
                                 std::move(delegate_for_transport_mode)) {}

void AutofillValuableDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  syncer::ConfigureContext overridden_context = configure_context;
  sync_mode_ = configure_context.sync_mode;
  DataTypeController::LoadModels(overridden_context, model_load_callback);
}

void AutofillValuableDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                              StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // For Valuable-related data types, we want to clear all data even when Sync
  // is stopped temporarily, regardless of incoming fate value.
  DataTypeController::Stop(syncer::SyncStopMetadataFate::CLEAR_METADATA,
                           std::move(callback));
}

}  // namespace autofill
