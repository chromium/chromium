// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_wallet_data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/sync_service.h"

namespace browser_sync {
namespace {

bool IsSignedInExplicitly(PrefService* pref_service) {
  CHECK(pref_service);

  return switches::IsExplicitBrowserSigninUIOnDesktopEnabled() &&
         pref_service->GetBoolean(prefs::kExplicitBrowserSignin);
}

}  // namespace

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    base::RepeatingCallback<void(bool)> on_load_models_with_transport_only_cb)
    : DataTypeController(type,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service),
      on_load_models_with_transport_only_cb_(
          std::move(on_load_models_with_transport_only_cb)) {
  CHECK(type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
        type == syncer::AUTOFILL_WALLET_DATA ||
        type == syncer::AUTOFILL_WALLET_METADATA ||
        type == syncer::AUTOFILL_WALLET_OFFER ||
        type == syncer::AUTOFILL_WALLET_USAGE);
  CHECK(pref_service);
  CHECK(sync_service);
  SubscribeToPrefChanges();
  sync_service_->AddObserver(this);
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {
  sync_service_->RemoveObserver(this);
}

void AutofillWalletDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  if (configure_context.sync_mode == syncer::SyncMode::kTransportOnly) {
    on_load_models_with_transport_only_cb_.Run(
        IsSignedInExplicitly(pref_service_));
  }
  DataTypeController::LoadModels(configure_context, model_load_callback);
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
