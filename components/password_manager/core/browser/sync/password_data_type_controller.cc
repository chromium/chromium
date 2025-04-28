// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_data_type_controller.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/service/configure_context.h"

namespace password_manager {

PasswordDataTypeController::PasswordDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    std::unique_ptr<syncer::DataTypeLocalDataBatchUploader> batch_uploader,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : DataTypeController(syncer::PASSWORDS,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode),
                         std::move(batch_uploader)),
      pref_service_(pref_service),
      identity_manager_(identity_manager) {
  identity_manager_observation_.Observe(identity_manager_);
}

PasswordDataTypeController::~PasswordDataTypeController() = default;

void PasswordDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  syncer::ConfigureContext overridden_context = configure_context;
#if BUILDFLAG(IS_ANDROID)
  if (features_util::CanCreateAccountStore(pref_service_)) {
    // Make syncing users behave like signed-in users, storage-wise.
    // TODO(crbug.com/40067058): Drop when kForceMigrateSyncingUserToSignedIn
    // is launched on Android, since there won't be syncing users anymore.
    overridden_context.sync_mode = syncer::SyncMode::kTransportOnly;
  }
#endif
  sync_mode_ = overridden_context.sync_mode;
  DataTypeController::LoadModels(overridden_context, model_load_callback);
}

void PasswordDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // In transport-only mode, storage is scoped to the Gaia account. That means
  // it should be cleared if Sync is stopped for any reason (other than browser
  // shutdown).
  // In particular the data should be removed when the user is in pending state.
  // This behavior is specific to autofill, and does not apply to other data
  // types.
  if (sync_mode_ == syncer::SyncMode::kTransportOnly) {
    fate = syncer::SyncStopMetadataFate::CLEAR_METADATA;
  }
  DataTypeController::Stop(fate, std::move(callback));
}

void PasswordDataTypeController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
#if BUILDFLAG(IS_ANDROID)
  const bool did_signin =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet;
  if (did_signin) {
    // If the flag is enabled, the transparency notice should not be shown to
    // newly signed-in users (the assumption being that the sign-in UIs are
    // transparent enough). Set the pref to true to prevent it from showing.
    // Arguably this isn't the ideal object to house this call, since it's not
    // related to the notice. But it has the right cardinality, access to all
    // required dependencies (IdentityManager and PrefService), and is an object
    // related to both passwords and identity. So good enough, especially given
    // the code is temporary.
    pref_service_->SetBoolean(prefs::kAccountStorageNoticeShown, true);
  }
#endif
}

void PasswordDataTypeController::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_observation_.Reset();
}

}  // namespace password_manager
