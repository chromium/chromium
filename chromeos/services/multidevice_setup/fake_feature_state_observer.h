// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_OBSERVER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_OBSERVER_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace multidevice_setup {

// Fake mojom::FeatureStateObserver implementation for tests.
class FakeFeatureStateObserver : public mojom::FeatureStateObserver {
 public:
  FakeFeatureStateObserver();
  ~FakeFeatureStateObserver() override;

  mojo::PendingRemote<mojom::FeatureStateObserver> GenerateRemote();

  const std::vector<base::flat_map<mojom::Feature, mojom::FeatureState>>&
  feature_state_updates() {
    return feature_state_updates_;
  }

 private:
  // mojom::FeatureStateObserver:
  void OnFeatureStatesChanged(
      const base::flat_map<mojom::Feature, mojom::FeatureState>&
          feature_states_map) override;

  std::vector<base::flat_map<mojom::Feature, mojom::FeatureState>>
      feature_state_updates_;

  mojo::ReceiverSet<mojom::FeatureStateObserver> receivers_;

  DISALLOW_COPY_AND_ASSIGN(FakeFeatureStateObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_OBSERVER_H_
