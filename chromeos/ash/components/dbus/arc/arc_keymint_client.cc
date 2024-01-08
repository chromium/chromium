// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/arc_keymint_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_keymint_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcKeyMintClient* g_instance = nullptr;

void OnVoidDBusMethod(chromeos::VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

class ArcKeyMintClientImpl : public ArcKeyMintClient {
 public:
  ArcKeyMintClientImpl() = default;

  ArcKeyMintClientImpl(const ArcKeyMintClientImpl&) = delete;
  ArcKeyMintClientImpl& operator=(const ArcKeyMintClientImpl&) = delete;

  ~ArcKeyMintClientImpl() override = default;

  void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc::keymint::kArcKeyMintInterfaceName,
                                 arc::keymint::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);

    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
  }

  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::keymint::kArcKeyMintServiceName,
        dbus::ObjectPath(arc::keymint::kArcKeyMintServicePath));
  }

 private:
  // Owned by the D-Bus implementation, who outlives this class.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcKeyMintClient

// static
ArcKeyMintClient* ArcKeyMintClient::Get() {
  return g_instance;
}

// static
void ArcKeyMintClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcKeyMintClientImpl())->Init(bus);
}

// static
void ArcKeyMintClient::InitializeFake() {
  (new FakeArcKeyMintClient())->Init(nullptr);
}

// static
void ArcKeyMintClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ArcKeyMintClient::ArcKeyMintClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcKeyMintClient::~ArcKeyMintClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
