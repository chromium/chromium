// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_

#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace diagnostics {

class SystemDataProvider : public mojom::SystemDataProvider {
 public:
  SystemDataProvider();

  ~SystemDataProvider() override;

  SystemDataProvider(const SystemDataProvider&) = delete;
  SystemDataProvider& operator=(const SystemDataProvider&) = delete;

  // mojom::SystemDataProvider:
  void GetSystemInfo(GetSystemInfoCallback callback) override;

 private:
  void BindCrosHealthdProbeServiceIfNeccessary();

  void OnProbeServiceDisconnect();

  void OnSystemInfoProbeResponse(
      GetSystemInfoCallback callback,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> probe_service_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_
