// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_model_type_controller.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
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
      sync_service_(sync_service),
      pref_service_(pref_service),
      account_storage_settings_watcher_(
          pref_service,
          sync_service,
          // base::Unretained() is safe because `this` outlives the watcher.
          base::BindRepeating(
              &OutgoingPasswordSharingInvitationModelTypeController::
                  OnAccountStorageSettingsChanged,
              base::Unretained(this))) {
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

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  if (!sync_service_->IsSyncFeatureEnabled() &&
      !sync_service_->IsLocalSyncEnabled() &&
      !features_util::IsOptedInForAccountStorage(pref_service_,
                                                 sync_service_)) {
    // The user is in transport mode and password sync is disabled because they
    // are not opted in to account storage. Sharing should be disabled too.
    // TODO(crbug.com/1484531): IsOptedInForAccountStorage() with "selected
    // types" and delete this check.
    return PreconditionState::kMustStopAndClearData;
  }
#endif

  return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void OutgoingPasswordSharingInvitationModelTypeController::
    OnPasswordSharingEnabledPolicyChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void OutgoingPasswordSharingInvitationModelTypeController::
    OnAccountStorageSettingsChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace password_manager
