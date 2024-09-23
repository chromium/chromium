// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/largest_draw_quad.h"

#include <stddef.h>

#include <algorithm>

#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"

namespace {

template <typename...>
struct MaxSize {};
template <class T, class... Args>
struct MaxSize<T, Args...> {
  static constexpr size_t value = sizeof(T) > MaxSize<Args...>::value
                                      ? sizeof(T)
                                      : MaxSize<Args...>::value;
};
template <>
struct MaxSize<> {
  static constexpr size_t value = 0;
};

constexpr size_t kLargestDrawQuadSize =
    MaxSize<viz::AggregatedRenderPassDrawQuad,
            viz::DebugBorderDrawQuad,
            viz::PictureDrawQuad,
            viz::CompositorRenderPassDrawQuad,
            viz::SolidColorDrawQuad,
            viz::SurfaceDrawQuad,
            viz::TextureDrawQuad,
            viz::TileDrawQuad>::value;

template <typename...>
struct MaxAlign {};
template <class T, class... Args>
struct MaxAlign<T, Args...> {
  static constexpr size_t value = alignof(T) > MaxAlign<Args...>::value
                                      ? alignof(T)
                                      : MaxAlign<Args...>::value;
};
template <>
struct MaxAlign<> {
  static constexpr size_t value = 0;
};

constexpr size_t kLargestDrawQuadAlignment =
    MaxAlign<viz::AggregatedRenderPassDrawQuad,
             viz::DebugBorderDrawQuad,
             viz::PictureDrawQuad,
             viz::CompositorRenderPassDrawQuad,
             viz::SolidColorDrawQuad,
             viz::SurfaceDrawQuad,
             viz::TextureDrawQuad,
             viz::TileDrawQuad>::value;

}  // namespace

namespace viz {

size_t LargestDrawQuadSize() {
  return kLargestDrawQuadSize;
}

size_t LargestDrawQuadAlignment() {
  return kLargestDrawQuadAlignment;
}

}  // namespace viz
