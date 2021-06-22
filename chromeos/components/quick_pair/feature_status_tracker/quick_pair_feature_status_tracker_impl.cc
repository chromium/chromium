// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"

#include <memory>

#include "base/bind.h"
#include "chromeos/components/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "chromeos/components/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

namespace chromeos {
namespace quick_pair {

FeatureStatusTrackerImpl::FeatureStatusTrackerImpl()
    : fast_pair_enabled_provider_(std::make_unique<FastPairEnabledProvider>(
          std::make_unique<BluetoothEnabledProvider>())) {
  fast_pair_enabled_provider_->SetCallback(base::BindRepeating(
      &FeatureStatusTrackerImpl::OnFastPairEnabledChanged,
      weak_factory_.GetWeakPtr()));
}

FeatureStatusTrackerImpl::~FeatureStatusTrackerImpl() =
    default;

void FeatureStatusTrackerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FeatureStatusTrackerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FeatureStatusTrackerImpl::IsFastPairEnabled() {
  return fast_pair_enabled_provider_->is_enabled();
}

void FeatureStatusTrackerImpl::OnFastPairEnabledChanged(
    bool is_enabled) {
  for (auto& observer : observers_) {
    observer.OnFastPairEnabledChanged(is_enabled);
  }
}

}  // namespace quick_pair
}  // namespace chromeos
