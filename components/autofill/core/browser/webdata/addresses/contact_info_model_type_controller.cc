// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_model_type_controller.h"

#include "base/functional/bind.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_precondition_checker.h"

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

syncer::ModelTypeController::PreconditionState
ContactInfoModelTypeController::GetPreconditionState() const {
  return precondition_checker_.GetPreconditionState();
}

}  // namespace autofill
