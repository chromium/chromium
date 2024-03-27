// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/tpm_manager/dbus-constants.h"

namespace chromeos {
namespace {

// An arbitrary timeout for taking ownership.
constexpr base::TimeDelta kTakeOwnershipTimeout = base::Seconds(80);

TpmManagerClient* g_instance = nullptr;

// Tries to parse a proto message from |response| into |proto|.
// Returns false if |response| is nullptr or the message cannot be parsed.
bool ParseProto(dbus::Response* response,
                google::protobuf::MessageLite* proto) {
  if (!response) {
    LOG(ERROR) << "Failed to call tpm_managerd";
    return false;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(proto)) {
    LOG(ERROR) << "Failed to parse response message from tpm_managerd";
    return false;
  }

  return true;
}

void OnSignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
  DCHECK_EQ(interface_name, ::tpm_manager::kTpmManagerInterface);
  LOG_IF(DFATAL, !success) << "Failed to connect to D-Bus signal; interface: "
                           << interface_name << "; signal: " << signal_name;
}

// "Real" implementation of TpmManagerClient talking to the TpmManager daemon
// on the Chrome OS side.
class TpmManagerClientImpl : public TpmManagerClient {
 public:
  TpmManagerClientImpl() = default;
  ~TpmManagerClientImpl() override = default;

  // Not copyable or movable.
  TpmManagerClientImpl(const TpmManagerClientImpl&) = delete;
  TpmManagerClientImpl& operator=(const TpmManagerClientImpl&) = delete;
  TpmManagerClientImpl(TpmManagerClientImpl&&) = delete;
  TpmManagerClientImpl& operator=(TpmManagerClientImpl&&) = delete;

  // TpmManagerClient overrides:
  void GetTpmNonsensitiveStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusRequest& request,
      GetTpmNonsensitiveStatusCallback callback) override {
    CallProtoMethod(::tpm_manager::kGetTpmNonsensitiveStatus, request,
                    std::move(callback));
  }
  void GetVersionInfo(const ::tpm_manager::GetVersionInfoRequest& request,
                      GetVersionInfoCallback callback) override {
    CallProtoMethod(::tpm_manager::kGetVersionInfo, request,
                    std::move(callback));
  }
  void GetSupportedFeatures(
      const ::tpm_manager::GetSupportedFeaturesRequest& request,
      GetSupportedFeaturesCallback callback) override {
    CallProtoMethod(::tpm_manager::kGetSupportedFeatures, request,
                    std::move(callback));
  }
  void GetDictionaryAttackInfo(
      const ::tpm_manager::GetDictionaryAttackInfoRequest& request,
      GetDictionaryAttackInfoCallback callback) override {
    CallProtoMethod(::tpm_manager::kGetDictionaryAttackInfo, request,
                    std::move(callback));
  }
  void TakeOwnership(const ::tpm_manager::TakeOwnershipRequest& request,
                     TakeOwnershipCallback callback) override {
    // Use a longer timeout for TPM ownership operation.
    CallProtoMethodWithTimeout(::tpm_manager::kTakeOwnership,
                               kTakeOwnershipTimeout.InMilliseconds(), request,
                               std::move(callback));
  }
  void ClearStoredOwnerPassword(
      const ::tpm_manager::ClearStoredOwnerPasswordRequest& request,
      ClearStoredOwnerPasswordCallback callback) override {
    CallProtoMethod(::tpm_manager::kClearStoredOwnerPassword, request,
                    std::move(callback));
  }
  void ClearTpm(const ::tpm_manager::ClearTpmRequest& request,
                ClearTpmCallback callback) override {
    CallProtoMethod(::tpm_manager::kClearTpm, request, std::move(callback));
  }

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::tpm_manager::kTpmManagerServiceName,
        dbus::ObjectPath(::tpm_manager::kTpmManagerServicePath));
    ConnectToOwnershipTakenSignal();
  }

 private:
  TestInterface* GetTestInterface() override { return nullptr; }

  // Calls tpm_managerd's |method_name| method, passing in |request| as input
  // with |timeout_ms|. Once the (asynchronous) call finishes, |callback| is
  // called with the response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethodWithTimeout(
      const char* method_name,
      int timeout_ms,
      const RequestType& request,
      base::OnceCallback<void(const ReplyType&)> callback) {
    dbus::MethodCall method_call(::tpm_manager::kTpmManagerInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      ReplyType reply;
      reply.set_status(::tpm_manager::STATUS_DBUS_ERROR);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), reply));
      return;
    }
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, timeout_ms,
        base::BindOnce(&TpmManagerClientImpl::HandleResponse<ReplyType>,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // Calls tpm_managerd's |method_name| method, passing in |request| as input
  // with the default timeout. Once the (asynchronous) call finishes, |callback|
  // is called with the response proto.
  template <typename RequestType, typename ReplyType>
  void CallProtoMethod(const char* method_name,
                       const RequestType& request,
                       base::OnceCallback<void(const ReplyType&)> callback) {
    CallProtoMethodWithTimeout(method_name,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, request,
                               std::move(callback));
  }

  // Parses the response proto message from |response| and calls |callback| with
  // the decoded message. Calls |callback| with an |STATUS_DBUS_ERROR| message
  // on error, including timeout.
  template <typename ReplyType>
  void HandleResponse(base::OnceCallback<void(const ReplyType&)> callback,
                      dbus::Response* response) {
    ReplyType reply_proto;
    if (!ParseProto(response, &reply_proto))
      reply_proto.set_status(::tpm_manager::STATUS_DBUS_ERROR);
    std::move(callback).Run(reply_proto);
  }

  // Called when receiving ownership taken signal.
  void OnOwnershipTakenSignal(dbus::Signal*) {
    for (auto& observer : observer_list_) {
      observer.OnOwnershipTaken();
    }
  }

  // Connects to ownership taken signal.
  void ConnectToOwnershipTakenSignal() {
    proxy_->ConnectToSignal(
        ::tpm_manager::kTpmManagerInterface,
        ::tpm_manager::kOwnershipTakenSignal,
        base::BindRepeating(&TpmManagerClientImpl::OnOwnershipTakenSignal,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
  }

  // D-Bus proxy for the TpmManager daemon, not owned.
  raw_ptr<dbus::ObjectProxy, LeakedDanglingUntriaged> proxy_ = nullptr;

  // The observer list of ownership taken signal.
  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<TpmManagerClientImpl> weak_factory_{this};
};

}  // namespace

TpmManagerClient::TpmManagerClient() {
  CHECK(!g_instance);
  g_instance = this;
}

TpmManagerClient::~TpmManagerClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void TpmManagerClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new TpmManagerClientImpl())->Init(bus);
}

// static
void TpmManagerClient::InitializeFake() {
  // Do not create a new instance if it was initialized early in a browser test
  // (for early setup calls dependent on TpmManagerClient).
  if (!FakeTpmManagerClient::Get())
    new FakeTpmManagerClient();
}

// static
void TpmManagerClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
TpmManagerClient* TpmManagerClient::Get() {
  return g_instance;
}

}  // namespace chromeos
