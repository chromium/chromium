// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_

#include <memory>

#include "base/optional.h"
#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace base {
class RepeatingTimer;
}  // namespace base

namespace chromeos {
namespace diagnostics {

class SystemDataProvider : public mojom::SystemDataProvider,
                           public PowerManagerClient::Observer {
 public:
  SystemDataProvider();

  ~SystemDataProvider() override;

  SystemDataProvider(const SystemDataProvider&) = delete;
  SystemDataProvider& operator=(const SystemDataProvider&) = delete;

  // mojom::SystemDataProvider:
  void GetSystemInfo(GetSystemInfoCallback callback) override;
  void GetBatteryInfo(GetBatteryInfoCallback callback) override;
  void ObserveBatteryChargeStatus(
      mojo::PendingRemote<mojom::BatteryChargeStatusObserver> observer)
      override;
  void ObserveBatteryHealth(
      mojo::PendingRemote<mojom::BatteryHealthObserver> observer) override;

  // PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  void SetBatteryChargeStatusTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer);

  void SetBatteryHealthTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer);

 private:
  void BindCrosHealthdProbeServiceIfNeccessary();

  void OnProbeServiceDisconnect();

  void OnSystemInfoProbeResponse(
      GetSystemInfoCallback callback,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnBatteryInfoProbeResponse(
      GetBatteryInfoCallback callback,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void UpdateBatteryChargeStatus();

  void UpdateBatteryHealth();

  void NotifyBatteryChargeStatusObservers(
      const mojom::BatteryChargeStatusPtr& battery_charge_status);

  void NotifyBatteryHealthObservers(
      const mojom::BatteryHealthPtr& battery_health);

  void OnBatteryChargeStatusUpdated(
      const base::Optional<power_manager::PowerSupplyProperties>&
          power_supply_properties,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnBatteryHealthUpdated(cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> probe_service_;
  mojo::RemoteSet<mojom::BatteryChargeStatusObserver>
      battery_charge_status_observers_;
  mojo::RemoteSet<mojom::BatteryHealthObserver> battery_health_observers_;

  std::unique_ptr<base::RepeatingTimer> battery_charge_status_timer_;
  std::unique_ptr<base::RepeatingTimer> battery_health_timer_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_DATA_PROVIDER_H_
