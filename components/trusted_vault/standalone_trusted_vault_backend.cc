// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_backend.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/physical_device_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection_impl.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

#if BUILDFLAG(IS_MAC)
#include "components/trusted_vault/icloud_keychain_recovery_factor.h"
#endif

namespace trusted_vault {

namespace {

// Note that it returns false upon transition from kUnknown to
// kNoPersistentAuthErrors.
bool PersistentAuthErrorWasResolved(
    StandaloneTrustedVaultBackend::RefreshTokenErrorState
        previous_refresh_token_error_state,
    StandaloneTrustedVaultBackend::RefreshTokenErrorState
        current_refresh_token_error_state) {
  return previous_refresh_token_error_state ==
             StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                 kPersistentAuthError &&
         current_refresh_token_error_state ==
             StandaloneTrustedVaultBackend::RefreshTokenErrorState::
                 kNoPersistentAuthErrors;
}

TrustedVaultRecoverKeysOutcomeForUMA
GetRecoverKeysOutcomeForUMAFromRecoveryStatus(
    LocalRecoveryFactor::RecoveryStatus recovery_status) {
  switch (recovery_status) {
    case LocalRecoveryFactor::RecoveryStatus::kSuccess:
      return TrustedVaultRecoverKeysOutcomeForUMA::kSuccess;
    case LocalRecoveryFactor::RecoveryStatus::kNoNewKeys:
      return TrustedVaultRecoverKeysOutcomeForUMA::kNoNewKeys;
    case LocalRecoveryFactor::RecoveryStatus::kFailure:
      return TrustedVaultRecoverKeysOutcomeForUMA::kFailure;
  }

  NOTREACHED();
}

TrustedVaultRecoveryFactorRegistrationOutcomeForUMA
GetRecoveryFactorRegistrationOutcomeForUMAFromResponse(
    TrustedVaultRegistrationStatus response_status) {
  switch (response_status) {
    case TrustedVaultRegistrationStatus::kRegistrationNotAttempted:
    case TrustedVaultRegistrationStatus::kRegistrationCancelled:
      NOTREACHED();
    case TrustedVaultRegistrationStatus::kSuccess:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::kSuccess;
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::
          kAlreadyRegistered;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::
          kLocalDataObsolete;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::
          kTransientAccessTokenFetchError;
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::
          kPersistentAccessTokenFetchError;
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::
          kPrimaryAccountChangeAccessTokenFetchError;
    case TrustedVaultRegistrationStatus::kNetworkError:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::kNetworkError;
    case TrustedVaultRegistrationStatus::kOtherError:
      return TrustedVaultRecoveryFactorRegistrationOutcomeForUMA::kOtherError;
  }
  NOTREACHED();
}

class LocalRecoveryFactorsFactoryImpl
    : public StandaloneTrustedVaultBackend::LocalRecoveryFactorsFactory {
 public:
#if BUILDFLAG(IS_MAC)
  explicit LocalRecoveryFactorsFactoryImpl(
      const std::string& icloud_keychain_access_group_prefix)
      : icloud_keychain_access_group_prefix_(
            icloud_keychain_access_group_prefix) {}
#else
  LocalRecoveryFactorsFactoryImpl() = default;
#endif
  LocalRecoveryFactorsFactoryImpl(const LocalRecoveryFactorsFactoryImpl&) =
      delete;
  ~LocalRecoveryFactorsFactoryImpl() override = default;

  LocalRecoveryFactorsFactoryImpl& operator=(
      const LocalRecoveryFactorsFactoryImpl&) = delete;

  std::vector<std::unique_ptr<LocalRecoveryFactor>> CreateLocalRecoveryFactors(
      SecurityDomainId security_domain_id,
      StandaloneTrustedVaultStorage* storage,
      TrustedVaultThrottlingConnection* connection,
      const CoreAccountInfo& primary_account) override {
    std::vector<std::unique_ptr<LocalRecoveryFactor>> local_recovery_factors;
    local_recovery_factors.emplace_back(
        std::make_unique<PhysicalDeviceRecoveryFactor>(
            security_domain_id, /*storage=*/storage,
            /*registration_storage=*/storage, /*key_storage=*/storage,
            connection, primary_account));
#if BUILDFLAG(IS_MAC)
    // Note: The iCloud Keychain recovery factor needs to come after the
    // physical device recovery factor.
    // Retrieval attempts are performed in order, and since retrieving using the
    // iCloud Keychain is significantly more heavy weight than from the physical
    // device recovery factor, we want to make sure that the latter is attempted
    // first.
    local_recovery_factors.emplace_back(
        std::make_unique<ICloudKeychainRecoveryFactor>(
            icloud_keychain_access_group_prefix_, security_domain_id,
            /*registration_storage=*/storage, /*key_storage=*/storage,
            connection, primary_account));
#endif

    return local_recovery_factors;
  }

 private:
#if BUILDFLAG(IS_MAC)
  const std::string icloud_keychain_access_group_prefix_;
#endif
};

}  // namespace

StandaloneTrustedVaultBackend::PendingTrustedRecoveryMethod::
    PendingTrustedRecoveryMethod() = default;

StandaloneTrustedVaultBackend::PendingTrustedRecoveryMethod::
    PendingTrustedRecoveryMethod(PendingTrustedRecoveryMethod&&) = default;

StandaloneTrustedVaultBackend::PendingTrustedRecoveryMethod&
StandaloneTrustedVaultBackend::PendingTrustedRecoveryMethod::operator=(
    PendingTrustedRecoveryMethod&&) = default;

StandaloneTrustedVaultBackend::PendingTrustedRecoveryMethod::
    ~PendingTrustedRecoveryMethod() = default;

StandaloneTrustedVaultBackend::PendingGetIsRecoverabilityDegraded::
    PendingGetIsRecoverabilityDegraded() = default;

StandaloneTrustedVaultBackend::PendingGetIsRecoverabilityDegraded::
    PendingGetIsRecoverabilityDegraded(PendingGetIsRecoverabilityDegraded&&) =
        default;

StandaloneTrustedVaultBackend::PendingGetIsRecoverabilityDegraded&
StandaloneTrustedVaultBackend::PendingGetIsRecoverabilityDegraded::operator=(
    PendingGetIsRecoverabilityDegraded&&) = default;

StandaloneTrustedVaultBackend::PendingGetIsRecoverabilityDegraded::
    ~PendingGetIsRecoverabilityDegraded() = default;

StandaloneTrustedVaultBackend::OngoingFetchKeys::OngoingFetchKeys() = default;

StandaloneTrustedVaultBackend::OngoingFetchKeys::OngoingFetchKeys(
    OngoingFetchKeys&&) = default;

StandaloneTrustedVaultBackend::OngoingFetchKeys&
StandaloneTrustedVaultBackend::OngoingFetchKeys::operator=(OngoingFetchKeys&&) =
    default;

StandaloneTrustedVaultBackend::OngoingFetchKeys::~OngoingFetchKeys() = default;

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
#if BUILDFLAG(IS_MAC)
    const std::string& icloud_keychain_access_group_prefix,
#endif
    SecurityDomainId security_domain_id,
    std::unique_ptr<StandaloneTrustedVaultStorage> storage,
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<TrustedVaultConnection> connection)
    : security_domain_id_(security_domain_id),
      storage_(std::move(storage)),
      delegate_(std::move(delegate)),
      connection_(connection
                      ? std::make_unique<TrustedVaultThrottlingConnectionImpl>(
                            std::move(connection),
                            storage_.get())
                      : nullptr),
#if BUILDFLAG(IS_MAC)
      local_recovery_factors_factory_(
          std::make_unique<LocalRecoveryFactorsFactoryImpl>(
              icloud_keychain_access_group_prefix))
#else
      local_recovery_factors_factory_(
          std::make_unique<LocalRecoveryFactorsFactoryImpl>())
#endif
{
}

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
    SecurityDomainId security_domain_id,
    std::unique_ptr<StandaloneTrustedVaultStorage> storage,
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<TrustedVaultThrottlingConnection> connection,
    std::unique_ptr<LocalRecoveryFactorsFactory> local_recovery_factors_factory)
    : security_domain_id_(security_domain_id),
      storage_(std::move(storage)),
      delegate_(std::move(delegate)),
      connection_(std::move(connection)),
      local_recovery_factors_factory_(
          std::move(local_recovery_factors_factory)) {}

StandaloneTrustedVaultBackend::~StandaloneTrustedVaultBackend() = default;

// static
scoped_refptr<StandaloneTrustedVaultBackend>
StandaloneTrustedVaultBackend::CreateForTesting(
    SecurityDomainId security_domain_id,
    std::unique_ptr<StandaloneTrustedVaultStorage> storage,
    std::unique_ptr<StandaloneTrustedVaultBackend::Delegate> delegate,
    std::unique_ptr<TrustedVaultThrottlingConnection> connection,
    std::unique_ptr<LocalRecoveryFactorsFactory>
        local_recovery_factors_factory) {
  return base::WrapRefCounted(new StandaloneTrustedVaultBackend(
      security_domain_id, std::move(storage), std::move(delegate),
      std::move(connection), std::move(local_recovery_factors_factory)));
}

void StandaloneTrustedVaultBackend::WriteDegradedRecoverabilityState(
    const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
        degraded_recoverability_state) {
  DCHECK(primary_account_.has_value());
  storage_->SetDegradedRecoverabilityState(primary_account_->gaia,
                                           security_domain_id_,
                                           degraded_recoverability_state);
}

void StandaloneTrustedVaultBackend::OnDegradedRecoverabilityChanged() {
  delegate_->NotifyRecoverabilityDegradedChanged();
}

void StandaloneTrustedVaultBackend::ReadDataFromDisk() {
  storage_->ReadDataFromDisk();
}

void StandaloneTrustedVaultBackend::FetchKeys(
    const CoreAccountInfo& account_info,
    FetchKeysCallback callback) {
  DCHECK(!callback.is_null());

  if (storage_->HasNonConstantKey(account_info.gaia, security_domain_id_) &&
      !storage_->GetKeysMarkedAsStaleByConsumer(account_info.gaia,
                                                security_domain_id_)) {
    // There are locally available keys, which weren't marked as stale. Keys
    // download attempt is not needed.
    FulfillFetchKeys(account_info.gaia, std::move(callback),
                     /*status_for_uma=*/std::nullopt);
    return;
  }
  if (!connection_) {
    // Keys downloading is disabled.
    FulfillFetchKeys(account_info.gaia, std::move(callback),
                     /*status_for_uma=*/std::nullopt);
    return;
  }
  if (!primary_account_.has_value() ||
      primary_account_->gaia != account_info.gaia) {
    // Keys download attempt is not possible because there is no primary
    // account.
    FulfillFetchKeys(account_info.gaia, std::move(callback),
                     TrustedVaultRecoverKeysOutcomeForUMA::kNoPrimaryAccount);
    return;
  }
  if (ongoing_fetch_keys_.has_value()) {
    // Keys downloading is only supported for primary account, thus gaia_id
    // should be the same for |ongoing_fetch_keys_| and |account_info|.
    CHECK_EQ(ongoing_fetch_keys_->gaia_id, primary_account_->gaia);
    CHECK_EQ(ongoing_fetch_keys_->gaia_id, account_info.gaia);
    // Download keys request is in progress already, |callback| will be invoked
    // upon its completion.
    ongoing_fetch_keys_->callbacks.emplace_back(std::move(callback));
    return;
  }

  ongoing_fetch_keys_ = OngoingFetchKeys();
  ongoing_fetch_keys_->gaia_id = account_info.gaia;
  ongoing_fetch_keys_->callbacks.emplace_back(std::move(callback));

  // |connection_| and |primary_account_| are checked to be present above, so
  // |local_recovery_factors| can't be empty.
  CHECK(!local_recovery_factors_.empty());
  AttemptRecoveryFactor(0);
}

void StandaloneTrustedVaultBackend::AttemptRecoveryFactor(
    size_t local_recovery_factor) {
  CHECK(local_recovery_factor >= 0 &&
        local_recovery_factor < local_recovery_factors_.size());
  local_recovery_factors_[local_recovery_factor]->AttemptRecovery(
      base::BindOnce(&StandaloneTrustedVaultBackend::OnKeysRecovered,
                     weak_ptr_factory_.GetWeakPtr(), local_recovery_factor));
}

void StandaloneTrustedVaultBackend::StoreKeys(
    const GaiaId& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Setters will create a user vault if it doesn't exist yet.

  // Having retrieved (or downloaded) new keys indicates that information
  // about past registration attempts (and probably failures) may no longer be
  // relevant.
  storage_->SetLastRegistrationReturnedLocalDataObsolete(
      gaia_id, security_domain_id_, false);

  // Replace all keys (this also clears the "marked as stale" flag).
  storage_->SetVaultKeys(gaia_id, security_domain_id_, keys, last_key_version);

  MaybeRegisterLocalRecoveryFactors();
}

void StandaloneTrustedVaultBackend::SetPrimaryAccount(
    const std::optional<CoreAccountInfo>& primary_account,
    RefreshTokenErrorState refresh_token_error_state) {
  const RefreshTokenErrorState previous_refresh_token_error_state =
      refresh_token_error_state_;
  refresh_token_error_state_ = refresh_token_error_state;

  if (primary_account == primary_account_) {
    // Still need to complete deferred deletion, e.g. if primary account was
    // cleared before browser shutdown but not handled here.
    RemoveNonPrimaryAccountKeysIfMarkedForDeletion();

    // A persistent auth error could have just been resolved.
    if (PersistentAuthErrorWasResolved(previous_refresh_token_error_state,
                                       refresh_token_error_state_)) {
      MaybeProcessPendingTrustedRecoveryMethod();
      MaybeRegisterLocalRecoveryFactors();

      CHECK(degraded_recoverability_handler_);
      degraded_recoverability_handler_->HintDegradedRecoverabilityChanged(
          TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
              kPersistentAuthErrorResolved);
    }

    return;
  }

  primary_account_ = primary_account;
  degraded_recoverability_handler_ = nullptr;
  ongoing_add_recovery_method_request_.reset();
  // This aborts all ongoing recoveries / registrations.
  local_recovery_factors_.clear();
  ongoing_registration_attempts_.clear();
  RemoveNonPrimaryAccountKeysIfMarkedForDeletion();
  // Make sure to call pending callbacks, now that ongoing recoveries were
  // aborted.
  FulfillOngoingFetchKeys(TrustedVaultRecoverKeysOutcomeForUMA::kAborted);

  if (!primary_account_.has_value()) {
    return;
  }

  if (connection_) {
    // |storage_| and |connection_| outlive |local_recovery_factors_|, so
    // passing raw pointers is ok.
    local_recovery_factors_ =
        local_recovery_factors_factory_->CreateLocalRecoveryFactors(
            security_domain_id_, storage_.get(), connection_.get(),
            *primary_account_);
  }

  degraded_recoverability_handler_ =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          connection_.get(), /*delegate=*/this, primary_account_.value(),
          storage_->GetDegradedRecoverabilityState(primary_account_->gaia,
                                                   security_domain_id_));
  // Should process `pending_get_is_recoverability_degraded_` if it belongs to
  // the current primary account.
  // TODO(crbug.com/40255601): |pending_get_is_recoverability_degraded_| should
  // be redundant now. GetRecoverabilityIsDegraded() should be called after
  // SetPrimaryAccount(). This logic is similar to FetchKeys() reporting
  // kNoPrimaryAccount, once there is data confirming that this bucked is not
  // recorded, it should be safe to remove.
  if (pending_get_is_recoverability_degraded_.has_value() &&
      pending_get_is_recoverability_degraded_->account_info ==
          primary_account_) {
    degraded_recoverability_handler_->GetIsRecoverabilityDegraded(std::move(
        pending_get_is_recoverability_degraded_->completion_callback));
  }
  pending_get_is_recoverability_degraded_.reset();

  MaybeRegisterLocalRecoveryFactors();
  MaybeProcessPendingTrustedRecoveryMethod();
  NotifyIdleForTestingIfNecessary();
}

void StandaloneTrustedVaultBackend::UpdateAccountsInCookieJarInfo(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  base::flat_set<GaiaId> known_gaia_ids;
  for (const gaia::ListedAccount& account :
       accounts_in_cookie_jar_info.GetAllAccounts()) {
    known_gaia_ids.insert(account.gaia_id);
  }
  storage_->ClearDataForUnknownUsersOrMarkForDeletion(
      known_gaia_ids,
      primary_account_ ? std::optional(primary_account_->gaia) : std::nullopt);
}

bool StandaloneTrustedVaultBackend::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info) {
  if (storage_->GetVaultKeys(account_info.gaia, security_domain_id_).empty() ||
      storage_->GetKeysMarkedAsStaleByConsumer(account_info.gaia,
                                               security_domain_id_)) {
    // No keys available for |account_info| or they are already marked as stale.
    return false;
  }

  storage_->SetKeysMarkedAsStaleByConsumer(account_info.gaia,
                                           security_domain_id_, true);
  return true;
}

void StandaloneTrustedVaultBackend::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  if (account_info == primary_account_) {
    degraded_recoverability_handler_->GetIsRecoverabilityDegraded(
        std::move(cb));
    return;
  }
  pending_get_is_recoverability_degraded_ =
      PendingGetIsRecoverabilityDegraded();
  pending_get_is_recoverability_degraded_->account_info = account_info;
  pending_get_is_recoverability_degraded_->completion_callback = std::move(cb);
}

void StandaloneTrustedVaultBackend::AddTrustedRecoveryMethod(
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure cb) {
  if (public_key.empty()) {
    std::move(cb).Run();
    return;
  }

  if (!primary_account_.has_value() ||
      refresh_token_error_state_ ==
          RefreshTokenErrorState::kPersistentAuthError) {
    // Defer until SetPrimaryAccount() gets called and there are no persistent
    // auth errors. Note that the latter is important, because this method can
    // be called while the auth error is being resolved and there is no order
    // guarantee.
    pending_trusted_recovery_method_ = PendingTrustedRecoveryMethod();
    pending_trusted_recovery_method_->gaia_id = gaia_id;
    pending_trusted_recovery_method_->public_key = public_key;
    pending_trusted_recovery_method_->method_type_hint = method_type_hint;
    pending_trusted_recovery_method_->completion_callback = std::move(cb);
    return;
  }

  DCHECK(!pending_trusted_recovery_method_.has_value());

  if (primary_account_->gaia != gaia_id) {
    std::move(cb).Run();
    return;
  }

  const std::vector<std::vector<uint8_t>> vault_keys =
      storage_->GetVaultKeys(gaia_id, security_domain_id_);
  const int last_key_version =
      storage_->GetLastKeyVersion(gaia_id, security_domain_id_);

  if (vault_keys.empty()) {
    // Can't add recovery method while there are no local keys.
    std::move(cb).Run();
    return;
  }

  std::unique_ptr<SecureBoxPublicKey> imported_public_key =
      SecureBoxPublicKey::CreateByImport(public_key);
  if (!imported_public_key) {
    // Invalid public key.
    std::move(cb).Run();
    return;
  }

  last_added_recovery_method_public_key_for_testing_ = public_key;

  if (!connection_) {
    // Feature disabled.
    std::move(cb).Run();
    return;
  }

  ongoing_add_recovery_method_request_ =
      connection_->RegisterAuthenticationFactor(
          *primary_account_,
          GetTrustedVaultKeysWithVersions(vault_keys, last_key_version),
          *imported_public_key,
          UnspecifiedAuthenticationFactorType(method_type_hint),
          base::IgnoreArgs<TrustedVaultRegistrationStatus, int>(base::BindOnce(
              &StandaloneTrustedVaultBackend::OnTrustedRecoveryMethodAdded,
              weak_ptr_factory_.GetWeakPtr(), std::move(cb))));
}

void StandaloneTrustedVaultBackend::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  storage_->ClearDataForUser(account_info.gaia);

  // This codepath invoked as part of sync reset. While sync reset can cause
  // resetting primary account, this is not the case for Chrome OS and Butter
  // mode. Trigger recovery factor registration attempt immediately as it can
  // succeed in these cases.
  MaybeRegisterLocalRecoveryFactors();
  NotifyIdleForTestingIfNecessary();
}

std::optional<CoreAccountInfo>
StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting() const {
  return primary_account_;
}

trusted_vault_pb::LocalDeviceRegistrationInfo
StandaloneTrustedVaultBackend::GetDeviceRegistrationInfoForTesting(
    const GaiaId& gaia_id) {
  return storage_->GetLocalDeviceRegistrationInfo(gaia_id);
}

std::vector<uint8_t>
StandaloneTrustedVaultBackend::GetLastAddedRecoveryMethodPublicKeyForTesting()
    const {
  return last_added_recovery_method_public_key_for_testing_;
}

int StandaloneTrustedVaultBackend::GetLastKeyVersionForTesting(
    const GaiaId& gaia_id) {
  return storage_->GetLastKeyVersion(gaia_id, security_domain_id_);
}

bool StandaloneTrustedVaultBackend::HasPendingTrustedRecoveryMethodForTesting()
    const {
  return pending_trusted_recovery_method_.has_value();
}

void StandaloneTrustedVaultBackend::MaybeRegisterLocalRecoveryFactors() {
  // TODO(crbug.com/40255601): in case of transient failure this function is
  // likely to be not called until the browser restart; implement retry logic.

  const bool should_record_metrics =
      !recovery_factor_registration_state_recorded_to_uma_;
  for (auto& factor : local_recovery_factors_) {
    const LocalRecoveryFactorType factor_type = factor->GetRecoveryFactorType();
    ongoing_registration_attempts_[factor_type]++;
    const std::optional<TrustedVaultRecoveryFactorRegistrationStateForUMA>
        registration_state = factor->MaybeRegister(base::BindOnce(
            &StandaloneTrustedVaultBackend::OnRecoveryFactorRegistered,
            weak_ptr_factory_.GetWeakPtr(), factor_type));

    if (registration_state.has_value() && should_record_metrics) {
      recovery_factor_registration_state_recorded_to_uma_ = true;
      base::UmaHistogramBoolean(
          base::StrCat({"TrustedVault.RecoveryFactorRegistered.",
                        GetLocalRecoveryFactorNameForUma(factor_type), ".",
                        GetSecurityDomainNameForUma(security_domain_id_)}),
          factor->IsRegistered());
      RecordTrustedVaultRecoveryFactorRegistrationState(
          factor_type, security_domain_id_, *registration_state);
    }
  }
}

void StandaloneTrustedVaultBackend::MaybeProcessPendingTrustedRecoveryMethod() {
  if (!primary_account_.has_value() ||
      refresh_token_error_state_ ==
          RefreshTokenErrorState::kPersistentAuthError ||
      !pending_trusted_recovery_method_.has_value() ||
      pending_trusted_recovery_method_->gaia_id != primary_account_->gaia) {
    return;
  }

  PendingTrustedRecoveryMethod recovery_method =
      std::move(*pending_trusted_recovery_method_);
  pending_trusted_recovery_method_.reset();

  AddTrustedRecoveryMethod(recovery_method.gaia_id, recovery_method.public_key,
                           recovery_method.method_type_hint,
                           std::move(recovery_method.completion_callback));

  DCHECK(!pending_trusted_recovery_method_.has_value());
}

void StandaloneTrustedVaultBackend::OnRecoveryFactorRegistered(
    LocalRecoveryFactorType local_recovery_factor_type,
    TrustedVaultRegistrationStatus status,
    int key_version,
    bool had_local_keys) {
  // SetPrimaryAccount() cancels ongoing registration attempts and clears
  // the map. However, there is a chance that a call for this callback is
  // already scheduled at that time. Checking for <= 0 defensively covers this
  // case.
  if (--ongoing_registration_attempts_[local_recovery_factor_type] <= 0) {
    ongoing_registration_attempts_.erase(local_recovery_factor_type);
  }

  if (status == TrustedVaultRegistrationStatus::kRegistrationNotAttempted ||
      status == TrustedVaultRegistrationStatus::kRegistrationCancelled) {
    NotifyIdleForTestingIfNecessary();
    return;
  }

  // If |primary_account_| was changed meanwhile, this callback must be
  // cancelled.
  DCHECK(primary_account_.has_value());

  RecordTrustedVaultRecoveryFactorRegistrationOutcome(
      local_recovery_factor_type, security_domain_id_,
      GetRecoveryFactorRegistrationOutcomeForUMAFromResponse(status));

  switch (status) {
    case TrustedVaultRegistrationStatus::kRegistrationNotAttempted:
    case TrustedVaultRegistrationStatus::kRegistrationCancelled:
      NOTREACHED();
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      if (!had_local_keys) {
        // Recover factor registration was triggered while no local non-constant
        // keys were available. Detected server-side key should be stored upon
        // successful completion (or if recovery factor was already registered,
        // e.g. previous response wasn't handled properly), but absence of
        // keys (non-constant or constant) still needs to be checked before that
        // - there might be StoreKeys() call during handling the request.
        if (storage_->GetVaultKeys(primary_account_->gaia, security_domain_id_)
                .empty()) {
          storage_->SetVaultKeys(primary_account_->gaia, security_domain_id_,
                                 {GetConstantTrustedVaultKey()}, key_version);
        }
      }
      break;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kNetworkError:
      // Request wasn't sent to the server, so there is no need for throttling.
      break;
    case TrustedVaultRegistrationStatus::kOtherError:
      connection_->RecordFailedRequestForThrottling(*primary_account_,
                                                    security_domain_id_);
      break;
  }
  NotifyIdleForTestingIfNecessary();
}

void StandaloneTrustedVaultBackend::OnKeysRecovered(
    size_t current_local_recovery_factor,
    LocalRecoveryFactor::RecoveryStatus recovery_status,
    const std::vector<std::vector<uint8_t>>& downloaded_vault_keys,
    int last_vault_key_version) {
  CHECK(primary_account_.has_value());
  // This method should be called only as a result of fetching keys attributed
  // to current |ongoing_fetch_keys_|.
  CHECK(ongoing_fetch_keys_);
  CHECK_EQ(ongoing_fetch_keys_->gaia_id, primary_account_->gaia);

  bool should_attempt_next_recovery_factor = true;
  switch (recovery_status) {
    case LocalRecoveryFactor::RecoveryStatus::kSuccess: {
      // |downloaded_vault_keys| doesn't necessary have all keys known to the
      // backend, because some old keys may have been deleted from the server
      // already. Not preserving old keys is acceptable and desired here, since
      // the opposite can make some operations (such as registering
      // authentication factors) impossible.
      StoreKeys(primary_account_->gaia, downloaded_vault_keys,
                last_vault_key_version);
      should_attempt_next_recovery_factor = false;
      break;
    }
    case LocalRecoveryFactor::RecoveryStatus::kNoNewKeys: {
      // Persist the keys even though there are no new ones, since some old keys
      // could be removed from the server.
      StoreKeys(primary_account_->gaia, downloaded_vault_keys,
                last_vault_key_version);
      // The server state for different recovery factors is guaranteed to be the
      // same (i.e. they'd return the same keys). So there's no point in trying
      // other recovery factors in this case.
      should_attempt_next_recovery_factor = false;
      break;
    }
    case LocalRecoveryFactor::RecoveryStatus::kFailure:
      break;
  }

  if (should_attempt_next_recovery_factor) {
    const size_t next_local_recovery_factor = current_local_recovery_factor + 1;
    if (next_local_recovery_factor < local_recovery_factors_.size()) {
      AttemptRecoveryFactor(next_local_recovery_factor);
      return;
    }
  }

  // We don't want to attempt the next recovery factor, or we ran out of local
  // recovery factors to try. Give up with the status from the last recovery
  // factor.
  FulfillOngoingFetchKeys(
      GetRecoverKeysOutcomeForUMAFromRecoveryStatus(recovery_status));
}

void StandaloneTrustedVaultBackend::OnTrustedRecoveryMethodAdded(
    base::OnceClosure cb) {
  DCHECK(ongoing_add_recovery_method_request_);
  ongoing_add_recovery_method_request_ = nullptr;

  std::move(cb).Run();

  degraded_recoverability_handler_->HintDegradedRecoverabilityChanged(
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
          kRecoveryMethodAdded);
  NotifyIdleForTestingIfNecessary();
}

void StandaloneTrustedVaultBackend::FulfillOngoingFetchKeys(
    std::optional<TrustedVaultRecoverKeysOutcomeForUMA> status_for_uma) {
  if (!ongoing_fetch_keys_.has_value()) {
    return;
  }

  // Invoking callbacks may in theory cause side effects (like changing
  // |ongoing_fetch_keys_|), making a local copy to avoid them.
  auto ongoing_fetch_keys = std::move(*ongoing_fetch_keys_);
  ongoing_fetch_keys_ = std::nullopt;

  for (auto& callback : ongoing_fetch_keys.callbacks) {
    FulfillFetchKeys(ongoing_fetch_keys.gaia_id, std::move(callback),
                     status_for_uma);
  }
}

void StandaloneTrustedVaultBackend::FulfillFetchKeys(
    const GaiaId& gaia_id,
    FetchKeysCallback callback,
    std::optional<TrustedVaultRecoverKeysOutcomeForUMA> status_for_uma) {
  if (status_for_uma.has_value()) {
    RecordTrustedVaultRecoverKeysOutcome(security_domain_id_, *status_for_uma);
  }

  std::vector<std::vector<uint8_t>> vault_keys =
      storage_->GetVaultKeys(gaia_id, security_domain_id_);
  std::erase_if(vault_keys, [](const std::vector<uint8_t>& key) {
    return key == GetConstantTrustedVaultKey();
  });

  std::move(callback).Run(vault_keys);
  NotifyIdleForTestingIfNecessary();
}

void StandaloneTrustedVaultBackend::
    RemoveNonPrimaryAccountKeysIfMarkedForDeletion() {
  storage_->ClearDataForUsersMarkedForDeletion(
      primary_account_ ? std::optional(primary_account_->gaia) : std::nullopt);
}

void StandaloneTrustedVaultBackend::WaitForIdleForTesting(
    base::OnceClosure cb) {
  idle_callbacks_for_testing_.push_back(std::move(cb));
  NotifyIdleForTestingIfNecessary();
}

void StandaloneTrustedVaultBackend::NotifyIdleForTestingIfNecessary() {
  if (idle_callbacks_for_testing_.empty()) {
    return;
  }

  if (ongoing_fetch_keys_.has_value() ||
      !ongoing_registration_attempts_.empty() ||
      ongoing_add_recovery_method_request_ != nullptr ||
      pending_trusted_recovery_method_.has_value() ||
      pending_get_is_recoverability_degraded_.has_value()) {
    return;
  }

  std::vector<base::OnceClosure> callbacks =
      std::exchange(idle_callbacks_for_testing_, {});
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
}

}  // namespace trusted_vault
