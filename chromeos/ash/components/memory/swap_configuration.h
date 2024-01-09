// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_SWAP_CONFIGURATION_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_SWAP_CONFIGURATION_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ash {

// Controls the threshold at which memory pressure signals are sent for
// arc-disabled devices.
COMPONENT_EXPORT(ASH_MEMORY)
BASE_DECLARE_FEATURE(kCrOSMemoryPressureSignalStudyNonArc);

COMPONENT_EXPORT(ASH_MEMORY)
extern const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyNonArcCriticalBps;

COMPONENT_EXPORT(ASH_MEMORY)
extern const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyNonArcModerateBps;

// Similar to above but for arc-enabled devices.
COMPONENT_EXPORT(ASH_MEMORY)
BASE_DECLARE_FEATURE(kCrOSMemoryPressureSignalStudyArc);

COMPONENT_EXPORT(ASH_MEMORY)
extern const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyArcCriticalBps;

COMPONENT_EXPORT(ASH_MEMORY)
extern const base::FeatureParam<int>
    kCrOSMemoryPressureSignalStudyArcModerateBps;

// Configure swap will configure any swap related experiments that this user may
// be opted into. This method should be called after the user logs in, since
// that is the earliest time when we know if arc is enabled or not. It can be
// called additional times if `arc_enabled` changes during a session. This is
// expected to be rare, and will mildly pollute UMA data.
COMPONENT_EXPORT(ASH_MEMORY) void ConfigureSwap(bool arc_enabled);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_SWAP_CONFIGURATION_H_
