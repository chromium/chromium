// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_feature_state_observer.h"

namespace ash {

namespace multidevice_setup {

FakeFeatureStateObserver::FakeFeatureStateObserver() = default;

FakeFeatureStateObserver::~FakeFeatureStateObserver() = default;

mojo::PendingRemote<mojom::FeatureStateObserver>
FakeFeatureStateObserver::GenerateRemote() {
  mojo::PendingRemote<mojom::FeatureStateObserver> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeFeatureStateObserver::OnFeatureStatesChanged(
    const base::flat_map<mojom::Feature, mojom::FeatureState>&
        feature_states_map) {
  feature_state_updates_.emplace_back(feature_states_map);
}

}  // namespace multidevice_setup

}  // namespace ash
