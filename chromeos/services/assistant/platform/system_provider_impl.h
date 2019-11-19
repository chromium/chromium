// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "libassistant/shared/public/platform_system.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/battery_status.mojom.h"

namespace chromeos {
namespace assistant {

class PowerManagerProviderImpl;

class COMPONENT_EXPORT(ASSISTANT_SERVICE) SystemProviderImpl
    : public assistant_client::SystemProvider {
 public:
  // Acceptable to pass in |nullptr| for |power_manager_provider| when no
  // platform power manager provider is available.
  SystemProviderImpl(
      std::unique_ptr<PowerManagerProviderImpl> power_manager_provider,
      mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor);
  ~SystemProviderImpl() override;

  // assistant_client::SystemProvider implementation:
  assistant_client::MicMuteState GetMicMuteState() override;
  void RegisterMicMuteChangeCallback(ConfigChangeCallback callback) override;
  assistant_client::PowerManagerProvider* GetPowerManagerProvider() override;
  bool GetBatteryState(BatteryState* state) override;
  void UpdateTimezoneAndLocale(const std::string& timezone,
                               const std::string& locale) override;

 private:
  friend class SystemProviderImplTest;
  void OnBatteryStatus(device::mojom::BatteryStatusPtr battery_status);

  void FlushForTesting();

  std::unique_ptr<PowerManagerProviderImpl> power_manager_provider_;

  mojo::Remote<device::mojom::BatteryMonitor> battery_monitor_;
  device::mojom::BatteryStatusPtr current_battery_status_;

  DISALLOW_COPY_AND_ASSIGN(SystemProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_SYSTEM_PROVIDER_IMPL_H_
