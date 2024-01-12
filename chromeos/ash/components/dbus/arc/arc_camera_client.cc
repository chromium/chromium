// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_camera_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_camera_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcCameraClient* g_instance = nullptr;

class ArcCameraClientImpl : public ArcCameraClient {
 public:
  explicit ArcCameraClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
            arc_camera::kArcCameraServiceName,
            dbus::ObjectPath(arc_camera::kArcCameraServicePath))) {}

  ArcCameraClientImpl(const ArcCameraClientImpl&) = delete;
  ArcCameraClientImpl& operator=(const ArcCameraClientImpl&) = delete;

  ~ArcCameraClientImpl() override = default;

  // ArcCameraClient overrides:
  void StartService(int fd,
                    const std::string& token,
                    chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc_camera::kArcCameraServiceInterface,
                                 "StartService");
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd);
    writer.AppendString(token);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcCameraClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnVoidMethod(chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response) {
    std::move(callback).Run(response);
  }

  raw_ptr<dbus::ObjectProxy> proxy_;
  base::WeakPtrFactory<ArcCameraClientImpl> weak_ptr_factory_{this};
};

}  // namespace

ArcCameraClient::ArcCameraClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ArcCameraClient::~ArcCameraClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ArcCameraClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ArcCameraClientImpl(bus);
}

// static
void ArcCameraClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (to
  // allow test properties to be set).
  if (!FakeArcCameraClient::Get())
    new FakeArcCameraClient();
}

// static
void ArcCameraClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ArcCameraClient* ArcCameraClient::Get() {
  return g_instance;
}

}  // namespace ash
