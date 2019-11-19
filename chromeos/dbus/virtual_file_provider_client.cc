// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/virtual_file_provider_client.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class VirtualFileProviderClientImpl : public VirtualFileProviderClient {
 public:
  VirtualFileProviderClientImpl() {}
  ~VirtualFileProviderClientImpl() override = default;

  // VirtualFileProviderClient override:
  void OpenFile(int64_t size, OpenFileCallback callback) override {
    dbus::MethodCall method_call(
        virtual_file_provider::kVirtualFileProviderInterface,
        virtual_file_provider::kOpenFileMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(size);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&VirtualFileProviderClientImpl::OnOpenFile,
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
  // Runs the callback with OpenFile method call result.
  void OnOpenFile(OpenFileCallback callback, dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::string(), base::ScopedFD());
      return;
    }
    dbus::MessageReader reader(response);
    std::string id;
    base::ScopedFD fd;
    if (!reader.PopString(&id) || !reader.PopFileDescriptor(&fd)) {
      LOG(ERROR) << "Invalid method call result.";
      std::move(callback).Run(std::string(), base::ScopedFD());
      return;
    }
    std::move(callback).Run(id, std::move(fd));
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<VirtualFileProviderClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VirtualFileProviderClientImpl);
};

}  // namespace

VirtualFileProviderClient::VirtualFileProviderClient() = default;

VirtualFileProviderClient::~VirtualFileProviderClient() = default;

// static
std::unique_ptr<VirtualFileProviderClient> VirtualFileProviderClient::Create() {
  return std::make_unique<VirtualFileProviderClientImpl>();
}

}  // namespace chromeos
