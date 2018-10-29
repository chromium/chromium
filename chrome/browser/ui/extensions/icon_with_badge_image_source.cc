// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"

namespace {

gfx::ImageSkiaRep ScaleImageSkiaRep(const gfx::ImageSkiaRep& rep,
                                    int target_width_dp,
                                    float target_scale) {
  int width_px = target_width_dp * target_scale;
  return gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(rep.GetBitmap(),
                                    skia::ImageOperations::RESIZE_BEST,
                                    width_px, width_px),
      target_scale);
}

float GetBlockedActionBadgeRadius() {
  return 12.0f;
}

}  // namespace

IconWithBadgeImageSource::Badge::Badge(const std::string& text,
                                       SkColor text_color,
                                       SkColor background_color)
    : text(text), text_color(text_color), background_color(background_color) {}

IconWithBadgeImageSource::Badge::~Badge() {}

IconWithBadgeImageSource::IconWithBadgeImageSource(const gfx::Size& size)
    : gfx::CanvasImageSource(size, false) {}

IconWithBadgeImageSource::~IconWithBadgeImageSource() {}

void IconWithBadgeImageSource::SetIcon(const gfx::Image& icon) {
  icon_ = icon;
}

void IconWithBadgeImageSource::SetBadge(std::unique_ptr<Badge> badge) {
  badge_ = std::move(badge);
}

void IconWithBadgeImageSource::Draw(gfx::Canvas* canvas) {
  // TODO(https://crbug.com/842856): There should be a cleaner delineation
  // between what is drawn here and what is handled by the button itself.

  if (icon_.IsEmpty())
    return;

  if (paint_blocked_actions_decoration_)
    PaintBlockedActionDecoration(canvas);

  gfx::ImageSkia skia = icon_.AsImageSkia();
  gfx::ImageSkiaRep rep = skia.GetRepresentation(canvas->image_scale());
  if (rep.scale() != canvas->image_scale()) {
    skia.AddRepresentation(ScaleImageSkiaRep(
        rep, ExtensionAction::ActionIconSize(), canvas->image_scale()));
  }
  if (grayscale_)
    skia = gfx::ImageSkiaOperations::CreateHSLShiftedImage(skia, {-1, 0, 0.75});

  int x_offset =
      std::floor((size().width() - ExtensionAction::ActionIconSize()) / 2.0);
  int y_offset =
      std::floor((size().height() - ExtensionAction::ActionIconSize()) / 2.0);
  canvas->DrawImageInt(skia, x_offset, y_offset);

  // Draw a badge on the provided browser action icon's canvas.
  PaintBadge(canvas);

  if (paint_page_action_decoration_)
    PaintPageActionDecoration(canvas);
}

// Paints badge with specified parameters to |canvas|.
void IconWithBadgeImageSource::PaintBadge(gfx::Canvas* canvas) {
  if (!badge_ || badge_->text.empty())
    return;

  SkColor text_color = SkColorGetA(badge_->text_color) == SK_AlphaTRANSPARENT
                           ? SK_ColorWHITE
                           : badge_->text_color;

  // Make sure the background color is opaque. See http://crbug.com/619499
  SkColor background_color =
      SkColorGetA(badge_->background_color) == SK_AlphaTRANSPARENT
          ? gfx::kGoogleBlue500
          : SkColorSetA(badge_->background_color, SK_AlphaOPAQUE);

  constexpr int kBadgeHeight = 12;
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = rb->GetFontList(ui::ResourceBundle::BaseFont)
                                .DeriveWithHeightUpperBound(kBadgeHeight);
  base::string16 utf16_text = base::UTF8ToUTF16(badge_->text);

  // See if we can squeeze a slightly larger font into the badge given the
  // actual string that is to be displayed.
  constexpr int kMaxIncrementAttempts = 5;
  for (size_t i = 0; i < kMaxIncrementAttempts; ++i) {
    int w = 0;
    int h = 0;
    gfx::FontList bigger_font =
        base_font.Derive(1, 0, gfx::Font::Weight::NORMAL);
    gfx::Canvas::SizeStringInt(utf16_text, bigger_font, &w, &h, 0,
                               gfx::Canvas::NO_ELLIPSIS);
    if (h > kBadgeHeight)
      break;
    base_font = bigger_font;
  }

  constexpr int kMaxTextWidth = 23;
  const int text_width =
        std::min(kMaxTextWidth, canvas->GetStringWidth(utf16_text, base_font));
  // Calculate badge size. It is clamped to a min width just because it looks
  // silly if it is too skinny.
  constexpr int kPadding = 2;
  int badge_width = text_width + kPadding * 2;

  const gfx::Rect icon_area = GetIconAreaRect();

  // Force the pixel width of badge to be either odd (if the icon width is odd)
  // or even otherwise. If there is a mismatch you get http://crbug.com/26400.
  if (icon_area.width() != 0 && (badge_width % 2 != icon_area.width() % 2))
    badge_width += 1;
  badge_width = std::max(kBadgeHeight, badge_width);

  // The minimum width for center-aligning the badge.
  constexpr int kCenterAlignThreshold = 20;
  // Calculate the badge background rect. It is usually right-aligned, but it
  // can also be center-aligned if it is large.
  const int badge_offset_x = badge_width >= kCenterAlignThreshold
                                 ? (icon_area.width() - badge_width) / 2
                                 : icon_area.width() - badge_width;
  const int badge_offset_y = icon_area.height() - kBadgeHeight;
  gfx::Rect rect(icon_area.x() + badge_offset_x, icon_area.y() + badge_offset_y,
                 badge_width, kBadgeHeight);
  cc::PaintFlags rect_flags;
  rect_flags.setStyle(cc::PaintFlags::kFill_Style);
  rect_flags.setAntiAlias(true);
  rect_flags.setColor(background_color);

  // Clear part of the background icon.
  gfx::Rect cutout_rect(rect);
  cutout_rect.Inset(-1, -1);
  cc::PaintFlags cutout_flags = rect_flags;
  cutout_flags.setBlendMode(SkBlendMode::kClear);
  constexpr int kOuterCornerRadius = 3;
  canvas->DrawRoundRect(cutout_rect, kOuterCornerRadius, cutout_flags);

  // Paint the backdrop.
  canvas->DrawRoundRect(rect, kOuterCornerRadius - 1, rect_flags);

  // Paint the text.
  rect.Inset(std::max(kPadding, (rect.width() - text_width) / 2),
             kBadgeHeight - base_font.GetHeight(), kPadding, 0);
  canvas->DrawStringRect(utf16_text, base_font, text_color, rect);
}

void IconWithBadgeImageSource::PaintPageActionDecoration(gfx::Canvas* canvas) {
  const gfx::Rect icon_area = GetIconAreaRect();
  constexpr float kMajorRadius = 4.5;
  constexpr float kMinorRadius = 3;
  // This decoration is positioned at the bottom left corner of the icon area.
  gfx::PointF center_point = gfx::PointF(icon_area.bottom_left());
  center_point.Offset(kMajorRadius + 1, -kMajorRadius - 1);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SK_ColorTRANSPARENT);
  flags.setBlendMode(SkBlendMode::kSrc);
  canvas->DrawCircle(center_point, kMajorRadius, flags);
  constexpr SkColor decoration_color = SkColorSetARGB(255, 70, 142, 226);
  flags.setColor(decoration_color);
  canvas->DrawCircle(center_point, kMinorRadius, flags);
}

void IconWithBadgeImageSource::PaintBlockedActionDecoration(
    gfx::Canvas* canvas) {
  // To match the CSS notion of blur (spread outside the bounding box) to the
  // Skia notion of blur (spread outside and inside the bounding box), we have
  // to double the CSS-based blur values.
  constexpr int kBlurCorrection = 2;

  constexpr int kKeyShadowOpacity = 0x4D;  // 30%
  const gfx::ShadowValue key_shadow(
      gfx::Vector2d(0, 1), kBlurCorrection * 2 /*blur*/,
      SkColorSetA(gfx::kGoogleGrey800, kKeyShadowOpacity));

  constexpr int kAmbientShadowOpacity = 0x26;  // 15%
  const gfx::ShadowValue ambient_shadow(
      gfx::Vector2d(0, 2), kBlurCorrection * 6 /*blur*/,
      SkColorSetA(gfx::kGoogleGrey800, kAmbientShadowOpacity));

  const float blocked_action_badge_radius = GetBlockedActionBadgeRadius();

  // Sanity checking.
  const gfx::Rect icon_rect = GetIconAreaRect();
  DCHECK_LE(2 * blocked_action_badge_radius, icon_rect.width());
  DCHECK_EQ(icon_rect.width(), icon_rect.height());

  cc::PaintFlags paint_flags;
  paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(SK_ColorWHITE);
  paint_flags.setLooper(
      gfx::CreateShadowDrawLooper({key_shadow, ambient_shadow}));

  canvas->DrawCircle(gfx::PointF(icon_rect.CenterPoint()),
                     blocked_action_badge_radius, paint_flags);
}

gfx::Rect IconWithBadgeImageSource::GetIconAreaRect() const {
  gfx::Rect icon_area(size());
  icon_area.ClampToCenteredSize(ToolbarActionsBar::GetIconAreaSize());
  return icon_area;
}
