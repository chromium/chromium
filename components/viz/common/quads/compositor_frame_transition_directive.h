// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

using NavigationID = base::UnguessableToken;

// This is a transition directive that can be associated with a compositor
// frame. The intent is to be able to animate a compositor frame into the right
// place instead of simply drawing the final result at the final destination.
// This is used by a JavaScript-exposed view transitions API. See
// third_party/blink/renderer/core/view_transition/README.md for more
// information.
class VIZ_COMMON_EXPORT CompositorFrameTransitionDirective {
 public:
  // What is the directive?
  // - Save means that the currently submitted frame will be used in the future
  //   as the source frame of the animation. The animation could be driven by
  //   the renderer or Viz process. This directive must be followed by the
  //   Animate or AnimateRenderer directive.
  //
  // - AnimateRenderer means that content in the current and subsequent frames
  //   will use cached resources from the frame with the Save directive.
  //   Ownership of the cached resources is passed to the renderer process. This
  //   directive must be followed by Release to delete the cached resources.
  //
  // - Release means that cached textures in the Viz process can be deleted.
  //   This is used in the mode where the renderer is driving this animation.
  enum class Type { kSave, kAnimateRenderer, kRelease };

  struct VIZ_COMMON_EXPORT SharedElement {
    SharedElement();
    ~SharedElement();

    SharedElement(const SharedElement&);
    SharedElement& operator=(const SharedElement&);

    SharedElement(SharedElement&&);
    SharedElement& operator=(SharedElement&&);

    bool operator==(const SharedElement& other) const;
    bool operator!=(const SharedElement& other) const;

    // The render pass corresponding to a DOM element. The id is scoped to the
    // same frame that the directive corresponds to.
    CompositorRenderPassId render_pass_id;

    // An identifier to tag the cached texture for this shared element in the
    // Viz process.
    ViewTransitionElementResourceId view_transition_element_resource_id;
  };

  CompositorFrameTransitionDirective();

  // Constructs a new directive. Note that if type is `kSave`, the effect should
  // be specified for a desired effect. These are ignored for the `kAnimate`
  // type.
  CompositorFrameTransitionDirective(
      NavigationID navigation_id,
      uint32_t sequence_id,
      Type type,
      std::vector<SharedElement> shared_elements = {});

  CompositorFrameTransitionDirective(const CompositorFrameTransitionDirective&);
  ~CompositorFrameTransitionDirective();

  CompositorFrameTransitionDirective& operator=(
      const CompositorFrameTransitionDirective&);

  // A monotonically increasing sequence_id for a given communication channel
  // (i.e. surface). This is used to distinguish new directives from directives
  // that have already been processed.
  uint32_t sequence_id() const { return sequence_id_; }

  // The type of this directive.
  Type type() const { return type_; }

  NavigationID navigation_id() const { return navigation_id_; }

  // Shared elements.
  const std::vector<SharedElement>& shared_elements() const {
    return shared_elements_;
  }

 private:
  NavigationID navigation_id_;

  uint32_t sequence_id_ = 0;

  Type type_ = Type::kSave;

  std::vector<SharedElement> shared_elements_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_
