// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/damage_frame_annotator.h"

#include <algorithm>
#include <utility>

#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"

namespace viz {

DamageFrameAnnotator::DamageFrameAnnotator() = default;
DamageFrameAnnotator::~DamageFrameAnnotator() = default;

void DamageFrameAnnotator::AnnotateAggregatedFrame(CompositorFrame* frame) {
  DCHECK(frame);
  auto* root_render_pass = frame->render_pass_list.back().get();

  const gfx::Rect& damage_rect = root_render_pass->damage_rect;
  gfx::Transform transform;
  transform.Translate(damage_rect.x(), damage_rect.y());

  annotations_.push_back(
      AnnotationData{gfx::Rect(damage_rect.size()), transform,
                     Highlight{SkColorSetARGB(128, 255, 0, 0), 4}});

  AnnotateRootRenderPass(root_render_pass);
  annotations_.clear();
}

void DamageFrameAnnotator::AnnotateRootRenderPass(RenderPass* render_pass) {
  const size_t num_quads_to_add = annotations_.size();

  // Insert |num_quads_to_add| new SharedQuadState at the start of the list.
  // This will invalidate all the existing SharedQuadState* in DrawQuads.
  auto sqs_iter =
      render_pass->shared_quad_state_list
          .InsertBeforeAndInvalidateAllPointers<SharedQuadState>(
              render_pass->shared_quad_state_list.begin(), num_quads_to_add);

  // Insert |num_quads_to_add| new DebugBorderDrawQuad at start of list. The
  // quads will be drawn on top of the original quads.
  auto quad_iter =
      render_pass->quad_list
          .InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
              render_pass->quad_list.begin(), num_quads_to_add);

  // Initialize the SharedQuadStates and DebugBorderDrawQuads.
  for (auto& annotation : annotations_) {
    gfx::Rect output_rect = cc::MathUtil::MapEnclosingClippedRect(
        annotation.transform, annotation.rect);

    SharedQuadState* new_sqs = *sqs_iter;
    new_sqs->SetAll(annotation.transform, output_rect, output_rect,
                    gfx::RRectF(), output_rect, true, false, 1.f,
                    SkBlendMode::kSrcOver, 0);

    DebugBorderDrawQuad* new_quad =
        static_cast<DebugBorderDrawQuad*>(*quad_iter);
    new_quad->SetNew(new_sqs, annotation.rect, annotation.rect,
                     annotation.highlight.color, annotation.highlight.width);

    ++sqs_iter;
    ++quad_iter;
  }

  // At this point |quad_iter| points to the first DrawQuad that needs
  // |shared_quad_state| fixed. |sqs_iter| points to the new address for the
  // first original SharedQuadState. Iterate through all DrawQuads updating
  // |shared_quad_state|.
  const SharedQuadState* last_quad_from_sqs = (*quad_iter)->shared_quad_state;
  while (quad_iter != render_pass->quad_list.end()) {
    DrawQuad* quad = *quad_iter;
    const SharedQuadState* from_sqs = quad->shared_quad_state;

    // If |shared_quad_state| of the current quad is different than the last
    // quad we know to advance |sqs_iter| as well.
    if (from_sqs != last_quad_from_sqs) {
      DCHECK(sqs_iter != render_pass->shared_quad_state_list.end());
      ++sqs_iter;
      last_quad_from_sqs = from_sqs;
    }

    quad->shared_quad_state = *sqs_iter;
    ++quad_iter;
  }
  DCHECK(++sqs_iter == render_pass->shared_quad_state_list.end());

  // Set the entire frame as damaged.
  render_pass->damage_rect = render_pass->output_rect;
}

}  // namespace viz
