// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ENABLED_STATE_NOTIFIER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ENABLED_STATE_NOTIFIER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/hotspot_controller.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

// This class captures hotspot and wifi state changes from HotspotController and
// HotspotStateHandler and relays them to its observers.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotEnabledStateNotifier
    : public HotspotController::Observer,
      public HotspotStateHandler::Observer {
 public:
  HotspotEnabledStateNotifier();
  HotspotEnabledStateNotifier(const HotspotEnabledStateNotifier&) = delete;
  HotspotEnabledStateNotifier& operator=(const HotspotEnabledStateNotifier&) =
      delete;
  ~HotspotEnabledStateNotifier() override;

  void Init(HotspotStateHandler* hotspot_state_handler,
            HotspotController* hotspot_controller);

  void ObserveEnabledStateChanges(
      mojo::PendingRemote<hotspot_config::mojom::HotspotEnabledStateObserver>
          observer);

  // HotspotController::Observer:
  void OnHotspotTurnedOn() override;
  void OnHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason) override;

 private:
  friend class HotspotNotifierTest;
  HotspotEnabledStateNotifier(HotspotStateHandler* hotspot_state_handler,
                              HotspotController* hotspot_controller);

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;

  raw_ptr<HotspotController> hotspot_controller_;
  raw_ptr<HotspotStateHandler> hotspot_state_handler_;
  mojo::RemoteSet<hotspot_config::mojom::HotspotEnabledStateObserver>
      observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ENABLED_STATE_NOTIFIER_H_
