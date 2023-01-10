// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_NETWORK_HEALTH_CONSTANTS_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_NETWORK_HEALTH_CONSTANTS_H_

#include "base/time/time.h"

namespace ash::network_health {

// The rate in seconds at which to sample all network's signal strengths.
constexpr base::TimeDelta kSignalStrengthSampleRate = base::Seconds(5);

// This value represents the size of the sampling window for all network's
// signal strength. Samples older than this duration are discarded.
constexpr base::TimeDelta kSignalStrengthSampleWindow = base::Minutes(15);

// Represents the interval at which we update tracked guids. See
// network_health.h for more information about tracked guids.
constexpr base::TimeDelta kUpdateTrackedGuidsInterval = base::Hours(1);

}  // namespace ash::network_health

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_NETWORK_HEALTH_CONSTANTS_H_
