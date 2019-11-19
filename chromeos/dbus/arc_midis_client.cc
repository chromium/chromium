// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/arc_midis_client.h"

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

// ArcMidisClient is used to bootstrap a Mojo connection with midis.
// The BootstrapMojoConnection callback should be called during browser
// initialization. It is expected that the midis will be started up / taken down
// during browser startup / shutdown respectively.
class ArcMidisClientImpl : public ArcMidisClient {
 public:
  ArcMidisClientImpl() {}

  ~ArcMidisClientImpl() override = default;

  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(midis::kMidisInterfaceName,
                                 midis::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);

    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcMidisClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(midis::kMidisServiceName,
                                 dbus::ObjectPath(midis::kMidisServicePath));
  }

 private:
  void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcMidisClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcMidisClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcMidisClient

// static
std::unique_ptr<ArcMidisClient> ArcMidisClient::Create() {
  return std::make_unique<ArcMidisClientImpl>();
}

}  // namespace chromeos
