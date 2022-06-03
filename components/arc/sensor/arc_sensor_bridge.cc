// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/sensor/arc_sensor_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/memory/singleton.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/arc/arc_sensor_service_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcSensorBridge.
class ArcSensorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSensorBridge,
          ArcSensorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSensorBridgeFactory";

  static ArcSensorBridgeFactory* GetInstance() {
    return base::Singleton<ArcSensorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSensorBridgeFactory>;
  ArcSensorBridgeFactory() = default;
  ~ArcSensorBridgeFactory() override = default;
};

}  // namespace

// static
ArcSensorBridge* ArcSensorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSensorBridgeFactory::GetForBrowserContext(context);
}

ArcSensorBridge::ArcSensorBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->sensor()->SetHost(this);
}

ArcSensorBridge::~ArcSensorBridge() {
  arc_bridge_service_->sensor()->SetHost(nullptr);
}

void ArcSensorBridge::GetSensorService(
    mojo::PendingReceiver<mojom::SensorService> receiver) {
  auto token = base::NumberToString(base::RandUint64());
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(token);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  // Fuse the receiver with the attached pipe so that the requester's pipe will
  // be connected to the sensor service.
  MojoResult result =
      mojo::FuseMessagePipes(receiver.PassPipe(), std::move(pipe));
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "FuseMessagePipes() failed: " << result;
    return;
  }
  base::ScopedFD fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
  chromeos::ArcSensorServiceClient::Get()->BootstrapMojoConnection(
      fd.get(), token, base::BindOnce([](bool success) {}));
}

}  // namespace arc
