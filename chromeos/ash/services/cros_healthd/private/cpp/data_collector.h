// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_

#include "chromeos/ash/services/cros_healthd/private/mojom/cros_healthd_internal.mojom.h"
#include "chromeos/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cros_healthd::internal {

class DataCollector
    : public mojom::ChromiumDataCollector,
      public chromeos::mojo_service_manager::mojom::ServiceProvider,
      public chromeos::sensors::mojom::SensorHalClient {
 public:
  // Delegate class to be replaced for testing.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Get the touchpad library name.
    virtual std::string GetTouchpadLibraryName() = 0;
  };

  DataCollector();
  DataCollector(Delegate* delegate);
  DataCollector(const DataCollector&) = delete;
  DataCollector& operator=(const DataCollector&) = delete;
  ~DataCollector() override;

  // Binds new pipe and returns the mojo remote.
  mojo::PendingRemote<mojom::ChromiumDataCollector> BindNewPipeAndPassRemote();

 private:
  // mojom::ChromiumDataCollector overrides.
  void GetTouchscreenDevices(GetTouchscreenDevicesCallback callback) override;
  void GetTouchpadLibraryName(GetTouchpadLibraryNameCallback callback) override;
  void BindSensorService(
      mojo::PendingReceiver<chromeos::sensors::mojom::SensorService>
          pending_receiver) override;

  // chromeos::mojo_service_manager::mojom::ServiceProvider overrides.
  void Request(
      chromeos::mojo_service_manager::mojom::ProcessIdentityPtr identity,
      mojo::ScopedMessagePipeHandle receiver) override;

  // chromeos::sensors::mojom::SensorHalClient overrides:
  void SetUpChannel(mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
                        pending_remote) override;

  // Pointer to the delegate.
  Delegate* const delegate_;
  // The mojo receiver of service provider.
  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceProvider>
      provider_receiver_{this};
  // The mojo receiver set of data collector.
  mojo::ReceiverSet<mojom::ChromiumDataCollector> receiver_set_;
  // The mojo receiver of sensor hal client.
  mojo::Receiver<chromeos::sensors::mojom::SensorHalClient>
      sensor_client_receiver_{this};
  // Holds receiver for Healthd until the SensorHalClient is ready to bind it.
  mojo::PendingReceiver<chromeos::sensors::mojom::SensorService>
      sensor_service_pending_receiver_;
};

}  // namespace ash::cros_healthd::internal

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PRIVATE_CPP_DATA_COLLECTOR_H_
