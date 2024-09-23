// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_data_type_controller.h"

#include "base/functional/bind.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"

namespace password_manager {

namespace {

std::unique_ptr<syncer::DataTypeControllerDelegate>
CreateDelegateFromPasswordSenderService(
    PasswordSenderService* password_sender_service) {
  CHECK(password_sender_service);

  return std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
      password_sender_service->GetControllerDelegate().get());
}

}  // namespace

OutgoingPasswordSharingInvitationDataTypeController::
    OutgoingPasswordSharingInvitationDataTypeController(
        syncer::SyncService* sync_service,
        PasswordSenderService* password_sender_service,
        PrefService* pref_service)
    : DataTypeController(
          syncer::OUTGOING_PASSWORD_SHARING_INVITATION,
          /*delegate_for_full_sync_mode=*/
          CreateDelegateFromPasswordSenderService(password_sender_service),
          /*delegate_for_transport_mode=*/
          CreateDelegateFromPasswordSenderService(password_sender_service)),
      sync_service_(sync_service) {
  password_sharing_enabled_policy_.Init(
      password_manager::prefs::kPasswordSharingEnabled, pref_service,
      base::BindRepeating(
          &OutgoingPasswordSharingInvitationDataTypeController ::
              OnPasswordSharingEnabledPolicyChanged,
          base::Unretained(this)));
}

OutgoingPasswordSharingInvitationDataTypeController::
    ~OutgoingPasswordSharingInvitationDataTypeController() = default;

syncer::DataTypeController::PreconditionState
OutgoingPasswordSharingInvitationDataTypeController::GetPreconditionState()
    const {
  DCHECK(CalledOnValidThread());

  if (!password_sharing_enabled_policy_.GetValue()) {
    return syncer::DataTypeController::PreconditionState::
        kMustStopAndClearData;
  }

  return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void OutgoingPasswordSharingInvitationDataTypeController::
    OnPasswordSharingEnabledPolicyChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace password_manager
