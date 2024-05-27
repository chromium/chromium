// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

PasswordModelTypeController::PasswordModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : ModelTypeController(syncer::PASSWORDS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service) {
  identity_manager_observation_.Observe(identity_manager_);
}

PasswordModelTypeController::~PasswordModelTypeController() = default;

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  syncer::ConfigureContext overridden_context = configure_context;
  if (features_util::CanCreateAccountStore(pref_service_) &&
      base::FeatureList::IsEnabled(
          syncer::kEnablePasswordsAccountStorageForSyncingUsers)) {
    overridden_context.sync_mode = syncer::SyncMode::kTransportOnly;
  }
  sync_mode_ = overridden_context.sync_mode;
  ModelTypeController::LoadModels(overridden_context, model_load_callback);
}

void PasswordModelTypeController::Stop(syncer::SyncStopMetadataFate fate,
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
  ModelTypeController::Stop(fate, std::move(callback));
}

void PasswordModelTypeController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
#if BUILDFLAG(IS_ANDROID)
  const bool did_signin =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet;
  if (did_signin && base::FeatureList::IsEnabled(
                        syncer::kReplaceSyncPromosWithSignInPromos)) {
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

void PasswordModelTypeController::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // If the account information is stale, do nothing for now - wait until there
  // is fresh information.
  if (!accounts_in_cookie_jar_info.accounts_are_fresh) {
    return;
  }
  // Collect all the known accounts (signed-in or signed-out).
  std::vector<std::string> gaia_ids;
  for (const gaia::ListedAccount& account :
       accounts_in_cookie_jar_info.signed_in_accounts) {
    gaia_ids.push_back(account.gaia_id);
  }
  for (const gaia::ListedAccount& account :
       accounts_in_cookie_jar_info.signed_out_accounts) {
    gaia_ids.push_back(account.gaia_id);
  }
  // Keep any account-storage settings only for known accounts.
  features_util::KeepAccountStorageSettingsOnlyForUsers(pref_service_,
                                                        gaia_ids);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

void PasswordModelTypeController::OnAccountsCookieDeletedByUserAction() {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  features_util::KeepAccountStorageSettingsOnlyForUsers(pref_service_, {});
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

}  // namespace password_manager
