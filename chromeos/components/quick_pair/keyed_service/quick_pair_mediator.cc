// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>

#include "chromeos/components/quick_pair/common/logging.h"
#include "chromeos/components/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"

namespace chromeos {
namespace quick_pair {

Mediator::Mediator(std::unique_ptr<FeatureStatusTracker> feature_status_tracker)
    : feature_status_tracker_(std::move(feature_status_tracker)) {
  observation_.Observe(feature_status_tracker_.get());

  SetFastPairState(feature_status_tracker_->IsFastPairEnabled());
}

Mediator::~Mediator() = default;

void Mediator::OnFastPairEnabledChanged(bool is_enabled) {
  SetFastPairState(is_enabled);
}

void Mediator::SetFastPairState(bool is_enabled) {
  QP_LOG(INFO) << "Setting Fast Pair state, is_enabled: " << is_enabled;
}

}  // namespace quick_pair
}  // namespace chromeos
