// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_wallet_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"

namespace browser_sync {

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    syncer::ModelType type,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : AsyncDirectoryTypeController(type,
                                   dump_stack,
                                   sync_client,
                                   syncer::GROUP_DB,
                                   std::move(db_thread)),
      callback_registered_(false),
      web_data_service_(web_data_service),
      currently_enabled_(IsEnabled()) {
  DCHECK(type == syncer::AUTOFILL_WALLET_DATA ||
         type == syncer::AUTOFILL_WALLET_METADATA);
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletImportEnabled,
      base::BindRepeating(&AutofillWalletDataTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(
      autofill::prefs::kAutofillCreditCardEnabled,
      base::BindRepeating(&AutofillWalletDataTypeController::OnUserPrefChanged,
                          base::AsWeakPtr(this)));
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {}

bool AutofillWalletDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);

  if (!IsEnabled()) {
    DisableForPolicy();
    return false;
  }

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::Bind(&AutofillWalletDataTypeController::OnModelLoaded,
                   base::AsWeakPtr(this)));
    callback_registered_ = true;
  }

  return false;
}

void AutofillWalletDataTypeController::StopModels() {
  DCHECK(CalledOnValidThread());

  // This controller is used by two data types, we need to clear the data only
  // once. (In particular, if AUTOFILL_WALLET_DATA is on USS (and thus doesn't
  // use this controller), we *don't* want any ClearAllServerData call).
  if (type() == syncer::AUTOFILL_WALLET_DATA) {
    // This function is called when shutting down (nothing is changing), when
    // sync is disabled completely, or when wallet sync is disabled. In the
    // cases where wallet sync or sync in general is disabled, clear wallet
    // cards and addresses copied from the server. This is different than other
    // sync cases since this type of data reflects what's on the server rather
    // than syncing local data between clients, so this extra step is required.
    syncer::SyncService* service = sync_client_->GetSyncService();

    // CanSyncFeatureStart indicates if sync is currently enabled at all. The
    // preferred data type indicates if wallet sync data is enabled, and
    // currently_enabled_ indicates if the other prefs are enabled. All of these
    // have to be enabled to sync wallet data.
    if (!service->CanSyncFeatureStart() ||
        !service->GetPreferredDataTypes().Has(type()) || !currently_enabled_) {
      autofill::PersonalDataManager* pdm =
          sync_client_->GetPersonalDataManager();
      if (pdm)
        pdm->ClearAllServerData();
    }
  }
}

bool AutofillWalletDataTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return currently_enabled_;
}

void AutofillWalletDataTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;  // No change to sync state.
  currently_enabled_ = new_enabled;

  if (currently_enabled_) {
    // The preference was just enabled. Trigger a reconfiguration. This will do
    // nothing if the type isn't preferred.
    syncer::SyncService* sync_service = sync_client_->GetSyncService();
    sync_service->ReenableDatatype(type());
  } else {
    DisableForPolicy();
  }
}

bool AutofillWalletDataTypeController::IsEnabled() {
  DCHECK(CalledOnValidThread());

  // Require the user-visible pref to be enabled to sync Wallet data/metadata.
  return sync_client_->GetPrefService()->GetBoolean(
             autofill::prefs::kAutofillWalletImportEnabled) &&
         sync_client_->GetPrefService()->GetBoolean(
             autofill::prefs::kAutofillCreditCardEnabled);
}
void AutofillWalletDataTypeController::DisableForPolicy() {
  if (state() != NOT_RUNNING && state() != STOPPING) {
    CreateErrorHandler()->OnUnrecoverableError(
        syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                          "Wallet syncing is disabled by policy.", type()));
  }
}

}  // namespace browser_sync
