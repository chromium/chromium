// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_wallet_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace browser_sync {

AutofillWalletModelTypeController::AutofillWalletModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : ModelTypeController(type, std::move(delegate_for_full_sync_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(type == syncer::AUTOFILL_WALLET_DATA ||
         type == syncer::AUTOFILL_WALLET_METADATA ||
         type == syncer::AUTOFILL_WALLET_OFFER ||
         type == syncer::AUTOFILL_WALLET_USAGE);
  SubscribeToPrefChanges();
  sync_service_->AddObserver(this);
}

AutofillWalletModelTypeController::AutofillWalletModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : ModelTypeController(type,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(type == syncer::AUTOFILL_WALLET_DATA ||
         type == syncer::AUTOFILL_WALLET_METADATA ||
         type == syncer::AUTOFILL_WALLET_OFFER ||
         type == syncer::AUTOFILL_WALLET_USAGE);
  SubscribeToPrefChanges();
  sync_service_->AddObserver(this);
}

AutofillWalletModelTypeController::~AutofillWalletModelTypeController() {
  sync_service_->RemoveObserver(this);
}

void AutofillWalletModelTypeController::Stop(
    syncer::ShutdownReason shutdown_reason,
    StopCallback callback) {
  DCHECK(CalledOnValidThread());
  switch (shutdown_reason) {
    case syncer::ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
      // Special case: For Wallet-related data types, we want to clear all data
      // even when Sync is stopped temporarily.
      shutdown_reason = syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA;
      break;
    case syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
    case syncer::ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      break;
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
}

syncer::DataTypeController::PreconditionState
AutofillWalletModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  bool preconditions_met =
      pref_service_->GetBoolean(
          autofill::prefs::kAutofillWalletImportEnabled) &&
      pref_service_->GetBoolean(autofill::prefs::kAutofillCreditCardEnabled);
  return preconditions_met ? PreconditionState::kPreconditionsMet
                           : PreconditionState::kMustStopAndClearData;
}

bool AutofillWalletModelTypeController::ShouldRunInTransportOnlyMode() const {
  if (type() != syncer::AUTOFILL_WALLET_DATA) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)) {
    return false;
  }
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase() &&
      !base::FeatureList::IsEnabled(
          syncer::kSyncAllowWalletDataInTransportModeWithCustomPassphrase)) {
    return false;
  }
  return true;
}

void AutofillWalletModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void AutofillWalletModelTypeController::SubscribeToPrefChanges() {
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletImportEnabled,
      base::BindRepeating(&AutofillWalletModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(
      autofill::prefs::kAutofillCreditCardEnabled,
      base::BindRepeating(&AutofillWalletModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

void AutofillWalletModelTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace browser_sync
