// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/dbus/arc/arc_midis_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_midis_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ArcMidisClient* g_instance = nullptr;

// ArcMidisClient is used to bootstrap a Mojo connection with midis.
// The BootstrapMojoConnection callback should be called during browser
// initialization. It is expected that the midis will be started up / taken down
// during browser startup / shutdown respectively.
class ArcMidisClientImpl : public ArcMidisClient {
 public:
  ArcMidisClientImpl() {}

  ArcMidisClientImpl(const ArcMidisClientImpl&) = delete;
  ArcMidisClientImpl& operator=(const ArcMidisClientImpl&) = delete;

  ~ArcMidisClientImpl() override = default;

  void BootstrapMojoConnection(
      base::ScopedFD fd,
      chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(midis::kMidisInterfaceName,
                                 midis::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);

    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ArcMidisClientImpl::OnVoidDBusMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(midis::kMidisServiceName,
                                 dbus::ObjectPath(midis::kMidisServicePath));
  }

 private:
  void OnVoidDBusMethod(chromeos::VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcMidisClientImpl> weak_ptr_factory_{this};
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcMidisClient

// static
ArcMidisClient* ArcMidisClient::Get() {
  return g_instance;
}

// static
void ArcMidisClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new ArcMidisClientImpl())->Init(bus);
}

// static
void ArcMidisClient::InitializeFake() {
  (new FakeArcMidisClient())->Init(nullptr);
}

// static
void ArcMidisClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

ArcMidisClient::ArcMidisClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ArcMidisClient::~ArcMidisClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
