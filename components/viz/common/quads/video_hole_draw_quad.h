// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_VIDEO_HOLE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_VIDEO_HOLE_DRAW_QUAD_H_

#include <stddef.h>

#include "base/unguessable_token.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/video_types.h"

namespace viz {

// A VideoHoleDrawQuad is used by Chromecast to instruct that a video
// overlay is to be activated. It carries |overlay_plane_id| which identifies
// the origin of the video overlay frame. |overlay_plane_id| will be used
// to find the right VideoDecoder to apply SetGeometry() on.
class VIZ_COMMON_EXPORT VideoHoleDrawQuad : public DrawQuad {
 public:
  static constexpr Material kMaterial = Material::kVideoHole;

  VideoHoleDrawQuad();
  VideoHoleDrawQuad(const VideoHoleDrawQuad& other);
  ~VideoHoleDrawQuad() override;

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              const base::UnguessableToken& plane_id);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              const base::UnguessableToken& plane_id);

  static const VideoHoleDrawQuad* MaterialCast(const DrawQuad*);

  base::UnguessableToken overlay_plane_id;

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_VIDEO_HOLE_DRAW_QUAD_H_
