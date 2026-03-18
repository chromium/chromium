// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_

#include <cstdint>

namespace page_content_annotations {

// This enum represents the feature that is tracking a blink element rect. This
// enum is meant to be cast to a cc::TrackedElementFeature when calling
// blink::Element::SetTrackedElementSubRect().

// For the blink TrackedElementRects definition, see
// third_party/blink/renderer/platform/graphics/paint/tracked_element_data.h.
// For the cc TrackedElementRects definition, see
// cc/trees/tracked_element_rects.h.

// If any new features are added, TRACKED_ELEMENT_FEATURE_MAX in
// cc/trees/tracked_element_rects.h should be updated.

// LINT.IfChange(TrackedElementFeature)
enum TrackedElementFeature : int32_t {
  kAIHighlight = 0,
  kIframeTracking = 1,
};
// LINT.ThenChange(//cc/trees/tracked_element_rects.h:TrackedElementFeature)

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_
