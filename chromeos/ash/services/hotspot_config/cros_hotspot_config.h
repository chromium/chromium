// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_CROS_HOTSPOT_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_CROS_HOTSPOT_CONFIG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_configuration_handler.h"
#include "chromeos/ash/components/network/hotspot_enabled_state_notifier.h"
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
                          public HotspotCapabilitiesProvider::Observer,
                          public HotspotConfigurationHandler::Observer,
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

 private:
  friend class CrosHotspotConfigTest;

  // Constructs an instance of CrosHotspotConfig with specific network subsystem
  // dependencies. This should only be used in test.
  CrosHotspotConfig(
      HotspotCapabilitiesProvider* hotspot_capabilities_provider,
      HotspotStateHandler* hotspot_state_handler,
      HotspotController* hotspot_controller,
      HotspotConfigurationHandler* hotspot_configuration_handler,
      HotspotEnabledStateNotifier* hotspot_enabled_state_notifier);
  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;
  // HotspotCapabilitiesProvider::Observer:
  void OnHotspotCapabilitiesChanged() override;
  // HotspotConfigurationHandler::Observer:
  void OnHotspotConfigurationChanged() override;

  void NotifyObservers();

  raw_ptr<HotspotCapabilitiesProvider, LeakedDanglingUntriaged>
      hotspot_capabilities_provider_;
  raw_ptr<HotspotStateHandler, LeakedDanglingUntriaged> hotspot_state_handler_;
  raw_ptr<ash::HotspotController, LeakedDanglingUntriaged> hotspot_controller_;
  raw_ptr<HotspotConfigurationHandler, LeakedDanglingUntriaged>
      hotspot_configuration_handler_;
  raw_ptr<HotspotEnabledStateNotifier, LeakedDanglingUntriaged>
      hotspot_enabled_state_notifier_;

  mojo::RemoteSet<mojom::CrosHotspotConfigObserver> observers_;
  mojo::ReceiverSet<mojom::CrosHotspotConfig> receivers_;

  base::WeakPtrFactory<CrosHotspotConfig> weak_factory_{this};
};

}  // namespace hotspot_config
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_CROS_HOTSPOT_CONFIG_H_
