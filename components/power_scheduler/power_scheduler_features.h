// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_SCHEDULER_FEATURES_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_SCHEDULER_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace power_scheduler {
namespace features {

COMPONENT_EXPORT(POWER_SCHEDULER)
extern const base::Feature kPowerScheduler;

// Features supported for legacy reasons.
// TODO(eseckler): Remove once use cases for these have been migrated.
COMPONENT_EXPORT(POWER_SCHEDULER)
extern const base::Feature kCpuAffinityRestrictToLittleCores;
COMPONENT_EXPORT(POWER_SCHEDULER)
extern const base::Feature kPowerSchedulerThrottleIdle;
COMPONENT_EXPORT(POWER_SCHEDULER)
extern const base::Feature kPowerSchedulerThrottleIdleAndNopAnimation;
COMPONENT_EXPORT(POWER_SCHEDULER)
extern const base::Feature kWebViewCpuAffinityRestrictToLittleCores;
COMPONENT_EXPORT(POWER_SCHEDULER)
extern const base::Feature kWebViewPowerSchedulerThrottleIdle;

}  // namespace features
}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_SCHEDULER_FEATURES_H_
