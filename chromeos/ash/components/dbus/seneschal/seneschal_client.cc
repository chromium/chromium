// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/seneschal/dbus-constants.h"

namespace ash {

namespace {

SeneschalClient* g_instance = nullptr;

}  // namespace

class SeneschalClientImpl : public SeneschalClient {
 public:
  SeneschalClientImpl() = default;

  SeneschalClientImpl(const SeneschalClientImpl&) = delete;
  SeneschalClientImpl& operator=(const SeneschalClientImpl&) = delete;

  ~SeneschalClientImpl() override = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    seneschal_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void SharePath(
      const vm_tools::seneschal::SharePathRequest& request,
      chromeos::DBusMethodCallback<vm_tools::seneschal::SharePathResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::seneschal::kSeneschalInterface,
                                 vm_tools::seneschal::kSharePathMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode SharePath protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    seneschal_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SeneschalClientImpl::OnDBusProtoResponse<
                           vm_tools::seneschal::SharePathResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnsharePath(
      const vm_tools::seneschal::UnsharePathRequest& request,
      chromeos::DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::seneschal::kSeneschalInterface,
                                 vm_tools::seneschal::kUnsharePathMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode UnsharePath protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    seneschal_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SeneschalClientImpl::OnDBusProtoResponse<
                           vm_tools::seneschal::UnsharePathResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) override {
    seneschal_proxy_ = bus->GetObjectProxy(
        vm_tools::seneschal::kSeneschalServiceName,
        dbus::ObjectPath(vm_tools::seneschal::kSeneschalServicePath));
    seneschal_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&SeneschalClientImpl::NameOwnerChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  template <typename ResponseProto>
  void OnDBusProtoResponse(chromeos::DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from " << dbus_response->GetMember();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    if (!old_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.SeneschalServiceStopped();
      }
    }
    if (!new_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.SeneschalServiceStarted();
      }
    }
  }

  base::ObserverList<Observer> observer_list_;

  raw_ptr<dbus::ObjectProxy> seneschal_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SeneschalClientImpl> weak_ptr_factory_{this};
};

SeneschalClient::SeneschalClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

SeneschalClient::~SeneschalClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SeneschalClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new SeneschalClientImpl())->Init(bus);
}

// static
void SeneschalClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test to
  // allow the test to set its own client.
  if (!FakeSeneschalClient::Get())
    new FakeSeneschalClient();
}

// static
void SeneschalClient::Shutdown() {
  delete g_instance;
}

// static
SeneschalClient* SeneschalClient::Get() {
  return g_instance;
}

}  // namespace ash
