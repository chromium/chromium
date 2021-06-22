// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "chromeos/components/quick_pair/common/quick_pair_features.h"
#include "chromeos/components/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "chromeos/components/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"

namespace chromeos {
namespace quick_pair {

FastPairEnabledProvider::FastPairEnabledProvider(
    std::unique_ptr<BluetoothEnabledProvider> bluetooth_enabled_provider)
    : bluetooth_enabled_provider_(std::move(bluetooth_enabled_provider)) {
  // If the flag isn't enabled, Fast Pair will never be enabled so don't hook
  // up any callbacks.
  if (base::FeatureList::IsEnabled(features::kFastPair)) {
    bluetooth_enabled_provider_->SetCallback(base::BindRepeating(
        &FastPairEnabledProvider::OnSubProviderEnabledChanged,
        weak_factory_.GetWeakPtr()));

    SetEnabledAndInvokeCallback(AreSubProvidersEnabled());
  }
}

FastPairEnabledProvider::~FastPairEnabledProvider() = default;

bool FastPairEnabledProvider::AreSubProvidersEnabled() {
  return base::FeatureList::IsEnabled(features::kFastPair) &&
         bluetooth_enabled_provider_->is_enabled();
}

void FastPairEnabledProvider::OnSubProviderEnabledChanged(bool) {
  SetEnabledAndInvokeCallback(AreSubProvidersEnabled());
}

}  // namespace quick_pair
}  // namespace chromeos
