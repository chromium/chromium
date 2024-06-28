// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_WAIT_FOR_SERVICE_CONNECTED_OBSERVER_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_WAIT_FOR_SERVICE_CONNECTED_OBSERVER_H_

#include <string>

#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// This class is used to block the test step execution until the cellular
// service for the profile with a provided ICCID is connected. When an eSIM
// profile is installed we will wait until the corresponding service
// automatically becomes connected, and if the service is not connected within a
// reasonable amount of time we explicitly initiate a connection request. This
// class waits for the service to be connectable a.k.a. the profile is enabled,
// and then "auto-connects" it.
class WaitForServiceConnectedObserver
    : public ui::test::ObservationStateObserver<bool,
                                                NetworkStateHandler,
                                                NetworkStateHandlerObserver> {
 public:
  explicit WaitForServiceConnectedObserver(const std::string& iccid);
  ~WaitForServiceConnectedObserver() override;

 private:
  // NetworkStateHandlerObserver:
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void NetworkConnectionStateChanged(const NetworkState*) override;

  // ui::test::ObservationStateObserver:
  bool GetStateObserverInitialState() const override;

  bool IsServiceConnected() const;

  // The ICCID of the cellular network that we are watching for.
  const std::string iccid_;
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_CELLULAR_WAIT_FOR_SERVICE_CONNECTED_OBSERVER_H_
