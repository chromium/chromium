// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/resourced/resourced_client.h"

#include "base/check_op.h"
#include "chromeos/dbus/resourced/fake_resourced_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

namespace chromeos {
namespace {

// Resource manager D-Bus method calls are all simple operations and should
// not take more than 1 second.
constexpr int kResourcedDBusTimeoutMilliseconds = 1000;

ResourcedClient* g_instance = nullptr;

class ResourcedClientImpl : public ResourcedClient {
 public:
  ResourcedClientImpl() = default;
  ~ResourcedClientImpl() override = default;
  ResourcedClientImpl(const ResourcedClientImpl&) = delete;
  ResourcedClientImpl& operator=(const ResourcedClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        resource_manager::kResourceManagerServiceName,
        dbus::ObjectPath(resource_manager::kResourceManagerServicePath));
  }

  // ResourcedClient interface:
  void GetAvailableMemoryKB(DBusMethodCallback<uint64_t> callback) override;

  void GetMemoryMarginsKB(
      DBusMethodCallback<MemoryMarginsKB> callback) override;

  void SetGameMode(bool state, DBusMethodCallback<bool> callback) override;

 private:
  // D-Bus response handlers:
  void HandleAvailableResponse(DBusMethodCallback<uint64_t> callback,
                               dbus::Response* response);
  void HandleMarginsResponse(DBusMethodCallback<MemoryMarginsKB> callback,
                             dbus::Response* response);
  void HandleSetGameModeResponse(DBusMethodCallback<bool> callback,
                                 bool status,
                                 dbus::Response* response);

  // Member variables.
  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<ResourcedClientImpl> weak_factory_{this};
};

void ResourcedClientImpl::HandleAvailableResponse(
    DBusMethodCallback<uint64_t> callback,
    dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  uint64_t result;
  if (!reader.PopUint64(&result)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(std::move(result));
}

void ResourcedClientImpl::HandleMarginsResponse(
    DBusMethodCallback<MemoryMarginsKB> callback,
    dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  uint64_t critical;
  if (!reader.PopUint64(&critical)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  uint64_t moderate;
  if (!reader.PopUint64(&moderate)) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(
      MemoryMarginsKB{.critical = critical, .moderate = moderate});
}

// Response will be true if entering game mode, false if exiting.
void ResourcedClientImpl::HandleSetGameModeResponse(
    DBusMethodCallback<bool> callback,
    bool status,
    dbus::Response* response) {
  if (!response) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(status);
}

void ResourcedClientImpl::GetAvailableMemoryKB(
    DBusMethodCallback<uint64_t> callback) {
  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kGetAvailableMemoryKBMethod);
  proxy_->CallMethod(
      &method_call, kResourcedDBusTimeoutMilliseconds,
      base::BindOnce(&ResourcedClientImpl::HandleAvailableResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ResourcedClientImpl::GetMemoryMarginsKB(
    DBusMethodCallback<MemoryMarginsKB> callback) {
  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kGetMemoryMarginsKBMethod);
  proxy_->CallMethod(
      &method_call, kResourcedDBusTimeoutMilliseconds,
      base::BindOnce(&ResourcedClientImpl::HandleMarginsResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ResourcedClientImpl::SetGameMode(bool status,
                                      DBusMethodCallback<bool> callback) {
  dbus::MethodCall method_call(resource_manager::kResourceManagerInterface,
                               resource_manager::kSetGameModeMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendByte(status);

  proxy_->CallMethod(
      &method_call, kResourcedDBusTimeoutMilliseconds,
      base::BindOnce(&ResourcedClientImpl::HandleSetGameModeResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback), status));
}

}  // namespace

ResourcedClient::ResourcedClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ResourcedClient::~ResourcedClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ResourcedClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ResourcedClientImpl())->Init(bus);
}

// static
void ResourcedClient::InitializeFake() {
  new FakeResourcedClient();
}

// static
void ResourcedClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
ResourcedClient* ResourcedClient::Get() {
  return g_instance;
}

}  // namespace chromeos
