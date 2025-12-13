// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_data_type_controller.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/service/configure_context.h"

namespace password_manager {

PasswordDataTypeController::PasswordDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    std::unique_ptr<syncer::DataTypeLocalDataBatchUploader> batch_uploader)
    : DataTypeController(syncer::PASSWORDS,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode),
                         std::move(batch_uploader)) {}

PasswordDataTypeController::~PasswordDataTypeController() = default;

void PasswordDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  syncer::ConfigureContext overridden_context = configure_context;
#if BUILDFLAG(IS_ANDROID)
  // Make syncing users behave like signed-in users, storage-wise.
  // TODO(crbug.com/40067058): Drop when kForceMigrateSyncingUserToSignedIn
  // is launched on Android, since there won't be syncing users anymore.
  overridden_context.sync_mode = syncer::SyncMode::kTransportOnly;
#endif
  sync_mode_ = overridden_context.sync_mode;
  DataTypeController::LoadModels(overridden_context, model_load_callback);
}

void PasswordDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // In transport-only mode, storage is scoped to the Gaia account. That means
  // it should be cleared if Sync is stopped for any reason (other than browser
  // shutdown).
  // In particular the data should be removed when the user is in pending state.
  // This behavior is specific to autofill, and does not apply to other data
  // types.
  if (sync_mode_ == syncer::SyncMode::kTransportOnly) {
    fate = syncer::SyncStopMetadataFate::CLEAR_METADATA;
  }
  DataTypeController::Stop(fate, std::move(callback));
}

}  // namespace password_manager
