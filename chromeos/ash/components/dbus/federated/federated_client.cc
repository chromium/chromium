// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/federated/federated_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/federated/fake_federated_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

FederatedClient* g_instance = nullptr;

class FederatedClientImpl : public FederatedClient {
 public:
  FederatedClientImpl() = default;
  ~FederatedClientImpl() override = default;
  FederatedClientImpl(const FederatedClientImpl&) = delete;
  FederatedClientImpl& operator=(const FederatedClientImpl&) = delete;

  // FederatedClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override {
    dbus::MethodCall method_call(federated::kFederatedInterfaceName,
                                 federated::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.get());
    federated_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&FederatedClientImpl::OnBootstrapMojoConnectionResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback)));
  }

  void Init(dbus::Bus* const bus) {
    federated_service_proxy_ =
        bus->GetObjectProxy(federated::kFederatedServiceName,
                            dbus::ObjectPath(federated::kFederatedServicePath));
  }

 private:
  // Passes the success/failure of `dbus_response` on to `result_callback`.
  void OnBootstrapMojoConnectionResponse(
      base::OnceCallback<void(bool success)> result_callback,
      dbus::Response* const dbus_response) {
    const bool success = dbus_response != nullptr;
    std::move(result_callback).Run(success);
  }

  raw_ptr<dbus::ObjectProxy> federated_service_proxy_ = nullptr;
  // Must be last class member.
  base::WeakPtrFactory<FederatedClientImpl> weak_ptr_factory_{this};
};

}  // namespace

FederatedClient::FederatedClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FederatedClient::~FederatedClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void FederatedClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new FederatedClientImpl())->Init(bus);
}

// static
void FederatedClient::InitializeFake() {
  new FakeFederatedClient();
}

// static
void FederatedClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
FederatedClient* FederatedClient::Get() {
  return g_instance;
}

}  // namespace ash
