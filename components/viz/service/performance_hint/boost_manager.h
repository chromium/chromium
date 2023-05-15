// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_BOOST_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_BOOST_MANAGER_H_

#include "base/metrics/field_trial_params.h"
#include "components/viz/common/features.h"
#include "components/viz/service/performance_hint/hint_session.h"

namespace viz {

class VIZ_SERVICE_EXPORT BoostManager {
 public:
  // Recalculate an actual frame duration based on the current boost type. This
  // is useful to speculate with the behavior of ADPF.
  //
  // This function consists of two steps:
  //
  // 1) Maybe update a boost type based on |preferable_boost_type| (read the
  // description of the |MaybeUpdateBoostType| function).
  //
  // 2) Recalculate an actual frame duration based on the reasulted boost type
  // from the previous step.
  // - If the boost type = kDefault, it returns the |actual_duration|
  // without any changes.
  // - If the boost type =  kScrollBoost or kWakeUpBoost, it returns a
  // fictitious duration that is greater than |target_duration|, to emulate the
  // behavior where a frame was missed.
  base::TimeDelta GetFrameDurationAndMaybeUpdateBoostType(
      base::TimeDelta target_duration,
      base::TimeDelta actual_duration,
      base::TimeTicks draw_start,
      HintSession::BoostType preferable_boost_type);

 protected:
  HintSession::BoostType GetCurrentBoostTypeForTesting() const {
    return boost_type_;
  }

 private:
  // Updates the boost type:
  // - kDefault takes effect after kScrollBoost or kWakeUpBoost expires. They
  // expire |features::kADPFBoostModeDuration| after it was set.
  // - kScrollBoost or kWakeUpBoost takes effect immediately.
  void MaybeUpdateBoostType(base::TimeTicks draw_start,
                            HintSession::BoostType boost_type);

  base::TimeTicks boost_end_time_ = base::TimeTicks::Min();
  HintSession::BoostType boost_type_ = HintSession::BoostType::kDefault;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_BOOST_MANAGER_H_
