// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/renderer_data_validation.h"

#include <utility>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace autofill {

gfx::RectF TransformToRootCoordinates(content::RenderFrameHost& source_rfh,
                                      gfx::RectF r) {
  // Transforms all corners to handle CSS `transform: scale(...)` correctly
  // (crbug.com/562177779).
  auto transform_to_root = [](content::RenderWidgetHostView& view,
                              const gfx::RectF& rect) {
    return gfx::QuadF(view.TransformPointToRootCoordSpaceF(rect.origin()),
                      view.TransformPointToRootCoordSpaceF(rect.top_right()),
                      view.TransformPointToRootCoordSpaceF(rect.bottom_right()),
                      view.TransformPointToRootCoordSpaceF(rect.bottom_left()))
        .BoundingBox();
  };

  content::RenderFrameHost* rfh = &source_rfh;
  content::RenderWidgetHostView* view = rfh->GetView();
  if (!view) {
    return r;
  }
  r = transform_to_root(*view, r);

  // Intersect the coordinates with the view's bounds and all ancestor views'
  // bounds. This is to make it hard for a malicious frame to position an
  // Autofill or Password Manager popup over a field from a different frame
  // (provided the attacker's and the victim's frames do not share the same
  // local root).
  content::RenderWidgetHostView* last_view = nullptr;
  for (; rfh; rfh = rfh->GetParentOrOuterDocument()) {
    view = rfh->GetView();
    if (!view || view == std::exchange(last_view, view)) {
      continue;
    }
    gfx::RectF bounds =
        transform_to_root(*view, gfx::RectF(view->GetViewBounds().size()));
    gfx::PointF origin = r.origin();
    if (!r.InclusiveIntersect(bounds)) {
      r = gfx::RectF(bounds.ClosestPoint(origin), gfx::SizeF());
    }
  }
  return r;
}

}  // namespace autofill
