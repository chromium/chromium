// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_HOTSPOT_ENABLED_STATE_TEST_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_HOTSPOT_ENABLED_STATE_TEST_OBSERVER_H_

#include "base/sequence_checker.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::hotspot_config {

class HotspotEnabledStateTestObserver
    : public mojom::HotspotEnabledStateObserver {
 public:
  HotspotEnabledStateTestObserver();

  HotspotEnabledStateTestObserver(const HotspotEnabledStateTestObserver&) =
      delete;
  HotspotEnabledStateTestObserver& operator=(
      const HotspotEnabledStateTestObserver&) = delete;

  ~HotspotEnabledStateTestObserver() override;

  mojo::PendingRemote<mojom::HotspotEnabledStateObserver> GenerateRemote();

  // mojom::HotspotEnabledStateObserver:
  void OnHotspotTurnedOn() override;

  void OnHotspotTurnedOff(mojom::DisableReason disable_reason) override;

  size_t hotspot_turned_on_count() const { return hotspot_turned_on_count_; }

  size_t hotspot_turned_off_count() const { return hotspot_turned_off_count_; }

  mojom::DisableReason last_disable_reason() const {
    return last_disable_reason_;
  }

  mojo::Receiver<mojom::HotspotEnabledStateObserver>& receiver() {
    return receiver_;
  }

 private:
  mojo::Receiver<mojom::HotspotEnabledStateObserver> receiver_{this};
  size_t hotspot_turned_on_count_ = 0;
  size_t hotspot_turned_off_count_ = 0;
  mojom::DisableReason last_disable_reason_;
};

}  // namespace ash::hotspot_config

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_HOTSPOT_ENABLED_STATE_TEST_OBSERVER_H_
