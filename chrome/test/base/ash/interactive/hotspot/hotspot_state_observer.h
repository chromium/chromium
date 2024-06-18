// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_HOTSPOT_HOTSPOT_STATE_OBSERVER_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_HOTSPOT_HOTSPOT_STATE_OBSERVER_H_

#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// This is a helper class that can be used in tests that use Kombucha to
// observe the hotspot state.
class HotspotStateObserver : public ui::test::ObservationStateObserver<
                                 hotspot_config::mojom::HotspotState,
                                 HotspotStateHandler,
                                 HotspotStateHandler::Observer> {
 public:
  HotspotStateObserver();
  ~HotspotStateObserver() override;

 private:
  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;

  // ui::test::ObservationStateObserver:
  hotspot_config::mojom::HotspotState GetStateObserverInitialState()
      const override;

  hotspot_config::mojom::HotspotState GetHotspotState() const;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_HOTSPOT_HOTSPOT_STATE_OBSERVER_H_
