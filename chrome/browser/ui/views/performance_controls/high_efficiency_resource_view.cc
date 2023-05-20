// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_resource_view.h"
#include <string>

#include "base/numerics/math_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/performance_manager/public/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    HighEfficiencyResourceView,
    kHighEfficiencyResourceViewMemorySavingsElementId);

namespace {

constexpr int kMemoryLabelSizeDelta = 12;
constexpr int kGaugeRadius = 70;
constexpr int kStrokeWidth = 8;
constexpr int kTickStrokeWidth = 2;
constexpr int kBucketCount = 4;
constexpr double kBucketWidthDegrees = 180 / kBucketCount;

// Returns which of the four quartiles of memory savings this number falls into.
// The lowest memory usage quartile (0-24th percentile) returns 0 and the
// highest quartile (75-99 percentile) returns 3.
int GetMemorySavingsQuartile(const int memory_savings_bytes) {
  if (memory_savings_bytes <
      performance_manager::features::kHighEfficiencyChartPmf25PercentileBytes
          .Get()) {
    return 0;
  } else if (memory_savings_bytes <
             performance_manager::features::
                 kHighEfficiencyChartPmf50PercentileBytes.Get()) {
    return 1;
  } else if (memory_savings_bytes <
             performance_manager::features::
                 kHighEfficiencyChartPmf75PercentileBytes.Get()) {
    return 2;
  } else {
    return 3;
  }
}

class GaugeView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(GaugeView);

  explicit GaugeView(const int memory_savings_bytes)
      : memory_savings_bytes_(memory_savings_bytes) {
    SetOrientation(views::LayoutOrientation::kVertical);
    SetMainAxisAlignment(views::LayoutAlignment::kEnd);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  }

  ~GaugeView() override = default;

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size((kGaugeRadius + kStrokeWidth) * 2,
                     kGaugeRadius + kStrokeWidth);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const gfx::PointF center = gfx::RectF(GetLocalBounds()).bottom_center();

    DrawArc(canvas, center, 180,
            GetColorProvider()->GetColor(ui::kColorMidground));

    // Map the memory savings to which of the 4 buckets it falls into and then
    // draw an arc to the middle of the corresponding bucket. This is why the
    // 0.5 parts of the multipliers are needed.
    const int memory_angle =
        (GetMemorySavingsQuartile(memory_savings_bytes_) + 0.5) *
        kBucketWidthDegrees;

    DrawArc(canvas, center, memory_angle,
            GetColorProvider()->GetColor(ui::kColorButtonBackgroundProminent));

    const SkColor tick_color =
        GetColorProvider()->GetColor(ui::kColorDialogBackground);
    for (int i = 1; i < kBucketCount; i++) {
      double angle = i * 180 / kBucketCount;
      DrawTick(canvas, center, angle, tick_color);
    }
  }

 private:
  const int memory_savings_bytes_;

  // Draws an arc starting at the far left, with the specified center point and
  // angle (in degrees).
  void DrawArc(gfx::Canvas* canvas,
               const gfx::PointF center,
               const int angle_degrees,
               const SkColor color) {
    SkPath arc_path;
    arc_path.addArc(
        SkRect::MakeXYWH(center.x() - kGaugeRadius, center.y() - kGaugeRadius,
                         2 * kGaugeRadius, 2 * kGaugeRadius),
        180, angle_degrees);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kStrokeWidth);
    flags.setColor(color);
    flags.setAntiAlias(true);

    canvas->DrawPath(arc_path, flags);
  }

  // Draw a tick mark over the arc, that is `angle_degrees` from the far left.
  void DrawTick(gfx::Canvas* canvas,
                const gfx::PointF center,
                const double angle_degrees,
                const SkColor color) {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kTickStrokeWidth);
    flags.setColor(color);
    flags.setAntiAlias(true);

    // Vector of length 1 in the direction of the tick mark.
    gfx::Vector2dF unit_vector(
        std::cos(-angle_degrees * base::kPiDouble / 180),
        std::sin(-angle_degrees * base::kPiDouble / 180));

    // Draw a line from the inner edge of the arc to the outer edge of the arc.
    canvas->DrawLine(
        center + ScaleVector2d(unit_vector, kGaugeRadius - kStrokeWidth / 2),
        center + ScaleVector2d(unit_vector, kGaugeRadius + kStrokeWidth / 2),
        flags);
  }
};

BEGIN_METADATA(GaugeView, views::View)
END_METADATA

}  // namespace

HighEfficiencyResourceView::HighEfficiencyResourceView(
    const int memory_savings_bytes) {
  SetOrientation(views::LayoutOrientation::kVertical);

  auto* gauge_view =
      AddChildView(std::make_unique<GaugeView>(memory_savings_bytes));

  std::u16string formatted_savings = ui::FormatBytes(memory_savings_bytes);
  auto* memory_label = gauge_view->AddChildView(
      std::make_unique<views::Label>(formatted_savings));
  memory_label->SetProperty(views::kElementIdentifierKey,
                            kHighEfficiencyResourceViewMemorySavingsElementId);
  memory_label->SetFontList(
      memory_label->font_list().DeriveWithSizeDelta(kMemoryLabelSizeDelta));
  memory_label->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_HIGH_EFFICIENCY_DIALOG_SAVINGS_ACCNAME, {formatted_savings}));

  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_HIGH_EFFICIENCY_DIALOG_SAVINGS_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
}
