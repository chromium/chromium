// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cros_healthd/cros_healthd_client.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::cros_healthd {

namespace {

CrosHealthdClient* g_instance = nullptr;

// Production implementation of CrosHealthdClient.
class CrosHealthdClientImpl : public CrosHealthdClient {
 public:
  CrosHealthdClientImpl() = default;

  CrosHealthdClientImpl(const CrosHealthdClientImpl&) = delete;
  CrosHealthdClientImpl& operator=(const CrosHealthdClientImpl&) = delete;

  ~CrosHealthdClientImpl() override = default;

  // CrosHealthdClient overrides:
  mojo::Remote<mojom::CrosHealthdServiceFactory> BootstrapMojoConnection(
      BootstrapMojoConnectionCallback result_callback) override {
    // Invalidate any pending attempts to bootstrap the mojo connection.
    bootstrap_weak_ptr_factory_.InvalidateWeakPtrs();

    mojo::PlatformChannel platform_channel;
    // Prepare a Mojo invitation to send through |platform_channel|.
    mojo::OutgoingInvitation invitation;
    // Include an initial Mojo pipe in the invitation.
    mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(
        diagnostics::kCrosHealthdMojoConnectionChannelToken);
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   base::kNullProcessHandle,
                                   platform_channel.TakeLocalEndpoint());

    // Bind our end of |pipe| to our CrosHealthdService remote. The daemon
    // should bind its end to a CrosHealthdService implementation.
    mojo::Remote<mojom::CrosHealthdServiceFactory> cros_healthd_service_factory;
    cros_healthd_service_factory.Bind(
        mojo::PendingRemote<mojom::CrosHealthdServiceFactory>(
            std::move(pipe), 0u /* version */));

    cros_healthd_service_proxy_->WaitForServiceToBeAvailable(base::BindOnce(
        &CrosHealthdClientImpl::OnDbusServiceAvailable,
        bootstrap_weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
        std::move(platform_channel)));

    return cros_healthd_service_factory;
  }

  void Init(dbus::Bus* const bus) {
    cros_healthd_service_proxy_ = bus->GetObjectProxy(
        diagnostics::kCrosHealthdServiceName,
        dbus::ObjectPath(diagnostics::kCrosHealthdServicePath));
  }

 private:
  dbus::ObjectProxy* cros_healthd_service_proxy_ = nullptr;

  // When the service is available, attempt to bootstrap the mojo connection.
  void OnDbusServiceAvailable(BootstrapMojoConnectionCallback result_callback,
                              mojo::PlatformChannel platform_channel,
                              bool success) {
    // The service is not available.
    if (!success) {
      std::move(result_callback).Run(false);
      return;
    }

    dbus::MethodCall method_call(
        diagnostics::kCrosHealthdServiceInterface,
        diagnostics::kCrosHealthdBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    base::ScopedFD fd =
        platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
    writer.AppendFileDescriptor(fd.get());
    writer.AppendBool(/*is_chrome=*/true);
    cros_healthd_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &CrosHealthdClientImpl::OnBootstrapMojoConnectionResponse,
            bootstrap_weak_ptr_factory_.GetWeakPtr(),
            std::move(result_callback)));
  }

  // Passes the success/failure of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      BootstrapMojoConnectionCallback result_callback,
      dbus::Response* const dbus_response) {
    const bool success = dbus_response != nullptr;
    std::move(result_callback).Run(success);
  }

  // Must be last class member. This WeakPtrFactory is specifically for the
  // bootstrapping callbacks.
  base::WeakPtrFactory<CrosHealthdClientImpl> bootstrap_weak_ptr_factory_{this};
};

}  // namespace

CrosHealthdClient::CrosHealthdClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CrosHealthdClient::~CrosHealthdClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CrosHealthdClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new CrosHealthdClientImpl())->Init(bus);
}

// static
void CrosHealthdClient::InitializeFake() {
  new FakeCrosHealthdClient();
}

// static
void CrosHealthdClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
CrosHealthdClient* CrosHealthdClient::Get() {
  return g_instance;
}

}  // namespace ash::cros_healthd
