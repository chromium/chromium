// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_MEDIA_VIEW_UTILS_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_MEDIA_VIEW_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "ui/gfx/geometry/size.h"

namespace global_media_controls {

// The time duration for seeking forward or backward.
inline constexpr base::TimeDelta kSeekTime = base::Seconds(10);

// The corner radius of the images in the media views.
inline constexpr int kDefaultArtworkCornerRadius = 12;

// View ID's.
// Buttons are using the `MediaSessionAction` enum as their view IDs. To avoid
// conflicts, the other views' IDs start from the
// `MediaSessionAction::kMaxValue`.
inline constexpr int kChapterItemViewTitleId =
    static_cast<int>(media_session::mojom::MediaSessionAction::kMaxValue) + 1;
inline constexpr int kChapterItemViewStartTimeId = kChapterItemViewTitleId + 1;

// If the image does not fit the view, scale the image to fill the view even if
// part of the image is cropped.
gfx::Size ScaleImageSizeToFitView(const gfx::Size& image_size,
                                  const gfx::Size& view_size);

// Adapts the functionality of `base::TimeDurationFormat` to format a
// `TimeDelta` object. Returns a formatted time string (e.g., "0:30", "3:30",
// "3:03:30").
// Uses the Singleton `GetMeasureFormat()` for efficient formatting. This avoids
// recreating `measure_format` on each function call. Provides a default
// formatted string if the `formatMeasures()` function encounters an error.
COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS)
std::u16string GetFormattedDuration(const base::TimeDelta& duration);

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_MEDIA_VIEW_UTILS_H_
