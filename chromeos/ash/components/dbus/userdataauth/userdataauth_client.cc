// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {
namespace {

// This suffix is appended to cryptohome_id to get hash in stub implementation:
// stub_hash = "[cryptohome_id]-hash";
constexpr char kUserIdStubHashSuffix[] = "-hash";

// The default timeout for all userdataauth method call.
// Note that it is known that cryptohomed could be slow to respond to calls
// certain conditions, especially Mount(). D-Bus call blocking for as long as 2
// minutes have been observed in testing conditions/CQ.
constexpr int kUserDataAuthDefaultTimeoutMS = 5 * 60 * 1000;

UserDataAuthClient* g_instance = nullptr;

// Tries to parse a proto message from |response| into |proto|.
// Returns false if |response| is nullptr or the message cannot be parsed.
bool ParseProto(dbus::Response* response,
                google::protobuf::MessageLite* proto) {
  if (!response) {
    LOG(ERROR) << "Failed to call cryptohomed";
    return false;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    LOG(ERROR) << "Failed to parse response message from cryptohomed";
    return false;
  }

  return true;
}

void OnSignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
  DCHECK_EQ(interface_name, ::user_data_auth::kUserDataAuthInterface);
  LOG_IF(DFATAL, !success) << "Failed to connect to D-Bus signal; interface: "
                           << interface_name << "; signal: " << signal_name;
}

// "Real" implementation of UserDataAuthClient talking to the cryptohomed's
// UserDataAuth interface on the Chrome OS side.
class UserDataAuthClientImpl : public UserDataAuthClient {
 public:
  UserDataAuthClientImpl() = default;
  ~UserDataAuthClientImpl() override = default;

  // Not copyable or movable.
  UserDataAuthClientImpl(const UserDataAuthClientImpl&) = delete;
  UserDataAuthClientImpl& operator=(const UserDataAuthClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::user_data_auth::kUserDataAuthServiceName,
        dbus::ObjectPath(::user_data_auth::kUserDataAuthServicePath));
    ConnectToSignals();
  }

  // UserDataAuthClient override:

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void AddFingerprintAuthObserver(FingerprintAuthObserver* observer) override {
    fp_observer_list_.AddObserver(observer);
  }

  void RemoveFingerprintAuthObserver(
      FingerprintAuthObserver* observer) override {
    fp_observer_list_.RemoveObserver(observer);
  }

  void AddPrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) override {
    progress_observer_list_.AddObserver(observer);
  }

  void RemovePrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) override {
    progress_observer_list_.RemoveObserver(observer);
  }

  void AddAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) override {
    auth_factor_status_observer_list_.AddObserver(observer);
  }

  void RemoveAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) override {
    auth_factor_status_observer_list_.RemoveObserver(observer);
  }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                 IsMountedCallback callback) override {
    CallProtoMethod(::user_data_auth::kIsMounted,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetVaultProperties(
      const ::user_data_auth::GetVaultPropertiesRequest& request,
      GetVaultPropertiesCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetVaultProperties,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void Unmount(const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback) override {
    CallProtoMethod(::user_data_auth::kUnmount,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void Remove(const ::user_data_auth::RemoveRequest& request,
              RemoveCallback callback) override {
    CallProtoMethod(::user_data_auth::kRemove,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void StartMigrateToDircrypto(
      const ::user_data_auth::StartMigrateToDircryptoRequest& request,
      StartMigrateToDircryptoCallback callback) override {
    CallProtoMethod(::user_data_auth::kStartMigrateToDircrypto,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void NeedsDircryptoMigration(
      const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
      NeedsDircryptoMigrationCallback callback) override {
    CallProtoMethod(::user_data_auth::kNeedsDircryptoMigration,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetSupportedKeyPolicies(
      const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
      GetSupportedKeyPoliciesCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetSupportedKeyPolicies,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetAccountDiskUsage(
      const ::user_data_auth::GetAccountDiskUsageRequest& request,
      GetAccountDiskUsageCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetAccountDiskUsage,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void StartAuthSession(
      const ::user_data_auth::StartAuthSessionRequest& request,
      StartAuthSessionCallback callback) override {
    CallProtoMethod(::user_data_auth::kStartAuthSession,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void PrepareGuestVault(
      const ::user_data_auth::PrepareGuestVaultRequest& request,
      PrepareGuestVaultCallback callback) override {
    CallProtoMethod(::user_data_auth::kPrepareGuestVault,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void PrepareEphemeralVault(
      const ::user_data_auth::PrepareEphemeralVaultRequest& request,
      PrepareEphemeralVaultCallback callback) override {
    CallProtoMethod(::user_data_auth::kPrepareEphemeralVault,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void CreatePersistentUser(
      const ::user_data_auth::CreatePersistentUserRequest& request,
      CreatePersistentUserCallback callback) override {
    CallProtoMethod(::user_data_auth::kCreatePersistentUser,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void PreparePersistentVault(
      const ::user_data_auth::PreparePersistentVaultRequest& request,
      PreparePersistentVaultCallback callback) override {
    CallProtoMethod(::user_data_auth::kPreparePersistentVault,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void PrepareVaultForMigration(
      const ::user_data_auth::PrepareVaultForMigrationRequest& request,
      PrepareVaultForMigrationCallback callback) override {
    CallProtoMethod(::user_data_auth::kPrepareVaultForMigration,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void InvalidateAuthSession(
      const ::user_data_auth::InvalidateAuthSessionRequest& request,
      InvalidateAuthSessionCallback callback) override {
    CallProtoMethod(::user_data_auth::kInvalidateAuthSession,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void ExtendAuthSession(
      const ::user_data_auth::ExtendAuthSessionRequest& request,
      ExtendAuthSessionCallback callback) override {
    CallProtoMethod(::user_data_auth::kExtendAuthSession,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void AddAuthFactor(const ::user_data_auth::AddAuthFactorRequest& request,
                     AddAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kAddAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void AuthenticateAuthFactor(
      const ::user_data_auth::AuthenticateAuthFactorRequest& request,
      AuthenticateAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kAuthenticateAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void UpdateAuthFactor(
      const ::user_data_auth::UpdateAuthFactorRequest& request,
      UpdateAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kUpdateAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void UpdateAuthFactorMetadata(
      const ::user_data_auth::UpdateAuthFactorMetadataRequest& request,
      UpdateAuthFactorMetadataCallback callback) override {
    CallProtoMethod(::user_data_auth::kUpdateAuthFactorMetadata,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void ReplaceAuthFactor(
      const ::user_data_auth::ReplaceAuthFactorRequest& request,
      ReplaceAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kReplaceAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void RemoveAuthFactor(
      const ::user_data_auth::RemoveAuthFactorRequest& request,
      RemoveAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kRemoveAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void ListAuthFactors(const ::user_data_auth::ListAuthFactorsRequest& request,
                       ListAuthFactorsCallback callback) override {
    CallProtoMethod(::user_data_auth::kListAuthFactors,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetAuthFactorExtendedInfo(
      const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
      GetAuthFactorExtendedInfoCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetAuthFactorExtendedInfo,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetAuthSessionStatus(
      const ::user_data_auth::GetAuthSessionStatusRequest& request,
      GetAuthSessionStatusCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetAuthSessionStatus,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void PrepareAuthFactor(
      const ::user_data_auth::PrepareAuthFactorRequest& request,
      PrepareAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kPrepareAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void TerminateAuthFactor(
      const ::user_data_auth::TerminateAuthFactorRequest& request,
      TerminateAuthFactorCallback callback) override {
    CallProtoMethod(::user_data_auth::kTerminateAuthFactor,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetArcDiskFeatures(
      const ::user_data_auth::GetArcDiskFeaturesRequest& request,
      GetArcDiskFeaturesCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetArcDiskFeatures,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void GetRecoverableKeyStores(
      const ::user_data_auth::GetRecoverableKeyStoresRequest& request,
      GetRecoverableKeyStoresCallback callback) override {
    CallProtoMethod(::user_data_auth::kGetRecoverableKeyStores,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

  void SetUserDataStorageWriteEnabled(
      const ::user_data_auth::SetUserDataStorageWriteEnabledRequest& request,
      SetUserDataStorageWriteEnabledCallback callback) override {
    CallProtoMethod(::user_data_auth::kSetUserDataStorageWriteEnabled,
                    ::user_data_auth::kUserDataAuthInterface, request,
                    std::move(callback));
  }

 private:
  // Calls cryptohomed's |method_name| method in |interface_name| interface,
  // passing in |request| as input with |timeout_ms|. Once the (asynchronous)
  // call finishes, |callback| is called with the response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethodWithTimeout(
      const char* method_name,
      const char* interface_name,
      int timeout_ms,
      const RequestType& request,
      chromeos::DBusMethodCallback<ReplyType> callback) {
    dbus::MethodCall method_call(interface_name, method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR)
          << "Failed to append protobuf when calling UserDataAuth method "
          << method_name;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&UserDataAuthClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls cryptohomed's |method_name| method in |interface_name| interface,
  // passing in |request| as input with the default UserDataAuth timeout. Once
  // the (asynchronous) call finishes, |callback| is called with the response
  // proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethod(const char* method_name,
                       const char* interface_name,
                       const RequestType& request,
                       chromeos::DBusMethodCallback<ReplyType> callback) {
    CallProtoMethodWithTimeout(method_name, interface_name,
                               kUserDataAuthDefaultTimeoutMS, request,
                               std::move(callback));
  }

  // Parses the response proto message from |response| and calls |callback| with
  // the decoded message. Calls |callback| with std::nullopt on error, including
  // timeout.
  template <typename ReplyType>
  void HandleResponse(chromeos::DBusMethodCallback<ReplyType> callback,
                      dbus::Response* response) {
    ReplyType reply_proto;
    if (!ParseProto(response, &reply_proto)) {
      LOG(ERROR) << "Failed to parse reply protobuf from UserDataAuth method";
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(reply_proto);
  }

  void OnDircryptoMigrationProgress(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    ::user_data_auth::DircryptoMigrationProgress proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse DircryptoMigrationProgress protobuf from "
                    "UserDataAuth signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.DircryptoMigrationProgress(proto);
    }
  }

  void OnLowDiskSpace(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    ::user_data_auth::LowDiskSpace proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR)
          << "Failed to parse LowDiskSpace protobuf from UserDataAuth signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.LowDiskSpace(proto);
    }
  }

  void OnAuthEnrollmentProgress(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    ::user_data_auth::AuthEnrollmentProgress proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse AuthEnrollmentProgress protobuf from "
                    "UserDataAuth signal";
      return;
    }
    for (auto& observer : fp_observer_list_) {
      observer.OnEnrollScanDone(
          proto.scan_result().fingerprint_result(), proto.done(),
          proto.fingerprint_progress().percent_complete());
    }
  }

  void OnPrepareAuthFactorProgress(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    ::user_data_auth::PrepareAuthFactorProgress proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse PrepareAuthFactorProgress protobuf from "
                    "UserDataAuth signal";
      return;
    }
    if (proto.purpose() == ::user_data_auth::PURPOSE_ADD_AUTH_FACTOR &&
        proto.add_progress().auth_factor_type() ==
            ::user_data_auth::AUTH_FACTOR_TYPE_FINGERPRINT) {
      for (auto& observer : progress_observer_list_) {
        observer.OnFingerprintEnrollProgress(
            proto.add_progress().biometrics_progress());
      }
    } else if (proto.purpose() ==
                   ::user_data_auth::PURPOSE_AUTHENTICATE_AUTH_FACTOR &&
               proto.auth_progress().auth_factor_type() ==
                   ::user_data_auth::AUTH_FACTOR_TYPE_FINGERPRINT) {
      for (auto& observer : progress_observer_list_) {
        observer.OnFingerprintAuthScan(
            proto.auth_progress().biometrics_progress());
      }
    } else if (proto.purpose() ==
                   ::user_data_auth::PURPOSE_AUTHENTICATE_AUTH_FACTOR &&
               proto.auth_progress().auth_factor_type() ==
                   ::user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT) {
      for (auto& observer : fp_observer_list_) {
        observer.OnFingerprintScan(proto.auth_progress()
                                       .biometrics_progress()
                                       .scan_result()
                                       .fingerprint_result());
      }
    } else {
      LOG(ERROR) << "Received a unrecognized PrepareAuthFactorProgress signal";
      return;
    }
  }

  void OnAuthFactorStatusUpdate(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    ::user_data_auth::AuthFactorStatusUpdate proto;
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse AuthFactorStatusUpdate protobuf from "
                    "UserDataAuth signal";
      return;
    }
    for (auto& observer : auth_factor_status_observer_list_) {
      observer.OnAuthFactorStatusUpdate(proto);
    }
  }

  // Connects the dbus signals.
  void ConnectToSignals() {
    proxy_->ConnectToSignal(
        ::user_data_auth::kUserDataAuthInterface,
        ::user_data_auth::kDircryptoMigrationProgress,
        base::BindRepeating(
            &UserDataAuthClientImpl::OnDircryptoMigrationProgress,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
    proxy_->ConnectToSignal(
        ::user_data_auth::kUserDataAuthInterface,
        ::user_data_auth::kLowDiskSpace,
        base::BindRepeating(&UserDataAuthClientImpl::OnLowDiskSpace,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
    proxy_->ConnectToSignal(
        ::user_data_auth::kUserDataAuthInterface,
        ::user_data_auth::kAuthEnrollmentProgressSignal,
        base::BindRepeating(&UserDataAuthClientImpl::OnAuthEnrollmentProgress,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
    proxy_->ConnectToSignal(
        ::user_data_auth::kUserDataAuthInterface,
        ::user_data_auth::kPrepareAuthFactorProgressSignal,
        base::BindRepeating(
            &UserDataAuthClientImpl::OnPrepareAuthFactorProgress,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
    proxy_->ConnectToSignal(
        ::user_data_auth::kUserDataAuthInterface,
        ::user_data_auth::kAuthFactorStatusUpdate,
        base::BindRepeating(&UserDataAuthClientImpl::OnAuthFactorStatusUpdate,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
  }

  // D-Bus proxy for cryptohomed, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // List of observers for dbus signals.
  base::ObserverList<Observer> observer_list_;

  // List of observers for dbus signals related to legacy fingerprint.
  base::ObserverList<FingerprintAuthObserver> fp_observer_list_;

  // List of observers for dbus signals related to fingerprint.
  base::ObserverList<PrepareAuthFactorProgressObserver> progress_observer_list_;

  // List of observers for dbus signal AuthFactorStatusUpdate.
  base::ObserverList<AuthFactorStatusUpdateObserver>
      auth_factor_status_observer_list_;

  base::WeakPtrFactory<UserDataAuthClientImpl> weak_factory_{this};
};

}  // namespace

UserDataAuthClient::UserDataAuthClient() = default;

UserDataAuthClient::~UserDataAuthClient() = default;

// static
void UserDataAuthClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  CHECK(!g_instance);
  auto* impl = new UserDataAuthClientImpl();
  g_instance = impl;
  impl->Init(bus);
}

// static
void UserDataAuthClient::InitializeFake() {
  if (g_instance) {
    // TODO(b/239430274): Certain tests call InitializeFake() before the
    // browser starts to set parameters. They should just access the fake
    // instance directly via FakeUserDataAuthClient::Get(), via
    // FakeUserDataAuthClient::TestApi or via CryptohomeMixin.
    CHECK(g_instance == FakeUserDataAuthClient::Get());
  } else {
    g_instance = FakeUserDataAuthClient::Get();
    CHECK(g_instance);
  }
}

void UserDataAuthClient::OverrideGlobalInstanceForTesting(
    UserDataAuthClient* client) {
  g_instance = client;
}

// static
void UserDataAuthClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

// static
UserDataAuthClient* UserDataAuthClient::Get() {
  return g_instance;
}

// static
std::string UserDataAuthClient::GetStubSanitizedUsername(
    const cryptohome::AccountIdentifier& id) {
  return id.account_id() + kUserIdStubHashSuffix;
}

}  // namespace ash
