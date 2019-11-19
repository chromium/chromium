// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_auth_util.h"
#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace sync_sessions {
namespace {

const base::Feature kStopSessionsIfSyncPaused{"StopSessionsIfSyncPaused",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

SessionModelTypeController::SessionModelTypeController(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate,
    const std::string& history_disabled_pref_name)
    : ModelTypeController(syncer::SESSIONS, std::move(delegate)),
      sync_service_(sync_service),
      pref_service_(pref_service),
      history_disabled_pref_name_(history_disabled_pref_name) {
  // TODO(crbug.com/906995): remove this observing mechanism once all sync
  // datatypes are stopped by ProfileSyncService, when sync is paused.
  sync_service_->AddObserver(this);
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::BindRepeating(
          &SessionModelTypeController::OnSavingBrowserHistoryPrefChanged,
          base::AsWeakPtr(this)));
}

SessionModelTypeController::~SessionModelTypeController() {
  sync_service_->RemoveObserver(this);
}

syncer::DataTypeController::PreconditionState
SessionModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  bool preconditions_met =
      !pref_service_->GetBoolean(history_disabled_pref_name_) &&
      !(syncer::IsWebSignout(sync_service_->GetAuthError()) &&
        base::FeatureList::IsEnabled(kStopSessionsIfSyncPaused));
  return preconditions_met ? PreconditionState::kPreconditionsMet
                           : PreconditionState::kMustStopAndKeepData;
}

void SessionModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  // Most of these calls will be no-ops but SyncService handles that just fine.
  sync_service_->DataTypePreconditionChanged(type());
}

void SessionModelTypeController::OnSavingBrowserHistoryPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace sync_sessions
