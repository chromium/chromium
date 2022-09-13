// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/overlay_strategy.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "components/viz/common/buildflags.h"

namespace viz {

std::vector<OverlayStrategy> ParseOverlayStrategies(
    const std::string& strategies_string) {
  std::vector<OverlayStrategy> strategies;

  auto strategy_names = base::SplitStringPiece(
      strategies_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (auto& strategy_name : strategy_names) {
    if (strategy_name == "single-fullscreen") {
      strategies.push_back(OverlayStrategy::kFullscreen);
    } else if (strategy_name == "single-on-top") {
      strategies.push_back(OverlayStrategy::kSingleOnTop);
    } else if (strategy_name == "underlay") {
      strategies.push_back(OverlayStrategy::kUnderlay);
#if BUILDFLAG(ENABLE_CAST_OVERLAY_STRATEGY)
    } else if (strategy_name == "cast") {
      strategies.push_back(OverlayStrategy::kUnderlayCast);
#endif
    } else {
      LOG(ERROR) << "Unrecognized overlay strategy " << strategy_name;
    }
  }
  return strategies;
}

}  // namespace viz
