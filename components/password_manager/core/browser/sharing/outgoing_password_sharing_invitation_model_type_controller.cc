// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_model_type_controller.h"

#include "base/functional/bind.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"

namespace password_manager {

namespace {

std::unique_ptr<syncer::ModelTypeControllerDelegate>
CreateDelegateFromPasswordSenderService(
    PasswordSenderService* password_sender_service) {
  CHECK(password_sender_service);

  return std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
      password_sender_service->GetControllerDelegate().get());
}

}  // namespace

OutgoingPasswordSharingInvitationModelTypeController::
    OutgoingPasswordSharingInvitationModelTypeController(
        syncer::SyncService* sync_service,
        PasswordSenderService* password_sender_service,
        PrefService* pref_service)
    : ModelTypeController(
          syncer::OUTGOING_PASSWORD_SHARING_INVITATION,
          /*delegate_for_full_sync_mode=*/
          CreateDelegateFromPasswordSenderService(password_sender_service),
          /*delegate_for_transport_mode=*/
          CreateDelegateFromPasswordSenderService(password_sender_service)),
      sync_service_(sync_service) {
  password_sharing_enabled_policy_.Init(
      password_manager::prefs::kPasswordSharingEnabled, pref_service,
      base::BindRepeating(
          &OutgoingPasswordSharingInvitationModelTypeController ::
              OnPasswordSharingEnabledPolicyChanged,
          base::Unretained(this)));
}

OutgoingPasswordSharingInvitationModelTypeController::
    ~OutgoingPasswordSharingInvitationModelTypeController() = default;

syncer::DataTypeController::PreconditionState
OutgoingPasswordSharingInvitationModelTypeController::GetPreconditionState()
    const {
  DCHECK(CalledOnValidThread());

  if (!password_sharing_enabled_policy_.GetValue()) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndClearData;
  }

  return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void OutgoingPasswordSharingInvitationModelTypeController::
    OnPasswordSharingEnabledPolicyChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace password_manager
