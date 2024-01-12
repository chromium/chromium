// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_appfuse_provider_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_appfuse_provider_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcAppfuseProviderClient* g_instance = nullptr;

class ArcAppfuseProviderClientImpl : public ArcAppfuseProviderClient {
 public:
  ArcAppfuseProviderClientImpl() = default;

  ArcAppfuseProviderClientImpl(const ArcAppfuseProviderClientImpl&) = delete;
  ArcAppfuseProviderClientImpl& operator=(const ArcAppfuseProviderClientImpl&) =
      delete;

  ~ArcAppfuseProviderClientImpl() override = default;

  // ArcAppfuseProviderClient override:
  void Mount(uint32_t uid,
             int32_t mount_id,
             chromeos::DBusMethodCallback<base::ScopedFD> callback) override {
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
               chromeos::VoidDBusMethodCallback callback) override {
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

  void OpenFile(
      uint32_t uid,
      int32_t mount_id,
      int32_t file_id,
      int32_t flags,
      chromeos::DBusMethodCallback<base::ScopedFD> callback) override {
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

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::appfuse::kArcAppfuseProviderServiceName,
        dbus::ObjectPath(arc::appfuse::kArcAppfuseProviderServicePath));
  }

 private:
  // Runs the callback with the method call result.
  void OnVoidDBusMethod(chromeos::VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  void OnFDMethod(chromeos::DBusMethodCallback<base::ScopedFD> callback,
                  dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    base::ScopedFD fd;
    if (!reader.PopFileDescriptor(&fd)) {
      LOG(ERROR) << "Failed to pop FD.";
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(std::move(fd));
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<ArcAppfuseProviderClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
ArcAppfuseProviderClient* ArcAppfuseProviderClient::Get() {
  return g_instance;
}

// static
void ArcAppfuseProviderClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcAppfuseProviderClientImpl())->Init(bus);
}

// static
void ArcAppfuseProviderClient::InitializeFake() {
  new FakeArcAppfuseProviderClient();
}

// static
void ArcAppfuseProviderClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ArcAppfuseProviderClient::ArcAppfuseProviderClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcAppfuseProviderClient::~ArcAppfuseProviderClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
