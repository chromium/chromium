// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/os_crypt/os_crypt.h"
#include "components/sync/base/time.h"
#include "components/sync/trusted_vault/proto_string_bytes_conversion.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_switches.h"

namespace syncer {

namespace {

sync_pb::LocalTrustedVault ReadEncryptedFile(const base::FilePath& file_path) {
  sync_pb::LocalTrustedVault proto;
  std::string ciphertext;
  std::string decrypted_content;
  if (base::ReadFileToString(file_path, &ciphertext) &&
      OSCrypt::DecryptString(ciphertext, &decrypted_content)) {
    proto.ParseFromString(decrypted_content);
  }

  return proto;
}

void WriteToDisk(const sync_pb::LocalTrustedVault& data,
                 const base::FilePath& file_path) {
  std::string encrypted_data;
  if (!OSCrypt::EncryptString(data.SerializeAsString(), &encrypted_data)) {
    DLOG(ERROR) << "Failed to encrypt trusted vault file.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path,
                                                      encrypted_data)) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
}

absl::optional<TrustedVaultKeyAndVersion> GetLastTrustedVaultKeyAndVersion(
    const sync_pb::LocalTrustedVaultPerUser& per_user_vault) {
  if (per_user_vault.vault_key_size() != 0) {
    return TrustedVaultKeyAndVersion(
        ProtoStringToBytes(per_user_vault.vault_key().rbegin()->key_material()),
        per_user_vault.last_vault_key_version());
  }
  return absl::nullopt;
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

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
    const base::FilePath& file_path,
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<TrustedVaultConnection> connection)
    : file_path_(file_path),
      delegate_(std::move(delegate)),
      connection_(std::move(connection)),
      clock_(base::DefaultClock::GetInstance()) {}

StandaloneTrustedVaultBackend::~StandaloneTrustedVaultBackend() = default;

void StandaloneTrustedVaultBackend::ReadDataFromDisk() {
  data_ = ReadEncryptedFile(file_path_);
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

  // TODO(crbug.com/1094326): currently there is no guarantee that
  // |primary_account_| is set before FetchKeys() call and this may cause
  // redundant sync error in the UI (for key retrieval), especially during the
  // browser startup. Try to find a way to avoid this issue.
  if (!connection_ || !primary_account_ || !per_user_vault ||
      !per_user_vault->local_device_registration_info().device_registered() ||
      AreConnectionRequestsThrottled(account_info.gaia)) {
    // Keys download attempt is not possible.
    FulfillOngoingFetchKeys();
    return;
  }
  if (per_user_vault->vault_key_size() != 0 &&
      !per_user_vault->keys_are_stale()) {
    // There are locally available keys, which weren't marked as stale. Keys
    // download attempt is not needed.
    FulfillOngoingFetchKeys();
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
    FulfillOngoingFetchKeys();
    return;
  }

  // |this| outlives |connection_| and |ongoing_connection_request_|, so it's
  // safe to use base::Unretained() here.
  ongoing_connection_request_ = connection_->DownloadNewKeys(
      *primary_account_, GetLastTrustedVaultKeyAndVersion(*per_user_vault),
      std::move(key_pair),
      base::BindOnce(&StandaloneTrustedVaultBackend::OnKeysDownloaded,
                     base::Unretained(this), account_info.gaia));
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
  MaybeRegisterDevice(gaia_id);
}

void StandaloneTrustedVaultBackend::RemoveAllStoredKeys() {
  base::DeleteFile(file_path_);
  data_.Clear();
  AbandonConnectionRequest();
}

void StandaloneTrustedVaultBackend::SetPrimaryAccount(
    const absl::optional<CoreAccountInfo>& primary_account) {
  if (primary_account == primary_account_) {
    return;
  }
  primary_account_ = primary_account;
  AbandonConnectionRequest();
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
  MaybeRegisterDevice(primary_account_->gaia);

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

bool StandaloneTrustedVaultBackend::MarkKeysAsStale(
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
  // TODO(crbug.com/1081649): Implement logic.
  NOTIMPLEMENTED();
  std::move(cb).Run(is_recoverability_degraded_for_testing_);
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

  if (primary_account_->gaia == gaia_id) {
    // TODO(crbug.com/1081649): Implement logic.
    NOTIMPLEMENTED();
    last_added_recovery_method_public_key_for_testing_ = public_key;
    is_recoverability_degraded_for_testing_ = false;
    delegate_->NotifyRecoverabilityDegradedChanged();
  }

  std::move(cb).Run();
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

void StandaloneTrustedVaultBackend::SetRecoverabilityDegradedForTesting() {
  is_recoverability_degraded_for_testing_ = true;
  delegate_->NotifyRecoverabilityDegradedChanged();
}

std::vector<uint8_t>
StandaloneTrustedVaultBackend::GetLastAddedRecoveryMethodPublicKeyForTesting()
    const {
  return last_added_recovery_method_public_key_for_testing_;
}

void StandaloneTrustedVaultBackend::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void StandaloneTrustedVaultBackend::MaybeRegisterDevice(
    const std::string& gaia_id) {
  // TODO(crbug.com/1102340): in case of transient failure this function is
  // likely to be not called until the browser restart; implement retry logic.
  if (!connection_) {
    // Feature disabled.
    return;
  }
  if (!primary_account_.has_value() || primary_account_->gaia != gaia_id) {
    // Device registration is supported only for |primary_account_|.
    return;
  }

  // |per_user_vault| must be created before calling this function.
  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  DCHECK(per_user_vault);

  absl::optional<TrustedVaultKeyAndVersion> last_trusted_vault_key_and_version =
      GetLastTrustedVaultKeyAndVersion(*per_user_vault);
  if (!last_trusted_vault_key_and_version.has_value() &&
      !base::FeatureList::IsEnabled(
          switches::kAllowSilentTrustedVaultDeviceRegistration)) {
    // Either vault keys should be available or registration without them should
    // be allowed through feature flag.
    return;
  }
  if (per_user_vault->keys_are_stale()) {
    // Client already knows that existing vault keys (or their absence) isn't
    // sufficient for device registration. Fresh keys should be obtained first.
    return;
  }
  if (per_user_vault->local_device_registration_info().device_registered()) {
    // Device is already registered.
    return;
  }
  if (AreConnectionRequestsThrottled(gaia_id)) {
    return;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair;
  if (per_user_vault->has_local_device_registration_info()) {
    key_pair = SecureBoxKeyPair::CreateByPrivateKeyImport(
        /*private_key_bytes=*/ProtoStringToBytes(
            per_user_vault->local_device_registration_info()
                .private_key_material()));
    if (!key_pair) {
      // Device key is corrupted.
      // TODO(crbug.com/1102340): consider generation of new key in this case.
      return;
    }
  } else {
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
  ongoing_connection_request_ = connection_->RegisterAuthenticationFactor(
      *primary_account_, last_trusted_vault_key_and_version,
      key_pair->public_key(), AuthenticationFactorType::kPhysicalDevice,
      base::BindOnce(&StandaloneTrustedVaultBackend::OnDeviceRegistered,
                     base::Unretained(this), gaia_id));
  DCHECK(ongoing_connection_request_);
}

void StandaloneTrustedVaultBackend::OnDeviceRegistered(
    const std::string& gaia_id,
    TrustedVaultRegistrationStatus status) {
  // If |primary_account_| was changed meanwhile, this callback must be
  // cancelled.
  DCHECK(primary_account_ && primary_account_->gaia == gaia_id);

  // This method should be called only as a result of
  // |ongoing_connection_request_| completion/failure, verify this condition
  // and destroy |ongoing_connection_request_| as it's not needed anymore.
  DCHECK(ongoing_connection_request_);
  ongoing_connection_request_ = nullptr;

  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  DCHECK(per_user_vault);

  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(true);
      WriteToDisk(data_, file_path_);
      return;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      per_user_vault->set_keys_are_stale(true);
      return;
    case TrustedVaultRegistrationStatus::kOtherError:
      RecordFailedConnectionRequestForThrottling(gaia_id);
      return;
  }
}

void StandaloneTrustedVaultBackend::OnKeysDownloaded(
    const std::string& gaia_id,
    TrustedVaultDownloadKeysStatus status,
    const std::vector<std::vector<uint8_t>>& vault_keys,
    int last_vault_key_version) {
  DCHECK(primary_account_ && primary_account_->gaia == gaia_id);
  DCHECK(!ongoing_fetch_keys_callback_.is_null());
  DCHECK(ongoing_fetch_keys_gaia_id_ == gaia_id);

  // This method should be called only as a result of
  // |ongoing_connection_request_| completion/failure, verify this condition
  // and destroy |ongoing_connection_request_| as it's not needed anymore.
  DCHECK(ongoing_connection_request_);
  ongoing_connection_request_ = nullptr;

  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  DCHECK(per_user_vault);

  switch (status) {
    case TrustedVaultDownloadKeysStatus::kSuccess:
      // TODO(crbug.com/1102340): consider keeping old keys as well.
      StoreKeys(gaia_id, vault_keys, last_vault_key_version);
      break;
    case TrustedVaultDownloadKeysStatus::kMemberNotFoundOrCorrupted:
    case TrustedVaultDownloadKeysStatus::kNoNewKeys:
    case TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed: {
      sync_pb::LocalTrustedVaultPerUser* per_user_vault =
          FindUserVault(gaia_id);
      // Unable to download new keys due to known protocol errors. The only way
      // to go out of these states is to receive new vault keys through external
      // StoreKeys() call. It's safe to mark device as not registered regardless
      // of the cause (device registration will be triggered once new vault keys
      // are available).
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(false);
      WriteToDisk(data_, file_path_);
      break;
    }
    case TrustedVaultDownloadKeysStatus::kOtherError:
      RecordFailedConnectionRequestForThrottling(gaia_id);
      break;
  }
  // Regardless of the |status| ongoing fetch keys request should be fulfilled.
  FulfillOngoingFetchKeys();
}

void StandaloneTrustedVaultBackend::AbandonConnectionRequest() {
  ongoing_connection_request_ = nullptr;
  FulfillOngoingFetchKeys();
}

void StandaloneTrustedVaultBackend::FulfillOngoingFetchKeys() {
  if (!ongoing_fetch_keys_gaia_id_.has_value()) {
    return;
  }
  DCHECK(!ongoing_fetch_keys_callback_.is_null());

  const sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(*ongoing_fetch_keys_gaia_id_);

  std::vector<std::vector<uint8_t>> vault_keys;
  if (per_user_vault) {
    for (const sync_pb::LocalTrustedVaultKey& key :
         per_user_vault->vault_key()) {
      vault_keys.emplace_back(ProtoStringToBytes(key.key_material()));
    }
  }

  std::move(ongoing_fetch_keys_callback_).Run(vault_keys);
  ongoing_fetch_keys_callback_.Reset();
  ongoing_fetch_keys_gaia_id_.reset();
}

bool StandaloneTrustedVaultBackend::AreConnectionRequestsThrottled(
    const std::string& gaia_id) {
  DCHECK(clock_);

  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  if (!per_user_vault) {
    return false;
  }

  const base::Time current_time = clock_->Now();
  base::Time last_failed_request_time = ProtoTimeToTime(
      per_user_vault->last_failed_request_millis_since_unix_epoch());

  // Fix |last_failed_request_time| if it's set to the future.
  if (last_failed_request_time > current_time) {
    // Immediately unthrottle, but don't write new state to the file.
    last_failed_request_time = base::Time();
  }

  return last_failed_request_time +
             switches::kTrustedVaultServiceThrottlingDuration.Get() >
         current_time;
}

void StandaloneTrustedVaultBackend::RecordFailedConnectionRequestForThrottling(
    const std::string& gaia_id) {
  DCHECK(clock_);
  DCHECK(FindUserVault(gaia_id));

  FindUserVault(gaia_id)->set_last_failed_request_millis_since_unix_epoch(
      TimeToProtoTime(clock_->Now()));
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

}  // namespace syncer
