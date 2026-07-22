// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_data_type_controller.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

using PreconditionState = syncer::DataTypeController::PreconditionState;

// Determines if the signed-in account is eligible to use the CONTACT_INFO type
// based on its managed state. Dasher users are not supported.
PreconditionState GetPreconditionStateFromAccountManagedStatus(
    signin::AccountManagedStatusFinderOutcome account_managed_status) {
  // If the feature is enabled, all account types are supported.
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForDasherUsers)) {
    return PreconditionState::kPreconditionsMet;
  }
  switch (account_managed_status) {
    case signin::AccountManagedStatusFinderOutcome::kConsumerGmail:
    case signin::AccountManagedStatusFinderOutcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kConsumerNotWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom:
      return PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinderOutcome::kEnterprise:
      return PreconditionState::kMustStopAndClearData;
    case signin::AccountManagedStatusFinderOutcome::kPending:
    case signin::AccountManagedStatusFinderOutcome::kError:
    case signin::AccountManagedStatusFinderOutcome::kTimeout:
      // If the account status cannot be determined (immediately), keep the data
      // to prevent redownloding once the status was determined.
      return PreconditionState::kMustStopAndKeepData;
  }
}

}  // namespace

ContactInfoDataTypeController::ContactInfoDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    std::unique_ptr<syncer::DataTypeLocalDataBatchUploader> batch_uploader)
    : DataTypeController(syncer::CONTACT_INFO,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode),
                         std::move(batch_uploader)) {
  CHECK(sync_service);
  sync_service_observation_.Observe(sync_service);
}

ContactInfoDataTypeController::~ContactInfoDataTypeController() = default;

void ContactInfoDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  sync_mode_ = configure_context.sync_mode;
  DataTypeController::LoadModels(configure_context, model_load_callback);
}

syncer::DataTypeController::PreconditionState
ContactInfoDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  // Can happen if this gets called after `OnSyncShutdown()` - in that case,
  // "stop and keep data" is a safe default.
  if (!sync_service_observation_.IsObserving()) {
    return PreconditionState::kMustStopAndKeepData;
  }
  const syncer::SyncService* sync_service =
      sync_service_observation_.GetSource();
  // Exclude explicit passphrase users.
  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase() &&
      !syncer::IsContactInfoDataTypeForCustomPassphraseUsersEnabled()) {
    return PreconditionState::kMustStopAndClearData;
  }
  // Exclude Dasher accounts.
  return GetPreconditionStateFromAccountManagedStatus(
      context.account_managed_status);
}

void ContactInfoDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
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
  DataTypeController::Stop(fate, std::move(callback));
}

void ContactInfoDataTypeController::OnStateChanged(syncer::SyncService* sync) {
  CHECK_EQ(sync, sync_service_observation_.GetSource());
  // The explicit passphrase state might have changed.
  sync->DataTypePreconditionChanged(type());
}

void ContactInfoDataTypeController::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observation_.Reset();
}

}  // namespace autofill
