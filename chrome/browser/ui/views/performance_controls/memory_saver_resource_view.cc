// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/performance_controls/memory_saver_resource_view.h"

#include <string>

#include "base/numerics/angle_conversions.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    MemorySaverResourceView,
    kMemorySaverResourceViewMemorySavingsElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    MemorySaverResourceView,
    kMemorySaverResourceViewMemoryLabelElementId);

namespace {

constexpr int kMemoryLabelSizeDelta = 12;
constexpr int kGaugeRadius = 70;
constexpr int kStrokeWidth = 8;
constexpr int kTickStrokeWidth = 2;
constexpr int kBucketCount = 4;
constexpr double kBucketWidthDegrees = 180 / kBucketCount;

constexpr int64_t kMemorySaverChartPmf25PercentileBytes = 62 * 1024 * 1024;
constexpr int64_t kMemorySaverChartPmf50PercentileBytes = 112 * 1024 * 1024;
constexpr int64_t kMemorySaverChartPmf75PercentileBytes = 197 * 1024 * 1024;
constexpr int64_t kMemorySaverChartPmf99PercentileBytes = 800 * 1024 * 1024;

// Enum to represent memory savings quartiles.
enum MemorySavingsQuartile {
  kLow = 0,
  kMedium = 1,
  kHigh = 2,
  kVeryHigh = 3,
  kHuge = 4,
  kMaxValue = kHuge,
};

// Each element represents the label for the chart when memory savings are in
// the quartile represented by `MemorySavingsQuartile`. The last "quartile"
// instead represents the 99th percentile (or a full chart).
constexpr int kQuartilesLabels[] = {
    IDS_MEMORY_SAVER_DIALOG_SMALL_SAVINGS_LABEL,
    IDS_MEMORY_SAVER_DIALOG_MEDIUM_SAVINGS_LABEL,
    IDS_MEMORY_SAVER_DIALOG_MEDIUM_SAVINGS_LABEL,
    IDS_MEMORY_SAVER_DIALOG_LARGE_SAVINGS_LABEL,
    IDS_MEMORY_SAVER_DIALOG_VERY_LARGE_SAVINGS_LABEL};

// Returns which of the four quartiles of memory savings this number falls into.
// The lowest memory usage quartile (0-24th percentile) returns 0 and the
// highest quartile (75-99 percentile) returns 3.
int GetMemorySavingsQuartile(const int64_t memory_savings_bytes) {
  if (memory_savings_bytes < kMemorySaverChartPmf25PercentileBytes) {
    return MemorySavingsQuartile::kLow;
  } else if (memory_savings_bytes < kMemorySaverChartPmf50PercentileBytes) {
    return MemorySavingsQuartile::kMedium;
  } else if (memory_savings_bytes < kMemorySaverChartPmf75PercentileBytes) {
    return MemorySavingsQuartile::kHigh;
  } else if (memory_savings_bytes < kMemorySaverChartPmf99PercentileBytes) {
    return MemorySavingsQuartile::kVeryHigh;
  } else {
    return MemorySavingsQuartile::kHuge;
  }
}

class GaugeView : public views::FlexLayoutView {
  METADATA_HEADER(GaugeView, views::FlexLayoutView)

 public:
  explicit GaugeView(const int64_t memory_savings_bytes)
      : memory_savings_bytes_(memory_savings_bytes) {
    SetOrientation(views::LayoutOrientation::kVertical);
    SetMainAxisAlignment(views::LayoutAlignment::kEnd);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  }

  ~GaugeView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
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
        std::min((GetMemorySavingsQuartile(memory_savings_bytes_) + 0.5) *
                     kBucketWidthDegrees,
                 180.0);

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
  const int64_t memory_savings_bytes_;

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
    const double angle_radians = base::DegToRad(angle_degrees);
    const gfx::Vector2dF unit_vector(std::cos(-angle_radians),
                                     std::sin(-angle_radians));

    // Draw a line from the inner edge of the arc to the outer edge of the arc.
    canvas->DrawLine(
        center + ScaleVector2d(unit_vector, kGaugeRadius - kStrokeWidth / 2),
        center + ScaleVector2d(unit_vector, kGaugeRadius + kStrokeWidth / 2),
        flags);
  }
};

BEGIN_METADATA(GaugeView)
END_METADATA

}  // namespace

MemorySaverResourceView::MemorySaverResourceView(
    const int64_t memory_savings_bytes) {
  SetOrientation(views::LayoutOrientation::kVertical);

  auto* gauge_view =
      AddChildView(std::make_unique<GaugeView>(memory_savings_bytes));

  std::u16string formatted_savings = ui::FormatBytes(memory_savings_bytes);
  auto* memory_savings = gauge_view->AddChildView(
      std::make_unique<views::Label>(formatted_savings));
  memory_savings->SetProperty(views::kElementIdentifierKey,
                              kMemorySaverResourceViewMemorySavingsElementId);
  memory_savings->SetFontList(
      memory_savings->font_list().DeriveWithSizeDelta(kMemoryLabelSizeDelta));
  memory_savings->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_MEMORY_SAVER_DIALOG_SAVINGS_ACCNAME, {formatted_savings}));

  auto* memory_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          kQuartilesLabels[GetMemorySavingsQuartile(memory_savings_bytes)]),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  memory_label->SetProperty(views::kElementIdentifierKey,
                            kMemorySaverResourceViewMemoryLabelElementId);
}
