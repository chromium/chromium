// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/attestation/attestation.pb.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/policy/proto/install_attributes.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Signature nonces are twenty bytes. This matches the attestation code.
constexpr char kTwentyBytesNonce[] = "+addtwentybytesnonce";
// A symbolic signature.
constexpr char kSignature[] = "signed";
// Interval to update the progress of MigrateToDircrypto in milliseconds.
constexpr int kDircryptoMigrationUpdateIntervalMs = 200;
// The number of updates the MigrateToDircrypto will send before it completes.
constexpr uint64_t kDircryptoMigrationMaxProgress = 15;
// Buffer size for reading install attributes file. 16k should be plenty. The
// file contains six attributes only (see InstallAttributes::LockDevice).
constexpr size_t kInstallAttributesFileMaxSize = 16384;

// Used to track the fake instance, mirrors the instance in the base class.
FakeCryptohomeClient* g_instance = nullptr;

}  // namespace

// static
constexpr char FakeCryptohomeClient::kStubTpmPassword[] = "Stub-TPM-password";

FakeCryptohomeClient::FakeCryptohomeClient() {
  DCHECK(!g_instance);
  g_instance = this;

  base::FilePath cache_path;
  locked_ = base::PathService::Get(dbus_paths::FILE_INSTALL_ATTRIBUTES,
                                   &cache_path) &&
            base::PathExists(cache_path);
  if (locked_)
    LoadInstallAttributes();
}

FakeCryptohomeClient::~FakeCryptohomeClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeCryptohomeClient* FakeCryptohomeClient::Get() {
  return g_instance;
}

void FakeCryptohomeClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCryptohomeClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeCryptohomeClient::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeCryptohomeClient::IsMounted(DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCryptohomeClient::UnmountEx(
    const cryptohome::UnmountRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::MigrateKeyEx(
    const cryptohome::AccountIdentifier& account,
    const cryptohome::AuthorizationRequest& auth_request,
    const cryptohome::MigrateKeyRequest& migrate_request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::RemoveEx(
    const cryptohome::AccountIdentifier& account,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::RenameCryptohome(
    const cryptohome::AccountIdentifier& cryptohome_id_from,
    const cryptohome::AccountIdentifier& cryptohome_id_to,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::GetAccountDiskUsage(
    const cryptohome::AccountIdentifier& account_id,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;
  cryptohome::GetAccountDiskUsageReply* get_account_disk_usage_reply =
      reply.MutableExtension(cryptohome::GetAccountDiskUsageReply::reply);
  // Sets 100 MB as a fake usage.
  get_account_disk_usage_reply->set_size(100 * 1024 * 1024);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::GetSystemSalt(
    DBusMethodCallback<std::vector<uint8_t>> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), system_salt_));
}

void FakeCryptohomeClient::GetSanitizedUsername(
    const cryptohome::AccountIdentifier& cryptohome_id,
    DBusMethodCallback<std::string> callback) {
  // Even for stub implementation we have to return different values so that
  // multi-profiles would work.
  auto id = service_is_available_
                ? base::make_optional(GetStubSanitizedUsername(cryptohome_id))
                : base::nullopt;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id));
}

std::string FakeCryptohomeClient::BlockingGetSanitizedUsername(
    const cryptohome::AccountIdentifier& cryptohome_id) {
  return service_is_available_ ? GetStubSanitizedUsername(cryptohome_id)
                               : std::string();
}

void FakeCryptohomeClient::MountGuestEx(
    const cryptohome::MountGuestRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::GetRsuDeviceId(
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;
  cryptohome::GetRsuDeviceIdReply* get_rsu_lookup_key_reply =
      reply.MutableExtension(cryptohome::GetRsuDeviceIdReply::reply);
  get_rsu_lookup_key_reply->set_rsu_device_id(rsu_device_id_);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::TpmIsReady(DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), tpm_is_ready_));
}

void FakeCryptohomeClient::TpmIsEnabled(DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), tpm_is_enabled_));
}

bool FakeCryptohomeClient::CallTpmIsEnabledAndBlock(bool* enabled) {
  *enabled = tpm_is_enabled_;
  return true;
}

void FakeCryptohomeClient::TpmGetPassword(
    DBusMethodCallback<std::string> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::string(kStubTpmPassword)));
}

void FakeCryptohomeClient::TpmIsOwned(DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool FakeCryptohomeClient::CallTpmIsOwnedAndBlock(bool* owned) {
  *owned = true;
  return true;
}

void FakeCryptohomeClient::TpmIsBeingOwned(DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool FakeCryptohomeClient::CallTpmIsBeingOwnedAndBlock(bool* owning) {
  *owning = true;
  return true;
}

void FakeCryptohomeClient::TpmCanAttemptOwnership(
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCryptohomeClient::TpmClearStoredPassword(
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool FakeCryptohomeClient::CallTpmClearStoredPasswordAndBlock() {
  return true;
}

void FakeCryptohomeClient::Pkcs11IsTpmTokenReady(
    DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCryptohomeClient::Pkcs11GetTpmTokenInfo(
    DBusMethodCallback<TpmTokenInfo> callback) {
  const char kStubTPMTokenName[] = "StubTPMTokenName";
  const char kStubUserPin[] = "012345";
  const int kStubSlot = 0;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     TpmTokenInfo{kStubTPMTokenName, kStubUserPin, kStubSlot}));
}

void FakeCryptohomeClient::Pkcs11GetTpmTokenInfoForUser(
    const cryptohome::AccountIdentifier& cryptohome_id,
    DBusMethodCallback<TpmTokenInfo> callback) {
  Pkcs11GetTpmTokenInfo(std::move(callback));
}

bool FakeCryptohomeClient::InstallAttributesGet(const std::string& name,
                                                std::vector<uint8_t>* value,
                                                bool* successful) {
  if (install_attrs_.find(name) != install_attrs_.end()) {
    *value = install_attrs_[name];
    *successful = true;
  } else {
    value->clear();
    *successful = false;
  }
  return true;
}

bool FakeCryptohomeClient::InstallAttributesSet(
    const std::string& name,
    const std::vector<uint8_t>& value,
    bool* successful) {
  install_attrs_[name] = value;
  *successful = true;
  return true;
}

bool FakeCryptohomeClient::InstallAttributesFinalize(bool* successful) {
  locked_ = true;
  *successful = true;

  // Persist the install attributes so that they can be reloaded if the
  // browser is restarted. This is used for ease of development when device
  // enrollment is required.
  base::FilePath cache_path;
  if (!base::PathService::Get(dbus_paths::FILE_INSTALL_ATTRIBUTES,
                              &cache_path)) {
    return false;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  for (const auto& it : install_attrs_) {
    const std::string& name = it.first;
    const std::vector<uint8_t>& value = it.second;
    cryptohome::SerializedInstallAttributes::Attribute* attr_entry =
        install_attrs_proto.add_attributes();
    attr_entry->set_name(name);
    attr_entry->mutable_value()->assign(value.data(),
                                        value.data() + value.size());
  }

  std::string result;
  install_attrs_proto.SerializeToString(&result);

  // The real implementation does a blocking wait on the dbus call; the fake
  // implementation must have this file written before returning.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::WriteFile(cache_path, result.data(), result.size());

  return true;
}

void FakeCryptohomeClient::InstallAttributesIsReady(
    DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool FakeCryptohomeClient::InstallAttributesIsInvalid(bool* is_invalid) {
  *is_invalid = false;
  return true;
}

bool FakeCryptohomeClient::InstallAttributesIsFirstInstall(
    bool* is_first_install) {
  *is_first_install = !locked_;
  return true;
}

void FakeCryptohomeClient::TpmAttestationIsPrepared(
    DBusMethodCallback<bool> callback) {
  auto result = service_is_available_
                    ? base::make_optional(tpm_attestation_is_prepared_)
                    : base::nullopt;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeCryptohomeClient::TpmAttestationGetEnrollmentId(
    bool ignore_cache,
    DBusMethodCallback<TpmAttestationDataResult> callback) {
  auto result =
      service_is_available_
          ? base::make_optional(TpmAttestationDataResult{
                true, ignore_cache ? tpm_attestation_enrollment_id_ignore_cache_
                                   : tpm_attestation_enrollment_id_})
          : base::nullopt;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeCryptohomeClient::TpmAttestationIsEnrolled(
    DBusMethodCallback<bool> callback) {
  auto result = service_is_available_
                    ? base::make_optional(tpm_attestation_is_enrolled_)
                    : base::nullopt;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeCryptohomeClient::AsyncTpmAttestationCreateEnrollRequest(
    chromeos::attestation::PrivacyCAType pca_type,
    AsyncMethodCallback callback) {
  ReturnAsyncMethodData(std::move(callback), std::string());
}

void FakeCryptohomeClient::AsyncTpmAttestationEnroll(
    chromeos::attestation::PrivacyCAType pca_type,
    const std::string& pca_response,
    AsyncMethodCallback callback) {
  ReturnAsyncMethodResult(std::move(callback));
}

void FakeCryptohomeClient::AsyncTpmAttestationCreateCertRequest(
    chromeos::attestation::PrivacyCAType pca_type,
    attestation::AttestationCertificateProfile certificate_profile,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& request_origin,
    AsyncMethodCallback callback) {
  ReturnAsyncMethodData(std::move(callback), std::string());
}

void FakeCryptohomeClient::AsyncTpmAttestationFinishCertRequest(
    const std::string& pca_response,
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    AsyncMethodCallback callback) {
  ReturnAsyncMethodData(std::move(callback), std::string());
}

void FakeCryptohomeClient::TpmAttestationDoesKeyExist(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    DBusMethodCallback<bool> callback) {
  if (!service_is_available_ ||
      !tpm_attestation_does_key_exist_should_succeed_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  bool result = false;
  switch (key_type) {
    case attestation::KEY_DEVICE:
      result = base::Contains(device_certificate_map_, key_name);
      break;
    case attestation::KEY_USER:
      result = base::Contains(user_certificate_map_,
                              std::make_pair(cryptohome_id, key_name));
      break;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeCryptohomeClient::TpmAttestationGetCertificate(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    DBusMethodCallback<TpmAttestationDataResult> callback) {
  TpmAttestationDataResult result;
  switch (key_type) {
    case attestation::KEY_DEVICE: {
      const auto it = device_certificate_map_.find(key_name);
      if (it != device_certificate_map_.end()) {
        result.success = true;
        result.data = it->second;
      }
      break;
    }
    case attestation::KEY_USER: {
      const auto it = user_certificate_map_.find({cryptohome_id, key_name});
      if (it != user_certificate_map_.end()) {
        result.success = true;
        result.data = it->second;
      }
      break;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void FakeCryptohomeClient::TpmAttestationGetPublicKey(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    DBusMethodCallback<TpmAttestationDataResult> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), TpmAttestationDataResult{}));
}

void FakeCryptohomeClient::TpmAttestationRegisterKey(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    AsyncMethodCallback callback) {
  ReturnAsyncMethodData(std::move(callback), std::string());
}

void FakeCryptohomeClient::TpmAttestationSignEnterpriseChallenge(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const std::string& key_name_for_spkac,
    AsyncMethodCallback callback) {
  ReturnAsyncMethodData(std::move(callback), std::string());
}

void FakeCryptohomeClient::TpmAttestationSignSimpleChallenge(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    const std::string& challenge,
    AsyncMethodCallback callback) {
  chromeos::attestation::SignedData signed_data;
  signed_data.set_data(challenge + kTwentyBytesNonce);
  signed_data.set_signature(kSignature);
  ReturnAsyncMethodData(std::move(callback), signed_data.SerializeAsString());
}

void FakeCryptohomeClient::TpmAttestationGetKeyPayload(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    DBusMethodCallback<TpmAttestationDataResult> callback) {
  TpmAttestationDataResult result;
  if (key_type == attestation::KEY_DEVICE) {
    const auto it = device_key_payload_map_.find(key_name);
    if (it != device_key_payload_map_.end()) {
      result.success = true;
      result.data = it->second;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void FakeCryptohomeClient::TpmAttestationSetKeyPayload(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    const std::string& payload,
    DBusMethodCallback<bool> callback) {
  bool result = false;
  // Currently only KEY_DEVICE case is supported just because there's no user
  // for KEY_USER.
  if (key_type == attestation::KEY_DEVICE) {
    device_key_payload_map_[key_name] = payload;
    result = true;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FakeCryptohomeClient::TpmAttestationDeleteKeys(
    attestation::AttestationKeyType key_type,
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_prefix,
    DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCryptohomeClient::TpmGetVersion(
    DBusMethodCallback<TpmVersionInfo> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), TpmVersionInfo()));
}

void FakeCryptohomeClient::GetKeyDataEx(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::GetKeyDataRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;
  const auto it = key_data_map_.find(cryptohome_id);
  if (it == key_data_map_.end()) {
    reply.set_error(cryptohome::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
  } else if (it->second.empty()) {
    reply.set_error(cryptohome::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  } else {
    auto key = FindKey(it->second, auth.key().data().label());
    if (key != it->second.end()) {
      cryptohome::GetKeyDataReply* key_data_reply =
          reply.MutableExtension(cryptohome::GetKeyDataReply::reply);
      *key_data_reply->add_key_data() = key->second.data();
    } else {
      reply.set_error(cryptohome::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    }
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::CheckKeyEx(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::CheckKeyRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;

  if (enable_auth_check_) {
    const auto it = key_data_map_.find(cryptohome_id);
    if (it == key_data_map_.end()) {
      reply.set_error(cryptohome::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    } else if (it->second.empty()) {
      reply.set_error(cryptohome::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    } else {
      auto key = FindKey(it->second, auth.key().data().label());
      if (key == it->second.end()) {
        reply.set_error(cryptohome::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
      } else if (key->second.secret() != auth.key().secret()) {
        reply.set_error(cryptohome::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
      }
    }
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::MountEx(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::MountRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::CryptohomeErrorCode error = cryptohome_error_;
  last_mount_request_ = request;
  last_mount_auth_request_ = auth;
  cryptohome::BaseReply reply;
  cryptohome::MountReply* mount =
      reply.MutableExtension(cryptohome::MountReply::reply);
  mount->set_sanitized_username(GetStubSanitizedUsername(cryptohome_id));
  if (!request.to_migrate_from_ecryptfs() &&
      request.force_dircrypto_if_available()) {
    error = cryptohome::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION;
  }
  if (mount_create_required_ && !request.has_create())
    error = cryptohome::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND;
  reply.set_error(error);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::LockToSingleUserMountUntilReboot(
    const cryptohome::LockToSingleUserMountUntilRebootRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;
  cryptohome::LockToSingleUserMountUntilRebootReply* mutable_reply =
      reply.MutableExtension(
          cryptohome::LockToSingleUserMountUntilRebootReply::reply);
  if (cryptohome_error_ == cryptohome::CRYPTOHOME_ERROR_NOT_SET) {
    mutable_reply->set_result(
        cryptohome::LockToSingleUserMountUntilRebootResult::SUCCESS);
    is_device_locked_to_single_user_ = true;
  } else {
    mutable_reply->set_result(
        cryptohome::LockToSingleUserMountUntilRebootResult::
            FAILED_TO_EXTEND_PCR);
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::AddKeyEx(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::AddKeyRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  key_data_map_[cryptohome_id][request.key().data().label()] = request.key();
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::AddDataRestoreKey(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::RemoveKeyEx(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::RemoveKeyRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  const auto it = key_data_map_.find(cryptohome_id);
  if (it != key_data_map_.end()) {
    auto key = FindKey(it->second, request.key().data().label());
    if (key != it->second.end())
      it->second.erase(key);
  }
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::UpdateKeyEx(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::UpdateKeyRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::MassRemoveKeys(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::MassRemoveKeysRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::GetBootAttribute(
    const cryptohome::GetBootAttributeRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;
  cryptohome::GetBootAttributeReply* attr_reply =
      reply.MutableExtension(cryptohome::GetBootAttributeReply::reply);
  attr_reply->set_value("");
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::SetBootAttribute(
    const cryptohome::SetBootAttributeRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::FlushAndSignBootAttributes(
    const cryptohome::FlushAndSignBootAttributesRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::GetTpmStatus(
    const cryptohome::GetTpmStatusRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::MigrateToDircrypto(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const cryptohome::MigrateToDircryptoRequest& request,
    VoidDBusMethodCallback callback) {
  id_for_disk_migrated_to_dircrypto_ = cryptohome_id;
  last_migrate_to_dircrypto_request_ = request;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
  dircrypto_migration_progress_ = 0;
  if (run_default_dircrypto_migration_) {
    dircrypto_migration_progress_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kDircryptoMigrationUpdateIntervalMs),
        this, &FakeCryptohomeClient::OnDircryptoMigrationProgressUpdated);
  }
}

void FakeCryptohomeClient::RemoveFirmwareManagementParametersFromTpm(
    const cryptohome::RemoveFirmwareManagementParametersRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  remove_firmware_management_parameters_from_tpm_call_count_++;
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::SetFirmwareManagementParametersInTpm(
    const cryptohome::SetFirmwareManagementParametersRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  ReturnProtobufMethodCallback(cryptohome::BaseReply(), std::move(callback));
}

void FakeCryptohomeClient::NeedsDircryptoMigration(
    const cryptohome::AccountIdentifier& cryptohome_id,
    DBusMethodCallback<bool> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), needs_dircrypto_migration_));
}

void FakeCryptohomeClient::GetSupportedKeyPolicies(
    const cryptohome::GetSupportedKeyPoliciesRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  cryptohome::BaseReply reply;
  cryptohome::GetSupportedKeyPoliciesReply* attr_reply =
      reply.MutableExtension(cryptohome::GetSupportedKeyPoliciesReply::reply);
  attr_reply->set_low_entropy_credentials(supports_low_entropy_credentials_);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeCryptohomeClient::IsQuotaSupported(DBusMethodCallback<bool> callback) {
}

void FakeCryptohomeClient::GetCurrentSpaceForUid(
    uid_t android_uid,
    DBusMethodCallback<int64_t> callback) {}

void FakeCryptohomeClient::GetCurrentSpaceForGid(
    gid_t android_gid,
    DBusMethodCallback<int64_t> callback) {}

void FakeCryptohomeClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(true);
}

void FakeCryptohomeClient::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(false);
}

void FakeCryptohomeClient::SetTpmAttestationUserCertificate(
    const cryptohome::AccountIdentifier& cryptohome_id,
    const std::string& key_name,
    const std::string& certificate) {
  user_certificate_map_[std::make_pair(cryptohome_id, key_name)] = certificate;
}

void FakeCryptohomeClient::SetTpmAttestationDeviceCertificate(
    const std::string& key_name,
    const std::string& certificate) {
  device_certificate_map_[key_name] = certificate;
}

void FakeCryptohomeClient::SetTpmAttestationDeviceKeyPayload(
    const std::string& key_name,
    const std::string& payload) {
  device_key_payload_map_[key_name] = payload;
}

base::Optional<std::string>
FakeCryptohomeClient::GetTpmAttestationDeviceKeyPayload(
    const std::string& key_name) const {
  const auto it = device_key_payload_map_.find(key_name);
  return it == device_key_payload_map_.end() ? base::nullopt
                                             : base::make_optional(it->second);
}

void FakeCryptohomeClient::NotifyLowDiskSpace(uint64_t disk_free_bytes) {
  for (auto& observer : observer_list_)
    observer.LowDiskSpace(disk_free_bytes);
}

// static
std::vector<uint8_t> FakeCryptohomeClient::GetStubSystemSalt() {
  const char kStubSystemSalt[] = "stub_system_salt";
  return std::vector<uint8_t>(
      kStubSystemSalt, kStubSystemSalt + base::size(kStubSystemSalt) - 1);
}

void FakeCryptohomeClient::ReturnProtobufMethodCallback(
    const cryptohome::BaseReply& reply,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

void FakeCryptohomeClient::ReturnAsyncMethodResult(
    AsyncMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeCryptohomeClient::ReturnAsyncMethodResultInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeCryptohomeClient::ReturnAsyncMethodData(AsyncMethodCallback callback,
                                                 const std::string& data) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeCryptohomeClient::ReturnAsyncMethodDataInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     data));
}

void FakeCryptohomeClient::ReturnAsyncMethodResultInternal(
    AsyncMethodCallback callback) {
  std::move(callback).Run(async_call_id_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeCryptohomeClient::NotifyAsyncCallStatus,
                                weak_ptr_factory_.GetWeakPtr(), async_call_id_,
                                true, cryptohome::MOUNT_ERROR_NONE));
  ++async_call_id_;
}

void FakeCryptohomeClient::ReturnAsyncMethodDataInternal(
    AsyncMethodCallback callback,
    const std::string& data) {
  std::move(callback).Run(async_call_id_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeCryptohomeClient::NotifyAsyncCallStatusWithData,
                     weak_ptr_factory_.GetWeakPtr(), async_call_id_, true,
                     data));
  ++async_call_id_;
}

void FakeCryptohomeClient::OnDircryptoMigrationProgressUpdated() {
  dircrypto_migration_progress_++;

  if (dircrypto_migration_progress_ >= kDircryptoMigrationMaxProgress) {
    NotifyDircryptoMigrationProgress(cryptohome::DIRCRYPTO_MIGRATION_SUCCESS,
                                     dircrypto_migration_progress_,
                                     kDircryptoMigrationMaxProgress);
    dircrypto_migration_progress_timer_.Stop();
    return;
  }
  NotifyDircryptoMigrationProgress(cryptohome::DIRCRYPTO_MIGRATION_IN_PROGRESS,
                                   dircrypto_migration_progress_,
                                   kDircryptoMigrationMaxProgress);
}

void FakeCryptohomeClient::NotifyAsyncCallStatus(int async_id,
                                                 bool return_status,
                                                 int return_code) {
  for (auto& observer : observer_list_)
    observer.AsyncCallStatus(async_id, return_status, return_code);
}

void FakeCryptohomeClient::NotifyAsyncCallStatusWithData(
    int async_id,
    bool return_status,
    const std::string& data) {
  for (auto& observer : observer_list_)
    observer.AsyncCallStatusWithData(async_id, return_status, data);
}

void FakeCryptohomeClient::NotifyTpmInitStatusUpdated(
    bool ready,
    bool owned,
    bool was_owned_this_boot) {
  for (auto& observer : observer_list_)
    observer.TpmInitStatusUpdated(ready, owned, was_owned_this_boot);
}

void FakeCryptohomeClient::NotifyDircryptoMigrationProgress(
    cryptohome::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  for (auto& observer : observer_list_)
    observer.DircryptoMigrationProgress(status, current, total);
}

bool FakeCryptohomeClient::LoadInstallAttributes() {
  base::FilePath cache_file;
  const bool file_exists =
      base::PathService::Get(dbus_paths::FILE_INSTALL_ATTRIBUTES,
                             &cache_file) &&
      base::PathExists(cache_file);
  DCHECK(file_exists);
  // Mostly copied from chrome/browser/chromeos/tpm/install_attributes.cc.
  std::string file_blob;
  if (!base::ReadFileToStringWithMaxSize(cache_file, &file_blob,
                                         kInstallAttributesFileMaxSize)) {
    PLOG(ERROR) << "Failed to read " << cache_file.value();
    return false;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  if (!install_attrs_proto.ParseFromString(file_blob)) {
    LOG(ERROR) << "Failed to parse install attributes cache.";
    return false;
  }

  for (const auto& entry : install_attrs_proto.attributes()) {
    install_attrs_[entry.name()].assign(
        entry.value().data(), entry.value().data() + entry.value().size());
  }

  return true;
}

std::map<std::string, cryptohome::Key>::const_iterator
FakeCryptohomeClient::FindKey(
    const std::map<std::string, cryptohome::Key>& keys,
    const std::string& label) {
  // Wildcard label.
  if (label.empty())
    return keys.begin();

  // Specific label
  return keys.find(label);
}

}  // namespace chromeos
