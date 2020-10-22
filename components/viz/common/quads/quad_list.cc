// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/quad_list.h"

#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/largest_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"

namespace {
const size_t kDefaultNumQuadsToReserve = 128;
}  // namespace

namespace viz {

QuadList::QuadList()
    : ListContainer<DrawQuad>(LargestDrawQuadAlignment(),
                              LargestDrawQuadSize(),
                              kDefaultNumQuadsToReserve) {}

QuadList::QuadList(size_t default_size_to_reserve)
    : ListContainer<DrawQuad>(LargestDrawQuadAlignment(),
                              LargestDrawQuadSize(),
                              default_size_to_reserve) {}

QuadList::Iterator QuadList::InsertCopyBeforeDrawQuad(Iterator at,
                                                      size_t count) {
  DCHECK(at->shared_quad_state);
  switch (at->material) {
    case DrawQuad::Material::kDebugBorder: {
      const auto copy = *DebugBorderDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
          at, count, copy);
    }
    case DrawQuad::Material::kPictureContent: {
      const auto copy = *PictureDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<PictureDrawQuad>(at, count,
                                                                   copy);
    }
    case DrawQuad::Material::kTextureContent: {
      const auto copy = *TextureDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<TextureDrawQuad>(at, count,
                                                                   copy);
    }
    case DrawQuad::Material::kSolidColor: {
      const auto copy = *SolidColorDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<SolidColorDrawQuad>(at, count,
                                                                      copy);
    }
    case DrawQuad::Material::kTiledContent: {
      const auto copy = *TileDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<TileDrawQuad>(at, count,
                                                                copy);
    }
    case DrawQuad::Material::kStreamVideoContent: {
      const auto copy = *StreamVideoDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<StreamVideoDrawQuad>(
          at, count, copy);
    }
    case DrawQuad::Material::kSurfaceContent: {
      const auto copy = *SurfaceDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<SurfaceDrawQuad>(at, count,
                                                                   copy);
    }
    case DrawQuad::Material::kVideoHole: {
      const auto copy = *VideoHoleDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<VideoHoleDrawQuad>(at, count,
                                                                     copy);
    }
    case DrawQuad::Material::kYuvVideoContent: {
      const auto copy = *YUVVideoDrawQuad::MaterialCast(*at);
      return InsertBeforeAndInvalidateAllPointers<YUVVideoDrawQuad>(at, count,
                                                                    copy);
    }
    // RenderPass quads should not be copied.
    case DrawQuad::Material::kAggregatedRenderPass:
    case DrawQuad::Material::kCompositorRenderPass:
    case DrawQuad::Material::kInvalid:
      NOTREACHED();  // Invalid DrawQuad material.
      return at;
  }
}

}  // namespace viz
