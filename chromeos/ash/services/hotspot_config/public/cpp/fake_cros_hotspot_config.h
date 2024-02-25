// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_FAKE_CROS_HOTSPOT_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_FAKE_CROS_HOTSPOT_CONFIG_H_

#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::hotspot_config {

// Implements a fake version of the CrosHotspotConfig mojo interface.
class FakeCrosHotspotConfig : public mojom::CrosHotspotConfig {
 public:
  FakeCrosHotspotConfig();
  FakeCrosHotspotConfig(const FakeCrosHotspotConfig&) = delete;
  FakeCrosHotspotConfig& operator=(const FakeCrosHotspotConfig&) = delete;
  ~FakeCrosHotspotConfig() override;

  // mojom::CrosHotspotConfig
  void ObserveEnabledStateChanges(
      mojo::PendingRemote<mojom::HotspotEnabledStateObserver> observer)
      override;
  void AddObserver(
      mojo::PendingRemote<mojom::CrosHotspotConfigObserver> observer) override;
  void GetHotspotInfo(GetHotspotInfoCallback callback) override;
  void SetHotspotConfig(mojom::HotspotConfigPtr config,
                        SetHotspotConfigCallback callback) override;
  void EnableHotspot(EnableHotspotCallback callback) override;
  void DisableHotspot(DisableHotspotCallback callback) override;

  mojo::PendingRemote<mojom::CrosHotspotConfig> GetPendingRemote();

  void SetFakeHotspotInfo(mojom::HotspotInfoPtr hotspot_info);

 private:
  void NotifyHotspotInfoObservers();
  void NotifyHotspotTurnedOn();
  void NotifyHotspotTurnedOff(mojom::DisableReason reason);

  mojom::HotspotInfoPtr hotspot_info_;
  mojo::RemoteSet<mojom::HotspotEnabledStateObserver>
      hotspot_enabled_state_observers_;
  mojo::RemoteSet<mojom::CrosHotspotConfigObserver>
      cros_hotspot_config_observers_;
  mojo::Receiver<mojom::CrosHotspotConfig> receiver_{this};
};

}  // namespace ash::hotspot_config

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_FAKE_CROS_HOTSPOT_CONFIG_H_
