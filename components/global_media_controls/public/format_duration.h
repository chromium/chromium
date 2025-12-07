// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_FORMAT_DURATION_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_FORMAT_DURATION_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"

namespace global_media_controls {

// Adapts the functionality of `base::TimeDurationFormat` to format a
// `TimeDelta` object. Returns a formatted time string (e.g., "0:30", "3:30",
// "3:03:30").
// Uses the Singleton `GetMeasureFormat()` for efficient formatting. This avoids
// recreating `measure_format` on each function call. Provides a default
// formatted string if the `formatMeasures()` function encounters an error.
COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS)
std::u16string GetFormattedDuration(const base::TimeDelta& duration);

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_FORMAT_DURATION_H_
