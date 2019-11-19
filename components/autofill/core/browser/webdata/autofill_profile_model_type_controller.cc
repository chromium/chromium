// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"

namespace browser_sync {

AutofillProfileModelTypeController::AutofillProfileModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : ModelTypeController(syncer::AUTOFILL_PROFILE,
                          std::move(delegate_for_full_sync_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      autofill::prefs::kAutofillProfileEnabled,
      base::BindRepeating(
          &AutofillProfileModelTypeController::OnUserPrefChanged,
          base::Unretained(this)));
}

AutofillProfileModelTypeController::~AutofillProfileModelTypeController() =
    default;

syncer::DataTypeController::PreconditionState
AutofillProfileModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  // Require the user-visible pref to be enabled to sync Autofill Profile data.
  return autofill::prefs::IsProfileAutofillEnabled(pref_service_)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void AutofillProfileModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace browser_sync
