// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_discovery_session_status_observer.h"

#include <utility>

#include "base/run_loop.h"

namespace ash::bluetooth_config {

FakeDiscoverySessionStatusObserver::FakeDiscoverySessionStatusObserver() =
    default;

FakeDiscoverySessionStatusObserver::~FakeDiscoverySessionStatusObserver() =
    default;

mojo::PendingRemote<mojom::DiscoverySessionStatusObserver>
FakeDiscoverySessionStatusObserver::GeneratePendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeDiscoverySessionStatusObserver::DisconnectMojoPipe() {
  receiver_.reset();

  // Allow the disconnection to propagate.
  base::RunLoop().RunUntilIdle();
}

void FakeDiscoverySessionStatusObserver::OnHasAtLeastOneDiscoverySessionChanged(
    bool has_at_least_one_discovery_session) {
  has_at_least_one_discovery_session_ = has_at_least_one_discovery_session;
  num_discovery_session_changed_calls_++;
}

}  // namespace ash::bluetooth_config
