// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_MODEL_TYPE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "components/sync/service/syncable_service_based_model_type_controller.h"

// A DataTypeController for supervised user sync datatypes, which enables or
// disables these types based on the profile's IsSupervised state. Runs in
// sync transport mode.
class SupervisedUserSyncModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController {
 public:
  // |sync_client| must not be null and must outlive this object.
  SupervisedUserSyncModelTypeController(
      syncer::ModelType type,
      const base::RepeatingCallback<bool()>& is_supervised_user,
      const base::RepeatingClosure& dump_stack,
      syncer::OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service);

  SupervisedUserSyncModelTypeController(
      const SupervisedUserSyncModelTypeController&) = delete;
  SupervisedUserSyncModelTypeController& operator=(
      const SupervisedUserSyncModelTypeController&) = delete;

  ~SupervisedUserSyncModelTypeController() override;

  // DataTypeController override.
  PreconditionState GetPreconditionState() const override;

 private:
  base::RepeatingCallback<bool()> is_supervised_user_;
};

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_MODEL_TYPE_CONTROLLER_H_
