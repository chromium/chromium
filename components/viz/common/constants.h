// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_CONSTANTS_H_
#define COMPONENTS_VIZ_COMMON_CONSTANTS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// Keep list in alphabetical order.

// The client_id that the browser process uses for the FrameSinkIds it
// allocates, e.g. for unbounded surfaces. Renderers use their own client_id,
// which is never 0.
VIZ_COMMON_EXPORT extern const uint32_t kBrowserClientId;

VIZ_COMMON_EXPORT extern const uint32_t kDefaultActivationDeadlineInFrames;

// When throttling is enabled we estimate if vsync and sink frame rate are at
// a simple cadence. If they are, then this value represents the maximum time
// until the next glitch (jank) would occur caused by the drift/error
// introduced per throttle due to the estimation not being exact. If any
// cadence combo where to make glitches happen more frequently than this value
// then it won't be considered a simple cadence and thus won't be throttled.
VIZ_COMMON_EXPORT extern const base::TimeDelta kMaxTimeUntilNextGlitch;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_CONSTANTS_H_
