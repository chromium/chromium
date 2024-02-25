// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_model_type_controller.h"

#include "base/functional/bind.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"

namespace password_manager {

namespace {

std::unique_ptr<syncer::ModelTypeControllerDelegate>
CreateDelegateFromPasswordReceiverService(
    PasswordReceiverService* password_receiver_service) {
  CHECK(password_receiver_service);

  return std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
      password_receiver_service->GetControllerDelegate().get());
}

}  // namespace

IncomingPasswordSharingInvitationModelTypeController::
    IncomingPasswordSharingInvitationModelTypeController(
        syncer::SyncService* sync_service,
        PasswordReceiverService* password_receiver_service,
        PrefService* pref_service)
    : ModelTypeController(
          syncer::INCOMING_PASSWORD_SHARING_INVITATION,
          /*delegate_for_full_sync_mode=*/
          CreateDelegateFromPasswordReceiverService(password_receiver_service),
          /*delegate_for_transport_mode=*/
          CreateDelegateFromPasswordReceiverService(password_receiver_service)),
      sync_service_(sync_service) {
  sync_observation_.Observe(sync_service);
  password_sharing_enabled_policy_.Init(
      password_manager::prefs::kPasswordSharingEnabled, pref_service,
      base::BindRepeating(
          &IncomingPasswordSharingInvitationModelTypeController ::
              OnPasswordSharingEnabledPolicyChanged,
          base::Unretained(this)));
}

IncomingPasswordSharingInvitationModelTypeController::
    ~IncomingPasswordSharingInvitationModelTypeController() = default;

syncer::DataTypeController::PreconditionState
IncomingPasswordSharingInvitationModelTypeController::GetPreconditionState()
    const {
  DCHECK(CalledOnValidThread());

  if (!password_sharing_enabled_policy_.GetValue()) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndClearData;
  }

  // Disable current data type if PASSWORDS encountered error. Note that
  // GetActiveDataTypes() can't be used here because it's always empty during
  // configuration (e.g. on browser startup) and disabling this data type during
  // browser startup might cause an extra GetUpdates request.
  if (sync_service_->GetDownloadStatusFor(syncer::PASSWORDS) ==
      syncer::SyncService::ModelTypeDownloadStatus::kError) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndClearData;
  }

  return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void IncomingPasswordSharingInvitationModelTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void IncomingPasswordSharingInvitationModelTypeController::
    OnPasswordSharingEnabledPolicyChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace password_manager
