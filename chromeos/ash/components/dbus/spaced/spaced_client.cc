// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/spaced/spaced_client.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/spaced/dbus-constants.h"

namespace ash {
namespace {

using spaced::kSpacedInterface;

SpacedClient* g_instance = nullptr;

// Implementation of SpacedClient talking to the Spaced interface on the Chrome
// OS side.
class SpacedClientImpl : public SpacedClient {
 public:
  SpacedClientImpl() = default;
  ~SpacedClientImpl() override = default;

  // Not copyable or movable.
  SpacedClientImpl(const SpacedClientImpl&) = delete;
  SpacedClientImpl& operator=(const SpacedClientImpl&) = delete;

  void GetFreeDiskSpace(const std::string& path,
                        GetSizeCallback callback) override {
    dbus::MethodCall method_call(kSpacedInterface,
                                 spaced::kGetFreeDiskSpaceMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SpacedClientImpl::HandleResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetTotalDiskSpace(const std::string& path,
                         GetSizeCallback callback) override {
    dbus::MethodCall method_call(kSpacedInterface,
                                 spaced::kGetTotalDiskSpaceMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SpacedClientImpl::HandleResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetRootDeviceSize(GetSizeCallback callback) override {
    dbus::MethodCall method_call(kSpacedInterface,
                                 spaced::kGetRootDeviceSizeMethod);
    dbus::MessageWriter writer(&method_call);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&SpacedClientImpl::HandleResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(spaced::kSpacedServiceName,
                                 dbus::ObjectPath(spaced::kSpacedServicePath));

    DCHECK(proxy_);
    proxy_->ConnectToSignal(
        kSpacedInterface, spaced::kStatefulDiskSpaceUpdate,
        base::BindRepeating(&SpacedClientImpl::OnSpaceUpdate,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SpacedClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Handles the result of Mount and calls |callback|.
  void HandleResponse(GetSizeCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(absl::nullopt);
      return;
    }

    dbus::MessageReader reader(response);
    int64_t size = 0;

    if (!reader.PopInt64(&size)) {
      LOG(ERROR) << "Spaced D-Bus method " << response->GetMember()
                 << ": Invalid response. " + response->ToString();
      std::move(callback).Run(absl::nullopt);
      return;
    }

    std::move(callback).Run(size);
  }

  void OnSpaceUpdate(dbus::Signal* const signal) const {
    Observer::SpaceEvent event;
    if (!dbus::MessageReader(signal).PopArrayOfBytesAsProto(&event)) {
      LOG(ERROR) << "Cannot parse StatefulDiskSpaceUpdate proto";
      return;
    }

    for (Observer& observer : observers_) {
      observer.OnSpaceUpdate(event);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         const bool connected) {
    connected_ = connected;
    LOG_IF(ERROR, !connected)
        << "Cannot connect to StatefulDiskSpaceUpdate signal";
    DCHECK_EQ(interface_name, kSpacedInterface);
    DCHECK_EQ(signal_name, spaced::kStatefulDiskSpaceUpdate);
  }

  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SpacedClientImpl> weak_ptr_factory_{this};
};

}  // namespace

SpacedClient::SpacedClient() {
  CHECK(!g_instance);
  g_instance = this;
}

SpacedClient::~SpacedClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void SpacedClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new SpacedClientImpl())->Init(bus);
}

// static
void SpacedClient::InitializeFake() {
  new FakeSpacedClient();
}

// static
void SpacedClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
SpacedClient* SpacedClient::Get() {
  return g_instance;
}

}  // namespace ash
