// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ClearedOnStartup {
  kOptedInSoNoNeedToClear = 0,
  kNotOptedInAndWasAlreadyEmpty = 1,
  kNotOptedInAndHadToClear = 2,
  kMaxValue = kNotOptedInAndHadToClear
};

void RecordClearedOnStartup(ClearedOnStartup state) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AccountStorage.ClearedOnStartup2", state);
}

void PasswordStoreClearDone(bool cleared) {
  RecordClearedOnStartup(cleared
                             ? ClearedOnStartup::kNotOptedInAndHadToClear
                             : ClearedOnStartup::kNotOptedInAndWasAlreadyEmpty);
}

}  // namespace

PasswordModelTypeController::PasswordModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    scoped_refptr<PasswordStoreInterface> account_password_store_for_cleanup,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : ModelTypeController(syncer::PASSWORDS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      account_storage_settings_watcher_(
          pref_service_,
          sync_service_,
          base::BindRepeating(
              &PasswordModelTypeController::OnOptInStateMaybeChanged,
              base::Unretained(this))) {
  identity_manager_observation_.Observe(identity_manager_);

  if (account_password_store_for_cleanup) {
    DCHECK(
        base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));
    // Note: Right now, we're still in the middle of SyncService initialization,
    // so we can't check IsOptedInForAccountStorage() yet (SyncService might not
    // have determined the syncing account yet). Post a task do to it after the
    // initialization is complete.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PasswordModelTypeController::MaybeClearStore,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  account_password_store_for_cleanup));
  }
}

PasswordModelTypeController::~PasswordModelTypeController() = default;

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  sync_service_observation_.Observe(sync_service_);
  sync_mode_ = configure_context.sync_mode;
  ModelTypeController::LoadModels(configure_context, model_load_callback);
}

void PasswordModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  sync_service_observation_.Reset();
  // In transport-only mode, our storage is scoped to the Gaia account. That
  // means it should be cleared if Sync is stopped for any reason (other than
  // just browser shutdown). E.g. when switching to full-Sync mode, we don't
  // want to end up with two copies of the passwords (one in the profile DB, one
  // in the account DB).
  if (sync_mode_ == syncer::SyncMode::kTransportOnly) {
    switch (shutdown_reason) {
      case syncer::ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
        shutdown_reason = syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA;
        break;
      case syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
      case syncer::ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
        break;
    }
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
}

syncer::DataTypeController::PreconditionState
PasswordModelTypeController::GetPreconditionState() const {
  // If Sync-the-feature is enabled, then the user has opted in to that, and no
  // additional opt-in is required here.
  if (sync_service_->IsSyncFeatureEnabled() ||
      sync_service_->IsLocalSyncEnabled()) {
    return PreconditionState::kPreconditionsMet;
  }
  // If Sync-the-feature is *not* enabled, then password sync should only be
  // turned on if the user has opted in to the account-scoped storage.
  return features_util::IsOptedInForAccountStorage(pref_service_, sync_service_)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

bool PasswordModelTypeController::ShouldRunInTransportOnlyMode() const {
  if (!base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage)) {
    return false;
  }
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return false;
  }
  return true;
}

void PasswordModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(syncer::PASSWORDS);
}

void PasswordModelTypeController::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
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
}

void PasswordModelTypeController::OnAccountsCookieDeletedByUserAction() {
  features_util::ClearAccountStorageSettingsForAllUsers(pref_service_);
}

void PasswordModelTypeController::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
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
}

void PasswordModelTypeController::OnOptInStateMaybeChanged() {
  // Note: This method gets called in many other situations as well, not just
  // when the opt-in state changes, but DataTypePreconditionChanged() is cheap
  // if nothing actually changed, so some spurious calls don't hurt.
  sync_service_->DataTypePreconditionChanged(syncer::PASSWORDS);
}

void PasswordModelTypeController::MaybeClearStore(
    scoped_refptr<PasswordStoreInterface> account_password_store_for_cleanup) {
  DCHECK(account_password_store_for_cleanup);
  if (features_util::IsOptedInForAccountStorage(pref_service_, sync_service_)) {
    RecordClearedOnStartup(ClearedOnStartup::kOptedInSoNoNeedToClear);
  } else {
    account_password_store_for_cleanup->RemoveLoginsCreatedBetween(
        base::Time(), base::Time::Max(),
        base::BindOnce(&PasswordStoreClearDone));
  }
}

}  // namespace password_manager
