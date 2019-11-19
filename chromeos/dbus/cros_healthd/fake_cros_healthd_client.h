// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

// Fake implementation of CrosHealthdClient.
class COMPONENT_EXPORT(CROS_HEALTHD) FakeCrosHealthdClient
    : public CrosHealthdClient {
 public:
  using TelemetryInfoPtr = cros_healthd::mojom::TelemetryInfoPtr;

  // FakeCrosHealthdClient can be embedded in unit tests, but the
  // InitializeFake/Shutdown pattern should be preferred. Constructing the
  // instance will set the global instance for the fake and for the base class,
  // so the static Get() accessor can be used with that pattern.
  FakeCrosHealthdClient();
  ~FakeCrosHealthdClient() override;

  // Checks that a FakeCrosHealthdClient instance was initialized and returns
  // it.
  static FakeCrosHealthdClient* Get();

  // CrosHealthdClient overrides:
  mojo::Remote<cros_healthd::mojom::CrosHealthdService> BootstrapMojoConnection(
      base::OnceCallback<void(bool success)> result_callback) override;

  // Set the TelemetryInfoPtr that will be used in the response to any
  // ProbeTelemetryInfo IPCs received.
  void SetProbeTelemetryInfoResponseForTesting(TelemetryInfoPtr& info);

 private:
  cros_healthd::FakeCrosHealthdService fake_service_;
  mojo::Receiver<cros_healthd::mojom::CrosHealthdService> receiver_{
      &fake_service_};

  DISALLOW_COPY_AND_ASSIGN(FakeCrosHealthdClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
