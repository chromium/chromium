// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_SYSTEM_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_SYSTEM_PROVIDER_IMPL_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/battery_status.mojom.h"

namespace ash::libassistant {

class PowerManagerProviderImpl;

class SystemProviderImpl : public assistant_client::SystemProvider {
 public:
  // Acceptable to pass in |nullptr| for |power_manager_provider| when no
  // platform power manager provider is available.
  explicit SystemProviderImpl(
      std::unique_ptr<PowerManagerProviderImpl> power_manager_provider);

  SystemProviderImpl(const SystemProviderImpl&) = delete;
  SystemProviderImpl& operator=(const SystemProviderImpl&) = delete;

  ~SystemProviderImpl() override;

  void Initialize(mojom::PlatformDelegate* platform_delegate);

  // assistant_client::SystemProvider implementation:
  assistant_client::MicMuteState GetMicMuteState() override;
  void RegisterMicMuteChangeCallback(ConfigChangeCallback callback) override;
  assistant_client::PowerManagerProvider* GetPowerManagerProvider() override;
  bool GetBatteryState(BatteryState* state) override;
  void UpdateTimezoneAndLocale(const std::string& timezone,
                               const std::string& locale) override;

 private:
  friend class AssistantSystemProviderImplTest;
  void OnBatteryStatus(device::mojom::BatteryStatusPtr battery_status);

  void FlushForTesting();

  std::unique_ptr<PowerManagerProviderImpl> power_manager_provider_;

  mojo::Remote<device::mojom::BatteryMonitor> battery_monitor_;
  device::mojom::BatteryStatusPtr current_battery_status_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_SYSTEM_PROVIDER_IMPL_H_
