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
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

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
#if BUILDFLAG(IS_ANDROID)
  // Unretained() is safe because this object outlives `local_upm_pref_`.
  local_upm_pref_.Init(
      prefs::kPasswordsUseUPMLocalAndSeparateStores, pref_service_,
      base::BindRepeating(&PasswordModelTypeController::OnLocalUpmPrefChanged,
                          base::Unretained(this)));
#endif
}

PasswordModelTypeController::~PasswordModelTypeController() = default;

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  syncer::ConfigureContext overridden_context = configure_context;
#if BUILDFLAG(IS_ANDROID)
  switch (GetLocalUpmPrefValue()) {
    case prefs::UseUpmLocalAndSeparateStoresState::kOff:
      break;
    case prefs::UseUpmLocalAndSeparateStoresState::kOn:
      overridden_context.sync_mode = syncer::SyncMode::kTransportOnly;
      break;
    case prefs::UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      // Disallowed by GetPreconditionState().
      NOTREACHED_NORETURN();
  }
#endif
  ModelTypeController::LoadModels(overridden_context, model_load_callback);
}

void PasswordModelTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  ModelTypeController::Stop(fate, std::move(callback));
}

syncer::DataTypeController::PreconditionState
PasswordModelTypeController::GetPreconditionState() const {
#if BUILDFLAG(IS_ANDROID)
  // If the local UPM migration is pending, wait until it succeeds/fails, so
  // LoadModels() knows whether to override SyncMode to kTransportOnly or not.
  return GetLocalUpmPrefValue() == prefs::UseUpmLocalAndSeparateStoresState::
                                       kOffAndMigrationPending
             ? PreconditionState::kMustStopAndKeepData
             : PreconditionState::kPreconditionsMet;
#else
  return PreconditionState::kPreconditionsMet;
#endif
}

bool PasswordModelTypeController::ShouldRunInTransportOnlyMode() const {
#if !BUILDFLAG(IS_IOS)
  // Outside iOS, passphrase errors aren't reported in the UI, so it doesn't
  // make sense to enable this datatype.
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return false;
  }
#endif  // !BUILDFLAG(IS_IOS)
  return true;
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

#if BUILDFLAG(IS_ANDROID)
prefs::UseUpmLocalAndSeparateStoresState
PasswordModelTypeController::GetLocalUpmPrefValue() const {
  auto value = static_cast<prefs::UseUpmLocalAndSeparateStoresState>(
      local_upm_pref_.GetValue());
  switch (value) {
    case prefs::UseUpmLocalAndSeparateStoresState::kOff:
    case prefs::UseUpmLocalAndSeparateStoresState::kOn:
    case prefs::UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      return value;
  }
  NOTREACHED_NORETURN();
}

void PasswordModelTypeController::OnLocalUpmPrefChanged() {
  // No-ops are fine.
  sync_service_->DataTypePreconditionChanged(type());
}
#endif

}  // namespace password_manager
