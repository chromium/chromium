// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_obb_mounter_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_obb_mounter_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcObbMounterClient* g_instance = nullptr;

class ArcObbMounterClientImpl : public ArcObbMounterClient {
 public:
  ArcObbMounterClientImpl() {}

  ArcObbMounterClientImpl(const ArcObbMounterClientImpl&) = delete;
  ArcObbMounterClientImpl& operator=(const ArcObbMounterClientImpl&) = delete;

  ~ArcObbMounterClientImpl() override = default;

  // ArcObbMounterClient override:
  void MountObb(const std::string& obb_file,
                const std::string& mount_path,
                int32_t owner_gid,
                chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc::obb_mounter::kArcObbMounterInterface,
                                 arc::obb_mounter::kMountObbMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(obb_file);
    writer.AppendString(mount_path);
    writer.AppendInt32(owner_gid);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcObbMounterClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UnmountObb(const std::string& mount_path,
                  chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc::obb_mounter::kArcObbMounterInterface,
                                 arc::obb_mounter::kUnmountObbMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(mount_path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcObbMounterClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::obb_mounter::kArcObbMounterServiceName,
        dbus::ObjectPath(arc::obb_mounter::kArcObbMounterServicePath));
  }

 private:
  // Runs the callback with the method call result.
  void OnVoidDBusMethod(chromeos::VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<ArcObbMounterClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
ArcObbMounterClient* ArcObbMounterClient::Get() {
  return g_instance;
}

// static
void ArcObbMounterClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcObbMounterClientImpl())->Init(bus);
}

// static
void ArcObbMounterClient::InitializeFake() {
  (new FakeArcObbMounterClient())->Init(nullptr);
}

// static
void ArcObbMounterClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ArcObbMounterClient::ArcObbMounterClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcObbMounterClient::~ArcObbMounterClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
