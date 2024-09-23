// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_STATE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_STATE_HANDLER_H_

// #include <string>

#include "base/observer_list.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

class FakeNetworkStateHandler : public NetworkStateHandler {
 public:
  FakeNetworkStateHandler();
  ~FakeNetworkStateHandler() override;

  bool ObserverListEmpty() { return observers_.empty(); }

  void ClearObserverList() { observers_.Clear(); }
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_FAKE_NETWORK_STATE_HANDLER_H_
