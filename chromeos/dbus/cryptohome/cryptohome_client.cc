// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cryptohome/cryptohome_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/blocking_method_caller.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
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

constexpr char kCryptohomeClientUmaPrefix[] = "CryptohomeClient.";

CryptohomeClient* g_instance = nullptr;

void UmaCallbackWraper(const std::string& metric_name,
                       const base::Time& start_time,
                       dbus::ObjectProxy::ResponseCallback callback,
                       dbus::Response* response) {
  UmaHistogramMediumTimes(metric_name, base::Time::Now() - start_time);
  std::move(callback).Run(response);
}

class DbusObjectProxyWithUma {
 public:
  explicit DbusObjectProxyWithUma(dbus::ObjectProxy* proxy) : proxy_(proxy) {}

  void CallMethod(dbus::MethodCall* method_call,
                  int timeout_ms,
                  dbus::ObjectProxy::ResponseCallback callback) {
    std::string metric_name =
        kCryptohomeClientUmaPrefix + method_call->GetMember();
    base::Time start_time = base::Time::Now();

    proxy_->CallMethod(method_call, timeout_ms,
                       base::BindOnce(&UmaCallbackWraper, metric_name,
                                      start_time, std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void ConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
    proxy_->ConnectToSignal(interface_name, signal_name,
                            std::move(signal_callback),
                            std::move(on_connected_callback));
  }

 private:
  dbus::ObjectProxy* proxy_;
};

// The CryptohomeClient implementation.
class CryptohomeClientImpl : public CryptohomeClient {
 public:
  CryptohomeClientImpl() {}

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
  void UnmountEx(const cryptohome::UnmountRequest& request,
                 DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeUnmountEx);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

    base::Time start_time = base::Time::Now();

    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call));

    UmaHistogramMediumTimes(
        kCryptohomeClientUmaPrefix + method_call.GetMember(),
        base::Time::Now() - start_time);

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

    base::Time start_time = base::Time::Now();

    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(&method_call));

    UmaHistogramMediumTimes(
        kCryptohomeClientUmaPrefix + method_call.GetMember(),
        base::Time::Now() - start_time);

    if (!response.get())
      return false;
    dbus::MessageReader reader(response.get());
    const uint8_t* bytes = NULL;
    size_t length = 0;
    if (!reader.PopArrayOfBytes(&bytes, &length) || !reader.PopBool(successful))
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

  void GetLoginStatus(
      const cryptohome::GetLoginStatusRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeGetLoginStatus);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);
    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
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

  void MountEx(const cryptohome::AccountIdentifier& id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeMountEx;
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

  void AddKeyEx(const cryptohome::AccountIdentifier& id,
                const cryptohome::AuthorizationRequest& auth,
                const cryptohome::AddKeyRequest& request,
                DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeAddKeyEx;
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

  void AddDataRestoreKey(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeAddDataRestoreKey;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface, method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(auth);

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

  void MassRemoveKeys(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::MassRemoveKeysRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name = cryptohome::kCryptohomeMassRemoveKeys;
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

  void StartFingerprintAuthSession(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::StartFingerprintAuthSessionRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeStartFingerprintAuthSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(id);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void EndFingerprintAuthSession(
      const cryptohome::EndFingerprintAuthSessionRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeEndFingerprintAuthSession);
    dbus::MessageWriter writer(&method_call);
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
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
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

  void GetCurrentSpaceForProjectId(
      const int project_id,
      DBusMethodCallback<int64_t> callback) override {
    dbus::MethodCall method_call(
        cryptohome::kCryptohomeInterface,
        cryptohome::kCryptohomeGetCurrentSpaceForProjectId);

    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(project_id);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnInt64DBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetProjectId(const int project_id,
                    const cryptohome::SetProjectIdAllowedPathType parent_path,
                    const std::string& child_path,
                    const cryptohome::AccountIdentifier& account_id,
                    DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                                 cryptohome::kCryptohomeSetProjectId);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(project_id);
    writer.AppendInt32(parent_path);
    writer.AppendString(child_path);
    writer.AppendProtoAsArrayOfBytes(account_id);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CryptohomeClientImpl::OnBoolMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetRsuDeviceId(
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    cryptohome::GetRsuDeviceIdRequest request;
    CallCryptohomeMethod(cryptohome::kCryptohomeGetRsuDeviceId, request,
                         std::move(callback));
  }

  void CheckHealth(
      const cryptohome::CheckHealthRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    CallCryptohomeMethod(cryptohome::kCryptohomeCheckHealth, request,
                         std::move(callback));
  }

  void LockToSingleUserMountUntilReboot(
      const cryptohome::LockToSingleUserMountUntilRebootRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override {
    const char* method_name =
        cryptohome::kCryptohomeLockToSingleUserMountUntilReboot;
    dbus::MethodCall method_call(cryptohome::kCryptohomeInterface, method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    proxy_->CallMethod(
        &method_call, kTpmDBusTimeoutMs,
        base::BindOnce(&CryptohomeClientImpl::OnBaseReplyMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) {
    dbus::ObjectProxy* proxy = bus->GetObjectProxy(
        cryptohome::kCryptohomeServiceName,
        dbus::ObjectPath(cryptohome::kCryptohomeServicePath));
    proxy_ = std::make_unique<DbusObjectProxyWithUma>(proxy);

    blocking_method_caller_.reset(new BlockingMethodCaller(bus, proxy));

    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface, cryptohome::kSignalAsyncCallStatus,
        base::BindRepeating(&CryptohomeClientImpl::AsyncCallStatusReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalAsyncCallStatusWithData,
        base::BindRepeating(
            &CryptohomeClientImpl::AsyncCallStatusWithDataReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface, cryptohome::kSignalLowDiskSpace,
        base::BindRepeating(&CryptohomeClientImpl::LowDiskSpaceReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CryptohomeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    proxy_->ConnectToSignal(
        cryptohome::kCryptohomeInterface,
        cryptohome::kSignalDircryptoMigrationProgress,
        base::BindRepeating(
            &CryptohomeClientImpl::DircryptoMigrationProgressReceived,
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
  bool CallBoolMethodAndBlock(dbus::MethodCall* method_call, bool* result) {
    base::Time start_time = base::Time::Now();

    std::unique_ptr<dbus::Response> response(
        blocking_method_caller_->CallMethodAndBlock(method_call));

    UmaHistogramMediumTimes(
        kCryptohomeClientUmaPrefix + method_call->GetMember(),
        base::Time::Now() - start_time);

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

  // Handles AsyncCallStatus signal.
  void AsyncCallStatusReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int async_id = 0;
    bool return_status = false;
    int return_code = 0;
    if (!reader.PopInt32(&async_id) || !reader.PopBool(&return_status) ||
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
    if (!reader.PopInt32(&async_id) || !reader.PopBool(&return_status) ||
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
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
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

  std::unique_ptr<DbusObjectProxyWithUma> proxy_ = nullptr;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::unique_ptr<BlockingMethodCaller> blocking_method_caller_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CryptohomeClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CryptohomeClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CryptohomeClient

CryptohomeClient::CryptohomeClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CryptohomeClient::~CryptohomeClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CryptohomeClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new CryptohomeClientImpl())->Init(bus);
}

// static
void CryptohomeClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (for
  // early setup calls dependent on CryptohomeClient).
  if (!FakeCryptohomeClient::Get())
    new FakeCryptohomeClient();
}

// static
void CryptohomeClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
CryptohomeClient* CryptohomeClient::Get() {
  return g_instance;
}

// static
std::string CryptohomeClient::GetStubSanitizedUsername(
    const cryptohome::AccountIdentifier& id) {
  return id.account_id() + kUserIdStubHashSuffix;
}

}  // namespace chromeos
