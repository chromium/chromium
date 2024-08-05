// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SETTINGS_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SETTINGS_DATA_TYPE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "components/sync/service/syncable_service_based_data_type_controller.h"

class PrefService;

// A DataTypeController for supervised user sync datatypes, which enables or
// disables these types based on the profile's IsSupervised state. Runs in
// sync transport mode.
class SupervisedUserSettingsDataTypeController
    : public syncer::SyncableServiceBasedDataTypeController {
 public:
  // |sync_client| must not be null and must outlive this object.
  SupervisedUserSettingsDataTypeController(
      const base::RepeatingClosure& dump_stack,
      syncer::OnceDataTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      PrefService* pref_service);

  SupervisedUserSettingsDataTypeController(
      const SupervisedUserSettingsDataTypeController&) = delete;
  SupervisedUserSettingsDataTypeController& operator=(
      const SupervisedUserSettingsDataTypeController&) = delete;

  ~SupervisedUserSettingsDataTypeController() override;

  // DataTypeController override.
  PreconditionState GetPreconditionState() const override;

 private:
  const raw_ptr<PrefService> pref_service_;
};

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SETTINGS_DATA_TYPE_CONTROLLER_H_
