// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_sensor_service_client.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_sensor_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcSensorServiceClient* g_instance = nullptr;

class ArcSensorServiceClientImpl : public ArcSensorServiceClient {
 public:
  explicit ArcSensorServiceClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(
            arc::sensor::kArcSensorServiceServiceName,
            dbus::ObjectPath(arc::sensor::kArcSensorServiceServicePath))) {}

  ~ArcSensorServiceClientImpl() override = default;

  ArcSensorServiceClientImpl(const ArcSensorServiceClientImpl&) = delete;
  ArcSensorServiceClientImpl& operator=(const ArcSensorServiceClientImpl&) =
      delete;

  // ArcSensorServiceClient overrides:
  void BootstrapMojoConnection(
      int fd,
      const std::string& token,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc::sensor::kArcSensorServiceInterface,
                                 arc::sensor::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd);
    writer.AppendString(token);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcSensorServiceClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnVoidMethod(chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response) {
    std::move(callback).Run(response);
  }

  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<ArcSensorServiceClientImpl> weak_ptr_factory_{this};
};

}  // namespace

ArcSensorServiceClient::ArcSensorServiceClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ArcSensorServiceClient::~ArcSensorServiceClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ArcSensorServiceClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ArcSensorServiceClientImpl(bus);
}

// static
void ArcSensorServiceClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (to
  // allow test properties to be set).
  if (!FakeArcSensorServiceClient::Get())
    new FakeArcSensorServiceClient();
}

// static
void ArcSensorServiceClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ArcSensorServiceClient* ArcSensorServiceClient::Get() {
  return g_instance;
}

}  // namespace ash
