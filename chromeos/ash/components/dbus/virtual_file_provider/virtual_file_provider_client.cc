// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/virtual_file_provider/virtual_file_provider_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/virtual_file_provider/fake_virtual_file_provider_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

VirtualFileProviderClient* g_instance = nullptr;

class VirtualFileProviderClientImpl : public VirtualFileProviderClient {
 public:
  VirtualFileProviderClientImpl() {}

  VirtualFileProviderClientImpl(const VirtualFileProviderClientImpl&) = delete;
  VirtualFileProviderClientImpl& operator=(
      const VirtualFileProviderClientImpl&) = delete;

  ~VirtualFileProviderClientImpl() override = default;

  // VirtualFileProviderClient override:
  void GenerateVirtualFileId(int64_t size,
                             GenerateVirtualFileIdCallback callback) override {
    dbus::MethodCall method_call(
        virtual_file_provider::kVirtualFileProviderInterface,
        virtual_file_provider::kGenerateVirtualFileIdMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(size);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&VirtualFileProviderClientImpl::OnGenerateVirtualFileId,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
  void OpenFileById(const std::string& id,
                    OpenFileByIdCallback callback) override {
    dbus::MethodCall method_call(
        virtual_file_provider::kVirtualFileProviderInterface,
        virtual_file_provider::kOpenFileByIdMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(std::move(id));
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&VirtualFileProviderClientImpl::OnOpenFileById,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        virtual_file_provider::kVirtualFileProviderServiceName,
        dbus::ObjectPath(
            virtual_file_provider::kVirtualFileProviderServicePath));
  }

 private:
  // Runs the callback with GenerateVirtualFileId method call result.
  void OnGenerateVirtualFileId(GenerateVirtualFileIdCallback callback,
                               dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string id;
    if (!reader.PopString(&id)) {
      LOG(ERROR) << "Invalid method call result.";
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(std::move(id));
  }

  // Runs the callback with OpenFileById method call result.
  void OnOpenFileById(OpenFileByIdCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::ScopedFD());
      return;
    }
    dbus::MessageReader reader(response);
    base::ScopedFD fd;
    if (!reader.PopFileDescriptor(&fd)) {
      LOG(ERROR) << "Invalid method call result.";
      std::move(callback).Run(base::ScopedFD());
      return;
    }
    std::move(callback).Run(std::move(fd));
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<VirtualFileProviderClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
VirtualFileProviderClient* VirtualFileProviderClient::Get() {
  return g_instance;
}

// static
void VirtualFileProviderClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new VirtualFileProviderClientImpl())->Init(bus);
}

// static
void VirtualFileProviderClient::InitializeFake() {
  (new FakeVirtualFileProviderClient())->Init(nullptr);
}

// static
void VirtualFileProviderClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

VirtualFileProviderClient::VirtualFileProviderClient() {
  CHECK(!g_instance);
  g_instance = this;
}

VirtualFileProviderClient::~VirtualFileProviderClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
