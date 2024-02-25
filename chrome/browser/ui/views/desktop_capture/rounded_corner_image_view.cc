// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/rounded_corner_image_view.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/layout/layout_provider.h"

bool RoundedCornerImageView::GetCanProcessEventsWithinSubtree() const {
  return false;
}

void RoundedCornerImageView::OnPaint(gfx::Canvas* canvas) {
  SkPath mask;
  CHECK(GetLayoutProvider());
  const int corner_radius =
      GetLayoutProvider()->GetCornerRadiusMetric(views::Emphasis::kMedium);
  mask.addRoundRect(gfx::RectToSkRect(GetImageBounds()), corner_radius,
                    corner_radius);
  canvas->ClipPath(mask, true);
  ImageView::OnPaint(canvas);
}

BEGIN_METADATA(RoundedCornerImageView)
END_METADATA
