// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/boost_manager.h"

namespace viz {
namespace {
using BoostType = HintSession::BoostType;
}

base::TimeDelta BoostManager::GetFrameDuration(
    base::TimeDelta target_duration,
    base::TimeDelta actual_duration,
    base::TimeTicks draw_start,
    BoostType preferable_boost_type) {
  switch (preferable_boost_type) {
    case BoostType::kDefault:
      return actual_duration;
    case BoostType::kWakeUpBoost:
      return target_duration * 1.5;
  }
  NOTREACHED();
}

}  // namespace viz
