// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_model_type_controller.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_user_settings.h"

namespace autofill {

ContactInfoModelTypeController::ContactInfoModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service)
    : ModelTypeController(syncer::CONTACT_INFO,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {
  sync_service_observation_.Observe(sync_service_);
}

ContactInfoModelTypeController::~ContactInfoModelTypeController() = default;

bool ContactInfoModelTypeController::ShouldRunInTransportOnlyMode() const {
  return base::FeatureList::IsEnabled(
      syncer::kSyncEnableContactInfoDataTypeInTransportMode);
}

syncer::DataTypeController::PreconditionState
ContactInfoModelTypeController::GetPreconditionState() const {
  return !sync_service_->GetUserSettings()->IsUsingExplicitPassphrase() ||
                 base::FeatureList::IsEnabled(
                     syncer::
                         kSyncEnableContactInfoDataTypeForCustomPassphraseUsers)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void ContactInfoModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace autofill
