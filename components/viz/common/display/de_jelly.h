// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DISPLAY_DE_JELLY_H_
#define COMPONENTS_VIZ_COMMON_DISPLAY_DE_JELLY_H_

#include "components/viz/common/viz_common_export.h"

namespace viz {

// Utility functions for use with de-jelly logic. Used in both viz process and
// render process.

// Whether experimental de-jelly is enabled. This indicates whether
// DeJellyActive below may ever return true, and is used by the renderer
// process to determine whether it should prepare frames for potential
// de-jelly.
bool VIZ_COMMON_EXPORT DeJellyEnabled();

// Whether experimental de-jelly is *currently* active. This is different from
// whether it is generally enabled, and may change frame-over-frame. This is
// queried from the Viz process, before drawing a frame.
bool VIZ_COMMON_EXPORT DeJellyActive();

// The screen width to use for de-jelly logic.
float VIZ_COMMON_EXPORT DeJellyScreenWidth();

// The maximum additional content which may be exposed in a layer by de-jelly
// skewing.
float VIZ_COMMON_EXPORT MaxDeJellyHeight();

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DISPLAY_DE_JELLY_H_
