// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_BOOST_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_BOOST_MANAGER_H_

#include "components/viz/common/features.h"
#include "components/viz/service/performance_hint/hint_session.h"

namespace viz {

class VIZ_SERVICE_EXPORT BoostManager {
 public:
  // Recalculate an actual frame duration based on the current boost type.
  // If the boost type = kDefault, it returns the |actual_duration|
  // without any changes.
  // If the boost type = kWakeUpBoost, it returns a fictitious duration that
  // is greater than |target_duration|, to emulate the behavior where a frame
  // was missed.
  base::TimeDelta GetFrameDuration(
      base::TimeDelta target_duration,
      base::TimeDelta actual_duration,
      base::TimeTicks draw_start,
      HintSession::BoostType preferable_boost_type);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_BOOST_MANAGER_H_
