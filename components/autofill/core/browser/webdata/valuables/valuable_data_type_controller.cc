// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/valuables/valuable_data_type_controller.h"

#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"

namespace autofill {

AutofillValuableDataTypeController::AutofillValuableDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ProxyDataTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : syncer::DataTypeController(type,
                                 std::move(delegate_for_full_sync_mode),
                                 std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  CHECK(pref_service);
  CHECK(sync_service);
  SubscribeToPrefChanges();
}

void AutofillValuableDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  syncer::ConfigureContext overridden_context = configure_context;
  sync_mode_ = configure_context.sync_mode;
  DataTypeController::LoadModels(overridden_context, model_load_callback);
}

void AutofillValuableDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                              StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // For Valuable-related data types, we want to clear all data even when Sync
  // is stopped temporarily, regardless of incoming fate value.
  DataTypeController::Stop(syncer::SyncStopMetadataFate::CLEAR_METADATA,
                           std::move(callback));
}

syncer::DataTypeController::PreconditionState
AutofillValuableDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  bool preconditions_met =
      pref_service_->GetBoolean(autofill::prefs::kAutofillCreditCardEnabled);
  return preconditions_met ? PreconditionState::kPreconditionsMet
                           : PreconditionState::kMustStopAndClearData;
}

void AutofillValuableDataTypeController::SubscribeToPrefChanges() {
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      autofill::prefs::kAutofillCreditCardEnabled,
      base::BindRepeating(
          &AutofillValuableDataTypeController::OnUserPrefChanged,
          base::Unretained(this)));
}

void AutofillValuableDataTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace autofill
