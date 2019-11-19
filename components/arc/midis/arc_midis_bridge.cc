// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/midis/arc_midis_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/dbus/arc_midis_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace arc {
namespace {

// Singleton factory for ArcMidisBridge
class ArcMidisBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcMidisBridge,
          ArcMidisBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcMidisBridgeFactory";

  static ArcMidisBridgeFactory* GetInstance() {
    return base::Singleton<ArcMidisBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcMidisBridgeFactory>;
  ArcMidisBridgeFactory() = default;
  ~ArcMidisBridgeFactory() override = default;
};

}  // namespace

// static
ArcMidisBridge* ArcMidisBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcMidisBridgeFactory::GetForBrowserContext(context);
}

ArcMidisBridge::ArcMidisBridge(content::BrowserContext* context,
                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->midis()->SetHost(this);
}

ArcMidisBridge::~ArcMidisBridge() {
  arc_bridge_service_->midis()->SetHost(nullptr);
}

void ArcMidisBridge::OnBootstrapMojoConnection(
    mojom::MidisServerRequest request,
    mojom::MidisClientPtr client_ptr,
    bool result) {
  if (!result) {
    LOG(ERROR) << "ArcMidisBridge had a failure in D-Bus with the daemon.";
    midis_host_ptr_.reset();
    return;
  }
  if (!midis_host_ptr_) {
    VLOG(1) << "ArcMidisBridge was already lost.";
    return;
  }
  DVLOG(1) << "ArcMidisBridge succeeded with Mojo bootstrapping.";
  midis_host_ptr_->Connect(std::move(request), std::move(client_ptr));
}

void ArcMidisBridge::Connect(mojom::MidisServerRequest request,
                             mojom::MidisClientPtr client_ptr) {
  if (midis_host_ptr_.is_bound()) {
    DVLOG(1) << "Re-using bootstrap connection for MidisServer Connect.";
    midis_host_ptr_->Connect(std::move(request), std::move(client_ptr));
    return;
  }
  DVLOG(1) << "Bootstrapping the Midis connection via D-Bus.";
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe("arc-midis-pipe");
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());

  midis_host_ptr_.Bind(
      mojo::InterfacePtrInfo<mojom::MidisHost>(std::move(server_pipe), 0u));
  DVLOG(1) << "Bound remote MidisHost interface to pipe.";

  midis_host_ptr_.set_connection_error_handler(base::BindOnce(
      &ArcMidisBridge::OnMojoConnectionError, weak_factory_.GetWeakPtr()));
  chromeos::DBusThreadManager::Get()
      ->GetArcMidisClient()
      ->BootstrapMojoConnection(
          channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
          base::BindOnce(&ArcMidisBridge::OnBootstrapMojoConnection,
                         weak_factory_.GetWeakPtr(), std::move(request),
                         std::move(client_ptr)));
}

void ArcMidisBridge::OnMojoConnectionError() {
  LOG(ERROR) << "ArcMidisBridge Mojo connection lost.";
  midis_host_ptr_.reset();
}

}  // namespace arc
