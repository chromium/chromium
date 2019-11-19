// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/arc_keymaster_client.h"

#include <memory>
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

void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

class ArcKeymasterClientImpl : public ArcKeymasterClient {
 public:
  ArcKeymasterClientImpl() = default;
  ~ArcKeymasterClientImpl() override = default;

  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(
        arc::keymaster::kArcKeymasterInterfaceName,
        arc::keymaster::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);

    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc::keymaster::kArcKeymasterServiceName,
        dbus::ObjectPath(arc::keymaster::kArcKeymasterServicePath));
  }

 private:
  // Owned by the D-Bus implementation, who outlives this class.
  dbus::ObjectProxy* proxy_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcKeymasterClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcKeymasterClient

ArcKeymasterClient::ArcKeymasterClient() = default;
ArcKeymasterClient::~ArcKeymasterClient() = default;

// static
std::unique_ptr<ArcKeymasterClient> ArcKeymasterClient::Create() {
  return std::make_unique<ArcKeymasterClientImpl>();
}

}  // namespace chromeos
