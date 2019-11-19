// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/arc_appfuse_provider_client.h"

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

class ArcAppfuseProviderClientImpl : public ArcAppfuseProviderClient {
 public:
  ArcAppfuseProviderClientImpl() {}
  ~ArcAppfuseProviderClientImpl() override = default;

  // ArcAppfuseProviderClient override:
  void Mount(uint32_t uid,
             int32_t mount_id,
             DBusMethodCallback<base::ScopedFD> callback) override {
    dbus::MethodCall method_call(arc::appfuse::kArcAppfuseProviderInterface,
                                 arc::appfuse::kMountMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(uid);
    writer.AppendInt32(mount_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcAppfuseProviderClientImpl::OnFDMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Unmount(uint32_t uid,
               int32_t mount_id,
               VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc::appfuse::kArcAppfuseProviderInterface,
                                 arc::appfuse::kUnmountMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(uid);
    writer.AppendInt32(mount_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcAppfuseProviderClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OpenFile(uint32_t uid,
                int32_t mount_id,
                int32_t file_id,
                int32_t flags,
                DBusMethodCallback<base::ScopedFD> callback) override {
    dbus::MethodCall method_call(arc::appfuse::kArcAppfuseProviderInterface,
                                 arc::appfuse::kOpenFileMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(uid);
    writer.AppendInt32(mount_id);
    writer.AppendInt32(file_id);
    writer.AppendInt32(flags);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcAppfuseProviderClientImpl::OnFDMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::appfuse::kArcAppfuseProviderServiceName,
        dbus::ObjectPath(arc::appfuse::kArcAppfuseProviderServicePath));
  }

 private:
  // Runs the callback with the method call result.
  void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  void OnFDMethod(DBusMethodCallback<base::ScopedFD> callback,
                  dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    base::ScopedFD fd;
    if (!reader.PopFileDescriptor(&fd)) {
      LOG(ERROR) << "Failed to pop FD.";
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(fd));
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<ArcAppfuseProviderClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAppfuseProviderClientImpl);
};

}  // namespace

ArcAppfuseProviderClient::ArcAppfuseProviderClient() = default;

ArcAppfuseProviderClient::~ArcAppfuseProviderClient() = default;

// static
std::unique_ptr<ArcAppfuseProviderClient> ArcAppfuseProviderClient::Create() {
  return std::make_unique<ArcAppfuseProviderClientImpl>();
}

}  // namespace chromeos
