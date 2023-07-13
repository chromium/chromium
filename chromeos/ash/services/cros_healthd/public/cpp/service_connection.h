// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include <string>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::cros_healthd {

// Encapsulates a connection to the Chrome OS cros_healthd daemon via its Mojo
// interface.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  // Gets the interface for the bound diagnostics service. In production, this
  // implementation is provided by cros_healthd. To customize mojo disconnect
  // handler, use |BindDiagnosticsService| instead. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.`
  virtual mojom::CrosHealthdDiagnosticsService* GetDiagnosticsService() = 0;

  // Gets the interface for the bound probe service. In production, this
  // implementation is provided by cros_healthd. To customize mojo disconnect
  // handler, use |BindProbeService| instead. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual mojom::CrosHealthdProbeService* GetProbeService() = 0;

  // Gets the interface for the bound event service. In production, this
  // implementation is provided by cros_healthd. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual mojom::CrosHealthdEventService* GetEventService() = 0;

  // Gets the interface for the bound routines service. In production, this
  // implementation is provided by cros_healthd. See
  // src/chromeos/ash/services/cros_healthd/public/mojom/
  // cros_healthd_routines.mojom for details.
  virtual mojom::CrosHealthdRoutinesService* GetRoutinesService() = 0;

  // Binds |service| to an implementation of CrosHealthdDiagnosticsService. This
  // function is only used to customize mojo disconnect handler, otherwise use
  // |GetDiagnosticsService| directly.
  virtual void BindDiagnosticsService(
      mojo::PendingReceiver<mojom::CrosHealthdDiagnosticsService> service) = 0;

  // Binds |service| to an implementation of CrosHealthdProbeService. This
  // function is only used to customize mojo disconnect handler, otherwise use
  // |GetProbeService| directly.
  virtual void BindProbeService(
      mojo::PendingReceiver<mojom::CrosHealthdProbeService> service) = 0;

  // Fetch touchpad stack driver library name.
  virtual std::string FetchTouchpadLibraryName() = 0;

  // Calls FlushForTesting method on all mojo::Remote objects owned by
  // ServiceConnection. This method can be used for example to gracefully
  // observe destruction of the cros_healthd client.
  virtual void FlushForTesting() = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace ash::cros_healthd

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
