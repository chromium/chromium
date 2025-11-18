// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_

#include <array>
#include <optional>

#include "base/values.h"
#include "cc/paint/paint_flags.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/video_types.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace viz {
namespace mojom {
class TextureQuadStateDataView;
}

// The priority for a quads to require being promoted to overlay.
enum class OverlayPriority { kLow, kRegular, kRequired };

class VIZ_COMMON_EXPORT TextureDrawQuad : public DrawQuad {
 public:
  static constexpr Material kMaterial = Material::kTextureContent;

  TextureDrawQuad();
  TextureDrawQuad(const TextureDrawQuad& other);

  ~TextureDrawQuad() override;

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              ResourceId resource_id,
              const gfx::PointF& top_left,
              const gfx::PointF& bottom_right,
              SkColor4f background,
              bool nearest,
              bool secure_output,
              gfx::ProtectedVideoType video_type);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              ResourceId resource_id,
              const gfx::PointF& top_left,
              const gfx::PointF& bottom_right,
              SkColor4f background,
              bool nearest,
              bool secure_output,
              gfx::ProtectedVideoType video_type);

  // Returns the texture coordinates in the range [0, 1].
  gfx::RectF GetNormalizedTexCoords(const gfx::Size& resource_size) const {
    // TODO(crbug.com/451876192): This parameter is currently unused because
    // tex_coord_rect_ is always normalized. It is included here to prepare for
    // the next CL where tex_coord_rect_ may be unnormalized, requiring
    // resource_size to perform the normalization.
    return tex_coord_rect_;
  }

  // Returns the texture coordinates in the range [0, resource_size].
  gfx::RectF GetUnnormalizedTexCoords(const gfx::Size& resource_size) const {
    // tex_coord_rect_ is currently always normalized, so we must scale it.
    // In the future, if the internal storage becomes unnormalized, this will
    // simply return tex_coord_rect_ directly.
    return gfx::ScaleRect(tex_coord_rect_,
                          static_cast<float>(resource_size.width()),
                          static_cast<float>(resource_size.height()));
  }

  // Sets the texture coordinates in the range [0, 1].
  void SetNormalizedTexCoordsForTesting(const gfx::RectF& normalized_rect,
                                        const gfx::Size& resource_size) {
    // TODO(crbug.com/451876192): This parameter is unused because the internal
    // storage is currently normalized. It is included to establish the API
    // for the future CL where we will need to scale the input `normalized_rect`
    // by `resource_size` to store it as unnormalized coordinates.
    tex_coord_rect_ = normalized_rect;
  }

  SkColor4f background_color = SkColors::kTransparent;
  cc::PaintFlags::DynamicRangeLimitMixture dynamic_range_limit;
  bool nearest_neighbor : 1;

  // True if the quad must only be GPU composited if shown on secure outputs.
  bool secure_output_only : 1;

  // True if this quad contains a video frame from VideoResourceUpdater instead
  // of canvas or webgl content.
  bool is_video_frame : 1;

  // If true we will treat the alpha in the texture as 1. This works like rgbx
  // and not like blend mode 'kSrc' which would copy the alpha.
  bool force_rgbx : 1 = false;

  // kClear if the contents do not require any special protection. See enum of a
  // list of protected content types. Protected contents cannot be displayed via
  // regular display path. They need either a protected output or a protected
  // hardware overlay.
  gfx::ProtectedVideoType protected_video_type : 2;
  // The overlay promotion hint.
  OverlayPriority overlay_priority_hint = OverlayPriority::kRegular;

  // This optional damage is in target render pass coordinate space.
  std::optional<gfx::Rect> damage_rect;

  struct VIZ_COMMON_EXPORT RoundedDisplayMasksInfo {
    static constexpr size_t kMaxRoundedDisplayMasksCount = 2;
    static constexpr size_t kOriginRoundedDisplayMaskIndex = 0;
    static constexpr size_t kOtherRoundedDisplayMaskIndex = 1;

    static RoundedDisplayMasksInfo CreateRoundedDisplayMasksInfo(
        int origin_rounded_display_mask_radius,
        int other_rounded_display_mask_radius,
        bool is_horizontally_positioned = true);

    // Returns the bounds of rounded display masks in target space that are
    // associated with the `quad`.
    static std::array<gfx::RectF, kMaxRoundedDisplayMasksCount>
    GetRoundedDisplayMasksBounds(const DrawQuad* quad);

    RoundedDisplayMasksInfo();

    bool IsEmpty() const;

    bool is_horizontally_positioned = true;

    // Radii of display's rounded corners masks in pixels.
    std::array<uint8_t, kMaxRoundedDisplayMasksCount> radii = {0, 0};
  };

  // Encodes the radii(in pixels) and position of rounded-display mask textures
  // in target space.
  //
  // Radius at index `kOriginRoundedDisplayMaskIndex` is always drawn at origin,
  // whereas radius at index `kOtherRoundedDisplayMaskIndex` is drawn either at
  // the upper right corner or lower left corner based on
  // `is_horizontally_positioned`.
  //
  // For example: If the resource in target space has dimensions of (10, 10,
  // 100, 50) and both radii has value of 15, the masks are drawn at bounds (10,
  // 10, 15, 15) and (95, 10, 15, 15) if `is_horizontally_positioned` is true
  // otherwise the masks are drawn at bounds (10, 10, 15, 15) and (10, 45, 15,
  // 15).
  RoundedDisplayMasksInfo rounded_display_masks_info;

  void set_force_rgbx(bool force_rgbx_value = true) {
    force_rgbx = force_rgbx_value;
  }

  static const TextureDrawQuad* MaterialCast(const DrawQuad*);

 private:
  // TODO(crbug.com/451876192): Remove friend classes after the refactor
  // to make TextureDrawQuad use unnormalized coordinates is complete
  friend struct mojo::StructTraits<mojom::TextureQuadStateDataView, DrawQuad>;
  friend void TextureDrawQuadToDict(const TextureDrawQuad* draw_quad,
                                    base::Value::Dict* dict);

  gfx::RectF tex_coord_rect_;

  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_TEXTURE_DRAW_QUAD_H_
