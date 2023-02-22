// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_sync_model_type_controller.h"

#include "base/functional/bind.h"
#include "components/sync/model/model_type_store_service.h"

SupervisedUserSyncModelTypeController::SupervisedUserSyncModelTypeController(
    syncer::ModelType type,
    const base::RepeatingCallback<bool()>& is_supervised_user,
    const base::RepeatingClosure& dump_stack,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service)
    : SyncableServiceBasedModelTypeController(
          type,
          std::move(store_factory),
          syncable_service,
          dump_stack,
          DelegateMode::kTransportModeWithSingleModel),
      is_supervised_user_(is_supervised_user) {
  DCHECK(type == syncer::SUPERVISED_USER_SETTINGS);
}

SupervisedUserSyncModelTypeController::
    ~SupervisedUserSyncModelTypeController() = default;

syncer::DataTypeController::PreconditionState
SupervisedUserSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return is_supervised_user_.Run() ? PreconditionState::kPreconditionsMet
                                   : PreconditionState::kMustStopAndClearData;
}
