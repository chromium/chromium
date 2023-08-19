// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_sync_model_type_controller.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/model/model_type_store_service.h"

SupervisedUserSyncModelTypeController::SupervisedUserSyncModelTypeController(
    syncer::ModelType type,
    const base::RepeatingClosure& dump_stack,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    PrefService* pref_service)
    : SyncableServiceBasedModelTypeController(
          type,
          std::move(store_factory),
          syncable_service,
          dump_stack,
          DelegateMode::kTransportModeWithSingleModel),
      pref_service_(pref_service) {
  DCHECK(type == syncer::SUPERVISED_USER_SETTINGS);
  DCHECK(pref_service);
}

SupervisedUserSyncModelTypeController::
    ~SupervisedUserSyncModelTypeController() = default;

syncer::DataTypeController::PreconditionState
SupervisedUserSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  // TODO(b/292493941): use IsSubjectToParentalControls() once it is decoupled
  // from SupervisedUserService.
  bool is_supervised_user =
      pref_service_->GetString(prefs::kSupervisedUserId) ==
      supervised_user::kChildAccountSUID;
  return is_supervised_user ? PreconditionState::kPreconditionsMet
                            : PreconditionState::kMustStopAndClearData;
}
