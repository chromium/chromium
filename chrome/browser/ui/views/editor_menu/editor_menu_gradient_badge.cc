// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_gradient_badge.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/badge_painter.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace chromeos::editor_menu {

namespace {

// TODO(b/301537126): Move this to chromeos_strings.grd after the text is
// finalised.
constexpr char16_t kBadgeText[] = u"Experiment";

// TODO(b/302209940): Replace these with color tokens to support dark mode.
constexpr SkColor kBadgeBackgroundColorStart = SkColorSetRGB(0xB5, 0xC4, 0xFF);
constexpr SkColor kBadgeBackgroundColorEnd = SkColorSetRGB(0xB3, 0xEF, 0xD4);

}  // namespace

EditorMenuGradientBadge::EditorMenuGradientBadge() = default;

EditorMenuGradientBadge::~EditorMenuGradientBadge() = default;

gfx::Size EditorMenuGradientBadge::CalculatePreferredSize() const {
  return views::BadgePainter::GetBadgeSize(kBadgeText,
                                           views::Label::GetDefaultFontList());
}

void EditorMenuGradientBadge::OnPaint(gfx::Canvas* canvas) {
  const gfx::FontList& primary_font = views::Label::GetDefaultFontList();
  gfx::FontList badge_font = views::BadgePainter::GetBadgeFont(primary_font);

  // Calculate the bounding box for badge text.
  const gfx::Rect badge_text_bounds(
      gfx::Point(views::BadgePainter::kBadgeInternalPadding,
                 gfx::GetFontCapHeightCenterOffset(primary_font, badge_font)),
      gfx::GetStringSize(kBadgeText, badge_font));

  // Outset the bounding box for padding.
  gfx::Rect badge_outset_around_text(badge_text_bounds);
  badge_outset_around_text.Inset(-gfx::AdjustVisualBorderForFont(
      badge_font, gfx::Insets(views::BadgePainter::kBadgeInternalPadding)));

  // Compute the rounded rect which will contain the gradient background.
  SkPath path;
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kBadgeRadius);
  path.addRoundRect(gfx::RectToSkRect(badge_outset_around_text), radius,
                    radius);

  // Draw the gradient background.
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrcOver);
  flags.setShader(gfx::CreateGradientShader(
      badge_outset_around_text.left_center(),
      badge_outset_around_text.right_center(), kBadgeBackgroundColorStart,
      kBadgeBackgroundColorEnd));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);

  // Draw the badge text.
  const SkColor foreground_color =
      GetColorProvider()->GetColor(ui::kColorBadgeForeground);
  canvas->DrawStringRect(kBadgeText, badge_font, foreground_color,
                         badge_text_bounds);
}

BEGIN_METADATA(EditorMenuGradientBadge, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
