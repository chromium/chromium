// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_

#include <cstdint>

#include "components/viz/common/surfaces/tracked_element_rects.h"

namespace page_content_annotations {

// This enum represents the feature that is tracking a blink element rect. This
// enum is meant to extend viz::TrackedElementFeature and is used to
// define values that should not be directly exposed to viz. The values of
// this enum can be cast to a viz::TrackedElementFeature when calling
// blink::Element::SetTrackedElementSubRect().

// The int values of this enum can change over time if new values are added to
// viz::TrackedElementFeature. You should avoid using the int values directly
// when logging or storing the values.

// For the viz TrackedElementRects definition, see
// components/viz/common/surfaces/tracked_element_rects.h.

// If any new features are added here,
// `viz::TrackedElementFeature::kTrackedElementFeatureMax` should be updated.

// LINT.IfChange(TrackedElementFeature)
enum class TrackedElementFeature : int32_t {
  kAIHighlight = static_cast<int32_t>(
                     viz::TrackedElementFeature::kTrackedElementFeatureEnd) +
                 1,
};
// LINT.ThenChange(//components/viz/common/surfaces/tracked_element_rects.h:TrackedElementFeature)

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_
