// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_

#include <vector>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// This is a transition directive that can be associcated with a compositor
// frame. The intent is to be able to animate a compositor frame into the right
// place instead of simply drawing the final result at the final destination.
// This is used by a JavaScript-exposed document transitions API. See
// third_party/blink/renderer/core/document_transition/README.md for more
// information.
class VIZ_COMMON_EXPORT CompositorFrameTransitionDirective {
 public:
  // What is the directive?
  // - Save means that the currently submitted frame will be used in the future
  //   as the source frame of the animation.
  // - Animate means that this frame should be used as a (new) destination frame
  //   of the animation, using the previously saved frame as the source.
  enum class Type { kSave, kAnimate };

  // The type of an effect that should be used in the animation.
  enum class Effect {
    kNone,
    kCoverDown,
    kCoverLeft,
    kCoverRight,
    kCoverUp,
    kExplode,
    kFade,
    kImplode,
    kRevealDown,
    kRevealLeft,
    kRevealRight,
    kRevealUp
  };

  CompositorFrameTransitionDirective();

  // Constructs a new directive. Note that if type is `kSave`, the effect should
  // be specified for a desired effect. These are ignored for the `kAnimate`
  // type.
  CompositorFrameTransitionDirective(
      uint32_t sequence_id,
      Type type,
      Effect effect = Effect::kNone,
      std::vector<CompositorRenderPassId> shared_render_pass_ids = {});

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

  // The effect for the transition.
  Effect effect() const { return effect_; }

  // Shared element render passes.
  const std::vector<CompositorRenderPassId>& shared_render_pass_ids() const {
    return shared_render_pass_ids_;
  }

 private:
  uint32_t sequence_id_ = 0;

  Type type_ = Type::kSave;

  Effect effect_ = Effect::kNone;

  std::vector<CompositorRenderPassId> shared_render_pass_ids_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COMPOSITOR_FRAME_TRANSITION_DIRECTIVE_H_
