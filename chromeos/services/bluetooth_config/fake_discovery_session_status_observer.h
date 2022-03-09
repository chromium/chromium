// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_STATUS_OBSERVER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_STATUS_OBSERVER_H_

#include "base/run_loop.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace bluetooth_config {

class FakeDiscoverySessionStatusObserver
    : public mojom::DiscoverySessionStatusObserver {
 public:
  FakeDiscoverySessionStatusObserver();
  ~FakeDiscoverySessionStatusObserver() override;

  // Generates a PendingRemote associated with this object. To disconnect the
  // associated Mojo pipe, use DisconnectMojoPipe().
  mojo::PendingRemote<mojom::DiscoverySessionStatusObserver>
  GeneratePendingRemote();

  // Disconnects the Mojo pipe associated with a PendingRemote returned by
  // GeneratePendingRemote().
  void DisconnectMojoPipe();

  bool has_at_least_one_discovery_session() const {
    return has_at_least_one_discovery_session_;
  }

  int num_discovery_session_changed_calls() const {
    return num_discovery_session_changed_calls_;
  }

 private:
  // mojom::DiscoverySessionStatusObserver:
  void OnHasAtLeastOneDiscoverySessionChanged(
      bool has_at_least_one_discovery_session) override;

  bool has_at_least_one_discovery_session_ = false;
  int num_discovery_session_changed_calls_ = 0;
  mojo::Receiver<mojom::DiscoverySessionStatusObserver> receiver_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DISCOVERY_SESSION_STATUS_OBSERVER_H_
