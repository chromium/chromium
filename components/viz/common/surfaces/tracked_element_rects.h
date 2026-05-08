// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_TRACKED_ELEMENT_RECTS_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_TRACKED_ELEMENT_RECTS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/token.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

using TrackedElementId = base::Token;

// The feature that is tracking the element. Some feature values are kept opaque
// at this level, and are instead maintained in the higher level browser
// code. If any new browser-level features are added,
// `kTrackedElementFeatureMax` should be updated. For the browser-level values,
// see components/page_content_annotations/core/tracked_element_feature.h

// LINT.IfChange(TrackedElementFeature)
enum class TrackedElementFeature : int32_t {
  kIframeTracking = 0,
  kTrackedElementFeatureEnd,
  kTrackedElementFeatureMax = kTrackedElementFeatureEnd + 1,
};
// LINT.ThenChange(//components/page_content_annotations/core/tracked_element_feature.h:TrackedElementFeature)

// New struct to hold the tracked element clipped/visible bounds and other data.
struct VIZ_COMMON_EXPORT TrackedElementRect {
  TrackedElementRect() = default;
  TrackedElementRect(
      TrackedElementId id,
      gfx::Rect visible_bounds,
      bool should_add_to_compositor_frame_metadata = false,
      std::optional<blink::FrameToken> frame_token = std::nullopt,
      std::optional<blink::LocalFrameToken> parent_frame_token = std::nullopt)
      : id(id),
        visible_bounds(visible_bounds),
        should_add_to_compositor_frame_metadata(
            should_add_to_compositor_frame_metadata),
        frame_token(frame_token),
        parent_frame_token(parent_frame_token) {}

  // The id of the element being tracked.
  TrackedElementId id;

  // Visible screen space bounds, clipped against layer visible surface and root
  // surface
  gfx::Rect visible_bounds;

  // Whether the element should be added to the compositor frame metadata. If
  // false, the element will be added to the render frame metadata.
  bool should_add_to_compositor_frame_metadata = false;

  // The frame token of the frame containing the element being tracked.
  std::optional<blink::FrameToken> frame_token;
  // The local frame token of the parent frame.
  std::optional<blink::LocalFrameToken> parent_frame_token;

  friend bool operator==(const TrackedElementRect&,
                         const TrackedElementRect&) = default;

  std::string ToString() const;
};

// TrackedElementRects maps precisely to the same-named mojom class, and is
// used to map a "feature" enum to the list of tracked elements being tracked by
// that feature. The element rectangle represents the region of the viewport
// where the element is rendered in a frame.
using TrackedElementRects =
    absl::flat_hash_map<TrackedElementFeature, std::vector<TrackedElementRect>>;

VIZ_COMMON_EXPORT std::string TrackedElementRectsToString(
    const TrackedElementRects& rects);

// Returns a reference to a global empty TrackedElementRects. This should
// only be used for functions that need to return a reference to a
// TrackedElementRects, not instead of the default constructor.
VIZ_COMMON_EXPORT const TrackedElementRects& TrackedElementRectsEmpty();

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_TRACKED_ELEMENT_RECTS_H_
