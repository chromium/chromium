// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/swap_management/swap_management_client.h"

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/swap_management/fake_swap_management_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace ash {

namespace {

SwapManagementClient* g_instance = nullptr;

// The SwapManagementClient implementation used in production.
class SwapManagementClientImpl : public SwapManagementClient {
 public:
  SwapManagementClientImpl() = default;

  SwapManagementClientImpl(const SwapManagementClientImpl&) = delete;
  SwapManagementClientImpl& operator=(const SwapManagementClientImpl&) = delete;

  ~SwapManagementClientImpl() override = default;

  void Init(dbus::Bus* bus) override {
    swap_management_proxy_ = bus->GetObjectProxy(
        swap_management::kSwapManagementServiceName,
        dbus::ObjectPath(swap_management::kSwapManagementServicePath));
  }

  void MGLRUSetEnable(uint8_t value,
                      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(swap_management::kSwapManagementInterface,
                                 swap_management::kMGLRUSetEnable);
    dbus::MessageWriter writer(&method_call);
    writer.AppendByte(value);
    swap_management_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SwapManagementClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnResponse(chromeos::VoidDBusMethodCallback callback,
                  dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  raw_ptr<dbus::ObjectProxy> swap_management_proxy_ = nullptr;
  base::WeakPtrFactory<SwapManagementClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
SwapManagementClient* SwapManagementClient::Get() {
  return g_instance;
}

// static
void SwapManagementClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  CHECK(!g_instance);
  g_instance = new SwapManagementClientImpl();
  g_instance->Init(bus);
}

// static
void SwapManagementClient::InitializeFake() {
  CHECK(!g_instance);
  g_instance = new FakeSwapManagementClient();
  g_instance->Init(nullptr);
}

// static
void SwapManagementClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

SwapManagementClient::SwapManagementClient() = default;

SwapManagementClient::~SwapManagementClient() = default;

}  // namespace ash
