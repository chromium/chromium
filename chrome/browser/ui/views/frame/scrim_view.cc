// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/scrim_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"

ScrimView::ScrimView() {
  SetCanProcessEventsWithinSubtree(false);
  // This view must be painted to a layer so that it can be drawn on top of the
  // contents view. Otherwise, the scrim is drawn on the BrowserView's layer,
  // which always stays behind the content's layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  GetViewAccessibility().SetIsInvisible(true);
  // Maybe consider using a different color for the scrim?
  // kColorSysStateScrim is a semi-transparent black which has no effect on a
  // pure black background. In contrast, macOS sheet uses a semi-transparent
  // grey scrim which lightens a dark background.
  SetBackground(views::CreateSolidBackground(ui::kColorSysStateScrim));
  SetVisible(false);
}

void ScrimView::SetRoundedCorners(const gfx::RoundedCornersF& radii) {
  layer()->SetRoundedCornerRadius(radii);
  layer()->SetIsFastRoundedCorner(true);
}

BEGIN_METADATA(ScrimView)
END_METADATA
