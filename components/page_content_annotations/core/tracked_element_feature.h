// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_

#include <cstdint>

namespace page_content_annotations {

// This enum represents the feature that is tracking a blink element rect. This
// enum is meant to extend viz::TrackedElementFeature and is used to
// define values that should not be directly exposed to viz. The values of
// this enum can be cast to a viz::TrackedElementFeature when calling
// blink::Element::SetTrackedElementSubRect().

// For the viz TrackedElementRects definition, see
// components/viz/common/surfaces/tracked_element_rects.h.

// If any new features are added here,
// `viz::TrackedElementFeature::kTrackedElementFeatureMax` should be updated.

// LINT.IfChange(TrackedElementFeature)
enum class TrackedElementFeature : int32_t {
  // TODO(http://crbug.com/441532128): Extend viz::TrackedElementFeature
  // properly by importing that file and incrementing from
  // kTrackedElementFeatureEnd. This will require fixing an ios build error that
  // is caused by adding a //components/viz/common dependency to this component.
  kAIHighlight = 2,
};
// LINT.ThenChange(//components/viz/common/surfaces/tracked_element_rects.h:TrackedElementFeature)

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_TRACKED_ELEMENT_FEATURE_H_
