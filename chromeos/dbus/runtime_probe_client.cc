// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/runtime_probe_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/runtime_probe/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class RuntimeProbeClientImpl : public RuntimeProbeClient {
 public:
  RuntimeProbeClientImpl() {}

  ~RuntimeProbeClientImpl() override = default;

  // RuntimeProbeClient override:
  void ProbeCategories(const runtime_probe::ProbeRequest& request,
                       RuntimeProbeCallback callback) override {
    dbus::MethodCall method_call(runtime_probe::kRuntimeProbeInterfaceName,
                                 runtime_probe::kProbeCategoriesMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ProbeRequest protobuf";
      std::move(callback).Run(base::nullopt);
      return;
    }
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&RuntimeProbeClientImpl::OnProbeCategories,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        runtime_probe::kRuntimeProbeServiceName,
        dbus::ObjectPath(runtime_probe::kRuntimeProbeServicePath));
    if (!proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << runtime_probe::kRuntimeProbeServiceName;
    }
  }

 private:
  void OnProbeCategories(RuntimeProbeCallback callback,
                         dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    runtime_probe::ProbeResult response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to parse proto from " << response->GetMember();
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(response_proto);
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RuntimeProbeClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RuntimeProbeClientImpl);
};

RuntimeProbeClient::RuntimeProbeClient() = default;

RuntimeProbeClient::~RuntimeProbeClient() = default;

std::unique_ptr<RuntimeProbeClient> RuntimeProbeClient::Create() {
  return std::make_unique<RuntimeProbeClientImpl>();
}

}  // namespace chromeos
