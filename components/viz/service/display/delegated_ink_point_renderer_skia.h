// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_SKIA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_SKIA_H_

#include <vector>

#include "components/viz/service/display/delegated_ink_point_renderer_base.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/transform.h"

class SkCanvas;

namespace viz {

// This class handles drawing the delegated ink trail when the Skia renderer
// is in use by filtering everything out with timestamps before the metadata,
// predicting another point or two, and drawing the points with bezier curves
// between them with Skia commands onto the canvas provided by the Skia
// renderer, the |current_canvas_|.
// TODO(crbug.com/40118757): Specify exactly how many points are predicted.
//
// When an ink trail is getting ready to be drawn, after points and metadata
// have already arrived, the first thing that will be called is
// FinalizePathForDraw(). This is called when determining the portion of the
// frame that needs to be redrawn, so that GetDamageRect() can return the union
// of the bounding box of the previous ink trail that had been drawn (stored in
// |new_trail_damage_rect_| at this time) and the new ink trail.
// FinalizePathForDraw() will filter points, predict new ones, and use the
// result to update |path_| with the new ink trail. It also calls
// SetDamageRect() with the new trail's damage rect, which moves the rect
// currently in |new_trail_damage_rect_| to |old_trail_damage_rect_| and the new
// damage rect goes to |new_trail_damage_rect_|. GetDamageRect() then returns
// the union of the two for drawing.
// Then, after everything else in the frame has been drawn,
// DrawDelegatedInkTrail() will be called to actually draw the |path_| that was
// determined in FinalizePathForDraw().
// After drawing and swapping the buffers has completed, the display will call
// GetDamageRect() in order to update the ink trail damage rect on the surface
// aggregator, which is used to ensure one more frame will be drawn so that a
// trail never sticks around for longer than intended.
//
// For more information on the feature, please see the explainer:
// https://github.com/WICG/ink-enhancement/blob/main/README.md
class VIZ_SERVICE_EXPORT DelegatedInkPointRendererSkia
    : public DelegatedInkPointRendererBase {
 public:
  DelegatedInkPointRendererSkia() = default;
  DelegatedInkPointRendererSkia(const DelegatedInkPointRendererSkia&) = delete;
  DelegatedInkPointRendererSkia& operator=(
      const DelegatedInkPointRendererSkia&) = delete;

  // Set |path_| that will be drawn in the DrawDelegatedInkTrail() call. This is
  // called before GetDamageRect() when determining what portion of the frame
  // needs to be redrawn - earlier in the execution than the actual drawing
  // happens. Finalizing the trail when determining the portion of the frame
  // that needs to be redrawn is necessary so that the damage rect of the new
  // trail is known and the new trail can be drawn entirely, while
  // simultaneously removing the old trail and optimizing the damage rect to be
  // as small as possible. The alternative is to use |metadata_|'s presentation
  // area as the damage rect instead - then the path could be finalized directly
  // before drawing instead. However, this could result in a noticeable
  // performance hit by drawing much more than necessary.
  void FinalizePathForDraw() override;

  // Returns the union of |old_trail_damage_rect_| and |new_trail_damage_rect_|.
  gfx::Rect GetDamageRect() override;

  // Draw the delegated ink trail to the provided canvas. Since the trail is
  // stored in root (metadata) space, it needs to be transformed to render pass
  // space.
  void DrawDelegatedInkTrail(SkCanvas* canvas,
                             const gfx::Transform& transform_to_render_pass);

 private:
  void SetDamageRect(gfx::RectF);

  // First filters the points that are stored to only keep points with a
  // timestamp equal to or later than |metadata_|'s, then predict points if
  // possible. Then converts those points into SkPoints and returns them.
  std::vector<SkPoint> GetPointsToDraw();

  int GetPathPointCountForTest() const override;

  // The path that will be drawn in DrawDelegatedInkTrail(). See class comments
  // and FinalizePathForDraw() comment to understand when and why this is
  // updated.
  SkPath path_;

  // The damage rects for the trail currently on the screen, and the next one
  // to be drawn, as of the DrawDelegatedInkTrail() call.
  gfx::RectF old_trail_damage_rect_;
  gfx::RectF new_trail_damage_rect_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_SKIA_H_
