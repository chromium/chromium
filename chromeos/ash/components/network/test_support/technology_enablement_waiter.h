// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TEST_SUPPORT_TECHNOLOGY_ENABLEMENT_WAITER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TEST_SUPPORT_TECHNOLOGY_ENABLEMENT_WAITER_H_

#include <optional>

#include "base/run_loop.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/network_type_pattern.h"

namespace ash {

class TechnologyEnablementWaiter : public NetworkStateHandlerObserver {
 public:
  TechnologyEnablementWaiter(NetworkStateHandler* network_state_handler);

  TechnologyEnablementWaiter(const TechnologyEnablementWaiter&) = delete;
  TechnologyEnablementWaiter& operator=(const TechnologyEnablementWaiter&) =
      delete;

  ~TechnologyEnablementWaiter() override;

  // Wait for technology that matches `type_pattern` to be in enablement state
  // `waiting_state`.
  void Wait(NetworkTypePattern type_pattern, bool waiting_state);

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;

 private:
  bool IsConditionFulfilled();

  raw_ptr<NetworkStateHandler> network_state_handler_;
  NetworkStateHandlerScopedObservation network_state_handler_observer_{this};

  NetworkTypePattern type_pattern_ = NetworkTypePattern::Default();
  bool waiting_state_;
  std::optional<base::RunLoop> run_loop_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TEST_SUPPORT_TECHNOLOGY_ENABLEMENT_WAITER_H_
