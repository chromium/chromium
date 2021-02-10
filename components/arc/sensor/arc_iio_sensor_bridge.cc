// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/sensor/arc_iio_sensor_bridge.h"

#include <utility>

#include "base/memory/singleton.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {

namespace {

// Singleton factory for ArcIioSensorBridge.
class ArcIioSensorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcIioSensorBridge,
          ArcIioSensorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcIioSensorBridgeFactory";

  static ArcIioSensorBridgeFactory* GetInstance() {
    return base::Singleton<ArcIioSensorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcIioSensorBridgeFactory>;
  ArcIioSensorBridgeFactory() = default;
  ~ArcIioSensorBridgeFactory() override = default;
};

}  // namespace

// static
ArcIioSensorBridge* ArcIioSensorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcIioSensorBridgeFactory::GetForBrowserContext(context);
}

ArcIioSensorBridge::ArcIioSensorBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->iio_sensor()->SetHost(this);
}

ArcIioSensorBridge::~ArcIioSensorBridge() {
  arc_bridge_service_->iio_sensor()->SetHost(nullptr);
}

void ArcIioSensorBridge::RegisterSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      std::move(remote));
}

}  // namespace arc
