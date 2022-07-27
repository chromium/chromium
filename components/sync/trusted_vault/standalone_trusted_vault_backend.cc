// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/os_crypt/os_crypt.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace syncer {

namespace {

constexpr int kCurrentLocalTrustedVaultVersion = 1;
constexpr int kCurrentDeviceRegistrationVersion = 1;
constexpr base::TimeDelta kVerifyDeviceRegistrationDelay = base::Seconds(10);

sync_pb::LocalTrustedVault ReadEncryptedFile(const base::FilePath& file_path) {
  sync_pb::LocalTrustedVault proto;
  std::string ciphertext;
  std::string decrypted_content;
  if (!base::ReadFileToString(file_path, &ciphertext)) {
    return proto;
  }

  const bool decryption_success =
      OSCrypt::DecryptString(ciphertext, &decrypted_content);
  base::UmaHistogramBoolean("Sync.TrustedVaultLocalDataDecryptionIsSuccessful",
                            decryption_success);
  if (decryption_success) {
    proto.ParseFromString(decrypted_content);
  }

  return proto;
}

void WriteToDisk(const sync_pb::LocalTrustedVault& data,
                 const base::FilePath& file_path) {
  std::string encrypted_data;
  const bool encryption_success =
      OSCrypt::EncryptString(data.SerializeAsString(), &encrypted_data);
  base::UmaHistogramBoolean("Sync.TrustedVaultLocalDataEncryptionIsSuccessful",
                            encryption_success);
  if (!encryption_success) {
    DLOG(ERROR) << "Failed to encrypt trusted vault file.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path,
                                                      encrypted_data)) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
}

bool HasNonConstantKey(
    const sync_pb::LocalTrustedVaultPerUser& per_user_vault) {
  std::string constant_key_as_proto_string;
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           &constant_key_as_proto_string);
  for (const sync_pb::LocalTrustedVaultKey& key : per_user_vault.vault_key()) {
    if (key.key_material() != constant_key_as_proto_string) {
      return true;
    }
  }
  return false;
}

std::vector<std::vector<uint8_t>> GetAllVaultKeys(
    const sync_pb::LocalTrustedVaultPerUser& per_user_vault) {
  std::vector<std::vector<uint8_t>> vault_keys;
  for (const sync_pb::LocalTrustedVaultKey& key : per_user_vault.vault_key()) {
    vault_keys.emplace_back(ProtoStringToBytes(key.key_material()));
  }
  return vault_keys;
}

void DownloadIsRecoverabilityDegradedCompleted(
    base::OnceCallback<void(bool)> cb,
    TrustedVaultRecoverabilityStatus status) {
  std::move(cb).Run(status == TrustedVaultRecoverabilityStatus::kDegraded);
}

base::flat_set<std::string> GetGaiaIDs(
    const std::vector<gaia::ListedAccount>& listed_accounts) {
  base::flat_set<std::string> result;
  for (const gaia::ListedAccount& listed_account : listed_accounts) {
    result.insert(listed_account.gaia_id);
  }
  return result;
}

// Version 0 may contain corrupted data: missing constant key if the client
// was affected by crbug.com/1267391, this function injects constant key if it's
// not stored and there is exactly one non-constant key. |local_trusted_vault|
// must not be null and must have |version| set to 0.
void UpgradeToVersion1(sync_pb::LocalTrustedVault* local_trusted_vault) {
  DCHECK(local_trusted_vault);
  DCHECK_EQ(local_trusted_vault->data_version(), 0);

  std::string constant_key_as_proto_string;
  AssignBytesToProtoString(GetConstantTrustedVaultKey(),
                           &constant_key_as_proto_string);

  for (sync_pb::LocalTrustedVaultPerUser& per_user_vault :
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

void RecordVerifyRegistrationStatus(
    TrustedVaultDownloadKeysStatusForUMA status) {
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultVerifyDeviceRegistrationState", status);
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
    case TrustedVaultDownloadKeysStatus::kOtherError:
      return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
  }

  NOTREACHED();
  return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
}

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
    const base::FilePath& file_path,
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<TrustedVaultConnection> connection)
    : file_path_(file_path),
      delegate_(std::move(delegate)),
      connection_(std::move(connection)),
      clock_(base::DefaultClock::GetInstance()) {}

StandaloneTrustedVaultBackend::~StandaloneTrustedVaultBackend() = default;

void StandaloneTrustedVaultBackend::WriteDegradedRecoverabilityState(
    const sync_pb::LocalTrustedVaultDegradedRecoverabilityState&
        degraded_recoverability_state) {
  DCHECK(primary_account_.has_value());
  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  *per_user_vault->mutable_degraded_recoverability_state() =
      degraded_recoverability_state;
  WriteToDisk(data_, file_path_);
}

void StandaloneTrustedVaultBackend::OnDegradedRecoverabilityChanged(
    bool value) {
  // TODO(crbug.com/1247990): To be implemented.
  NOTIMPLEMENTED();
}

void StandaloneTrustedVaultBackend::ReadDataFromDisk() {
  data_ = ReadEncryptedFile(file_path_);
  if (data_.user_size() == 0) {
    // No data, set the current version and omit writing the file.
    data_.set_data_version(kCurrentLocalTrustedVaultVersion);
  }

  if (data_.data_version() == 0) {
    UpgradeToVersion1(&data_);
    WriteToDisk(data_, file_path_);
  }

  DCHECK_EQ(data_.data_version(), kCurrentLocalTrustedVaultVersion);
}

void StandaloneTrustedVaultBackend::FetchKeys(
    const CoreAccountInfo& account_info,
    FetchKeysCallback callback) {
  // Concurrent keys fetches aren't supported.
  DCHECK(ongoing_fetch_keys_callback_.is_null());
  DCHECK(!callback.is_null());

  ongoing_fetch_keys_callback_ = std::move(callback);
  ongoing_fetch_keys_gaia_id_ = account_info.gaia;

  const sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);

  if (per_user_vault && HasNonConstantKey(*per_user_vault) &&
      !per_user_vault->keys_are_stale()) {
    // There are locally available keys, which weren't marked as stale. Keys
    // download attempt is not needed.
    FulfillOngoingFetchKeys(/*status_for_uma=*/absl::nullopt);
    return;
  }
  if (!connection_) {
    // Feature disabled.
    FulfillOngoingFetchKeys(/*status_for_uma=*/absl::nullopt);
    return;
  }
  // TODO(crbug.com/1094326): currently there is no guarantee that
  // |primary_account_| is set before FetchKeys() call and this may cause
  // redundant sync error in the UI (for key retrieval), especially during the
  // browser startup. Try to find a way to avoid this issue.
  if (!primary_account_.has_value() ||
      primary_account_->gaia != account_info.gaia) {
    // Keys download attempt is not possible because there is no primary
    // account.
    FulfillOngoingFetchKeys(
        TrustedVaultDownloadKeysStatusForUMA::kNoPrimaryAccount);
    return;
  }
  DCHECK(per_user_vault);
  if (!per_user_vault->local_device_registration_info().device_registered()) {
    // Keys download attempt is not possible because the device is not
    // registered.
    FulfillOngoingFetchKeys(
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered);
    return;
  }
  if (AreConnectionRequestsThrottled()) {
    // Keys download attempt is not possible.
    FulfillOngoingFetchKeys(
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide);
    return;
  }

  // Current state guarantees there is no ongoing requests to the server:
  // 1. Current |primary_account_| is |account_info|, so there is no ongoing
  // request for other accounts.
  // 2. Device is already registered, so there is no device registration for
  // |account_info|.
  // 3. Concurrent FetchKeys() calls aren't supported, so there is no keys
  // download for |account_info|.
  DCHECK(!ongoing_connection_request_);

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          ProtoStringToBytes(per_user_vault->local_device_registration_info()
                                 .private_key_material()));
  if (!key_pair) {
    // Corrupted state: device is registered, but |key_pair| can't be imported.
    // TODO(crbug.com/1094326): restore from this state (throw away the key and
    // trigger device registration again).
    FulfillOngoingFetchKeys(TrustedVaultDownloadKeysStatusForUMA::
                                kCorruptedLocalDeviceRegistration);
    return;
  }

  // Guaranteed by |device_registered| check above.
  DCHECK(!per_user_vault->vault_key().empty());
  // |this| outlives |connection_| and |ongoing_connection_request_|, so it's
  // safe to use base::Unretained() here.
  ongoing_connection_request_ = connection_->DownloadNewKeys(
      *primary_account_,
      TrustedVaultKeyAndVersion(
          ProtoStringToBytes(
              per_user_vault->vault_key().rbegin()->key_material()),
          per_user_vault->last_vault_key_version()),
      std::move(key_pair),
      base::BindOnce(&StandaloneTrustedVaultBackend::OnKeysDownloaded,
                     base::Unretained(this)));
  DCHECK(ongoing_connection_request_);
}

void StandaloneTrustedVaultBackend::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Find or create user for |gaid_id|.
  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  if (!per_user_vault) {
    per_user_vault = data_.add_user();
    per_user_vault->set_gaia_id(gaia_id);
  }

  // Replace all keys.
  per_user_vault->set_last_vault_key_version(last_key_version);
  per_user_vault->set_keys_are_stale(false);
  per_user_vault->clear_vault_key();
  for (const std::vector<uint8_t>& key : keys) {
    AssignBytesToProtoString(
        key, per_user_vault->add_vault_key()->mutable_key_material());
  }

  WriteToDisk(data_, file_path_);
  // This codepath doesn't record Sync.TrustedVaultDeviceRegistrationState, so
  // it's safe to pass any value for |has_persistent_auth_error_for_uma|.
  MaybeRegisterDevice(
      /*has_persistent_auth_error_for_uma=*/false);
}

void StandaloneTrustedVaultBackend::SetPrimaryAccount(
    const absl::optional<CoreAccountInfo>& primary_account,
    bool has_persistent_auth_error) {
  if (primary_account == primary_account_) {
    // Still need to complete deferred deletion, e.g. if primary account was
    // cleared before browser shutdown but not handled here.
    RemoveNonPrimaryAccountKeysIfMarkedForDeletion();
    return;
  }
  primary_account_ = primary_account;
  AbandonConnectionRequest();
  ongoing_get_recoverability_request_.reset();
  ongoing_add_recovery_method_request_.reset();
  RemoveNonPrimaryAccountKeysIfMarkedForDeletion();
  if (!primary_account_.has_value()) {
    DCHECK(!pending_trusted_recovery_method_.has_value());
    return;
  }

  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account->gaia);
  if (!per_user_vault) {
    per_user_vault = data_.add_user();
    per_user_vault->set_gaia_id(primary_account->gaia);
  }

  const absl::optional<TrustedVaultDeviceRegistrationStateForUMA>
      registration_state = MaybeRegisterDevice(has_persistent_auth_error);

  if (registration_state.has_value() &&
      !device_registration_state_recorded_to_uma_) {
    device_registration_state_recorded_to_uma_ = true;
    base::UmaHistogramBoolean(
        "Sync.TrustedVaultDeviceRegistered",
        per_user_vault->local_device_registration_info().device_registered());
    RecordTrustedVaultDeviceRegistrationState(*registration_state);

    // If the local state indicates that the device is already registered and
    // there is no ongoing re-registration attempt, and behind a feature toggle,
    // trigger a procedure to verify that the server has a consistent state
    // (i.e. downloading of new keys should succeed but return no new keys).
    if ((*registration_state ==
             TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV0 ||
         *registration_state ==
             TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1) &&
        base::FeatureList::IsEnabled(
            kSyncTrustedVaultVerifyDeviceRegistration)) {
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &StandaloneTrustedVaultBackend::VerifyDeviceRegistrationForUMA,
              base::WrapRefCounted(this), primary_account->gaia),
          kVerifyDeviceRegistrationDelay);
    }
  }

  if (pending_trusted_recovery_method_.has_value()) {
    PendingTrustedRecoveryMethod recovery_method =
        std::move(*pending_trusted_recovery_method_);
    pending_trusted_recovery_method_.reset();

    AddTrustedRecoveryMethod(recovery_method.gaia_id,
                             recovery_method.public_key,
                             recovery_method.method_type_hint,
                             std::move(recovery_method.completion_callback));
  }
}

void StandaloneTrustedVaultBackend::UpdateAccountsInCookieJarInfo(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  const base::flat_set<std::string> gaia_ids_in_cookie_jar =
      base::STLSetUnion<base::flat_set<std::string>>(
          GetGaiaIDs(accounts_in_cookie_jar_info.signed_in_accounts),
          GetGaiaIDs(accounts_in_cookie_jar_info.signed_out_accounts));

  // Primary account data shouldn't be removed immediately, but it needs to be
  // removed once account become non-primary if it was ever removed from cookie
  // jar.
  if (primary_account_.has_value() &&
      !base::Contains(gaia_ids_in_cookie_jar, primary_account_->gaia)) {
    sync_pb::LocalTrustedVaultPerUser* primary_account_data_ =
        FindUserVault(primary_account_->gaia);
    primary_account_data_->set_should_delete_keys_when_non_primary(true);
  }

  auto should_remove_user_data =
      [&gaia_ids_in_cookie_jar, &primary_account = primary_account_](
          const sync_pb::LocalTrustedVaultPerUser& per_user_data) {
        const std::string& gaia_id = per_user_data.gaia_id();
        if (primary_account.has_value() && gaia_id == primary_account->gaia) {
          // Don't delete primary account data.
          return false;
        }
        // Delete data if account isn't in cookie jar.
        return !base::Contains(gaia_ids_in_cookie_jar, gaia_id);
      };

  data_.mutable_user()->erase(
      base::ranges::remove_if(*data_.mutable_user(), should_remove_user_data),
      data_.mutable_user()->end());
  WriteToDisk(data_, file_path_);
}

bool StandaloneTrustedVaultBackend::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info) {
  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);
  if (!per_user_vault || per_user_vault->keys_are_stale()) {
    // No keys available for |account_info| or they are already marked as stale.
    return false;
  }

  per_user_vault->set_keys_are_stale(true);
  WriteToDisk(data_, file_path_);
  return true;
}

void StandaloneTrustedVaultBackend::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  // TODO(crbug.com/1201659): Improve this logic properly and add test coverage,
  // including throttling and periodic polling.
  ongoing_get_recoverability_request_ =
      connection_->DownloadIsRecoverabilityDegraded(
          account_info,
          base::BindOnce(&DownloadIsRecoverabilityDegradedCompleted,
                         std::move(cb)));
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

  if (!primary_account_.has_value()) {
    // Defer until SetPrimaryAccount() gets called.
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

  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
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
          *primary_account_, GetAllVaultKeys(*per_user_vault),
          per_user_vault->last_vault_key_version(), *imported_public_key,
          AuthenticationFactorType::kUnspecified, method_type_hint,
          base::BindOnce(
              &StandaloneTrustedVaultBackend::OnTrustedRecoveryMethodAdded,
              base::Unretained(this), std::move(cb)));
}

void StandaloneTrustedVaultBackend::ClearDataForAccount(
    const CoreAccountInfo& account_info) {
  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);
  if (!per_user_vault) {
    return;
  }

  *per_user_vault = sync_pb::LocalTrustedVaultPerUser();
  per_user_vault->set_gaia_id(account_info.gaia);
  WriteToDisk(data_, file_path_);

  // This codepath invoked as part of sync reset. While sync reset can cause
  // resetting primary account, this is not the case for Chrome OS and Butter
  // mode. Trigger device registration attempt immediately as it can succeed in
  // these cases.
  MaybeRegisterDevice(/*has_persistent_auth_error_for_uma=*/false);
}

absl::optional<CoreAccountInfo>
StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting() const {
  return primary_account_;
}

sync_pb::LocalDeviceRegistrationInfo
StandaloneTrustedVaultBackend::GetDeviceRegistrationInfoForTesting(
    const std::string& gaia_id) {
  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  if (!per_user_vault) {
    return sync_pb::LocalDeviceRegistrationInfo();
  }
  return per_user_vault->local_device_registration_info();
}

std::vector<uint8_t>
StandaloneTrustedVaultBackend::GetLastAddedRecoveryMethodPublicKeyForTesting()
    const {
  return last_added_recovery_method_public_key_for_testing_;
}

void StandaloneTrustedVaultBackend::SetDeviceRegisteredVersionForTesting(
    const std::string& gaia_id,
    int version) {
  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  DCHECK(per_user_vault);
  per_user_vault->mutable_local_device_registration_info()
      ->set_device_registered_version(version);
  WriteToDisk(data_, file_path_);
}

void StandaloneTrustedVaultBackend::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

absl::optional<TrustedVaultDeviceRegistrationStateForUMA>
StandaloneTrustedVaultBackend::MaybeRegisterDevice(
    bool has_persistent_auth_error_for_uma) {
  // TODO(crbug.com/1102340): in case of transient failure this function is
  // likely to be not called until the browser restart; implement retry logic.
  if (!connection_) {
    // Feature disabled.
    return absl::nullopt;
  }

  if (!primary_account_.has_value()) {
    // Device registration is supported only for |primary_account_|.
    return absl::nullopt;
  }

  // |per_user_vault| must be created before calling this function.
  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);

  if (per_user_vault->vault_key().empty() &&
      !base::FeatureList::IsEnabled(
          kAllowSilentTrustedVaultDeviceRegistration)) {
    // Either vault key with known version should be available or registration
    // without it should be allowed through feature flag.
    return absl::nullopt;
  }

  if (per_user_vault->local_device_registration_info().device_registered() &&
      per_user_vault->local_device_registration_info()
              .device_registered_version() ==
          kCurrentDeviceRegistrationVersion) {
    static_assert(kCurrentDeviceRegistrationVersion == 1);
    return TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1;
  }

  if (per_user_vault->local_device_registration_info().device_registered() &&
      !base::FeatureList::IsEnabled(kSyncTrustedVaultRedoDeviceRegistration)) {
    return TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV0;
  }

  if (per_user_vault->keys_are_stale()) {
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
    WriteToDisk(data_, file_path_);
  }

  // Cancel existing callbacks passed to |connection_| to ensure there is only
  // one ongoing request.
  AbandonConnectionRequest();
  // |this| outlives |connection_| and |ongoing_connection_request_|, so it's
  // safe to use base::Unretained() here.
  if (per_user_vault->vault_key().empty()) {
    ongoing_connection_request_ = connection_->RegisterDeviceWithoutKeys(
        *primary_account_, key_pair->public_key(),
        base::BindOnce(
            &StandaloneTrustedVaultBackend::OnDeviceRegisteredWithoutKeys,
            base::Unretained(this)));
  } else {
    ongoing_connection_request_ = connection_->RegisterAuthenticationFactor(
        *primary_account_, GetAllVaultKeys(*per_user_vault),
        per_user_vault->last_vault_key_version(), key_pair->public_key(),
        AuthenticationFactorType::kPhysicalDevice,
        /*authentication_factor_type_hint=*/absl::nullopt,
        base::BindOnce(&StandaloneTrustedVaultBackend::OnDeviceRegistered,
                       base::Unretained(this)));
  }

  DCHECK(ongoing_connection_request_);
  if (has_persistent_auth_error_for_uma) {
    return TrustedVaultDeviceRegistrationStateForUMA::
        kAttemptingRegistrationWithPersistentAuthError;
  }

  return had_generated_key_pair ? TrustedVaultDeviceRegistrationStateForUMA::
                                      kAttemptingRegistrationWithExistingKeyPair
                                : TrustedVaultDeviceRegistrationStateForUMA::
                                      kAttemptingRegistrationWithNewKeyPair;
}

void StandaloneTrustedVaultBackend::OnDeviceRegistered(
    TrustedVaultRegistrationStatus status) {
  // If |primary_account_| was changed meanwhile, this callback must be
  // cancelled.
  DCHECK(primary_account_.has_value());

  // This method should be called only as a result of
  // |ongoing_connection_request_| completion/failure, verify this condition
  // and destroy |ongoing_connection_request_| as it's not needed anymore.
  DCHECK(ongoing_connection_request_);
  ongoing_connection_request_ = nullptr;

  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);

  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      // kAlreadyRegistered handled as success, because it only means that
      // client doesn't fully handled successful device registration before.
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(true);
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered_version(kCurrentDeviceRegistrationVersion);
      WriteToDisk(data_, file_path_);
      return;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      per_user_vault->set_keys_are_stale(true);
      return;
    case TrustedVaultRegistrationStatus::kAccessTokenFetchingFailure:
      // Request wasn't sent to the server, so there is no need for throttling.
      return;
    case TrustedVaultRegistrationStatus::kOtherError:
      RecordFailedConnectionRequestForThrottling();
      return;
  }
}

void StandaloneTrustedVaultBackend::OnDeviceRegisteredWithoutKeys(
    TrustedVaultRegistrationStatus status,
    const TrustedVaultKeyAndVersion& vault_key_and_version) {
  // If |primary_account_| was changed meanwhile, this callback must be
  // cancelled.
  DCHECK(primary_account_.has_value());

  // This method should be called only as a result of
  // |ongoing_connection_request_| completion/failure, verify this condition,
  // |ongoing_connection_request_| will be destroyed later by
  // OnDeviceRegistered() call.
  DCHECK(ongoing_connection_request_);

  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
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
      // while no local keys available. Detected server-side key should be
      // stored upon successful completion (or if device was already registered,
      // e.g. previous response wasn't handled properly), but |vault_key|
      // emptiness still needs to be checked before that - there might be
      // StoreKeys() call during handling the request.
      if (per_user_vault->vault_key().empty()) {
        AssignBytesToProtoString(
            vault_key_and_version.key,
            per_user_vault->add_vault_key()->mutable_key_material());
        per_user_vault->set_last_vault_key_version(
            vault_key_and_version.version);
        // WriteToDisk() will be called by OnDeviceRegistered().
      }
      break;
    case TrustedVaultRegistrationStatus::kAccessTokenFetchingFailure:
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
    case TrustedVaultRegistrationStatus::kOtherError:
      break;
  }
  OnDeviceRegistered(status);
}

void StandaloneTrustedVaultBackend::OnKeysDownloaded(
    TrustedVaultDownloadKeysStatus status,
    const std::vector<std::vector<uint8_t>>& new_vault_keys,
    int last_vault_key_version) {
  DCHECK(primary_account_.has_value());
  DCHECK(!ongoing_fetch_keys_callback_.is_null());
  DCHECK_EQ(*ongoing_fetch_keys_gaia_id_, primary_account_->gaia);

  // This method should be called only as a result of
  // |ongoing_connection_request_| completion/failure, verify this condition
  // and destroy |ongoing_connection_request_| as it's not needed anymore.
  DCHECK(ongoing_connection_request_);
  ongoing_connection_request_ = nullptr;

  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(primary_account_->gaia);
  DCHECK(per_user_vault);
  switch (status) {
    case TrustedVaultDownloadKeysStatus::kSuccess: {
      // Store all vault keys (including already known) as they required for
      // adding recovery method and might still be useful for decryption (e.g.
      // key rotation wasn't complete).
      std::vector<std::vector<uint8_t>> vault_keys =
          GetAllVaultKeys(*per_user_vault);
      base::ranges::copy(new_vault_keys, std::back_inserter(vault_keys));
      StoreKeys(primary_account_->gaia, vault_keys, last_vault_key_version);
      break;
    }
    case TrustedVaultDownloadKeysStatus::kMemberNotFound:
    case TrustedVaultDownloadKeysStatus::kMembershipNotFound:
    case TrustedVaultDownloadKeysStatus::kMembershipCorrupted:
    case TrustedVaultDownloadKeysStatus::kMembershipEmpty:
    case TrustedVaultDownloadKeysStatus::kNoNewKeys:
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
      WriteToDisk(data_, file_path_);
      break;
    }
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
      // Request wasn't sent to the server, so there is no need for throttling.
      break;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      RecordFailedConnectionRequestForThrottling();
      break;
  }

  // In all cases the ongoing fetch keys request should be fulfilled.
  FulfillOngoingFetchKeys(GetDownloadKeysStatusForUMAFromResponse(status));
}

void StandaloneTrustedVaultBackend::OnTrustedRecoveryMethodAdded(
    base::OnceClosure cb,
    TrustedVaultRegistrationStatus status) {
  DCHECK(ongoing_add_recovery_method_request_);
  ongoing_add_recovery_method_request_ = nullptr;

  std::move(cb).Run();
  delegate_->NotifyRecoverabilityDegradedChanged();
}

void StandaloneTrustedVaultBackend::AbandonConnectionRequest() {
  ongoing_connection_request_ = nullptr;
  FulfillOngoingFetchKeys(TrustedVaultDownloadKeysStatusForUMA::kAborted);
}

void StandaloneTrustedVaultBackend::FulfillOngoingFetchKeys(
    absl::optional<TrustedVaultDownloadKeysStatusForUMA> status_for_uma) {
  if (!ongoing_fetch_keys_gaia_id_.has_value()) {
    return;
  }
  DCHECK(!ongoing_fetch_keys_callback_.is_null());

  if (status_for_uma.has_value()) {
    RecordTrustedVaultDownloadKeysStatus(*status_for_uma);
  }

  const sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(*ongoing_fetch_keys_gaia_id_);
  std::vector<std::vector<uint8_t>> vault_keys;
  if (per_user_vault) {
    vault_keys = GetAllVaultKeys(*per_user_vault);
    base::EraseIf(vault_keys, [](const std::vector<uint8_t>& key) {
      return key == GetConstantTrustedVaultKey();
    });
  }

  std::move(ongoing_fetch_keys_callback_).Run(vault_keys);
  ongoing_fetch_keys_callback_.Reset();
  ongoing_fetch_keys_gaia_id_.reset();
}

bool StandaloneTrustedVaultBackend::AreConnectionRequestsThrottled() {
  DCHECK(clock_);
  DCHECK(primary_account_.has_value());

  sync_pb::LocalTrustedVaultPerUser* per_user_vault =
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

  return last_failed_request_time +
             kTrustedVaultServiceThrottlingDuration.Get() >
         current_time;
}

void StandaloneTrustedVaultBackend::
    RecordFailedConnectionRequestForThrottling() {
  DCHECK(clock_);
  DCHECK(primary_account_.has_value());

  FindUserVault(primary_account_->gaia)
      ->set_last_failed_request_millis_since_unix_epoch(
          TimeToProtoTime(clock_->Now()));
  WriteToDisk(data_, file_path_);
}

void StandaloneTrustedVaultBackend::
    RemoveNonPrimaryAccountKeysIfMarkedForDeletion() {
  auto should_remove_user_data =
      [&primary_account = primary_account_](
          const sync_pb::LocalTrustedVaultPerUser& per_user_data) {
        return per_user_data.should_delete_keys_when_non_primary() &&
               (!primary_account.has_value() ||
                primary_account->gaia != per_user_data.gaia_id());
      };

  data_.mutable_user()->erase(
      base::ranges::remove_if(*data_.mutable_user(), should_remove_user_data),
      data_.mutable_user()->end());
  WriteToDisk(data_, file_path_);
}

sync_pb::LocalTrustedVaultPerUser* StandaloneTrustedVaultBackend::FindUserVault(
    const std::string& gaia_id) {
  for (int i = 0; i < data_.user_size(); ++i) {
    if (data_.user(i).gaia_id() == gaia_id) {
      return data_.mutable_user(i);
    }
  }
  return nullptr;
}

void StandaloneTrustedVaultBackend::VerifyDeviceRegistrationForUMA(
    const std::string& gaia_id) {
  const sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(gaia_id);

  // Ignore call if things have changed since the task was scheduled, although
  // in normal circumstances it shouldn't happen.
  if (!connection_ || !primary_account_.has_value() ||
      primary_account_->gaia != gaia_id || !per_user_vault ||
      !per_user_vault->local_device_registration_info().device_registered()) {
    return;
  }

  if (AreConnectionRequestsThrottled()) {
    // Keys download attempt is not possible.
    RecordVerifyRegistrationStatus(
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide);
    return;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          ProtoStringToBytes(per_user_vault->local_device_registration_info()
                                 .private_key_material()));
  if (!key_pair) {
    RecordVerifyRegistrationStatus(TrustedVaultDownloadKeysStatusForUMA::
                                       kCorruptedLocalDeviceRegistration);
    return;
  }

  // Guaranteed by |device_registered| check above.
  DCHECK(!per_user_vault->vault_key().empty());

  ongoing_verify_registration_request_ = connection_->DownloadNewKeys(
      *primary_account_,
      TrustedVaultKeyAndVersion(
          ProtoStringToBytes(
              per_user_vault->vault_key().rbegin()->key_material()),
          per_user_vault->last_vault_key_version()),
      std::move(key_pair),
      base::BindOnce([](TrustedVaultDownloadKeysStatus status,
                        const std::vector<std::vector<uint8_t>>& new_vault_keys,
                        int last_vault_key_version) {
        RecordVerifyRegistrationStatus(
            GetDownloadKeysStatusForUMAFromResponse(status));
      }));
}

}  // namespace syncer
