// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cryptohome_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_switches.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {

// This suffix is appended to cryptohome_id to get hash in stub implementation:
// stub_hash = "[cryptohome_id]-hash";
static const char kUserIdStubHashSuffix[] = "-hash";

// Timeout for TPM operations. On slow machines it should be larger, than
// default DBus timeout. TPM operations can take up to 80 seconds, so limit
// is 2 minutes.
const int kTpmDBusTimeoutMs = 2 * 60 * 1000;

// Values for the attestation server switch.
const char kAttestationServerDefault[] = "default";
const char kAttestationServerTest[] = "test";

static attestation::VerifiedAccessType GetVerifiedAccessType() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kAttestationServer);
  if (value.empty() || value == kAttestationServerDefault) {
    return attestation::DEFAULT_VA;
  }
  if (value == kAttestationServerTest) {
    return attestation::TEST_VA;
  }
  LOG(WARNING) << "Invalid Verified Access server value: " << value
               << ". Using default.";
  return attestation::DEFAULT_VA;
}

// The CryptohomeClient implementation.
class CryptohomeClientImpl : public CryptohomeClient {
 public:
  CryptohomeClientImpl() : weak_ptr_factory_(this) {}

  // CryptohomeClient override.
  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  // CryptohomeClient override.
  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  // CryptohomeClient override.
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  // CryptohomeClient override.
  void IsMounted(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeIsMounted);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void Unmount(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeUnmount);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void MigrateKeyEx(
      const cryptohome::AccountIdentifier& account,
      const cryptohome::AuthorizationRequest& auth_request,
      const cryptohome::MigrateKeyRequest& migrate_request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeMigrateKeyEx);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(account);
    writer.AppendProtoAsArrayOfBytes(auth_request);
    writer.AppendProtoAsArrayOfBytes(migrate_request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void RemoveEx(const cryptohome::AccountIdentifier& account,
                DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeRemoveEx);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(account);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void RenameCryptohome(
      const cryptohome::AccountIdentifier& id_from,
      const cryptohome::AccountIdentifier& id_to,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeRenameCryptohome;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface, method_name);

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id_from);
    writer.AppendProtoAsArrayOfBytes(id_to);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void GetAccountDiskUsage(
      const cryptohome::AccountIdentifier& account_id,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetAccountDiskUsage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(account_id);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void GetSystemSalt(
      DBusMethodCallback<std::vector<uint8_t>> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetSystemSalt);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnGetSystemSalt,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override,
  void GetSanitizedUsername(const cryptohome::AccountIdentifier& id,
                            DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetSanitizedUsername);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(id.account_id());
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  std::string BlockingGetSanitizedUsername(
      const cryptohome::AccountIdentifier& id) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetSanitizedUsername);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(id.account_id());

    std::unique_ptr<dbus::Response> response =
        blocking_method_caller_->CallMethodAndBlock(&method_call);

    std::string sanitized_username;
    if (response) {
      dbus::MessageReader reader(response.get());
      reader.PopString(&sanitized_username);
    }

    return sanitized_username;
  }

  // CryptohomeClient override.
  void MountGuestEx(
      const cryptohome::MountGuestRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeMountGuestEx);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmIsReady(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsReady);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void TpmIsEnabled(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsEnabled);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141006
  bool CallTpmIsEnabledAndBlock(bool* enabled) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsEnabled);
    return CallBoolMethodAndBlock(&method_call, enabled);
  }

  // CryptohomeClient override.
  void TpmGetPassword(DBusMethodCallback<std::string> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmGetPassword);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnStringMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmIsOwned(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsOwned);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141012
  bool CallTpmIsOwnedAndBlock(bool* owned) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsOwned);
    return CallBoolMethodAndBlock(&method_call, owned);
  }

  // CryptohomeClient override.
  void TpmIsBeingOwned(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsBeingOwned);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141011
  bool CallTpmIsBeingOwnedAndBlock(bool* owning) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmIsBeingOwned);
    return CallBoolMethodAndBlock(&method_call, owning);
  }

  // CryptohomeClient override.
  void TpmCanAttemptOwnership(VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmCanAttemptOwnership);
    CallVoidMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient overrides.
  void TpmClearStoredPassword(VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmClearStoredPassword);
    CallVoidMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  // TODO(hashimoto): Remove this method. crbug.com/141010
  bool CallTpmClearStoredPasswordAndBlock() override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeTpmClearStoredPassword);
    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call));
    return response.get() != NULL;
  }

  // CryptohomeClient override.
  void Pkcs11IsTpmTokenReady(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomePkcs11IsTpmTokenReady);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void Pkcs11GetTpmTokenInfo(
      DBusMethodCallback<TpmTokenInfo> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomePkcs11GetTpmTokenInfo);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnPkcs11GetTpmTokenInfo,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void Pkcs11GetTpmTokenInfoForUser(
      const cryptohome::AccountIdentifier& id,
      DBusMethodCallback<TpmTokenInfo> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomePkcs11GetTpmTokenInfoForUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(id.account_id());
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnPkcs11GetTpmTokenInfo,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  bool InstallAttributesGet(const std::string& name,
                            std::vector<uint8_t>* value,
                            bool* successful) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeInstallAttributesGet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call));
    if (!response.get())
      return false;
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length) ||
        !reader.PopBool(successful))
      return false;
    value->assign(bytes, bytes + length);
    return true;
  }

  // CryptohomeClient override.
  bool InstallAttributesSet(const std::string& name,
                            const std::vector<uint8_t>& value,
                            bool* successful) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeInstallAttributesSet);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    writer.AppendArrayOfBytes(value.data(), value.size());
    return CallBoolMethodAndBlock(&method_call, successful);
  }

  // CryptohomeClient override.
  bool InstallAttributesFinalize(bool* successful) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesFinalize);
    return CallBoolMethodAndBlock(&method_call, successful);
  }

  // CryptohomeClient override.
  void InstallAttributesIsReady(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesIsReady);
    return CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  bool InstallAttributesIsInvalid(bool* is_invalid) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesIsInvalid);
    return CallBoolMethodAndBlock(&method_call, is_invalid);
  }

  // CryptohomeClient override.
  bool InstallAttributesIsFirstInstall(bool* is_first_install) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeInstallAttributesIsFirstInstall);
    return CallBoolMethodAndBlock(&method_call, is_first_install);
  }

  // CryptohomeClient override.
  void TpmAttestationIsPrepared(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmIsAttestationPrepared);
    return CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void TpmAttestationGetEnrollmentId(
      bool ignore_cache,
      DBusMethodCallback<TpmAttestationDataResult> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationGetEnrollmentId);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(ignore_cache);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnTpmAttestationDataMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationIsEnrolled(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmIsAttestationEnrolled);
    return CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void AsyncTpmAttestationCreateEnrollRequest(
      attestation::PrivacyCAType pca_type,
      AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationCreateEnrollRequest);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(pca_type);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void AsyncTpmAttestationEnroll(attestation::PrivacyCAType pca_type,
                                 const std::string& pca_response,
                                 AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationEnroll);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(pca_type);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(pca_response.data()),
        pca_response.size());
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void AsyncTpmAttestationCreateCertRequest(
      attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const cryptohome::AccountIdentifier& id,
      const std::string& request_origin,
      AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationCreateCertRequest);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(pca_type);
    writer.AppendInt32(certificate_profile);
    writer.AppendString(id.account_id());
    writer.AppendString(request_origin);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeAsyncTpmAttestationFinishCertRequest);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(pca_response.data()),
        pca_response.size());
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationDoesKeyExist(attestation::AttestationKeyType key_type,
                                  const cryptohome::AccountIdentifier& id,
                                  const std::string& key_name,
                                  DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationDoesKeyExist);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationGetCertificate);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnTpmAttestationDataMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationGetPublicKey);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnTpmAttestationDataMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationRegisterKey(attestation::AttestationKeyType key_type,
                                 const cryptohome::AccountIdentifier& id,
                                 const std::string& key_name,
                                 AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationRegisterKey);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationSignEnterpriseVaChallenge);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(GetVerifiedAccessType());
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    writer.AppendString(domain);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(device_id.data()), device_id.size());
    bool include_signed_public_key =
        (options & attestation::CHALLENGE_INCLUDE_SIGNED_PUBLIC_KEY);
    writer.AppendBool(include_signed_public_key);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(challenge.data()), challenge.size());
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      const std::string& challenge,
      AsyncMethodCallback callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationSignSimpleChallenge);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(challenge.data()), challenge.size());
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnAsyncMethodCall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationGetKeyPayload);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnTpmAttestationDataMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // CryptohomeClient override.
  void TpmAttestationSetKeyPayload(attestation::AttestationKeyType key_type,
                                   const cryptohome::AccountIdentifier& id,
                                   const std::string& key_name,
                                   const std::string& payload,
                                   DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationSetKeyPayload);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_name);
    writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(payload.data()),
                              payload.size());
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void TpmAttestationDeleteKeys(attestation::AttestationKeyType key_type,
                                const cryptohome::AccountIdentifier& id,
                                const std::string& key_prefix,
                                DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmAttestationDeleteKeys);
    dbus::MessageWriter writer(&method_call);
    bool is_user_specific = (key_type == attestation::KEY_USER);
    writer.AppendBool(is_user_specific);
    writer.AppendString(id.account_id());
    writer.AppendString(key_prefix);
    CallBoolMethod(&method_call, std::move(callback));
  }

  // CryptohomeClient override.
  void TpmGetVersion(DBusMethodCallback<TpmVersionInfo> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeTpmGetVersionStructured);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnTpmGetVersion,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetKeyDataEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::GetKeyDataRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetKeyDataEx);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CheckKeyEx(const cryptohome::AccountIdentifier& id,
                  const cryptohome::AuthorizationRequest& auth,
                  const cryptohome::CheckKeyRequest& request,
                  DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeCheckKeyEx;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void MountEx(const cryptohome::AccountIdentifier& id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeMountEx;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AddKeyEx(const cryptohome::AccountIdentifier& id,
                const cryptohome::AuthorizationRequest& auth,
                const cryptohome::AddKeyRequest& request,
                DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeAddKeyEx;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpdateKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::UpdateKeyRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeUpdateKeyEx;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RemoveKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::RemoveKeyRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeRemoveKeyEx;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface, method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeGetBootAttribute, request,
                         std::move(callback));
  }

  void SetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeSetBootAttribute, request,
                         std::move(callback));
  }

  void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeFlushAndSignBootAttributes,
                         request, std::move(callback));
  }

  void GetTpmStatus(
      const cryptohome::GetTpmStatusRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeGetTpmStatus, request,
                         std::move(callback));
  }

  void RemoveFirmwareManagementParametersFromTpm(
      const cryptohome::RemoveFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(
        cryptohome::kCryptohomeRemoveFirmwareManagementParameters, request,
        std::move(callback));
  }

  void SetFirmwareManagementParametersInTpm(
      const cryptohome::SetFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeSetFirmwareManagementParameters,
                         request, std::move(callback));
  }

  void MigrateToDircrypto(const cryptohome::AccountIdentifier& id,
                          const cryptohome::MigrateToDircryptoRequest& request,
                          VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeMigrateToDircrypto);

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(request);

    // The migration progress takes unpredicatable time depending on the
    // user file size and the number. Setting the time limit to infinite.
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void NeedsDircryptoMigration(const cryptohome::AccountIdentifier& id,
                               DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeNeedsDircryptoMigration);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnBoolMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSupportedKeyPolicies(
      const cryptohome::GetSupportedKeyPoliciesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeGetSupportedKeyPolicies,
                         request, std::move(callback));
  }

  void IsQuotaSupported(DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeIsQuotaSupported);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnBoolMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetCurrentSpaceForUid(const uid_t android_uid,
                             DBusMethodCallback<int64_t> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetCurrentSpaceForUid);

    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(android_uid);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnInt64DBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetCurrentSpaceForGid(const gid_t android_gid,
                             DBusMethodCallback<int64_t> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetCurrentSpaceForGid);

    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(android_gid);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnInt64DBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        cryptohome::kCryptohomeServiceName,
        dbus::ObjectPath(cryptohome::kCryptohomeServicePath));

    blocking_method_caller_.reset(new BlockingMethodCaller(bus, proxy_));

    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface, cryptohome::kSignalAsyncCallStatus,
        base::Bind(&CryptohomeClientImpl::AsyncCallStatusReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalAsyncCallStatusWithData,
        base::Bind(&CryptohomeClientImpl::AsyncCallStatusWithDataReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface, cryptohome::kSignalLowDiskSpace,
        base::Bind(&CryptohomeClientImpl::LowDiskSpaceReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalDircryptoMigrationProgress,
        base::Bind(&CryptohomeClientImpl::DircryptoMigrationProgressReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Handles the result of AsyncXXX methods.
  void OnAsyncMethodCall(AsyncMethodCallback callback,
                         dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    int async_id = 0;
    if (!reader.PopInt32(&async_id)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(async_id);
  }

  // Handles the result of GetSystemSalt().
  void OnGetSystemSalt(DBusMethodCallback<std::vector<uint8_t>> callback,
                       dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    const uint8_t* bytes = nullptr;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length)) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::vector<uint8_t>(bytes, bytes + length));
  }

  // Calls a method without result values.
  void CallVoidMethod(dbus::MethodCall* method_call,
                      VoidDBusMethodCallback callback) {
    proxy_->CallMethod(
        method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnVoidMethod(VoidDBusMethodCallback callback, dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  // Calls a method with a bool value reult and block.
  bool CallBoolMethodAndBlock(dbus::MethodCall* method_call,
                              bool* result) {
    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(method_call));
    if (!response.get())
      return false;
    dbus::MessageReader reader(response.get());
    return reader.PopBool(result);
  }

  // Calls a method with a bool value result.
  void CallBoolMethod(dbus::MethodCall* method_call,
                      DBusMethodCallback<bool> callback) {
    proxy_->CallMethod(
        method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBoolMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Handles responses for methods with a bool value result.
  void OnBoolMethod(DBusMethodCallback<bool> callback,
                    dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    bool result = false;
    if (!reader.PopBool(&result)) {
      LOG(ERROR) << "Invalid response: " << response->ToString();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(result);
  }

  void OnInt64DBusMethod(DBusMethodCallback<int64_t> callback,
                          dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    int64_t value = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt64(&value)) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(value);
  }

  // Handles responses for methods with a string value result.
  void OnStringMethod(DBusMethodCallback<std::string> callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string result;
    if (!reader.PopString(&result)) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(result));
  }

  // Handles responses for methods with a bool result and data.
  void OnTpmAttestationDataMethod(
      DBusMethodCallback<TpmAttestationDataResult> callback,
      dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    TpmAttestationDataResult result;
    const uint8_t* data_buffer = nullptr;
    size_t data_length = 0;
    if (!reader.PopArrayOfBytes(&data_buffer, &data_length) ||
        !reader.PopBool(&result.success)) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    result.data.assign(reinterpret_cast<const char*>(data_buffer), data_length);
    std::move(callback).Run(std::move(result));
  }

  // Handles responses for methods with a BaseReply protobuf method.
  void OnBaseReplyMethod(DBusMethodCallback<cryptohome::BaseReply> callback,
                         dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    cryptohome::BaseReply result;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&result)) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(result));
  }

  // Handles responses for Pkcs11GetTpmTokenInfo and
  // Pkcs11GetTpmTokenInfoForUser.
  void OnPkcs11GetTpmTokenInfo(DBusMethodCallback<TpmTokenInfo> callback,
                               dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    TpmTokenInfo token_info;
    if (!reader.PopString(&token_info.label) ||
        !reader.PopString(&token_info.user_pin) ||
        !reader.PopInt32(&token_info.slot)) {
      std::move(callback).Run(base::nullopt);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    std::move(callback).Run(std::move(token_info));
  }

  // Handles responses for TpmGetVersion.
  void OnTpmGetVersion(DBusMethodCallback<TpmVersionInfo> callback,
                       dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    TpmVersionInfo version;
    if (!reader.PopUint32(&version.family) ||
        !reader.PopUint64(&version.spec_level) ||
        !reader.PopUint32(&version.manufacturer) ||
        !reader.PopUint32(&version.tpm_model) ||
        !reader.PopUint64(&version.firmware_version) ||
        !reader.PopString(&version.vendor_specific)) {
      std::move(callback).Run(base::nullopt);
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    std::move(callback).Run(std::move(version));
  }

  // Handles AsyncCallStatus signal.
  void AsyncCallStatusReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int async_id = 0;
    bool return_status = false;
    int return_code = 0;
    if (!reader.PopInt32(&async_id) ||
        !reader.PopBool(&return_status) ||
        !reader.PopInt32(&return_code)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    for (auto& observer : observer_list_)
      observer.AsyncCallStatus(async_id, return_status, return_code);
  }

  // Handles AsyncCallStatusWithData signal.
  void AsyncCallStatusWithDataReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int async_id = 0;
    bool return_status = false;
    const uint8_t* return_data_buffer = NULL;
    size_t return_data_length = 0;
    if (!reader.PopInt32(&async_id) ||
        !reader.PopBool(&return_status) ||
        !reader.PopArrayOfBytes(&return_data_buffer, &return_data_length)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    std::string return_data(reinterpret_cast<const char*>(return_data_buffer),
                            return_data_length);
    for (auto& observer : observer_list_)
      observer.AsyncCallStatusWithData(async_id, return_status, return_data);
  }

  // Handles LowDiskSpace signal.
  void LowDiskSpaceReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t disk_free_bytes = 0;
    if (!reader.PopUint64(&disk_free_bytes)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    for (auto& observer : observer_list_)
      observer.LowDiskSpace(disk_free_bytes);
  }

  // Handles DircryptoMigrationProgess signal.
  void DircryptoMigrationProgressReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int status = 0;
    uint64_t current_bytes = 0, total_bytes = 0;
    if (!reader.PopInt32(&status) || !reader.PopUint64(&current_bytes) ||
        !reader.PopUint64(&total_bytes)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    for (auto& observer : observer_list_) {
      observer.DircryptoMigrationProgress(
          static_cast<cryptohome::DircryptoMigrationStatus>(status),
          current_bytes, total_bytes);
    }
  }

  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " " <<
        signal << " failed.";
  }

  // Makes an asynchronous D-Bus call, using cryptohome interface. |method_name|
  // is the name of the method to be called. |request| is the specific request
  // for the method, including the data required to be sent. |callback| is
  // invoked when the response is received.
  void CallCryptohomeMethod(
      const std::string& method_name,
      const google::protobuf::MessageLite& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface, method_name);

    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  dbus::ObjectProxy* proxy_ = nullptr;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::unique_ptr<BlockingMethodCaller> blocking_method_caller_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CryptohomeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CryptohomeClient

CryptohomeClient::CryptohomeClient() = default;

CryptohomeClient::~CryptohomeClient() = default;

// static
CryptohomeClient* CryptohomeClient::Create() {
  return new CryptohomeClientImpl();
}

// static
std::string CryptohomeClient::GetStubSanitizedUsername(
    const cryptohome::AccountIdentifier& id) {
  return id.account_id() + kUserIdStubHashSuffix;
}

}  // namespace chromeos
