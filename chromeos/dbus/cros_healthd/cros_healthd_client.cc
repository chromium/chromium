// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

CrosHealthdClient* g_instance = nullptr;

// Production implementation of CrosHealthdClient.
class CrosHealthdClientImpl : public CrosHealthdClient {
 public:
  CrosHealthdClientImpl() = default;
  ~CrosHealthdClientImpl() override = default;

  // CrosHealthdClient overrides:
  mojo::Remote<cros_healthd::mojom::CrosHealthdService> BootstrapMojoConnection(
      base::OnceCallback<void(bool success)> result_callback) override {
    mojo::PlatformChannel platform_channel;

    // Prepare a Mojo invitation to send through |platform_channel|.
    mojo::OutgoingInvitation invitation;
    // Include an initial Mojo pipe in the invitation.
    mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(
        diagnostics::kCrosHealthdMojoConnectionChannelToken);
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   base::kNullProcessHandle,
                                   platform_channel.TakeLocalEndpoint());

    // Bind our end of |pipe| to our CrosHealthdServicePtr. The daemon should
    // bind its end to a CrosHealthdService implementation.
    mojo::Remote<cros_healthd::mojom::CrosHealthdService> cros_healthd_service;
    cros_healthd_service.Bind(
        mojo::PendingRemote<cros_healthd::mojom::CrosHealthdService>(
            std::move(pipe), 0u /* version */));

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
            weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));

    return cros_healthd_service;
  }

  void Init(dbus::Bus* const bus) {
    cros_healthd_service_proxy_ = bus->GetObjectProxy(
        diagnostics::kCrosHealthdServiceName,
        dbus::ObjectPath(diagnostics::kCrosHealthdServicePath));
  }

 private:
  dbus::ObjectProxy* cros_healthd_service_proxy_ = nullptr;

  // Passes the success/failure of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      base::OnceCallback<void(bool success)> result_callback,
      dbus::Response* const dbus_response) {
    const bool success = dbus_response != nullptr;
    std::move(result_callback).Run(success);
  }

  // Must be last class member.
  base::WeakPtrFactory<CrosHealthdClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdClientImpl);
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

}  // namespace chromeos
