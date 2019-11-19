// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/kerberos/kerberos_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/kerberos/fake_kerberos_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/kerberos/dbus-constants.h"

namespace chromeos {
namespace {

KerberosClient* g_instance = nullptr;

// Tries to parse a proto message from |response| into |proto|.
// Returns false if |response| is nullptr or the message cannot be parsed.
bool ParseProto(dbus::Response* response,
                google::protobuf::MessageLite* proto) {
  if (!response) {
    LOG(ERROR) << "Failed to call kerberosd";
    return false;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    LOG(ERROR) << "Failed to parse response message from kerberosd";
    return false;
  }

  return true;
}

void OnSignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
  DCHECK_EQ(interface_name, kerberos::kKerberosInterface);
  DCHECK(success);
}

// "Real" implementation of KerberosClient taking to the Kerberos daemon on the
// Chrome OS side.
class KerberosClientImpl : public KerberosClient {
 public:
  KerberosClientImpl() = default;
  ~KerberosClientImpl() override = default;

  // KerberosClient overrides:
  void AddAccount(const kerberos::AddAccountRequest& request,
                  AddAccountCallback callback) override {
    CallProtoMethod(kerberos::kAddAccountMethod, request, std::move(callback));
  }

  void RemoveAccount(const kerberos::RemoveAccountRequest& request,
                     RemoveAccountCallback callback) override {
    CallProtoMethod(kerberos::kRemoveAccountMethod, request,
                    std::move(callback));
  }

  void ClearAccounts(const kerberos::ClearAccountsRequest& request,
                     ClearAccountsCallback callback) override {
    CallProtoMethod(kerberos::kClearAccountsMethod, request,
                    std::move(callback));
  }

  void ListAccounts(const kerberos::ListAccountsRequest& request,
                    ListAccountsCallback callback) override {
    CallProtoMethod(kerberos::kListAccountsMethod, request,
                    std::move(callback));
  }

  void SetConfig(const kerberos::SetConfigRequest& request,
                 SetConfigCallback callback) override {
    CallProtoMethod(kerberos::kSetConfigMethod, request, std::move(callback));
  }

  void ValidateConfig(const kerberos::ValidateConfigRequest& request,
                      ValidateConfigCallback callback) override {
    CallProtoMethod(kerberos::kValidateConfigMethod, request,
                    std::move(callback));
  }

  void AcquireKerberosTgt(const kerberos::AcquireKerberosTgtRequest& request,
                          int password_fd,
                          AcquireKerberosTgtCallback callback) override {
    // kAcquireKerberosTgtMethod takes |password_fd| as extra arg.
    CallProtoMethodWithExtraArgs(
        kerberos::kAcquireKerberosTgtMethod, request, std::move(callback),
        base::BindOnce(
            [](int in_password_fd, dbus::MessageWriter* writer) {
              writer->AppendFileDescriptor(in_password_fd);
            },
            password_fd));
  }

  void GetKerberosFiles(const kerberos::GetKerberosFilesRequest& request,
                        GetKerberosFilesCallback callback) override {
    CallProtoMethod(kerberos::kGetKerberosFilesMethod, request,
                    std::move(callback));
  }

  void ConnectToKerberosFileChangedSignal(
      KerberosFilesChangedCallback callback) override {
    DCHECK(callback);
    kerberos_files_changed_callback_ = callback;

    proxy_->ConnectToSignal(
        kerberos::kKerberosInterface, kerberos::kKerberosFilesChangedSignal,
        base::BindRepeating(&KerberosClientImpl::OnKerberosFilesChanged,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
  }

  void ConnectToKerberosTicketExpiringSignal(
      KerberosTicketExpiringCallback callback) override {
    DCHECK(callback);
    kerberos_ticket_expiring_callback_ = callback;

    proxy_->ConnectToSignal(
        kerberos::kKerberosInterface, kerberos::kKerberosTicketExpiringSignal,
        base::BindRepeating(&KerberosClientImpl::OnKerberosTicketExpiring,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
  }

  void OnKerberosFilesChanged(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), kerberos::kKerberosInterface);
    DCHECK_EQ(signal->GetMember(), kerberos::kKerberosFilesChangedSignal);

    dbus::MessageReader signal_reader(signal);
    std::string principal_name;
    if (!signal_reader.PopString(&principal_name)) {
      LOG(ERROR)
          << "Failed to read principal name for KerberosFilesChanged signal";
      return;
    }

    DCHECK(kerberos_files_changed_callback_);
    kerberos_files_changed_callback_.Run(principal_name);
  }

  void OnKerberosTicketExpiring(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), kerberos::kKerberosInterface);
    DCHECK_EQ(signal->GetMember(), kerberos::kKerberosTicketExpiringSignal);

    dbus::MessageReader signal_reader(signal);
    std::string principal_name;
    if (!signal_reader.PopString(&principal_name)) {
      LOG(ERROR)
          << "Failed to read principal name for KerberosTicketExpiring signal";
      return;
    }

    DCHECK(kerberos_ticket_expiring_callback_);
    kerberos_ticket_expiring_callback_.Run(principal_name);
  }

  void Init(dbus::Bus* bus) {
    proxy_ =
        bus->GetObjectProxy(kerberos::kKerberosServiceName,
                            dbus::ObjectPath(kerberos::kKerberosServicePath));
  }

 private:
  TestInterface* GetTestInterface() override { return nullptr; }

  // Calls kerberosd's |method_name| method, passing in |request| as input. Once
  // the (asynchronous) call finishes, |callback| is called with the response
  // proto (on the same thread as this call).
  template <class TRequest, class TResponse>
  void CallProtoMethodWithExtraArgs(
      const char* method_name,
      const TRequest& request,
      base::OnceCallback<void(const TResponse&)> callback,
      base::OnceCallback<void(dbus::MessageWriter*)> write_extra_args) {
    dbus::MethodCall method_call(kerberos::kKerberosInterface, method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      TResponse response;
      response.set_error(kerberos::ERROR_DBUS_FAILURE);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), response));
      return;
    }
    if (write_extra_args)
      std::move(write_extra_args).Run(&writer);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&KerberosClientImpl::HandleResponse<TResponse>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Same as CallProtoMethodWithExtraArgs, but doesn't pass in extra args.
  // Use for methods that only take a request proto as input.
  template <class TRequest, class TResponse>
  void CallProtoMethod(const char* method_name,
                       const TRequest& request,
                       base::OnceCallback<void(const TResponse&)> callback) {
    CallProtoMethodWithExtraArgs(method_name, request, std::move(callback), {});
  }

  // Parses the response proto message from |response| and calls |callback| with
  // the decoded message. Calls |callback| with an |ERROR_DBUS_FAILURE| message
  // on error.
  template <class TProto>
  void HandleResponse(base::OnceCallback<void(const TProto&)> callback,
                      dbus::Response* response) {
    TProto response_proto;
    if (!ParseProto(response, &response_proto))
      response_proto.set_error(kerberos::ERROR_DBUS_FAILURE);
    std::move(callback).Run(response_proto);
  }

  // D-Bus proxy for the Kerberos daemon, not owned.
  dbus::ObjectProxy* proxy_ = nullptr;

  // Signal callbacks.
  KerberosFilesChangedCallback kerberos_files_changed_callback_;
  KerberosTicketExpiringCallback kerberos_ticket_expiring_callback_;

  base::WeakPtrFactory<KerberosClientImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(KerberosClientImpl);
};

}  // namespace

KerberosClient::KerberosClient() {
  CHECK(!g_instance);
  g_instance = this;
}

KerberosClient::~KerberosClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void KerberosClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new KerberosClientImpl())->Init(bus);
}

// static
void KerberosClient::InitializeFake() {
  new FakeKerberosClient();
}

// static
void KerberosClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
KerberosClient* KerberosClient::Get() {
  return g_instance;
}

}  // namespace chromeos
