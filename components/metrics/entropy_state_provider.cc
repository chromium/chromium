// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state_provider.h"

#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

EntropyStateProvider::EntropyStateProvider(PrefService* local_state)
    : entropy_state_(local_state) {}

EntropyStateProvider::~EntropyStateProvider() = default;

void EntropyStateProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  system_profile->set_low_entropy_source(entropy_state_.GetLowEntropySource());
  system_profile->set_old_low_entropy_source(
      entropy_state_.GetOldLowEntropySource());
  system_profile->set_pseudo_low_entropy_source(
      entropy_state_.GetPseudoLowEntropySource());
}

}  // namespace metrics
