// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/virtual_file_provider/virtual_file_provider_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

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

 protected:
  // DBusClient override.
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
      std::move(callback).Run(absl::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    std::string id;
    if (!reader.PopString(&id)) {
      LOG(ERROR) << "Invalid method call result.";
      std::move(callback).Run(absl::nullopt);
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

  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<VirtualFileProviderClientImpl> weak_ptr_factory_{this};
};

}  // namespace

VirtualFileProviderClient::VirtualFileProviderClient() = default;

VirtualFileProviderClient::~VirtualFileProviderClient() = default;

// static
std::unique_ptr<VirtualFileProviderClient> VirtualFileProviderClient::Create() {
  return std::make_unique<VirtualFileProviderClientImpl>();
}

}  // namespace chromeos
