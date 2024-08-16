// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_wallet_data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

namespace browser_sync {

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : DataTypeController(type,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
         type == syncer::AUTOFILL_WALLET_DATA ||
         type == syncer::AUTOFILL_WALLET_METADATA ||
         type == syncer::AUTOFILL_WALLET_OFFER ||
         type == syncer::AUTOFILL_WALLET_USAGE);
  SubscribeToPrefChanges();
  sync_service_->AddObserver(this);
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {
  sync_service_->RemoveObserver(this);
}

void AutofillWalletDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                             StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // Special case: For Wallet-related data types, we want to clear all data
  // even when Sync is stopped temporarily, regardless of incoming fate value.
  DataTypeController::Stop(syncer::SyncStopMetadataFate::CLEAR_METADATA,
                            std::move(callback));
}

syncer::DataTypeController::PreconditionState
AutofillWalletDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  bool preconditions_met =
      pref_service_->GetBoolean(autofill::prefs::kAutofillCreditCardEnabled);
  return preconditions_met ? PreconditionState::kPreconditionsMet
                           : PreconditionState::kMustStopAndClearData;
}

void AutofillWalletDataTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void AutofillWalletDataTypeController::SubscribeToPrefChanges() {
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      autofill::prefs::kAutofillCreditCardEnabled,
      base::BindRepeating(&AutofillWalletDataTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

void AutofillWalletDataTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace browser_sync
