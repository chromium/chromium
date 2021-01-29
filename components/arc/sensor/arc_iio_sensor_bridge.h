// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SENSOR_ARC_IIO_SENSOR_BRIDGE_H_
#define COMPONENTS_ARC_SENSOR_ARC_IIO_SENSOR_BRIDGE_H_

#include "components/arc/mojom/iio_sensor.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Sensor-related requests from the ARC container.
class ArcIioSensorBridge : public KeyedService, public mojom::IioSensorHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcIioSensorBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcIioSensorBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcIioSensorBridge() override;
  ArcIioSensorBridge(const ArcIioSensorBridge&) = delete;
  ArcIioSensorBridge& operator=(const ArcIioSensorBridge&) = delete;

  // mojom::IioSensorHost overrides:
  void RegisterSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SENSOR_ARC_IIO_SENSOR_BRIDGE_H_
