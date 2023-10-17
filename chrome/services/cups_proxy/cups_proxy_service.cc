// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/cups_proxy_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/proxy_manager.h"
#include "chromeos/ash/components/dbus/cups_proxy/cups_proxy_client.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cups_proxy {

namespace {

CupsProxyService* GetCupsProxyService() {
  static base::NoDestructor<CupsProxyService> service;
  return service.get();
}

}  // namespace

CupsProxyService::CupsProxyService() = default;
CupsProxyService::~CupsProxyService() = default;

// static
void CupsProxyService::Spawn(
    std::unique_ptr<CupsProxyServiceDelegate> delegate) {
  GetCupsProxyService()->BindToCupsProxyDaemon(std::move(delegate));
}

// static
void CupsProxyService::Shutdown() {
  GetCupsProxyService()->ShutdownImpl();
}

void CupsProxyService::BindToCupsProxyDaemon(
    std::unique_ptr<CupsProxyServiceDelegate> delegate) {
  DCHECK(delegate);
  if (bootstrap_attempted_) {
    return;
  }

  mojo::PlatformChannel platform_channel;

  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(
      ::printing::kBootstrapMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 platform_channel.TakeLocalEndpoint());

  // Bind our end of pipe to our |proxy_manager_|. The daemon should
  // bind its end to a remote<CupsProxier>;
  proxy_manager_ = ProxyManager::Create(
      mojo::PendingReceiver<mojom::CupsProxier>(std::move(pipe)),
      std::move(delegate));

  // Send the file descriptor for the other end of |platform_channel| to the
  // CupsProxyDaemon over D-Bus.
  ash::CupsProxyClient::Get()->BootstrapMojoConnection(
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&CupsProxyService::OnBindToCupsProxyDaemon,
                     weak_factory_.GetWeakPtr()));
  bootstrap_attempted_ = true;
}

void CupsProxyService::OnBindToCupsProxyDaemon(bool success) {
  if (!success) {
    LOG(ERROR) << "CupsProxyService: bootstrap failure.";
    // TODO(crbug.com/945409): reset handler.
    return;
  }

  DVLOG(1) << "CupsProxyService: bootstrap success!";
}

void CupsProxyService::ShutdownImpl() {
  proxy_manager_.reset();
}

}  // namespace cups_proxy
