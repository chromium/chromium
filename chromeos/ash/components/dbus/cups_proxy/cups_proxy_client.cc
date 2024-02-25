// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cups_proxy/cups_proxy_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cups_proxy/fake_cups_proxy_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

CupsProxyClient* g_instance = nullptr;

class CupsProxyClientImpl : public CupsProxyClient {
 public:
  CupsProxyClientImpl() = default;

  CupsProxyClientImpl(const CupsProxyClientImpl&) = delete;
  CupsProxyClientImpl& operator=(const CupsProxyClientImpl&) = delete;

  ~CupsProxyClientImpl() override = default;

  // CupsProxyClient overrides.
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override {
    dbus::MethodCall method_call(::printing::kCupsProxyDaemonInterface,
                                 ::printing::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.get());
    daemon_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CupsProxyClientImpl::OnBootstrapMojoConnectionResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    daemon_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* const bus) {
    daemon_proxy_ =
        bus->GetObjectProxy(::printing::kCupsProxyDaemonName,
                            dbus::ObjectPath(::printing::kCupsProxyDaemonPath));
  }

 private:
  raw_ptr<dbus::ObjectProxy> daemon_proxy_ = nullptr;

  // Passes the success/failure of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      base::OnceCallback<void(bool success)> result_callback,
      dbus::Response* const dbus_response) {
    const bool success = dbus_response != nullptr;
    std::move(result_callback).Run(success);
  }

  // Must be last class member.
  base::WeakPtrFactory<CupsProxyClientImpl> weak_ptr_factory_{this};
};

}  // namespace

CupsProxyClient::CupsProxyClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CupsProxyClient::~CupsProxyClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CupsProxyClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new CupsProxyClientImpl())->Init(bus);
}

// static
void CupsProxyClient::InitializeFake() {
  new FakeCupsProxyClient();
}

// static
void CupsProxyClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
CupsProxyClient* CupsProxyClient::Get() {
  return g_instance;
}

}  // namespace ash
