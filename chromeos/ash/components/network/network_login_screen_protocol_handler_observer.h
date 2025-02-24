// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_LOGIN_SCREEN_PROTOCOL_HANDLER_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_LOGIN_SCREEN_PROTOCOL_HANDLER_OBSERVER_H_

#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

// This allows investigating the potential network availability while migrating.
//
// A policy enforced, zero touch migration requires a network connection after
// migration to automatically re-enroll the device.
// Certificate based networks will not be available to reconnect as they will
// lose their hardware backed certificate handling in the process. We do
// however know that most devices have also less secure PSK networks which could
// be used instead in that case.
//
// This experiment will check for the availability of the least secure network
// while the user is on the login screen. The result will then give us a
// percentage of managed users which might require hand migrating of devices.

class NetworkLoginScreenProtocolHandlerObserver
    : public NetworkStateHandlerObserver {
 public:
  NetworkLoginScreenProtocolHandlerObserver() = default;

  NetworkLoginScreenProtocolHandlerObserver(
      const NetworkLoginScreenProtocolHandlerObserver&) = delete;
  NetworkLoginScreenProtocolHandlerObserver& operator=(
      const NetworkLoginScreenProtocolHandlerObserver&) = delete;

  ~NetworkLoginScreenProtocolHandlerObserver() override;

  void Init(NetworkStateHandler* network_state_handler);

  void NetworkConnectionStateChanged(const NetworkState* network) override;

  void ScanCompleted(const DeviceState* device) override;

 private:
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_LOGIN_SCREEN_PROTOCOL_HANDLER_OBSERVER_H_
