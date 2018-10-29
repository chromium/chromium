// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_wallet_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"

namespace browser_sync {

AutofillWalletModelTypeController::AutofillWalletModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
    syncer::SyncClient* sync_client)
    : ModelTypeController(type, std::move(delegate_on_disk)),
      sync_client_(sync_client) {
  DCHECK(type == syncer::AUTOFILL_WALLET_DATA ||
         type == syncer::AUTOFILL_WALLET_METADATA);
  currently_enabled_ = IsEnabled();
  SubscribeToPrefChanges();
}

AutofillWalletModelTypeController::AutofillWalletModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_in_memory,
    syncer::SyncClient* sync_client)
    : ModelTypeController(type,
                          std::move(delegate_on_disk),
                          std::move(delegate_in_memory)),
      sync_client_(sync_client) {
  DCHECK(type == syncer::AUTOFILL_WALLET_DATA ||
         type == syncer::AUTOFILL_WALLET_METADATA);
  currently_enabled_ = IsEnabled();
  SubscribeToPrefChanges();
}

AutofillWalletModelTypeController::~AutofillWalletModelTypeController() {}

bool AutofillWalletModelTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return currently_enabled_;
}

void AutofillWalletModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());

  bool newly_enabled = IsEnabled();
  if (currently_enabled_ == newly_enabled) {
    return;  // No change to sync state.
  }
  currently_enabled_ = newly_enabled;

  syncer::SyncService* sync_service = sync_client_->GetSyncService();
  sync_service->ReadyForStartChanged(type());
}

bool AutofillWalletModelTypeController::IsEnabled() const {
  DCHECK(CalledOnValidThread());

  // Require two user-visible prefs to be enabled to sync Wallet data/metadata.
  return sync_client_->GetPrefService()->GetBoolean(
             autofill::prefs::kAutofillWalletImportEnabled) &&
         sync_client_->GetPrefService()->GetBoolean(
             autofill::prefs::kAutofillCreditCardEnabled);
}

void AutofillWalletModelTypeController::SubscribeToPrefChanges() {
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletImportEnabled,
      base::BindRepeating(&AutofillWalletModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(
      autofill::prefs::kAutofillCreditCardEnabled,
      base::BindRepeating(&AutofillWalletModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

}  // namespace browser_sync
