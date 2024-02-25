// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/damage_frame_annotator.h"

#include <algorithm>
#include <utility>

#include "cc/base/math_util.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/service/display/aggregated_frame.h"

namespace viz {

DamageFrameAnnotator::DamageFrameAnnotator() = default;
DamageFrameAnnotator::~DamageFrameAnnotator() = default;

void DamageFrameAnnotator::AnnotateAggregatedFrame(AggregatedFrame* frame) {
  DCHECK(frame);
  auto* root_render_pass = frame->render_pass_list.back().get();

  const gfx::Rect& damage_rect = root_render_pass->damage_rect;
  gfx::Transform transform;
  transform.Translate(damage_rect.x(), damage_rect.y());

  annotations_.push_back(
      AnnotationData{gfx::Rect(damage_rect.size()), transform,
                     Highlight{SkColor4f{1.0, 0, 0, 0.5}, 4}});

  AnnotateRootRenderPass(root_render_pass);
  annotations_.clear();
}

void DamageFrameAnnotator::AnnotateRootRenderPass(
    AggregatedRenderPass* render_pass) {
  const size_t num_quads_to_add = annotations_.size();

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

    SharedQuadState* new_sqs = render_pass->shared_quad_state_list
                                   .AllocateAndConstruct<SharedQuadState>();
    new_sqs->SetAll(annotation.transform, output_rect, output_rect,
                    gfx::MaskFilterInfo(), output_rect,
                    /*contents_opaque=*/false, /*opacity_f=*/1.f,
                    SkBlendMode::kSrcOver, /*sorting_context=*/0,
                    /*layer_id=*/0u, /*fast_rounded_corner=*/false);

    DebugBorderDrawQuad* new_quad =
        static_cast<DebugBorderDrawQuad*>(*quad_iter);
    new_quad->SetNew(new_sqs, annotation.rect, annotation.rect,
                     annotation.highlight.color, annotation.highlight.width);

    ++quad_iter;
  }

  // Set the entire frame as damaged.
  render_pass->damage_rect = render_pass->output_rect;
}

}  // namespace viz
