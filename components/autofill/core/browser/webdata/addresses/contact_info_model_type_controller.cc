// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_model_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_precondition_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/sync_service.h"

namespace autofill {

ContactInfoModelTypeController::ContactInfoModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : ModelTypeController(syncer::CONTACT_INFO,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      precondition_checker_(
          sync_service,
          identity_manager,
          base::BindRepeating(&syncer::SyncService::DataTypePreconditionChanged,
                              base::Unretained(sync_service),
                              type())) {}

ContactInfoModelTypeController::~ContactInfoModelTypeController() = default;

void ContactInfoModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  sync_mode_ = configure_context.sync_mode;
  ModelTypeController::LoadModels(configure_context, model_load_callback);
}

syncer::ModelTypeController::PreconditionState
ContactInfoModelTypeController::GetPreconditionState() const {
  return precondition_checker_.GetPreconditionState();
}

void ContactInfoModelTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                          StopCallback callback) {
  // In transport-only mode, storage is scoped to the Gaia account. That means
  // it should be cleared if Sync is stopped for any reason (other than browser
  // shutdown).
  // In particular the data should be removed when the user is in pending state.
  // This behavior is specific to autofill, and does not apply to other data
  // types.
  if (sync_mode_ == syncer::SyncMode::kTransportOnly) {
    fate = syncer::SyncStopMetadataFate::CLEAR_METADATA;
  }
  ModelTypeController::Stop(fate, std::move(callback));
}

}  // namespace autofill
