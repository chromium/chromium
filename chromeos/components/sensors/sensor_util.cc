// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sensors/sensor_util.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {
namespace sensors {

bool BindSensorHalClient(mojo::PendingRemote<mojom::SensorHalClient> remote) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* dispatcher = SensorHalDispatcher::GetInstance();
  if (!dispatcher) {
    // In some unit tests, it's not initialized.
    return false;
  }

  dispatcher->RegisterClient(std::move(remote));
  return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service) {
    // In unit tests.
    return false;
  }

  if (!lacros_service
           ->IsSupported<chromeos::sensors::mojom::SensorHalClient>()) {
    return false;
  }

  lacros_service->BindSensorHalClient(std::move(remote));
  return true;
#else
#error "This file should only be used in either Ash-Chrome or Lacros-Chrome"
#endif
}

}  // namespace sensors
}  // namespace chromeos
