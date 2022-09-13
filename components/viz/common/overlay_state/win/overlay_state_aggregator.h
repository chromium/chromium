// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_OVERLAY_STATE_WIN_OVERLAY_STATE_AGGREGATOR_H_
#define COMPONENTS_VIZ_COMMON_OVERLAY_STATE_WIN_OVERLAY_STATE_AGGREGATOR_H_

namespace viz {

class OverlayStateAggregator {
 public:
  OverlayStateAggregator() = default;
  ~OverlayStateAggregator() = default;

  enum class PromotionState { kUnset, kNotPromoted, kPromoted };

  // Sets a new promotion hint & returns whether the promotion recommendation
  // state has changed.
  bool SetPromotionHint(bool promoted);
  // Gets current promotion recommendation.
  PromotionState GetPromotionState();

 private:
  PromotionState promotion_state_ = PromotionState::kUnset;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_OVERLAY_STATE_WIN_OVERLAY_STATE_AGGREGATOR_H_
