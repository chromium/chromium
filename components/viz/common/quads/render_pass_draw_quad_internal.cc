// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/common/quads/render_pass_draw_quad_internal.h"

#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/traced_value.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace viz {

RenderPassDrawQuadInternal::RenderPassDrawQuadInternal() = default;

RenderPassDrawQuadInternal::RenderPassDrawQuadInternal(
    const RenderPassDrawQuadInternal& other) = default;

RenderPassDrawQuadInternal::~RenderPassDrawQuadInternal() = default;

void RenderPassDrawQuadInternal::ExtendValue(
    base::trace_event::TracedValue* value) const {
  value->SetInteger("mask_resource_id",
                    resources.ids[kMaskResourceIdIndex].GetUnsafeValue());
  cc::MathUtil::AddToTracedValue("mask_texture_size", mask_texture_size, value);
  cc::MathUtil::AddToTracedValue("mask_uv_rect", mask_uv_rect, value);
  cc::MathUtil::AddToTracedValue("tex_coord_rect", tex_coord_rect, value);
  value->SetBoolean("force_anti_aliasing_off", force_anti_aliasing_off);
  value->SetDouble("backdrop_filter_quality", backdrop_filter_quality);
  value->SetBoolean("intersects_damage_under", intersects_damage_under);
}

}  // namespace viz
