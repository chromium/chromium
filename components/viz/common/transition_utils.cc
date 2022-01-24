// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/transition_utils.h"

#include <memory>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"

namespace viz {

// static
float TransitionUtils::ComputeAccumulatedOpacity(
    const CompositorRenderPassList& render_passes,
    CompositorRenderPassId target_id) {
  float opacity = 1.f;
  bool found_render_pass = false;
  for (auto& render_pass : render_passes) {
    // If we haven't even reached the needed render pass, then we don't need to
    // iterate the quads. Note that we also don't iterate the quads of the
    // target render pass itself, since it can't draw itself.
    if (!found_render_pass) {
      found_render_pass = render_pass->id == target_id;
      continue;
    }

    for (auto* quad : render_pass->quad_list) {
      if (quad->material != DrawQuad::Material::kCompositorRenderPass)
        continue;

      const auto* pass_quad = CompositorRenderPassDrawQuad::MaterialCast(quad);
      if (pass_quad->render_pass_id != target_id)
        continue;

      // TODO(vmpstr): We need to consider different blend modes as well,
      // although it's difficult in general. For the simple case of common
      // SrcOver blend modes however, we can just multiply the opacity.
      opacity *= pass_quad->shared_quad_state->opacity;
      target_id = render_pass->id;
      break;
    }
  }
  return opacity;
}

// static
std::unique_ptr<CompositorRenderPass>
TransitionUtils::CopyPassWithQuadFiltering(
    const CompositorRenderPass& source_pass,
    FilterCallback filter_callback) {
  // This code is similar to CompositorRenderPass::DeepCopy, but does special
  // logic when copying compositor render pass draw quads.
  auto copy_pass = CompositorRenderPass::Create(
      source_pass.shared_quad_state_list.size(), source_pass.quad_list.size());

  copy_pass->SetAll(
      source_pass.id, source_pass.output_rect, source_pass.damage_rect,
      source_pass.transform_to_root_target, source_pass.filters,
      source_pass.backdrop_filters, source_pass.backdrop_filter_bounds,
      source_pass.subtree_capture_id, source_pass.subtree_size,
      source_pass.shared_element_resource_id,
      source_pass.has_transparent_background, source_pass.cache_render_pass,
      source_pass.has_damage_from_contributing_content,
      source_pass.generate_mipmap, source_pass.has_per_quad_damage);

  if (source_pass.shared_quad_state_list.empty())
    return copy_pass;

  SharedQuadStateList::ConstIterator sqs_iter =
      source_pass.shared_quad_state_list.begin();
  SharedQuadState* copy_shared_quad_state =
      copy_pass->CreateAndAppendSharedQuadState();
  *copy_shared_quad_state = **sqs_iter;

  for (auto* quad : source_pass.quad_list) {
    while (quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
      DCHECK(sqs_iter != source_pass.shared_quad_state_list.end());
      copy_shared_quad_state = copy_pass->CreateAndAppendSharedQuadState();
      *copy_shared_quad_state = **sqs_iter;
    }
    DCHECK(quad->shared_quad_state == *sqs_iter);

    if (filter_callback.Run(*quad, *copy_pass.get()))
      continue;

    if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
      const auto* pass_quad = CompositorRenderPassDrawQuad::MaterialCast(quad);
      copy_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad,
                                                     pass_quad->render_pass_id);
    } else {
      copy_pass->CopyFromAndAppendDrawQuad(quad);
    }
  }

  return copy_pass;
}

}  // namespace viz
