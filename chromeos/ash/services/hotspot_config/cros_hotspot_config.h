// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_CROS_HOTSPOT_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_CROS_HOTSPOT_CONFIG_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

class HotspotController;

namespace hotspot_config {

class CrosHotspotConfig : public mojom::CrosHotspotConfig,
                          public HotspotStateHandler::Observer {
 public:
  // Constructs an instance of CrosHotspotConfig with default network subsystem
  // dependencies appropriate for a production environment.
  CrosHotspotConfig();

  CrosHotspotConfig(const CrosHotspotConfig&) = delete;
  CrosHotspotConfig& operator=(const CrosHotspotConfig&) = delete;

  ~CrosHotspotConfig() override;

  // Binds a PendingReceiver to this instance. Clients wishing to use the
  // CrosHotspotConfig API should use this function as an entrypoint.
  void BindPendingReceiver(
      mojo::PendingReceiver<mojom::CrosHotspotConfig> pending_receiver);

  // mojom::CrosHotspotConfig
  void AddObserver(
      mojo::PendingRemote<mojom::CrosHotspotConfigObserver> observer) override;
  void GetHotspotInfo(GetHotspotInfoCallback callback) override;
  void SetHotspotConfig(mojom::HotspotConfigPtr config,
                        SetHotspotConfigCallback callback) override;
  void EnableHotspot(EnableHotspotCallback callback) override;
  void DisableHotspot(DisableHotspotCallback callback) override;

 private:
  friend class CrosHotspotConfigTest;

  // Constructs an instance of CrosHotspotConfig with specific network subsystem
  // dependencies. This should only be used in test.
  CrosHotspotConfig(HotspotStateHandler* hotspot_state_handler,
                    HotspotController* hotspot_controller);
  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;
  void OnHotspotCapabilitiesChanged() override;

  HotspotStateHandler* hotspot_state_handler_;
  ash::HotspotController* hotspot_controller_;

  mojo::RemoteSet<mojom::CrosHotspotConfigObserver> observers_;
  mojo::ReceiverSet<mojom::CrosHotspotConfig> receivers_;

  base::WeakPtrFactory<CrosHotspotConfig> weak_factory_{this};
};

}  // namespace hotspot_config
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_CROS_HOTSPOT_CONFIG_H_
