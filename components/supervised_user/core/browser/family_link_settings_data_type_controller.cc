// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_settings_data_type_controller.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/service/sync_service.h"

FamilyLinkSettingsDataTypeController::FamilyLinkSettingsDataTypeController(
    const base::RepeatingClosure& dump_stack,
    syncer::OnceDataTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : SyncableServiceBasedDataTypeController(
          syncer::SUPERVISED_USER_SETTINGS,
          std::move(store_factory),
          syncable_service,
          dump_stack,
          DelegateMode::kTransportModeWithSingleModel),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(pref_service);
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(
          &FamilyLinkSettingsDataTypeController::OnSupervisedUserIdChanged,
          base::Unretained(this)));
}

FamilyLinkSettingsDataTypeController::~FamilyLinkSettingsDataTypeController() =
    default;

syncer::DataTypeController::PreconditionState
FamilyLinkSettingsDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  DCHECK(CalledOnValidThread());
  bool is_supervised_user =
      supervised_user::IsSubjectToParentalControls(*pref_service_);
  return is_supervised_user ? PreconditionState::kPreconditionsMet
                            : PreconditionState::kMustStopAndClearData;
}

void FamilyLinkSettingsDataTypeController::OnSupervisedUserIdChanged() {
  if (sync_service_) {
    sync_service_->DataTypePreconditionChanged(type());
  }
}
