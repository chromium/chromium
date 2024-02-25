// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/runtime_probe/runtime_probe_client.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/runtime_probe/fake_runtime_probe_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/runtime_probe/dbus-constants.h"

namespace ash {
namespace {

RuntimeProbeClient* g_instance = nullptr;

class RuntimeProbeClientImpl : public RuntimeProbeClient {
 public:
  RuntimeProbeClientImpl() {}

  RuntimeProbeClientImpl(const RuntimeProbeClientImpl&) = delete;
  RuntimeProbeClientImpl& operator=(const RuntimeProbeClientImpl&) = delete;

  ~RuntimeProbeClientImpl() override = default;

  // RuntimeProbeClient override:
  void ProbeCategories(const runtime_probe::ProbeRequest& request,
                       RuntimeProbeCallback callback) override {
    dbus::MethodCall method_call(runtime_probe::kRuntimeProbeInterfaceName,
                                 runtime_probe::kProbeCategoriesMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ProbeRequest protobuf";
      std::move(callback).Run(std::nullopt);
      return;
    }
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&RuntimeProbeClientImpl::OnProbeCategories,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

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
      std::move(callback).Run(std::nullopt);
      return;
    }
    runtime_probe::ProbeResult response_proto;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
      LOG(ERROR) << "Failed to parse proto from " << response->GetMember();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(response_proto);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RuntimeProbeClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
RuntimeProbeClient* RuntimeProbeClient::Get() {
  return g_instance;
}

// static
void RuntimeProbeClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new RuntimeProbeClientImpl())->Init(bus);
}

// static
void RuntimeProbeClient::InitializeFake() {
  (new FakeRuntimeProbeClient())->Init(nullptr);
}

// static
void RuntimeProbeClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

RuntimeProbeClient::RuntimeProbeClient() {
  CHECK(!g_instance);
  g_instance = this;
}

RuntimeProbeClient::~RuntimeProbeClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
