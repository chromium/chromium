// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_keymaster_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_keymaster_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcKeymasterClient* g_instance = nullptr;

void OnVoidDBusMethod(chromeos::VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

class ArcKeymasterClientImpl : public ArcKeymasterClient {
 public:
  ArcKeymasterClientImpl() = default;

  ArcKeymasterClientImpl(const ArcKeymasterClientImpl&) = delete;
  ArcKeymasterClientImpl& operator=(const ArcKeymasterClientImpl&) = delete;

  ~ArcKeymasterClientImpl() override = default;

  void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::keymaster::kArcKeymasterInterfaceName,
        arc::keymaster::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);

    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
  }

  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::keymaster::kArcKeymasterServiceName,
        dbus::ObjectPath(arc::keymaster::kArcKeymasterServicePath));
  }

 private:
  // Owned by the D-Bus implementation, who outlives this class.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcKeymasterClient

// static
ArcKeymasterClient* ArcKeymasterClient::Get() {
  return g_instance;
}

// static
void ArcKeymasterClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcKeymasterClientImpl())->Init(bus);
}

// static
void ArcKeymasterClient::InitializeFake() {
  (new FakeArcKeymasterClient())->Init(nullptr);
}

// static
void ArcKeymasterClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ArcKeymasterClient::ArcKeymasterClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcKeymasterClient::~ArcKeymasterClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
