// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_OVERLAY_STRATEGY_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_OVERLAY_STRATEGY_H_

#include <string>
#include <vector>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// Enum used for UMA histogram. These enum values must not be changed or
// reused.
enum class OverlayStrategy {
  kUnknown = 0,
  kNoStrategyUsed = 1,
  kFullscreen = 2,
  kSingleOnTop = 3,
  kUnderlay = 4,
  kUnderlayCast = 5,
  kMaxValue = kUnderlayCast,
};

// Parses a comma separated list of overlay strategy types and returns a list
// of the corresponding OverlayStrategy enum values.
VIZ_COMMON_EXPORT std::vector<OverlayStrategy> ParseOverlayStrategies(
    const std::string& strategies_string);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_OVERLAY_STRATEGY_H_
