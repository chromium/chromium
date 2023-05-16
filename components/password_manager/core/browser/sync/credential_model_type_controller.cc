// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/credential_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager {

namespace {

#if BUILDFLAG(IS_IOS)
// Master kill switch that can be used to disable enabling PASSWORDS transport
// mode for users using non-standard encryption passphrase types (explicit
// passphrase or kTrustedVaultPassphrase). Note that this is necessary but not
// sufficient to enable PASSWORDS in transport mode.
BASE_FEATURE(kSyncAllowTransportModeWithNonStandardEncryption,
             "SyncAllowTransportModeWithNonStandardEncryption",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace

CredentialModelTypeController::CredentialModelTypeController(
    syncer::ModelType model_type,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : ModelTypeController(model_type,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      account_storage_settings_watcher_(
          pref_service_,
          sync_service_,
          base::BindRepeating(
              &CredentialModelTypeController::OnOptInStateMaybeChanged,
              base::Unretained(this))) {
  CHECK(model_type == syncer::PASSWORDS ||
        model_type == syncer::WEBAUTHN_CREDENTIAL);
  identity_manager_observation_.Observe(identity_manager_);
}

CredentialModelTypeController::~CredentialModelTypeController() = default;

void CredentialModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  sync_service_observation_.Observe(sync_service_);
  sync_mode_ = configure_context.sync_mode;
  ModelTypeController::LoadModels(configure_context, model_load_callback);
}

void CredentialModelTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                         StopCallback callback) {
  DCHECK(CalledOnValidThread());
  sync_service_observation_.Reset();
  // In transport-only mode, our storage is scoped to the Gaia account. That
  // means it should be cleared if Sync is stopped for any reason (other than
  // just browser shutdown). E.g. when switching to full-Sync mode, we don't
  // want to end up with two copies of the passwords (one in the profile DB, one
  // in the account DB).
  if (sync_mode_ == syncer::SyncMode::kTransportOnly) {
    fate = syncer::SyncStopMetadataFate::CLEAR_METADATA;
  }
  ModelTypeController::Stop(fate, std::move(callback));
}

syncer::DataTypeController::PreconditionState
CredentialModelTypeController::GetPreconditionState() const {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // If Sync-the-feature is enabled, then the user has opted in to that, and no
  // additional opt-in is required here.
  if (sync_service_->IsSyncFeatureEnabled() ||
      sync_service_->IsLocalSyncEnabled()) {
    return PreconditionState::kPreconditionsMet;
  }
  // If Sync-the-feature is *not* enabled, then credential sync should only be
  // turned on if the user has opted in to the account-scoped storage.
  return features_util::IsOptedInForAccountStorage(pref_service_, sync_service_)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
#else
  // On Android and iOS, there is no explicit opt-in - instead the user's choice
  // is handled via Sync's selected types (see `UserSelectableType`). So nothing
  // to check here.
  return PreconditionState::kPreconditionsMet;
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

bool CredentialModelTypeController::ShouldRunInTransportOnlyMode() const {
  if (type() == syncer::PASSWORDS &&
      !base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage)) {
    return false;
  }
#if BUILDFLAG(IS_IOS)
  // Non-standard passphrase types require UI support to deal with error cases.
  // On iOS, these UI changes (for transport mode) are guarded behind
  // kIndicateAccountStorageErrorInAccountCell.
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase() ||
      sync_service_->GetUserSettings()->GetPassphraseType() ==
          syncer::PassphraseType::kTrustedVaultPassphrase) {
    if (!base::FeatureList::IsEnabled(
            syncer::kIndicateAccountStorageErrorInAccountCell) ||
        !base::FeatureList::IsEnabled(
            kSyncAllowTransportModeWithNonStandardEncryption)) {
      return false;
    }
  }
#else
  // Outside iOS, passphrase errors aren't reported in the UI, so it doesn't
  // make sense to enable this datatype.
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return false;
  }
#endif  // BUILDFLAG(IS_IOS)
  return true;
}

void CredentialModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void CredentialModelTypeController::OnAccountsInCookieUpdated(
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

void CredentialModelTypeController::OnAccountsCookieDeletedByUserAction() {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  features_util::ClearAccountStorageSettingsForAllUsers(pref_service_);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

void CredentialModelTypeController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    // Note: kCleared event for ConsentLevel::kSync basically means that the
    // consent for Sync-the-feature was revoked. In this case, also clear any
    // possible matching opt-in for the account-scoped storage, since it'd
    // probably be surprising to the user if their account passwords still
    // remained after disabling Sync.
    features_util::OptOutOfAccountStorageAndClearSettingsForAccount(
        pref_service_, event.GetPreviousState().primary_account.gaia);
  }
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

void CredentialModelTypeController::OnOptInStateMaybeChanged() {
  // Note: This method gets called in many other situations as well, not just
  // when the opt-in state changes, but DataTypePreconditionChanged() is cheap
  // if nothing actually changed, so some spurious calls don't hurt.
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace password_manager
