// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_SETTINGS_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_SETTINGS_DATA_TYPE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/syncable_service_based_data_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

// A DataTypeController for Family Link supervised user sync datatypes, which
// enables or disables these types based on the profile's
// IsSubjectToParentalControls state. Runs in sync transport mode.
class FamilyLinkSettingsDataTypeController
    : public syncer::SyncableServiceBasedDataTypeController {
 public:
  // `sync_service` must not be null and must outlive this object.
  FamilyLinkSettingsDataTypeController(
      const base::RepeatingClosure& dump_stack,
      syncer::OnceDataTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      PrefService* pref_service,
      syncer::SyncService* sync_service);

  FamilyLinkSettingsDataTypeController(
      const FamilyLinkSettingsDataTypeController&) = delete;
  FamilyLinkSettingsDataTypeController& operator=(
      const FamilyLinkSettingsDataTypeController&) = delete;

  ~FamilyLinkSettingsDataTypeController() override;

  // DataTypeController override.
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

 private:
  void OnSupervisedUserIdChanged();

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<syncer::SyncService> sync_service_;
  PrefChangeRegistrar pref_registrar_;
};

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_SETTINGS_DATA_TYPE_CONTROLLER_H_
