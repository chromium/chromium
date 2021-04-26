// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/transition_utils.h"

#include <vector>

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

}  // namespace viz
