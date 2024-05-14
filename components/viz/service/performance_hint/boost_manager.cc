// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/boost_manager.h"

namespace viz {
namespace {
using BoostType = HintSession::BoostType;
}

base::TimeDelta BoostManager::GetFrameDurationAndMaybeUpdateBoostType(
    base::TimeDelta target_duration,
    base::TimeDelta actual_duration,
    base::TimeTicks draw_start,
    BoostType preferable_boost_type) {
  if (!base::FeatureList::IsEnabled(features::kEnableADPFScrollBoost)) {
    switch (preferable_boost_type) {
      case BoostType::kDefault:
      case BoostType::kScrollBoost:
        return actual_duration;
      case BoostType::kWakeUpBoost:
        return target_duration * 1.5;
    }
    NOTREACHED_IN_MIGRATION();
  }

  MaybeUpdateBoostType(draw_start, preferable_boost_type);
  switch (boost_type_) {
    case BoostType::kDefault:
      return actual_duration;
    case BoostType::kScrollBoost:
      return target_duration * 3;
    case BoostType::kWakeUpBoost:
      return target_duration * 1.5;
  }
}

void BoostManager::MaybeUpdateBoostType(base::TimeTicks draw_start,
                                        BoostType boost_type) {
  switch (boost_type) {
    case BoostType::kDefault:
      if (draw_start > boost_end_time_) {
        boost_type_ = BoostType::kDefault;
      }
      return;
    case BoostType::kScrollBoost:
    case BoostType::kWakeUpBoost:
      boost_end_time_ = draw_start + features::kADPFBoostTimeout.Get();
      boost_type_ = boost_type;
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace viz
