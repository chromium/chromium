// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/authpolicy/authpolicy_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
#include "components/account_id/account_id.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace ash {

namespace {

// Policy fetch may take up to 300 seconds.  To ensure that a second policy
// fetch queuing after the first one can succeed (e.g. user policy following
// device policy), the D-Bus timeout needs to be at least twice that value.
// JoinADDomain() is an exception since it's always guaranteed to be the first
// call.
constexpr int kSlowDbusTimeoutMilliseconds = 630 * 1000;

AuthPolicyClient* g_instance = nullptr;

authpolicy::ErrorType GetErrorFromReader(dbus::MessageReader* reader) {
  int32_t int_error;
  if (!reader->PopInt32(&int_error)) {
    DLOG(ERROR) << "AuthPolicyClient: Failed to get an error from the response";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  if (int_error < 0 || int_error >= authpolicy::ERROR_COUNT)
    return authpolicy::ERROR_UNKNOWN;
  return static_cast<authpolicy::ErrorType>(int_error);
}

authpolicy::ErrorType GetErrorAndProto(
    dbus::Response* response,
    google::protobuf::MessageLite* protobuf) {
  if (!response) {
    DLOG(ERROR) << "Auth: Failed to  call to authpolicy";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  dbus::MessageReader reader(response);
  const authpolicy::ErrorType error(GetErrorFromReader(&reader));

  if (error != authpolicy::ERROR_NONE)
    return error;

  if (!reader.PopArrayOfBytesAsProto(protobuf)) {
    DLOG(ERROR) << "Failed to parse protobuf.";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  return authpolicy::ERROR_NONE;
}

class AuthPolicyClientImpl : public AuthPolicyClient {
 public:
  AuthPolicyClientImpl() {}

  AuthPolicyClientImpl(const AuthPolicyClientImpl&) = delete;
  AuthPolicyClientImpl& operator=(const AuthPolicyClientImpl&) = delete;

  ~AuthPolicyClientImpl() override = default;

  // AuthPolicyClient override.
  void JoinAdDomain(const authpolicy::JoinDomainRequest& request,
                    int password_fd,
                    JoinCallback callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kJoinADDomainMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), authpolicy::ERROR_DBUS_FAILURE,
                         std::string()));
      return;
    }
    writer.AppendFileDescriptor(password_fd);
    proxy_->CallMethod(
        &method_call, kSlowDbusTimeoutMilliseconds,
        base::BindOnce(&AuthPolicyClientImpl::HandleJoinCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AuthenticateUser(const authpolicy::AuthenticateUserRequest& request,
                        int password_fd,
                        AuthCallback callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthenticateUserMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), authpolicy::ERROR_DBUS_FAILURE,
                         authpolicy::ActiveDirectoryAccountInfo()));
      return;
    }
    writer.AppendFileDescriptor(password_fd);
    proxy_->CallMethod(
        &method_call, kSlowDbusTimeoutMilliseconds,
        base::BindOnce(&AuthPolicyClientImpl::HandleCallback<
                           authpolicy::ActiveDirectoryAccountInfo>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetUserStatus(const authpolicy::GetUserStatusRequest& request,
                     GetUserStatusCallback callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kGetUserStatusMethod);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), authpolicy::ERROR_DBUS_FAILURE,
                         authpolicy::ActiveDirectoryUserStatus()));
      return;
    }
    proxy_->CallMethod(
        &method_call, kSlowDbusTimeoutMilliseconds,
        base::BindOnce(&AuthPolicyClientImpl::HandleCallback<
                           authpolicy::ActiveDirectoryUserStatus>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetUserKerberosFiles(const std::string& object_guid,
                            GetUserKerberosFilesCallback callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kGetUserKerberosFilesMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(object_guid);
    proxy_->CallMethod(
        &method_call, kSlowDbusTimeoutMilliseconds,
        base::BindOnce(
            &AuthPolicyClientImpl::HandleCallback<authpolicy::KerberosFiles>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RefreshDevicePolicy(RefreshPolicyCallback callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kRefreshDevicePolicyMethod);
    proxy_->CallMethod(
        &method_call, kSlowDbusTimeoutMilliseconds,
        base::BindOnce(&AuthPolicyClientImpl::HandleRefreshPolicyCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RefreshUserPolicy(const AccountId& account_id,
                         RefreshPolicyCallback callback) override {
    DCHECK(account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY);
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kRefreshUserPolicyMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id.GetObjGuid());
    proxy_->CallMethod(
        &method_call, kSlowDbusTimeoutMilliseconds,
        base::BindOnce(&AuthPolicyClientImpl::HandleRefreshPolicyCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ConnectToSignal(
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) override {
    proxy_->ConnectToSignal(authpolicy::kAuthPolicyInterface, signal_name,
                            std::move(signal_callback),
                            std::move(on_connected_callback));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* bus) {
    bus_ = bus;
    proxy_ = bus_->GetObjectProxy(
        authpolicy::kAuthPolicyServiceName,
        dbus::ObjectPath(authpolicy::kAuthPolicyServicePath));
  }

 private:
  void HandleRefreshPolicyCallback(RefreshPolicyCallback callback,
                                   dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "RefreshDevicePolicy: failed to call to authpolicy";
      std::move(callback).Run(authpolicy::ERROR_DBUS_FAILURE);
      return;
    }
    dbus::MessageReader reader(response);
    std::move(callback).Run(GetErrorFromReader(&reader));
  }

  void HandleJoinCallback(JoinCallback callback, dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "Join: Couldn't call to authpolicy";
      std::move(callback).Run(authpolicy::ERROR_DBUS_FAILURE, std::string());
      return;
    }

    dbus::MessageReader reader(response);
    authpolicy::ErrorType error = GetErrorFromReader(&reader);
    std::string machine_domain;
    if (error == authpolicy::ERROR_NONE) {
      if (!reader.PopString(&machine_domain))
        error = authpolicy::ERROR_DBUS_FAILURE;
    }
    std::move(callback).Run(error, machine_domain);
  }

  template <class T>
  void HandleCallback(base::OnceCallback<void(authpolicy::ErrorType error,
                                              const T& response)> callback,
                      dbus::Response* response) {
    T proto;
    authpolicy::ErrorType error(GetErrorAndProto(response, &proto));
    std::move(callback).Run(error, proto);
  }

  dbus::Bus* bus_ = nullptr;
  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AuthPolicyClientImpl> weak_ptr_factory_{this};
};

}  // namespace

AuthPolicyClient::AuthPolicyClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

AuthPolicyClient::~AuthPolicyClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void AuthPolicyClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new AuthPolicyClientImpl)->Init(bus);
}

// static
void AuthPolicyClient::InitializeFake() {
  if (!FakeAuthPolicyClient::Get())
    new FakeAuthPolicyClient();
}

// static
void AuthPolicyClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
AuthPolicyClient* AuthPolicyClient::Get() {
  return g_instance;
}

}  // namespace ash
