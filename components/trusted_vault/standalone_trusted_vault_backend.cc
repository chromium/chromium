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
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/recovery_key_store_connection_impl.h"
#include "components/trusted_vault/recovery_key_store_controller.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace trusted_vault {

namespace {

constexpr int kCurrentLocalTrustedVaultVersion = 2;
constexpr int kCurrentDeviceRegistrationVersion = 1;

trusted_vault_pb::LocalTrustedVault ReadDataFromDiskImpl(
    const base::FilePath& file_path,
    SecurityDomainId security_domain_id) {
  std::string file_content;

  trusted_vault_pb::LocalTrustedVault data_proto;
  if (!base::PathExists(file_path)) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id, TrustedVaultFileReadStatusForUMA::kNotFound);
    return data_proto;
  }
  if (!base::ReadFileToString(file_path, &file_content)) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id, TrustedVaultFileReadStatusForUMA::kFileReadFailed);
    return data_proto;
  }
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  if (!file_proto.ParseFromString(file_content)) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kFileProtoDeserializationFailed);
    return data_proto;
  }

  if (base::MD5String(file_proto.serialized_local_trusted_vault()) !=
      file_proto.md5_digest_hex_string()) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kMD5DigestMismatch);
    return data_proto;
  }

  if (!data_proto.ParseFromString(
          file_proto.serialized_local_trusted_vault())) {
    RecordTrustedVaultFileReadStatus(
        security_domain_id,
        TrustedVaultFileReadStatusForUMA::kDataProtoDeserializationFailed);
    return data_proto;
  }
  RecordTrustedVaultFileReadStatus(security_domain_id,
                                   TrustedVaultFileReadStatusForUMA::kSuccess);
  return data_proto;
}

void WriteDataToDiskImpl(const trusted_vault_pb::LocalTrustedVault& data,
                         const base::FilePath& file_path,
                         SecurityDomainId security_domain_id) {
  trusted_vault_pb::LocalTrustedVaultFileContent file_proto;
  file_proto.set_serialized_local_trusted_vault(data.SerializeAsString());
  file_proto.set_md5_digest_hex_string(
      base::MD5String(file_proto.serialized_local_trusted_vault()));
  bool success = base::ImportantFileWriter::WriteFileAtomically(
      file_path, file_proto.SerializeAsString(), "TrustedVault");
  if (!success) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
  base::UmaHistogramBoolean("TrustedVault.FileWriteSuccess." +
                                GetSecurityDomainNameForUma(security_domain_id),
                            success);
}

bool HasNonConstantKey(
    const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault) {
  std::string constant_key_as_proto_string;
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           &constant_key_as_proto_string);
  for (const trusted_vault_pb::LocalTrustedVaultKey& key :
       per_user_vault.vault_key()) {
    if (key.key_material() != constant_key_as_proto_string) {
      return true;
    }
  }
  return false;
}

std::vector<std::vector<uint8_t>> GetAllVaultKeys(
    const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault) {
  std::vector<std::vector<uint8_t>> vault_keys;
  for (const trusted_vault_pb::LocalTrustedVaultKey& key :
       per_user_vault.vault_key()) {
    vault_keys.emplace_back(ProtoStringToBytes(key.key_material()));
  }
  return vault_keys;
}

base::flat_set<std::string> GetGaiaIDs(
    const std::vector<gaia::ListedAccount>& listed_accounts) {
  base::flat_set<std::string> result;
  for (const gaia::ListedAccount& listed_account : listed_accounts) {
    result.insert(listed_account.gaia_id);
  }
  return result;
}

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

TrustedVaultDeviceRegistrationOutcomeForUMA
GetDeviceRegistrationOutcomeForUMAFromResponse(
    TrustedVaultRegistrationStatus response_status) {
  switch (response_status) {
    case TrustedVaultRegistrationStatus::kSuccess:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::kSuccess;
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::kAlreadyRegistered;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::kLocalDataObsolete;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::
          kTransientAccessTokenFetchError;
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::
          kPersistentAccessTokenFetchError;
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::
          kPrimaryAccountChangeAccessTokenFetchError;
    case TrustedVaultRegistrationStatus::kNetworkError:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::kNetworkError;
    case TrustedVaultRegistrationStatus::kOtherError:
      return TrustedVaultDeviceRegistrationOutcomeForUMA::kOtherError;
  }
  NOTREACHED_IN_MIGRATION();
  return TrustedVaultDeviceRegistrationOutcomeForUMA::kOtherError;
}

// Version 0 may contain corrupted data: missing constant key if the client
// was affected by crbug.com/1267391, this function injects constant key if it's
// not stored and there is exactly one non-constant key. |local_trusted_vault|
// must not be null and must have |version| set to 0.
void UpgradeToVersion1(
    trusted_vault_pb::LocalTrustedVault* local_trusted_vault) {
  DCHECK(local_trusted_vault);
  DCHECK_EQ(local_trusted_vault->data_version(), 0);

  std::string constant_key_as_proto_string;
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           &constant_key_as_proto_string);

  for (trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault :
       *local_trusted_vault->mutable_user()) {
    if (per_user_vault.vault_key_size() == 1 &&
        per_user_vault.vault_key(0).key_material() !=
            constant_key_as_proto_string) {
      // Add constant key in the beginning.
      *per_user_vault.add_vault_key() = per_user_vault.vault_key(0);
      per_user_vault.mutable_vault_key(0)->set_key_material(
          constant_key_as_proto_string);
    }
  }
  local_trusted_vault->set_data_version(1);
}

// Version 1 may contain `keys_marked_as_stale_by_consumer` (before the field
// was renamed) accidentally set to true, upgrade to version 2 resets it to
// false.
void UpgradeToVersion2(
    trusted_vault_pb::LocalTrustedVault* local_trusted_vault) {
  DCHECK(local_trusted_vault);
  DCHECK_EQ(local_trusted_vault->data_version(), 1);

  for (trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault :
       *local_trusted_vault->mutable_user()) {
    per_user_vault.set_keys_marked_as_stale_by_consumer(false);
  }
  local_trusted_vault->set_data_version(2);
}

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

// static
TrustedVaultDownloadKeysStatusForUMA
StandaloneTrustedVaultBackend::GetDownloadKeysStatusForUMAFromResponse(
    TrustedVaultDownloadKeysStatus response_status) {
  switch (response_status) {
    case TrustedVaultDownloadKeysStatus::kSuccess:
      return TrustedVaultDownloadKeysStatusForUMA::kSuccess;
    case TrustedVaultDownloadKeysStatus::kMemberNotFound:
      return TrustedVaultDownloadKeysStatusForUMA::kMemberNotFound;
    case TrustedVaultDownloadKeysStatus::kMembershipNotFound:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipNotFound;
    case TrustedVaultDownloadKeysStatus::kMembershipCorrupted:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipCorrupted;
    case TrustedVaultDownloadKeysStatus::kMembershipEmpty:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty;
    case TrustedVaultDownloadKeysStatus::kNoNewKeys:
      return TrustedVaultDownloadKeysStatusForUMA::kNoNewKeys;
    case TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed:
      return TrustedVaultDownloadKeysStatusForUMA::kKeyProofsVerificationFailed;
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
      return TrustedVaultDownloadKeysStatusForUMA::kAccessTokenFetchingFailure;
    case TrustedVaultDownloadKeysStatus::kNetworkError:
      return TrustedVaultDownloadKeysStatusForUMA::kNetworkError;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
  }

  NOTREACHED_IN_MIGRATION();
  return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
}

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
    SecurityDomainId security_domain_id,
    const base::FilePath& file_path,
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<TrustedVaultConnection> connection,
    std::unique_ptr<RecoveryKeyStoreController::RecoveryKeyProvider>
        recovery_key_provider,
    std::unique_ptr<RecoveryKeyStoreConnection> recovery_key_store_connection)
    : security_domain_id_(security_domain_id),
      file_path_(file_path),
      delegate_(std::move(delegate)),
      connection_(std::move(connection)),
      clock_(base::DefaultClock::GetInstance()) {
  if (recovery_key_provider) {
    // TODO(crbug.com/40187814): Initialize/recreate in SetPrimaryAccount().
    CHECK(recovery_key_store_connection);
    recovery_key_store_controller_ =
        std::make_unique<RecoveryKeyStoreController>(
            std::move(recovery_key_provider),
            std::move(recovery_key_store_connection), this);
  }
}

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
    SecurityDomainId security_domain_id,
    const base::FilePath& file_path,
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<TrustedVaultConnection> connection)
    : StandaloneTrustedVaultBackend(security_domain_id,
                                    file_path,
                                    std::move(delegate),
                                    std::move(connection),
                                    /*recovery_key_provider=*/nullptr,
                                    /*recovery_key_store_connection=*/nullptr) {
}

StandaloneTrustedVaultBackend::~StandaloneTrustedVaultBackend() = default;

void StandaloneTrustedVaultBackend::WriteDegradedRecoverabilityState(
    const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
        degraded_recoverability_state) {
  DCHECK(primary_account_.has_value());
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  *per_user_vault->mutable_degraded_recoverability_state() =
      degraded_recoverability_state;
  WriteDataToDisk();
}

void StandaloneTrustedVaultBackend::OnDegradedRecoverabilityChanged() {
  delegate_->NotifyRecoverabilityDegradedChanged();
}

void StandaloneTrustedVaultBackend::ReadDataFromDisk() {
  data_ = ReadDataFromDiskImpl(file_path_, security_domain_id_);

  if (data_.user_size() == 0) {
    // No data, set the current version and omit writing the file.
    data_.set_data_version(kCurrentLocalTrustedVaultVersion);
  }

  if (data_.data_version() == 0) {
    UpgradeToVersion1(&data_);
    WriteDataToDisk();
  }

  if (data_.data_version() == 1) {
    UpgradeToVersion2(&data_);
    WriteDataToDisk();
  }

  DCHECK_EQ(data_.data_version(), kCurrentLocalTrustedVaultVersion);
}

void StandaloneTrustedVaultBackend::FetchKeys(
    const CoreAccountInfo& account_info,
    FetchKeysCallback callback) {
  DCHECK(!callback.is_null());

  const trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);

  if (per_user_vault && HasNonConstantKey(*per_user_vault) &&
      !per_user_vault->keys_marked_as_stale_by_consumer()) {
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
                     TrustedVaultDownloadKeysStatusForUMA::kNoPrimaryAccount);
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
  DCHECK(per_user_vault);
  if (!per_user_vault->local_device_registration_info().device_registered()) {
    // Keys download attempt is not possible because the device is not
    // registered.
    FulfillFetchKeys(
        account_info.gaia, std::move(callback),
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered);
    return;
  }
  if (AreConnectionRequestsThrottled()) {
    // Keys download attempt is not possible.
    FulfillFetchKeys(
        account_info.gaia, std::move(callback),
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide);
    return;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          ProtoStringToBytes(per_user_vault->local_device_registration_info()
                                 .private_key_material()));
  if (!key_pair) {
    // Corrupted state: device is registered, but |key_pair| can't be imported.
    // TODO(crbug.com/40699425): restore from this state (throw away the key and
    // trigger device registration again).
    FulfillFetchKeys(account_info.gaia, std::move(callback),
                     TrustedVaultDownloadKeysStatusForUMA::
                         kCorruptedLocalDeviceRegistration);
    return;
  }

  ongoing_fetch_keys_ = OngoingFetchKeys();
  ongoing_fetch_keys_->gaia_id = account_info.gaia;
  ongoing_fetch_keys_->callbacks.emplace_back(std::move(callback));
  // Guaranteed by |device_registered| check above.
  DCHECK(!per_user_vault->vault_key().empty());
  // |this| outlives |connection_| and |ongoing_keys_downloading_request_|, so
  // it's safe to use base::Unretained() here.
  ongoing_fetch_keys_->request = connection_->DownloadNewKeys(
      *primary_account_,
      TrustedVaultKeyAndVersion(
          ProtoStringToBytes(
              per_user_vault->vault_key().rbegin()->key_material()),
          per_user_vault->last_vault_key_version()),
      std::move(key_pair),
      base::BindOnce(&StandaloneTrustedVaultBackend::OnKeysDownloaded,
                     base::Unretained(this)));
  DCHECK(ongoing_fetch_keys_->request);
}

void StandaloneTrustedVaultBackend::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Find or create user for |gaid_id|.
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);
  if (!per_user_vault) {
    per_user_vault = data_.add_user();
    per_user_vault->set_gaia_id(gaia_id);
  }

  // Having retrieved (or downloaded) new keys indicates that past failures may
  // no longer be relevant.
  per_user_vault->mutable_local_device_registration_info()
      ->set_last_registration_returned_local_data_obsolete(false);

  // Replace all keys.
  per_user_vault->set_last_vault_key_version(last_key_version);
  per_user_vault->set_keys_marked_as_stale_by_consumer(false);
  per_user_vault->clear_vault_key();
  for (const std::vector<uint8_t>& key : keys) {
    AssignBytesToProtoString(
        key, per_user_vault->add_vault_key()->mutable_key_material());
  }

  WriteDataToDisk();
  MaybeRegisterDevice();
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
      MaybeRegisterDevice();

      CHECK(degraded_recoverability_handler_);
      degraded_recoverability_handler_->HintDegradedRecoverabilityChanged(
          TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
              kPersistentAuthErrorResolved);
    }

    return;
  }

  primary_account_ = primary_account;
  ongoing_device_registration_request_ = nullptr;
  degraded_recoverability_handler_ = nullptr;
  ongoing_add_recovery_method_request_.reset();
  RemoveNonPrimaryAccountKeysIfMarkedForDeletion();
  FulfillOngoingFetchKeys(TrustedVaultDownloadKeysStatusForUMA::kAborted);

  if (!primary_account_.has_value()) {
    return;
  }

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account->gaia);
  if (!per_user_vault) {
    per_user_vault = data_.add_user();
    per_user_vault->set_gaia_id(primary_account->gaia);
  }

  degraded_recoverability_handler_ =
      std::make_unique<TrustedVaultDegradedRecoverabilityHandler>(
          connection_.get(), /*delegate=*/this, primary_account_.value(),
          per_user_vault->degraded_recoverability_state());
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

  const std::optional<TrustedVaultDeviceRegistrationStateForUMA>
      registration_state = MaybeRegisterDevice();

  if (registration_state.has_value() &&
      !device_registration_state_recorded_to_uma_) {
    device_registration_state_recorded_to_uma_ = true;
    base::UmaHistogramBoolean(
        "TrustedVault.DeviceRegistered." +
            GetSecurityDomainNameForUma(security_domain_id_),
        per_user_vault->local_device_registration_info().device_registered());
    RecordTrustedVaultDeviceRegistrationState(security_domain_id_,
                                              *registration_state);
  }

  MaybeProcessPendingTrustedRecoveryMethod();
  MaybeStartRecoveryKeyStoreUploads();
}

void StandaloneTrustedVaultBackend::UpdateAccountsInCookieJarInfo(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  const base::flat_set<std::string> gaia_ids_in_cookie_jar =
      base::STLSetUnion<base::flat_set<std::string>>(
          GetGaiaIDs(accounts_in_cookie_jar_info
                         .GetPotentiallyInvalidSignedInAccounts()),
          GetGaiaIDs(accounts_in_cookie_jar_info.GetSignedOutAccounts()));

  // Primary account data shouldn't be removed immediately, but it needs to be
  // removed once account become non-primary if it was ever removed from cookie
  // jar.
  if (primary_account_.has_value() &&
      !gaia_ids_in_cookie_jar.contains(primary_account_->gaia)) {
    trusted_vault_pb::LocalTrustedVaultPerUser* primary_account_data_ =
        FindUserVault(primary_account_->gaia);
    primary_account_data_->set_should_delete_keys_when_non_primary(true);
  }

  auto should_remove_user_data =
      [&gaia_ids_in_cookie_jar, &primary_account = primary_account_](
          const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_data) {
        const std::string& gaia_id = per_user_data.gaia_id();
        if (primary_account.has_value() && gaia_id == primary_account->gaia) {
          // Don't delete primary account data.
          return false;
        }
        // Delete data if account isn't in cookie jar.
        return !gaia_ids_in_cookie_jar.contains(gaia_id);
      };

  data_.mutable_user()->erase(
      base::ranges::remove_if(*data_.mutable_user(), should_remove_user_data),
      data_.mutable_user()->end());
  WriteDataToDisk();
}

bool StandaloneTrustedVaultBackend::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);
  if (!per_user_vault || per_user_vault->keys_marked_as_stale_by_consumer()) {
    // No keys available for |account_info| or they are already marked as stale.
    return false;
  }

  per_user_vault->set_keys_marked_as_stale_by_consumer(true);
  WriteDataToDisk();
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
    const std::string& gaia_id,
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

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);
  DCHECK(per_user_vault);

  if (per_user_vault->vault_key().empty()) {
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

  // |this| outlives |connection_| and
  // |ongoing_add_recovery_method_request_|, so it's safe to use
  // base::Unretained() here.
  ongoing_add_recovery_method_request_ =
      connection_->RegisterAuthenticationFactor(
          *primary_account_,
          GetTrustedVaultKeysWithVersions(
              GetAllVaultKeys(*per_user_vault),
              per_user_vault->last_vault_key_version()),
          *imported_public_key,
          UnspecifiedAuthenticationFactorType(method_type_hint),
          base::IgnoreArgs<TrustedVaultRegistrationStatus, int>(base::BindOnce(
              &StandaloneTrustedVaultBackend::OnTrustedRecoveryMethodAdded,
              base::Unretained(this), std::move(cb))));
}

void StandaloneTrustedVaultBackend::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);
  if (!per_user_vault) {
    return;
  }

  *per_user_vault = trusted_vault_pb::LocalTrustedVaultPerUser();
  per_user_vault->set_gaia_id(account_info.gaia);
  WriteDataToDisk();

  // This codepath invoked as part of sync reset. While sync reset can cause
  // resetting primary account, this is not the case for Chrome OS and Butter
  // mode. Trigger device registration attempt immediately as it can succeed in
  // these cases.
  MaybeRegisterDevice();
}

void StandaloneTrustedVaultBackend::SetRecoveryKeyStoreUploadEnabled(
    const CoreAccountInfo& account_info,
    bool is_enabled) {
  // `recovery_key_store_controller_` may not be nullopt at construction if this
  // method is called.
  CHECK(recovery_key_store_controller_);

  // TODO(crbug.com/40187814): Shift responsibility for updating the enabled bit
  // in the state to the RecoveryKeyStoreController.
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);
  if (!per_user_vault) {
    return;
  }
  trusted_vault_pb::RecoveryKeyStoreState* recovery_key_store_state =
      per_user_vault->mutable_recovery_key_store_state();
  recovery_key_store_state->set_recovery_key_store_upload_enabled(is_enabled);
  WriteDataToDisk();

  if (primary_account_ != account_info) {
    // Only the primary account uploads to recovery key store.
    return;
  }

  if (!is_enabled) {
    recovery_key_store_controller_->StopPeriodicUploads();
    return;
  }

  MaybeStartRecoveryKeyStoreUploads();
}

void StandaloneTrustedVaultBackend::MaybeStartRecoveryKeyStoreUploads() {
  CHECK(primary_account_);

  if (!recovery_key_store_controller_) {
    return;
  }

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  CHECK(per_user_vault);
  const trusted_vault_pb::RecoveryKeyStoreState& recovery_key_store_state =
      per_user_vault->recovery_key_store_state();
  if (!recovery_key_store_state.recovery_key_store_upload_enabled()) {
    return;
  }
  recovery_key_store_controller_->StartPeriodicUploads(
      *primary_account_, recovery_key_store_state,
      RecoveryKeyStoreController::kDefaultUpdatePeriod);
}

std::optional<CoreAccountInfo>
StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting() const {
  return primary_account_;
}

trusted_vault_pb::LocalDeviceRegistrationInfo
StandaloneTrustedVaultBackend::GetDeviceRegistrationInfoForTesting(
    const std::string& gaia_id) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);
  if (!per_user_vault) {
    return trusted_vault_pb::LocalDeviceRegistrationInfo();
  }
  return per_user_vault->local_device_registration_info();
}

std::vector<uint8_t>
StandaloneTrustedVaultBackend::GetLastAddedRecoveryMethodPublicKeyForTesting()
    const {
  return last_added_recovery_method_public_key_for_testing_;
}

int StandaloneTrustedVaultBackend::GetLastKeyVersionForTesting(
    const std::string& gaia_id) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);
  if (!per_user_vault) {
    return -1;
  }
  return per_user_vault->last_vault_key_version();
}

void StandaloneTrustedVaultBackend::SetDeviceRegisteredVersionForTesting(
    const std::string& gaia_id,
    int version) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);
  DCHECK(per_user_vault);
  per_user_vault->mutable_local_device_registration_info()
      ->set_device_registered_version(version);
  WriteDataToDisk();
}

void StandaloneTrustedVaultBackend::
    SetLastRegistrationReturnedLocalDataObsoleteForTesting(
        const std::string& gaia_id) {
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);
  DCHECK(per_user_vault);
  per_user_vault->mutable_local_device_registration_info()
      ->set_last_registration_returned_local_data_obsolete(true);
  WriteDataToDisk();
}

void StandaloneTrustedVaultBackend::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

bool StandaloneTrustedVaultBackend::HasPendingTrustedRecoveryMethodForTesting()
    const {
  return pending_trusted_recovery_method_.has_value();
}

bool StandaloneTrustedVaultBackend::AreConnectionRequestsThrottledForTesting() {
  return AreConnectionRequestsThrottled();
}

std::optional<TrustedVaultDeviceRegistrationStateForUMA>
StandaloneTrustedVaultBackend::MaybeRegisterDevice() {
  // TODO(crbug.com/40255601): in case of transient failure this function is
  // likely to be not called until the browser restart; implement retry logic.
  if (!connection_) {
    // Feature disabled.
    return std::nullopt;
  }

  if (!primary_account_.has_value()) {
    // Device registration is supported only for |primary_account_|.
    return std::nullopt;
  }

  // |per_user_vault| must be created before calling this function.
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);

  if (per_user_vault->local_device_registration_info().device_registered() &&
      per_user_vault->local_device_registration_info()
              .device_registered_version() ==
          kCurrentDeviceRegistrationVersion) {
    static_assert(kCurrentDeviceRegistrationVersion == 1);
    return TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1;
  }

  if (per_user_vault->local_device_registration_info()
          .last_registration_returned_local_data_obsolete()) {
    // Client already knows that existing vault keys (or their absence) isn't
    // sufficient for device registration. Fresh keys should be obtained first.
    return TrustedVaultDeviceRegistrationStateForUMA::kLocalKeysAreStale;
  }

  if (AreConnectionRequestsThrottled()) {
    return TrustedVaultDeviceRegistrationStateForUMA::kThrottledClientSide;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair;
  if (per_user_vault->has_local_device_registration_info()) {
    key_pair = SecureBoxKeyPair::CreateByPrivateKeyImport(
        /*private_key_bytes=*/ProtoStringToBytes(
            per_user_vault->local_device_registration_info()
                .private_key_material()));
  }

  const bool had_generated_key_pair = key_pair != nullptr;

  if (!key_pair) {
    key_pair = SecureBoxKeyPair::GenerateRandom();
    // It's possible that device will be successfully registered, but the client
    // won't persist this state (for example response doesn't reach the client
    // or registration callback is cancelled). To avoid duplicated registrations
    // device key is stored before sending the registration request, so the same
    // key will be used for future registration attempts.
    AssignBytesToProtoString(
        key_pair->private_key().ExportToBytes(),
        per_user_vault->mutable_local_device_registration_info()
            ->mutable_private_key_material());
    WriteDataToDisk();
  }

  // |this| outlives |connection_| and |ongoing_device_registration_request_|,
  // so it's safe to use base::Unretained() here.
  if (HasNonConstantKey(*per_user_vault)) {
    ongoing_device_registration_request_ =
        connection_->RegisterAuthenticationFactor(
            *primary_account_,
            GetTrustedVaultKeysWithVersions(
                GetAllVaultKeys(*per_user_vault),
                per_user_vault->last_vault_key_version()),
            key_pair->public_key(), LocalPhysicalDevice(),
            base::BindOnce(&StandaloneTrustedVaultBackend::OnDeviceRegistered,
                           base::Unretained(this)));
  } else {
    ongoing_device_registration_request_ =
        connection_->RegisterLocalDeviceWithoutKeys(
            *primary_account_, key_pair->public_key(),
            base::BindOnce(
                &StandaloneTrustedVaultBackend::OnDeviceRegisteredWithoutKeys,
                base::Unretained(this)));
  }

  DCHECK(ongoing_device_registration_request_);

  return had_generated_key_pair ? TrustedVaultDeviceRegistrationStateForUMA::
                                      kAttemptingRegistrationWithExistingKeyPair
                                : TrustedVaultDeviceRegistrationStateForUMA::
                                      kAttemptingRegistrationWithNewKeyPair;
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

void StandaloneTrustedVaultBackend::OnDeviceRegistered(
    TrustedVaultRegistrationStatus status,
    int key_version_unused) {
  // |key_version_unused| is unused because this callback is invoked when
  // adding a member to an existing security domain. In this case the key
  // version is already known.

  // If |primary_account_| was changed meanwhile, this callback must be
  // cancelled.
  DCHECK(primary_account_.has_value());

  // This method should be called only as a result of
  // |ongoing_device_registration_request_| completion/failure, verify this
  // condition and destroy |ongoing_device_registration_request_| as it's not
  // needed anymore.
  DCHECK(ongoing_device_registration_request_);
  ongoing_device_registration_request_ = nullptr;

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);

  // Registration is only attempted if the was no previous failure with
  // |kLocalDataObsolete|. If this precondition wasn't guaranteed here, the
  // field would need to be reset for some cases below such as `kSuccess` and
  // `kAlreadyRegistered`.
  DCHECK(!per_user_vault->local_device_registration_info()
              .last_registration_returned_local_data_obsolete());
  RecordTrustedVaultDeviceRegistrationOutcome(
      security_domain_id_,
      GetDeviceRegistrationOutcomeForUMAFromResponse(status));
  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      // kAlreadyRegistered handled as success, because it only means that
      // client doesn't fully handled successful device registration before.
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(true);
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered_version(kCurrentDeviceRegistrationVersion);
      WriteDataToDisk();
      return;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      per_user_vault->mutable_local_device_registration_info()
          ->set_last_registration_returned_local_data_obsolete(true);
      WriteDataToDisk();
      return;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kNetworkError:
      // Request wasn't sent to the server, so there is no need for throttling.
      return;
    case TrustedVaultRegistrationStatus::kOtherError:
      RecordFailedConnectionRequestForThrottling();
      return;
  }
}

void StandaloneTrustedVaultBackend::OnDeviceRegisteredWithoutKeys(
    TrustedVaultRegistrationStatus status,
    int key_version) {
  // If |primary_account_| was changed meanwhile, this callback must be
  // cancelled.
  DCHECK(primary_account_.has_value());

  // This method should be called only as a result of
  // |ongoing_device_registration_request_| completion/failure, verify this
  // condition, |ongoing_device_registration_request_| will be destroyed later
  // by OnDeviceRegistered() call.
  DCHECK(ongoing_device_registration_request_);

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);

  // This method can be called only if device registration was triggered while
  // no local keys available. Detected server-side key should be stored upon
  // successful completion, but |vault_key| emptiness still needs to be checked
  // before that - there might be StoreKeys() call during handling the request.
  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      // This method can be called only if device registration was triggered
      // while no local non-constant keys available. Detected server-side key
      // should be stored upon successful completion (or if device was already
      // registered, e.g. previous response wasn't handled properly), but
      // absence of non-constant keys still needs to be checked before that -
      // there might be StoreKeys() call during handling the request.
      if (!HasNonConstantKey(*per_user_vault)) {
        AssignBytesToProtoString(
            GetConstantTrustedVaultKey(),
            per_user_vault->add_vault_key()->mutable_key_material());
        per_user_vault->set_last_vault_key_version(key_version);
        // WriteToDisk() will be called by OnDeviceRegistered().
      }
      break;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
    case TrustedVaultRegistrationStatus::kNetworkError:
    case TrustedVaultRegistrationStatus::kOtherError:
      break;
  }
  OnDeviceRegistered(status, key_version);
}

void StandaloneTrustedVaultBackend::OnKeysDownloaded(
    TrustedVaultDownloadKeysStatus status,
    const std::vector<std::vector<uint8_t>>& downloaded_vault_keys,
    int last_vault_key_version) {
  DCHECK(primary_account_.has_value());

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);
  switch (status) {
    case TrustedVaultDownloadKeysStatus::kSuccess: {
      // |downloaded_vault_keys| doesn't necessary have all keys known to the
      // backend, because some old keys may have been deleted from the server
      // already. Not preserving old keys is acceptable and desired here, since
      // the opposite can make some operations (such as registering
      // authentication factors) impossible.
      StoreKeys(primary_account_->gaia, downloaded_vault_keys,
                last_vault_key_version);
      break;
    }
    case TrustedVaultDownloadKeysStatus::kMemberNotFound:
    case TrustedVaultDownloadKeysStatus::kMembershipNotFound:
    case TrustedVaultDownloadKeysStatus::kMembershipCorrupted:
    case TrustedVaultDownloadKeysStatus::kMembershipEmpty:
    case TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed: {
      // Unable to download new keys due to known protocol errors. The only way
      // to go out of these states is to receive new vault keys through external
      // StoreKeys() call. It's safe to mark device as not registered regardless
      // of the cause (device registration will be triggered once new vault keys
      // are available).
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(false);
      per_user_vault->mutable_local_device_registration_info()
          ->clear_device_registered_version();
      WriteDataToDisk();
      break;
    }
    case TrustedVaultDownloadKeysStatus::kNoNewKeys: {
      // The registration itself exists, but there's no additional keys to
      // download. This is bad because key download attempts are triggered for
      // the case where local keys have been marked as stale, which means the
      // user is likely in an unrecoverable state.
      RecordFailedConnectionRequestForThrottling();
      // Persist the keys anyway, since some old keys could be removed from the
      // server.
      StoreKeys(primary_account_->gaia, downloaded_vault_keys,
                last_vault_key_version);
      break;
    }
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
    case TrustedVaultDownloadKeysStatus::kNetworkError:
      // Request wasn't sent to the server, so there is no need for throttling.
      break;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      RecordFailedConnectionRequestForThrottling();
      break;
  }

  // This method should be called only as a result of keys downloading
  // attributed to current |ongoing_fetch_keys_|.
  DCHECK(ongoing_fetch_keys_);
  DCHECK_EQ(ongoing_fetch_keys_->gaia_id, primary_account_->gaia);

  FulfillOngoingFetchKeys(GetDownloadKeysStatusForUMAFromResponse(status));
}

void StandaloneTrustedVaultBackend::OnTrustedRecoveryMethodAdded(
    base::OnceClosure cb) {
  DCHECK(ongoing_add_recovery_method_request_);
  ongoing_add_recovery_method_request_ = nullptr;

  std::move(cb).Run();

  degraded_recoverability_handler_->HintDegradedRecoverabilityChanged(
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA::
          kRecoveryMethodAdded);
}

void StandaloneTrustedVaultBackend::FulfillOngoingFetchKeys(
    std::optional<TrustedVaultDownloadKeysStatusForUMA> status_for_uma) {
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
    const std::string& gaia_id,
    FetchKeysCallback callback,
    std::optional<TrustedVaultDownloadKeysStatusForUMA> status_for_uma) {
  const trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);

  if (status_for_uma.has_value()) {
    const bool also_log_with_v1_suffix =
        per_user_vault &&
        per_user_vault->local_device_registration_info().device_registered() &&
        per_user_vault->local_device_registration_info()
                .device_registered_version() == 1;
    RecordTrustedVaultDownloadKeysStatus(*status_for_uma,
                                         also_log_with_v1_suffix);
  }

  std::vector<std::vector<uint8_t>> vault_keys;
  if (per_user_vault) {
    vault_keys = GetAllVaultKeys(*per_user_vault);
    std::erase_if(vault_keys, [](const std::vector<uint8_t>& key) {
      return key == GetConstantTrustedVaultKey();
    });
  }

  std::move(callback).Run(vault_keys);
}

bool StandaloneTrustedVaultBackend::AreConnectionRequestsThrottled() {
  DCHECK(clock_);
  DCHECK(primary_account_.has_value());

  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);

  const base::Time current_time = clock_->Now();
  base::Time last_failed_request_time = ProtoTimeToTime(
      per_user_vault->last_failed_request_millis_since_unix_epoch());

  // Fix |last_failed_request_time| if it's set to the future.
  if (last_failed_request_time > current_time) {
    // Immediately unthrottle, but don't write new state to the file.
    last_failed_request_time = base::Time();
  }

  return last_failed_request_time + kThrottlingDuration > current_time;
}

void StandaloneTrustedVaultBackend::
    RecordFailedConnectionRequestForThrottling() {
  DCHECK(clock_);
  DCHECK(primary_account_.has_value());

  FindUserVault(primary_account_->gaia)
      ->set_last_failed_request_millis_since_unix_epoch(
          TimeToProtoTime(clock_->Now()));
  WriteDataToDisk();
}

void StandaloneTrustedVaultBackend::
    RemoveNonPrimaryAccountKeysIfMarkedForDeletion() {
  auto should_remove_user_data =
      [&primary_account = primary_account_](
          const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_data) {
        return per_user_data.should_delete_keys_when_non_primary() &&
               (!primary_account.has_value() ||
                primary_account->gaia != per_user_data.gaia_id());
      };

  data_.mutable_user()->erase(
      base::ranges::remove_if(*data_.mutable_user(), should_remove_user_data),
      data_.mutable_user()->end());
  WriteDataToDisk();
}

trusted_vault_pb::LocalTrustedVaultPerUser*
StandaloneTrustedVaultBackend::FindUserVault(const std::string& gaia_id) {
  for (int i = 0; i < data_.user_size(); ++i) {
    if (data_.user(i).gaia_id() == gaia_id) {
      return data_.mutable_user(i);
    }
  }
  return nullptr;
}

void StandaloneTrustedVaultBackend::WriteDataToDisk() {
  WriteDataToDiskImpl(data_, file_path_, security_domain_id_);
  delegate_->NotifyStateChanged();
}

void StandaloneTrustedVaultBackend::WriteRecoveryKeyStoreState(
    const trusted_vault_pb::RecoveryKeyStoreState& state) {
  CHECK(primary_account_);
  CHECK(state.recovery_key_store_upload_enabled());
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  *per_user_vault->mutable_recovery_key_store_state() = state;
  WriteDataToDisk();
}

void StandaloneTrustedVaultBackend::AddRecoveryKeyToSecurityDomain(
    const std::vector<uint8_t>& public_key_bytes,
    RecoveryKeyRegistrationCallback callback) {
  CHECK(primary_account_);
  trusted_vault_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  CHECK(per_user_vault);

  std::unique_ptr<SecureBoxPublicKey> public_key =
      SecureBoxPublicKey::CreateByImport(public_key_bytes);
  if (!public_key) {
    // Invalid public key.
    return;
  }

  // Resetting `ongoing_recovery_key_registration_request_` will cancel any
  // other recovery key registration request that is already in progress.
  //
  // AreConnectionRequestsThrottled() isn't used because we ensure externally
  // that this doesn't spam the server.
  ongoing_recovery_key_registration_request_ =
      connection_->RegisterAuthenticationFactor(
          *primary_account_,
          GetTrustedVaultKeysWithVersions(
              GetAllVaultKeys(*per_user_vault),
              per_user_vault->last_vault_key_version()),
          *public_key, LockScreenKnowledgeFactor(),
          base::BindOnce(&StandaloneTrustedVaultBackend::
                             OnRecoveryKeyAddedToSecurityDomain,
                         base::Unretained(this), std::move(callback)));
}

void StandaloneTrustedVaultBackend::OnRecoveryKeyAddedToSecurityDomain(
    RecoveryKeyRegistrationCallback callback,
    TrustedVaultRegistrationStatus status,
    int key_version_unused) {
  // |key_version_unused| is unused because this callback is invoked when
  // adding a member to an existing security domain. In this case the key
  // version is already known.
  CHECK(primary_account_);
  CHECK(ongoing_recovery_key_registration_request_);

  ongoing_recovery_key_registration_request_.reset();
  std::move(callback).Run(status);
}

}  // namespace trusted_vault
