// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_prediction_improvements/prediction_improvements_icon_background.h"

#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/view.h"

namespace autofill_prediction_improvements {

PredictionImprovementsIconBackground::PredictionImprovementsIconBackground(
    views::Emphasis radius)
    : radius_(radius) {}

PredictionImprovementsIconBackground::~PredictionImprovementsIconBackground() =
    default;

void PredictionImprovementsIconBackground::OnViewThemeChanged(
    views::View* view) {
  view->SchedulePaint();
}

void PredictionImprovementsIconBackground::Paint(gfx::Canvas* canvas,
                                                 views::View* view) const {
  if (ui::ColorProvider* color_provider = view->GetColorProvider()) {
    const gfx::Rect bounds = view->GetContentsBounds();
    const std::array<gfx::Point, 2> points = {bounds.origin(),
                                              bounds.bottom_right()};
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    const SkColor start_color =
        color_provider->GetColor(ui::kColorSysGradientTertiary);
    const SkColor end_color =
        color_provider->GetColor(ui::kColorSysGradientPrimary);
    flags.setShader(gfx::CreateGradientShader(points.front(), points.back(),
                                              start_color, end_color));

    canvas->DrawRoundRect(
        bounds, ChromeLayoutProvider::Get()->GetCornerRadiusMetric(radius_),
        flags);
  }
}

}  // namespace autofill_prediction_improvements
