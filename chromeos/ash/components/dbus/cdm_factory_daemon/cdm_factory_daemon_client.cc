// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/dbus/cdm_factory_daemon/cdm_factory_daemon_client.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cdm_factory_daemon/fake_cdm_factory_daemon_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

CdmFactoryDaemonClient* g_instance = nullptr;

// "Real" implementation of CdmFactoryDaemonClient talking to the
// CdmFactoryDaemon on the Chrome OS side.
class CdmFactoryDaemonClientImpl : public CdmFactoryDaemonClient {
 public:
  CdmFactoryDaemonClientImpl() = default;

  CdmFactoryDaemonClientImpl(const CdmFactoryDaemonClientImpl&) = delete;
  CdmFactoryDaemonClientImpl& operator=(const CdmFactoryDaemonClientImpl&) =
      delete;

  ~CdmFactoryDaemonClientImpl() override = default;

  // CdmFactoryDaemonClient overrides:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> callback) override {
    dbus::MethodCall method_call(
        cdm_oemcrypto::kCdmFactoryDaemonServiceInterface,
        cdm_oemcrypto::kBootstrapCdmFactoryDaemonMojoConnection);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.get());
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CdmFactoryDaemonClientImpl::OnBootstrap,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        cdm_oemcrypto::kCdmFactoryDaemonServiceName,
        dbus::ObjectPath(cdm_oemcrypto::kCdmFactoryDaemonServicePath));
  }

 private:
  // Runs the callback with the method call result.
  void OnBootstrap(base::OnceCallback<void(bool success)> callback,
                   dbus::Response* response) {
    std::move(callback).Run(response != nullptr);
  }

  // D-Bus proxy for the CdmFactoryDaemon, not owned.
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  base::WeakPtrFactory<CdmFactoryDaemonClientImpl> weak_factory_{this};
};

}  // namespace

CdmFactoryDaemonClient::CdmFactoryDaemonClient() {
  CHECK(!g_instance);
  g_instance = this;
}

CdmFactoryDaemonClient::~CdmFactoryDaemonClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CdmFactoryDaemonClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new CdmFactoryDaemonClientImpl())->Init(bus);
}

// static
void CdmFactoryDaemonClient::InitializeFake() {
  new FakeCdmFactoryDaemonClient();
}

// static
void CdmFactoryDaemonClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
CdmFactoryDaemonClient* CdmFactoryDaemonClient::Get() {
  return g_instance;
}

}  // namespace ash
