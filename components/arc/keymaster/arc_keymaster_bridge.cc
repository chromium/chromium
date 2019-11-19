// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/keymaster/arc_keymaster_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "chromeos/dbus/arc_keymaster_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace arc {
namespace {

// Singleton factory for ArcKeymasterBridge
class ArcKeymasterBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcKeymasterBridge,
          ArcKeymasterBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcKeymasterBridgeFactory";

  static ArcKeymasterBridgeFactory* GetInstance() {
    return base::Singleton<ArcKeymasterBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcKeymasterBridgeFactory>;
  ArcKeymasterBridgeFactory() = default;
  ~ArcKeymasterBridgeFactory() override = default;
};

}  // namespace

// static
ArcKeymasterBridge* ArcKeymasterBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcKeymasterBridgeFactory::GetForBrowserContext(context);
}

ArcKeymasterBridge::ArcKeymasterBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), weak_factory_(this) {
  arc_bridge_service_->keymaster()->SetHost(this);
}

ArcKeymasterBridge::~ArcKeymasterBridge() {
  arc_bridge_service_->keymaster()->SetHost(nullptr);
}

void ArcKeymasterBridge::GetServer(GetServerCallback callback) {
  if (!keymaster_server_proxy_.is_bound()) {
    BootstrapMojoConnection(std::move(callback));
    return;
  }
  std::move(callback).Run(std::move(keymaster_server_proxy_));
}

void ArcKeymasterBridge::OnBootstrapMojoConnection(GetServerCallback callback,
                                                   bool result) {
  if (!result) {
    LOG(ERROR) << "Error bootstrapping Mojo in arc-keymasterd.";
    keymaster_server_proxy_.reset();
    std::move(callback).Run(nullptr);
    return;
  }
  DVLOG(1) << "Success bootstrapping Mojo in arc-keymasterd.";
  std::move(callback).Run(std::move(keymaster_server_proxy_));
}

void ArcKeymasterBridge::BootstrapMojoConnection(GetServerCallback callback) {
  DVLOG(1) << "Bootstrapping arc-keymasterd Mojo connection via D-Bus.";
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe("arc-keymaster-pipe");
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());

  keymaster_server_proxy_.Bind(mojo::InterfacePtrInfo<mojom::KeymasterServer>(
      std::move(server_pipe), 0u));
  DVLOG(1) << "Bound remote KeymasterServer interface to pipe.";
  keymaster_server_proxy_.set_connection_error_handler(
      base::BindOnce(&mojo::InterfacePtr<mojom::KeymasterServer>::reset,
                     base::Unretained(&keymaster_server_proxy_)));
  chromeos::DBusThreadManager::Get()
      ->GetArcKeymasterClient()
      ->BootstrapMojoConnection(
          channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
          base::BindOnce(&ArcKeymasterBridge::OnBootstrapMojoConnection,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace arc
