// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_VIZ_COMMON_QUADS_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_DRAW_QUAD_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace viz {

// DrawQuad is a bag of data used for drawing a quad. Because different
// materials need different bits of per-quad data to render, classes that derive
// from DrawQuad store additional data in their derived instance. The Material
// enum is used to "safely" downcast to the derived class.
// Note: quads contain rects and sizes, which live in different spaces. There is
// the "content space", which is the arbitrary space in which the quad's
// geometry is defined (generally related to the layer that produced the quad,
// e.g. the geometry space for PictureLayerImpls or the layer's coordinate space
// for most other layers). There is also the "target space", which is the space,
// in "physical" pixels, of the render target where the quads is drawn. The
// quad's transform maps the content space to the target space.
class VIZ_COMMON_EXPORT DrawQuad {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Material {
    kInvalid = 0,
    kDebugBorder = 1,
    kPictureContent = 2,
    // This is the compositor, pre-aggregation, draw quad.
    kCompositorRenderPass = 3,
    // This is the viz, post-aggregation, draw quad.
    kAggregatedRenderPass = 4,
    kSolidColor = 5,
    kSharedElement = 6,
    // kStreamVideoContent = 7,  // Removed. Replaced with kTextureContent.
    kSurfaceContent = 8,
    kTextureContent = 9,
    kTiledContent = 10,
    // kYuvVideoContent = 11,  // Removed. kTextureContent used instead.
    kVideoHole = 12,
    kMaxValue = kVideoHole
  };

  DrawQuad(const DrawQuad& other);
  virtual ~DrawQuad();

  Material material;

  // This rect, after applying the quad_transform(), gives the geometry that
  // this quad should draw to. This rect lives in content space.
  gfx::Rect rect;

  // Allows changing the rect that gets drawn to make it smaller. This value
  // should be clipped to |rect|. This rect lives in content space.
  gfx::Rect visible_rect;

  // By default blending is used when some part of the quad is not opaque.
  // With this setting, it is possible to force blending on regardless of the
  // opaque area.
  bool needs_blending;

  // Stores state common to a large bundle of quads; kept separate for memory
  // efficiency. There is special treatment to reconstruct these pointers
  // during serialization.
  // RAW_PTR_EXCLUSION: Performance reasons (rendering.mobile,
  // Graphics.Smoothness, see crbug.com/345298647)
  RAW_PTR_EXCLUSION const SharedQuadState* shared_quad_state;

  bool IsDebugQuad() const { return material == Material::kDebugBorder; }

  bool ShouldDrawWithBlendingForReasonOtherThanMaskFilter() const {
    return needs_blending || shared_quad_state->opacity < 1.0f ||
           shared_quad_state->blend_mode != SkBlendMode::kSrcOver;
  }

  bool ShouldDrawWithBlending() const {
    return ShouldDrawWithBlendingForReasonOtherThanMaskFilter() ||
           !shared_quad_state->mask_filter_info.IsEmpty();
  }

  // Is the left edge of this tile aligned with the originating layer's
  // left edge?
  bool IsLeftEdge() const {
    return rect.x() == shared_quad_state->quad_layer_rect.x();
  }

  // Is the top edge of this tile aligned with the originating layer's
  // top edge?
  bool IsTopEdge() const {
    return rect.y() == shared_quad_state->quad_layer_rect.y();
  }

  // Is the right edge of this tile aligned with the originating layer's
  // right edge?
  bool IsRightEdge() const {
    return rect.right() == shared_quad_state->quad_layer_rect.right();
  }

  // Is the bottom edge of this tile aligned with the originating layer's
  // bottom edge?
  bool IsBottomEdge() const {
    return rect.bottom() == shared_quad_state->quad_layer_rect.bottom();
  }

  // Is any edge of this tile aligned with the originating layer's
  // corresponding edge?
  bool IsEdge() const {
    return IsLeftEdge() || IsTopEdge() || IsRightEdge() || IsBottomEdge();
  }

  void AsValueInto(base::trace_event::TracedValue* value) const;

  struct VIZ_COMMON_EXPORT Resources {
    enum : size_t { kMaxResourceIdCount = 1 };
    Resources();

    ResourceId* begin() { return ids; }
    ResourceId* end() {
      DCHECK_LE(count, kMaxResourceIdCount);
      return ids + count;
    }

    const ResourceId* begin() const { return ids; }
    const ResourceId* end() const {
      DCHECK_LE(count, kMaxResourceIdCount);
      return ids + count;
    }

    uint32_t count;
    ResourceId ids[kMaxResourceIdCount];
  };

  // TODO(crbug.com/332564976): Change this to be one ResourceId since there is
  // now max one resource per quad.
  Resources resources;

  template <typename T>
  const T* DynamicCast() const {
    return this->material == T::kMaterial ? static_cast<const T*>(this)
                                          : nullptr;
  }

 protected:
  DrawQuad();

  void SetAll(const SharedQuadState* quad_state,
              Material m,
              const gfx::Rect& r,
              const gfx::Rect& visible_r,
              bool blending);
  virtual void ExtendValue(base::trace_event::TracedValue* value) const = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_DRAW_QUAD_H_
