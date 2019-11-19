// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/seneschal_client.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/seneschal/dbus-constants.h"

namespace chromeos {

class SeneschalClientImpl : public SeneschalClient {
 public:
  SeneschalClientImpl() {}

  ~SeneschalClientImpl() override = default;

  void SharePath(const vm_tools::seneschal::SharePathRequest& request,
                 DBusMethodCallback<vm_tools::seneschal::SharePathResponse>
                     callback) override {
    dbus::MethodCall method_call(vm_tools::seneschal::kSeneschalInterface,
                                 vm_tools::seneschal::kSharePathMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode SharePath protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    seneschal_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SeneschalClientImpl::OnDBusProtoResponse<
                           vm_tools::seneschal::SharePathResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnsharePath(const vm_tools::seneschal::UnsharePathRequest& request,
                   DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
                       callback) override {
    dbus::MethodCall method_call(vm_tools::seneschal::kSeneschalInterface,
                                 vm_tools::seneschal::kUnsharePathMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode UnsharePath protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    seneschal_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SeneschalClientImpl::OnDBusProtoResponse<
                           vm_tools::seneschal::UnsharePathResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    seneschal_proxy_ = bus->GetObjectProxy(
        vm_tools::seneschal::kSeneschalServiceName,
        dbus::ObjectPath(vm_tools::seneschal::kSeneschalServicePath));
  }

 private:
  template <typename ResponseProto>
  void OnDBusProtoResponse(DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from " << dbus_response->GetMember();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  dbus::ObjectProxy* seneschal_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SeneschalClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SeneschalClientImpl);
};

SeneschalClient::SeneschalClient() = default;

SeneschalClient::~SeneschalClient() = default;

std::unique_ptr<SeneschalClient> SeneschalClient::Create() {
  return std::make_unique<SeneschalClientImpl>();
}

}  // namespace chromeos
