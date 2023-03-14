// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_HOTSPOT_ENABLED_STATE_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_HOTSPOT_ENABLED_STATE_PROVIDER_H_

#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::hotspot_config {

// Notifies observers about changes to following hotspot states: turned on and
// turned off. Hotspot turned on notification is delivered when the user
// explicitly turns on the hotspot and turned off notification is delivered when
// the system disables the hotspot.
class HotspotEnabledStateProvider {
 public:
  virtual ~HotspotEnabledStateProvider();

  // Adds an observer to a list of observers who will be notified when hotspot
  // is turned on or off.
  void ObserveEnabledStateChanges(
      mojo::PendingRemote<mojom::HotspotEnabledStateObserver> observer);

 protected:
  HotspotEnabledStateProvider();

  void NotifyHotspotTurnedOn(bool wifi_turned_off);

  void NotifyHotspotTurnedOff(mojom::DisableReason reason);

 private:
  mojo::RemoteSet<mojom::HotspotEnabledStateObserver> observers_;
};

}  // namespace ash::hotspot_config

#endif