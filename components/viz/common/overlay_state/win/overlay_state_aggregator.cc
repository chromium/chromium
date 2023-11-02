// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/overlay_state/win/overlay_state_aggregator.h"

namespace viz {

bool OverlayStateAggregator::SetPromotionHint(bool promoted) {
  OverlayStateAggregator::PromotionState recommendation =
      promoted ? OverlayStateAggregator::PromotionState::kPromoted
               : OverlayStateAggregator::PromotionState::kNotPromoted;

  if (recommendation != promotion_state_) {
    promotion_state_ = recommendation;
    return true;
  }

  // No change.
  return false;
}

OverlayStateAggregator::PromotionState
OverlayStateAggregator::GetPromotionState() {
  return promotion_state_;
}

}  // namespace viz
